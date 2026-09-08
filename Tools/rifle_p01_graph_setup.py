# @Description: Prepare and wire the P01 aim mask and per-sample loop rate in the existing MHC AnimGraph.
"""Unreal Python authoring, in two explicit stages; this file never compiles/saves an AnimBP.

main() audits. main(stage='prepare') backs up the clean disk package, creates the
nodes, and exposes the inner PlayRate pin. Compile through the native editor route.
Create an AimAlpha variable getter through that same native route, then call
main(stage='wire', aim_alpha_getter_guid='...'). Compile/save through the native
route again and main(stage='verify') checks the recorded connections/properties.

AddVariableGetNode is deliberately NOT invoked here: its current implementation
calls ReconstructNode, which is unsafe from Python. The helper's C++/native editor
invocation is available to the caller. No preview, PIE, tests, source asset edits,
new caches, replacement BlendStack, or edits to the user's BSOutput cache chain.
"""

import gc
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


BLUEPRINT = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
OWNER = 'rifle_p01_graph_setup:v1'
TAG = 'P01BlendStackInput'
AG = unreal.AZ_AnimGraphNodeUtils
BG = unreal.AZ_BlueprintNodeUtils
DEFAULT_ANCHORS = {
    'cache_save': ('F4FF', 'AnimGraphNode_SaveCachedPose', 'AdiativePoses'),
    'cache_use': ('45418', 'AnimGraphNode_UseCachedPose', 'AdiativePoses'),
    'full_body': ('5BF', 'AnimGraphNode_Slot', 'FullBody'),
    'blend_stack': ('FE2714024C4D005AE05D6692993792E0', 'AnimGraphNode_BlendStack', ''),
    'blend_input': ('F8EDC5AB43265F0BEF2B8EBA66532DA9', 'AnimGraphNode_BlendStackInput', ''),
}
MAIN_CLASSES = {
    'aim': 'AnimGraphNode_RotationOffsetBlendSpace',
    'mask': 'AnimGraphNode_LayeredBoneBlend',
    'mask_base': 'AnimGraphNode_UseCachedPose',
}


def guid(value):
    return str(value).replace('-', '').replace('{', '').replace('}', '').upper()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def saved_root():
    return Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())).resolve() / 'RifleP01Graph'


def nodes(outer=None):
    lines = AG.list_blend_stack_graph_nodes(BLUEPRINT, outer) if outer else AG.list_anim_graph_nodes(BLUEPRINT)
    result = {}
    for line in lines:
        match = re.search(r'GUID=([A-Fa-f0-9-]+) Class=(\w+) Title=([\s\S]*)', str(line))
        require(match is not None, 'Unsupported node inventory format: ' + str(line))
        key = guid(match.group(1))
        require(key not in result, 'Duplicate node GUID in inventory: ' + key)
        result[key] = {'class': match.group(2), 'description': match.group(3)}
    return result


def properties(node, outer=''):
    result = {}
    for line in AG.inspect_anim_node_properties(BLUEPRINT, node, outer):
        key, separator, value = str(line).partition(' = ')
        require(separator, 'Unsupported node property response: ' + str(line))
        result[key] = value
    return result


def pin_dump():
    lines = [str(line) for line in BG.list_function_nodes(BLUEPRINT, 'AnimGraph')]
    require(lines, 'Cannot inspect the live AnimGraph pins')
    result, current = {}, None
    for line in lines:
        match = re.match(r'\s*\[([A-Fa-f0-9]{8})\] ', line)
        if match:
            current = match.group(1).upper()
            require(current not in result, 'Ambiguous abbreviated node GUID: ' + current)
            result[current] = []
        elif current and re.match(r'\s+(in|out)\s', line):
            result[current].append(line.strip())
    return result, lines


def incoming(node, pin):
    found, _ = pin_dump()
    lines = found.get(guid(node)[:8], [])
    matches = [line for line in lines if re.match(r'in\s+' + re.escape(pin) + r':', line)]
    require(len(matches) <= 1, 'Ambiguous input pin: ' + str(node) + '.' + pin)
    return re.findall(r'<-([A-Fa-f0-9]{8})\.([^\s]+)', matches[0]) if matches else []


def check_connection(source, source_pin, target, target_pin):
    wanted = [(guid(source)[:8], source_pin)]
    require(incoming(target, target_pin) == wanted,
            'Connection not verified: ' + str(source) + '.' + source_pin + ' -> ' + str(target) + '.' + target_pin)


def choose_anchor(inventory, specification):
    prefix, cls, title = specification
    candidates = [key for key, value in inventory.items()
                  if key.startswith(guid(prefix)) and value['class'] == cls and title in value['description']]
    require(len(candidates) == 1, 'Live anchor is missing or ambiguous: ' + repr(specification))
    return candidates[0]


def snapshot(anchors=None):
    for name in ('list_anim_graph_nodes', 'list_blend_stack_graph_nodes', 'inspect_anim_node_properties',
                 'add_anim_graph_node', 'set_anim_node_property', 'set_pin_binding', 'connect_pose_link',
                 'expose_anim_node_pin', 'add_blend_stack_graph_anim_node_ref',
                 'add_blend_stack_graph_function_call', 'connect_blend_stack_graph_pins'):
        require(hasattr(AG, name), 'Required native authoring API is not loaded: AZ_AnimGraphNodeUtils.' + name)
    for name in ('list_function_nodes', 'connect_nodes', 'set_pin_default_value'):
        require(hasattr(BG, name), 'Required native authoring API is not loaded: AZ_BlueprintNodeUtils.' + name)
    specs = dict(DEFAULT_ANCHORS)
    if anchors:
        for key, value in anchors.items():
            require(key in specs, 'Unknown anchor: ' + key)
            specs[key] = (value, specs[key][1], specs[key][2])
    main = nodes()
    resolved = {key: choose_anchor(main, spec) for key, spec in specs.items() if key != 'blend_input'}
    inner = nodes(resolved['blend_stack'])
    resolved['blend_input'] = choose_anchor(inner, specs['blend_input'])
    save = properties(resolved['cache_save'])
    use = properties(resolved['cache_use'])
    cache_name = save.get('CacheName', '').strip('"')
    require(cache_name == 'AdiativePoses', 'Save node no longer owns the expected cache')
    cache_reference = use.get('SaveCachedPoseNode', '')
    require(BLUEPRINT + '.' in cache_reference and ':AnimGraph.' in cache_reference,
            'Existing cache use has no inspectable in-package SaveCachedPoseNode reference')
    reference_path = re.search(r"'([^']+)'", cache_reference)
    require(reference_path is not None, 'Unsupported weak cached-pose reference format: ' + cache_reference)
    referenced_save = unreal.load_object(None, reference_path.group(1))
    require(referenced_save is not None and str(referenced_save.get_editor_property('cache_name')) == cache_name,
            'Existing cached-pose reference does not resolve to AdiativePoses')
    _, pins = pin_dump()
    return {'anchors': resolved, 'main': main, 'inner': inner, 'cache_reference': cache_reference,
            'cache_name': cache_name, 'pins': pins}


def write_receipt(path, receipt):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(receipt, indent=2), encoding='utf-8')


def backup_package():
    dirty = {str(package.get_path_name()) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(BLUEPRINT not in dirty, 'Target AnimBP is dirty; save its intended baseline through the native editor route before prepare')
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    original = (project / 'Content' / (BLUEPRINT[len('/Game/'):] + '.uasset')).resolve()
    original.relative_to((project / 'Content').resolve())
    require(original.is_file(), 'Missing on-disk AnimBP package: ' + str(original))
    folder = saved_root() / 'Backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
    files = []
    for extension in ('.uasset', '.uexp', '.ubulk', '.uptnl'):
        source = original.with_suffix(extension)
        if source.is_file():
            target = folder / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            fingerprint = hashlib.sha256(source.read_bytes()).hexdigest()
            require(hashlib.sha256(target.read_bytes()).hexdigest() == fingerprint, 'Backup checksum mismatch')
            files.append({'source': str(source), 'backup': str(target), 'sha256': fingerprint})
    return {'directory': str(folder), 'files': files}


def check_receipt(receipt, live):
    require(receipt.get('owner') == OWNER and receipt.get('blueprint') == BLUEPRINT, 'Unrecognized graph receipt')
    require(receipt['anchors'] == live['anchors'], 'Anchor GUIDs changed since preparation')
    require(receipt['cache_reference'] == live['cache_reference'], 'Original cached-pose reference changed')
    require(set(receipt['baseline_main']).issubset(live['main']), 'An original AnimGraph node disappeared')
    for key, node in receipt['created'].items():
        inventory = live['inner'] if key in ('rate_ref', 'rate_call') else live['main']
        require(node['guid'] in inventory and inventory[node['guid']]['class'] == node['class'],
                'Recorded node is missing or changed; refusing to create a duplicate: ' + key)


def set_property(node, name, value, outer=''):
    require(AG.set_anim_node_property(BLUEPRINT, node, name, value, outer), 'Failed property write: ' + name)


def prepare(receipt, path):
    a = receipt['anchors']
    for key, cls, x, y in (
            ('aim', MAIN_CLASSES['aim'], 300, 400),
            ('mask_base', MAIN_CLASSES['mask_base'], 300, 700),
            ('mask', MAIN_CLASSES['mask'], 650, 500)):
        if key not in receipt['created']:
            node = guid(AG.add_anim_graph_node(BLUEPRINT, '/Script/AnimGraph.' + cls, x, y))
            require(len(node) == 32, 'Failed node creation: ' + key)
            receipt['created'][key] = {'guid': node, 'class': cls}
            write_receipt(path, receipt)
        node = receipt['created'][key]['guid']
        set_property(node, 'NodeComment', 'P01 authored by ' + OWNER + ': ' + key)
    aim, mask, cache = (receipt['created'][key]['guid'] for key in ('aim', 'mask', 'mask_base'))
    set_property(cache, 'SaveCachedPoseNode', receipt['cache_reference'])
    set_property(cache, 'NameOfCache', receipt['cache_name'])
    set_property(aim, 'Alpha', '1.0')
    set_property(aim, 'BlendSpace', "/Script/Engine.AimOffsetBlendSpace'/Game/AZ/Blueprints/Animation/AO_Rifle_Aim.AO_Rifle_Aim'")
    require(BG.set_pin_default_value(BLUEPRINT, 'AnimGraph', aim, 'Alpha', '1.0'), 'AO Alpha pin unavailable')
    for pin, member in (('BlendSpace', 'WeaponAimOffset'), ('X', 'AimYaw'), ('Y', 'AimPitch')):
        require(AG.set_pin_binding(BLUEPRINT, aim, pin, member, False), 'AO binding failed: ' + pin)
    set_property(mask, 'BlendMode', 'BranchFilter')
    set_property(mask, 'LayerSetup', '((BranchFilters=((BoneName="spine_02",BlendDepth=1))))')
    set_property(mask, 'bMeshSpaceRotationBlend', 'True')
    set_property(mask, 'bRootSpaceRotationBlend', 'False')
    set_property(mask, 'bMeshSpaceScaleBlend', 'False')
    set_property(mask, 'bBlendRootMotionBasedOnRootBone', 'True')
    set_property(mask, 'CurveBlendOption', 'UseBasePose')
    set_property(mask, 'BlendWeights', '(0.0)')
    require(BG.set_pin_default_value(BLUEPRINT, 'AnimGraph', mask, 'BlendWeights_0', '0.0'), 'Mask weight pin unavailable')

    # Use reflection on the INNER wrapper, not SetAnimNodeTag (which searches only the main graph).
    set_property(a['blend_input'], 'Tag', TAG, a['blend_stack'])
    set_property(a['blend_input'], 'bOverridePlayRate', 'False', a['blend_stack'])
    require(AG.expose_anim_node_pin(BLUEPRINT, a['blend_input'], 'PlayRate', a['blend_stack']), 'Cannot expose inner PlayRate')
    for key, cls in (('rate_ref', 'K2Node_AnimNodeReference'), ('rate_call', 'K2Node_CallFunction')):
        if key not in receipt['created']:
            if key == 'rate_ref':
                node = AG.add_blend_stack_graph_anim_node_ref(BLUEPRINT, a['blend_stack'], -500, 250)
            else:
                target_class = unreal.load_class(None, '/Script/AZ.AZ_MoverAnimInstance')
                require(target_class is not None, 'Native MoverAnimInstance class unavailable')
                node = AG.add_blend_stack_graph_function_call(BLUEPRINT, a['blend_stack'], target_class,
                                                            'GetWeaponLoopPlayRate', -250, 250)
            require(len(guid(node)) == 32, 'Failed inner node creation: ' + key)
            receipt['created'][key] = {'guid': guid(node), 'class': cls}
            write_receipt(path, receipt)
    # The original cached use still feeds FullBody until the caller compiles and completes wire().
    check_connection(a['cache_use'], 'Pose', a['full_body'], 'Source')
    receipt['phase'] = 'prepared_awaiting_native_compile'
    receipt['next'] = 'Native compile; native AddVariableGetNode(BP, AnimGraph, AimAlpha); wire with its GUID'


def wire(receipt, alpha_guid):
    require(alpha_guid, 'Supply the AimAlpha getter GUID created through the native editor route, not Python AddVariableGetNode')
    a, created = receipt['anchors'], receipt['created']
    alpha_guid = guid(alpha_guid)
    inventory = nodes()
    require(alpha_guid in inventory and inventory[alpha_guid]['class'] == 'K2Node_VariableGet', 'AimAlpha getter GUID/class mismatch')
    require('Aim Alpha' in inventory[alpha_guid]['description'] or 'AimAlpha' in inventory[alpha_guid]['description'], 'Getter does not identify AimAlpha')
    inner = nodes(a['blend_stack'])
    require('PlayRate(In,' in inner[a['blend_input']]['description'], 'Compile has not materialized the inner PlayRate pin')
    allowed_sources = ([(a['cache_use'][:8], 'Pose')], [(created['mask']['guid'][:8], 'Pose')])
    require(incoming(a['full_body'], 'Source') in allowed_sources, 'FullBody input changed; inspect before rerouting')
    for source, target, pin in (
            (a['cache_use'], created['aim']['guid'], 'BasePose'),
            (created['mask_base']['guid'], created['mask']['guid'], 'BasePose'),
            (created['aim']['guid'], created['mask']['guid'], 'BlendPoses_0')):
        require(AG.connect_pose_link(BLUEPRINT, source, target, pin), 'Pose connection failed: ' + pin)
    require(BG.connect_nodes(BLUEPRINT, 'AnimGraph', alpha_guid, 'AimAlpha', created['mask']['guid'], 'BlendWeights_0'), 'AimAlpha weight connection failed')
    for source, source_pin, target, target_pin in (
            (created['rate_ref']['guid'], 'Value', created['rate_call']['guid'], 'BlendStackInput'),
            (created['rate_call']['guid'], 'ReturnValue', a['blend_input'], 'PlayRate')):
        require(AG.connect_blend_stack_graph_pins(BLUEPRINT, a['blend_stack'], source, source_pin, target, target_pin),
                'Inner rate connection failed: ' + target_pin)
    set_property(a['blend_input'], 'bOverridePlayRate', 'True', a['blend_stack'])
    # Replace the FullBody source only after every new branch has a valid source and weight.
    require(AG.connect_pose_link(BLUEPRINT, created['mask']['guid'], a['full_body'], 'Source'), 'FullBody reroute failed')
    receipt['aim_alpha_getter_guid'] = alpha_guid
    receipt['phase'] = 'wired_awaiting_native_compile_and_save'
    receipt['next'] = 'Native compile and save, then verify'


def verify(receipt):
    a, c = receipt['anchors'], receipt['created']
    for source, source_pin, target, target_pin in (
            (a['cache_use'], 'Pose', c['aim']['guid'], 'BasePose'),
            (c['mask_base']['guid'], 'Pose', c['mask']['guid'], 'BasePose'),
            (c['aim']['guid'], 'Pose', c['mask']['guid'], 'BlendPoses_0'),
            (receipt['aim_alpha_getter_guid'], 'AimAlpha', c['mask']['guid'], 'BlendWeights_0'),
            (c['mask']['guid'], 'Pose', a['full_body'], 'Source'),
            (c['rate_ref']['guid'], 'Value', c['rate_call']['guid'], 'BlendStackInput'),
            (c['rate_call']['guid'], 'ReturnValue', a['blend_input'], 'PlayRate')):
        check_connection(source, source_pin, target, target_pin)
    aim = properties(c['aim']['guid'])
    alpha = re.search(r'(?:^|[,\s(])Alpha=([+\-\d.eE]+)', '\n'.join(aim.values()))
    require(alpha is not None and float(alpha.group(1)) == 1.0, 'AO node must apply at Alpha=1; the mask owns AimAlpha')
    for pin, member in (('BlendSpace', 'WeaponAimOffset'), ('X', 'AimYaw'), ('Y', 'AimPitch')):
        require(aim.get('Binding:' + pin) == member + ' (prop)', 'Binding readback failed: ' + pin)
    mask = '\n'.join(properties(c['mask']['guid']).values())
    for token in ('spine_02', 'BlendDepth=1', 'bMeshSpaceRotationBlend=True', 'CurveBlendOption=UseBasePose', 'bBlendRootMotionBasedOnRootBone=True'):
        require(token in mask, 'Mask property readback failed: ' + token)
    inner = properties(a['blend_input'], a['blend_stack'])
    require(inner.get('Tag', '').strip('"') == TAG, 'Inner input tag mismatch')
    require('bOverridePlayRate=True' in '\n'.join(inner.values()), 'Inner play-rate override is not enabled')
    require(properties(c['rate_ref']['guid'], a['blend_stack']).get('Tag', '').strip('"') == TAG,
            'AnimNodeReference does not target the tagged INNER BlendStackInput')
    require(properties(c['mask_base']['guid']).get('SaveCachedPoseNode') == receipt['cache_reference'], 'New cached use points to a different save node')
    dirty = {str(package.get_path_name()) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(BLUEPRINT not in dirty, 'Graph connections verified in memory; native compile/save still required')
    receipt['phase'] = 'verified_saved_graph'
    receipt['next'] = 'Runtime animation behavior remains for user validation'


def main(stage='audit', receipt_path=None, anchors=None, aim_alpha_getter_guid=None):
    require(stage in ('audit', 'prepare', 'wire', 'verify'), 'Unknown graph stage')
    path = Path(receipt_path).resolve() if receipt_path else saved_root() / 'graph-receipt.json'
    receipt = json.loads(path.read_text(encoding='utf-8')) if path.is_file() else None
    try:
        live = snapshot(anchors)
        if receipt:
            check_receipt(receipt, live)
        if stage == 'audit':
            return {'status': 'read_only', 'live': live, 'receipt': receipt, 'receipt_path': str(path),
                    'unsupported_python_action': 'AddVariableGetNode calls ReconstructNode; create AimAlpha getter through native editor/C++'}
        if receipt is None:
            require(stage == 'prepare', 'Run prepare before wiring or verification')
            check_connection(live['anchors']['cache_use'], 'Pose', live['anchors']['full_body'], 'Source')
            require(not any(node['class'] == 'K2Node_AnimNodeReference' for node in live['inner'].values()),
                    'Existing unowned inner references would be affected by retagging; inspect them first')
            receipt = {'owner': OWNER, 'blueprint': BLUEPRINT, 'phase': 'preparing',
                       'anchors': live['anchors'], 'cache_reference': live['cache_reference'],
                       'cache_name': live['cache_name'], 'baseline_main': list(live['main']),
                       'baseline_snapshot': live, 'backup': backup_package(), 'created': {}}
            write_receipt(path, receipt)
        if stage == 'prepare':
            require(receipt['phase'] in ('preparing', 'prepared_awaiting_native_compile'), 'Do not prepare over an already wired graph')
            prepare(receipt, path)
        elif stage == 'wire':
            require(receipt['phase'] in ('prepared_awaiting_native_compile', 'wired_awaiting_native_compile_and_save'), 'Graph is not prepared')
            wire(receipt, aim_alpha_getter_guid or receipt.get('aim_alpha_getter_guid'))
        else:
            require(receipt['phase'] in ('wired_awaiting_native_compile_and_save', 'verified_saved_graph'), 'Graph is not wired')
            verify(receipt)
        receipt.pop('last_error', None)
        receipt['latest_snapshot'] = snapshot(anchors)
        write_receipt(path, receipt)
        return receipt
    except Exception as error:
        if receipt is not None and stage != 'audit':
            receipt['last_error'] = str(error)
            write_receipt(path, receipt)
        raise
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
