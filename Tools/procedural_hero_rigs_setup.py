# @Description: Prepare and adapt AZ-owned MetaHuman foot-placement and paired-hand rigs.
"""Run main('audit'), main('prepare'), main('author'), main('verify').

The optional main('graph') stage requires the newly built native snapshot and
AZ_ProceduralRigEditorUtils. It changes the hero graph in memory only. Compile
and save that AnimBlueprint through the native editor tools after this Python
call has returned. No PIE, gameplay tests, AnimBlueprint compile, reconstruction,
or AnimBlueprint save occurs here. Imported GASP assets are never edited.
"""
import gc
import hashlib
import json
import math
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/ProceduralHero'
BODY = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
ANIM_BP = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
FOLDER = '/Game/AZ/Blueprints/Animation/Procedural'
RIGS = {
    'feet': ('/Game/GameAnimationSample/Blueprints/ControlRigs/CR_Biped_FootPlacement',
             FOLDER + '/CR_AZ_MHC_FootPlacement'),
    'fullbody': ('/Game/GameAnimationSample/Characters/UEFN_Mannequin/Rigs/CR_UEFN_Mannequin_FullBodyIK',
                 FOLDER + '/CR_AZ_MHC_PairedHands'),
}
OWNER = 'procedural_hero_rigs_setup:v1'
OWNER_KEY = 'AZ.ProceduralRig.Owner'
FEET_BINDINGS = {
    'Alpha': 'ProceduralFeetAlpha',
    'Ground Normal': 'ProceduralGroundNormal',
    'GroundMovementWorldDelta': 'ProceduralBasedMovementDelta',
    'DoRaycast': 'bProceduralFeetRaycast',
    'Enable Foot Pinning': 'bProceduralFootPinning',
    'Enable Slope Warping': 'bProceduralSlopeWarping',
    'WorldZDamperEnabled': 'bProceduralSlopeWarping',
    'Has Teleported': 'bProceduralHasTeleported',
    'ForceReset': 'bProceduralFeetReset',
}
HAND_BINDINGS = {
    'Alpha': 'ProceduralInteractionAlpha',
    'hand_l_alpha': 'InteractionLeftHandAlpha',
    'hand_r_alpha': 'InteractionRightHandAlpha',
    'hand_l_target': 'InteractionLeftHandTargetCS',
    'hand_r_target': 'InteractionRightHandTargetCS',
}
FEET_CONSTANTS = {'UseIKBoneTargets': 'false', 'DebugDraw': 'false',
                  'MaxFootPinRadius': '40.0',
                  'FootContactLockThreshold': '0.97', 'Toe Length': '5.0',
                  'Hyper Extension Limit Factor': '0.98',
                  'Pelvis Smoothing Time': '0.2', 'Floor Smoothing Time': '0.05',
                  'PinBlendOutSmoothingTime': '0.2', 'RootDamperSmoothingTime': '0.15'}
EAL = unreal.EditorAssetLibrary
AG = unreal.AZ_AnimGraphNodeUtils
BG = unreal.AZ_BlueprintNodeUtils


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load(path):
    result = unreal.load_asset(path)
    require(result is not None, 'Missing asset: ' + path)
    return result


def editor(asset):
    return asset.get_editor_asset() if isinstance(asset, unreal.ControlRigRuntimeAsset) else asset


def write(name, value):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')


def read(name):
    return json.loads((OUTPUT / (name + '.json')).read_text(encoding='utf-8'))


def filename(path):
    require(path.startswith('/Game/'), 'Non-project package')
    result = (ROOT / 'Content' / (path[6:] + '.uasset')).resolve()
    require(result.is_relative_to((ROOT / 'Content').resolve()), 'Invalid package path')
    return result


def file_hash(path):
    file = filename(path)
    return hashlib.sha256(file.read_bytes()).hexdigest() if file.is_file() else None


def require_editor_idle():
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),
            'Stop PIE before authoring assets; this script never starts or stops PIE')


def bone_key(name):
    return unreal.RigElementKey(unreal.RigElementType.BONE, name)


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def normalized(v):
    size = math.sqrt(sum(x * x for x in vec(v)))
    require(size > 1e-5, 'Degenerate reference-pose axis')
    return unreal.Vector(v.x / size, v.y / size, v.z / size)


def vector_text(v):
    return '(X=%.9f,Y=%.9f,Z=%.9f)' % tuple(vec(v))


def rig_snapshot(path):
    asset = load(path)
    ed = editor(asset)
    h = ed.get_hierarchy()
    graphs = []
    # Include the function library and all contained graphs, not just the top graph.
    pending = list(ed.get_all_models())
    seen = set()
    while pending:
        graph = pending.pop(0)
        if graph.get_path_name() in seen:
            continue
        seen.add(graph.get_path_name())
        rows = []
        for node in graph.get_nodes():
            rows.append({'name': node.get_name(), 'class': node.get_class().get_name(),
                         'pins': [{'path': pin.get_pin_path(), 'value': pin.get_default_value()}
                                  for pin in node.get_pins()]})
            if hasattr(node, 'get_contained_graph'):
                child = node.get_contained_graph()
                if child:
                    pending.append(child)
        graphs.append({'name': graph.get_path_name(), 'nodes': rows})
    return {'path': path, 'class': asset.get_class().get_name(), 'sha256': file_hash(path),
            'bones': [str(k.name) for k in h.get_all_keys() if k.type == unreal.RigElementType.BONE],
            'graphs': graphs, 'owner': EAL.get_metadata_tag(asset, OWNER_KEY)}


def audit():
    modifier = unreal.SkeletonModifier()
    require(modifier.set_skeletal_mesh(load(BODY)), 'Cannot read actual MetaHuman mesh')
    bones = [str(x) for x in modifier.get_all_bone_names()]
    require(not any(x.startswith('ik_') for x in bones), 'Mesh IK-bone assumptions changed; review setup')
    result = {'utc': datetime.now(timezone.utc).isoformat(), 'body': BODY, 'body_sha256': file_hash(BODY),
              'bone_count': len(bones), 'source': {key: rig_snapshot(paths[0]) for key, paths in RIGS.items()},
              'target_hashes': {p: file_hash(p) for p in [ANIM_BP] + [x[1] for x in RIGS.values()]},
              'hero_graph': list(BG.list_function_nodes(ANIM_BP, 'AnimGraph'))}
    write('audit', result)
    return {'bone_count': len(bones), 'source_rigs': list(result['source']), 'targets': result['target_hashes']}


def prepare():
    require_editor_idle()
    baseline = read('audit')
    folder = OUTPUT / ('Backups/' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    folder.mkdir(parents=True, exist_ok=False)
    backups = {}
    for path, expected in baseline['target_hashes'].items():
        require(file_hash(path) == expected, 'Target changed since audit: ' + path)
        if expected:
            target = folder / (path.split('/')[-1] + '.uasset')
            shutil.copy2(filename(path), target)
            backups[path] = str(target)
    baseline['backups'] = backups
    write('prepared', baseline)
    return {'backup_folder': str(folder), 'backups': backups}


def set_pin(ed, graph, path, value):
    require(ed.get_controller(graph).set_pin_default_value(path, value, True, False, False, False, False),
            'Cannot author rig pin: ' + path)


def author_rig(kind, source, target, mesh):
    # The created receipt also permits recovery from an interrupted author stage.
    created_file = OUTPUT / 'created.json'
    created = read('created') if created_file.exists() else {}
    exists = EAL.does_asset_exist(target)
    require(not exists or (created.get(target) == source and
                           EAL.get_metadata_tag(load(target), OWNER_KEY) == OWNER),
            'Existing destination is not an interrupted asset owned by this setup: ' + target)
    asset = unreal.load_asset(target) if exists else EAL.duplicate_asset(source, target)
    require(asset is not None, 'Rig duplication failed: ' + target)
    require(target not in created or created[target] == source, 'Conflicting creation receipt')
    created[target] = source
    write('created', created)
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    ed = editor(asset)
    ed.set_auto_vm_recompile(False)
    h = ed.get_hierarchy()
    controls_before = {str(k.name) for k in h.get_all_keys() if k.type != unreal.RigElementType.BONE}
    imported = h.get_controller().import_bones_from_skeletal_mesh(mesh, '', True, True, False, False, False)
    bone_count = len([k for k in h.get_all_keys() if k.type == unreal.RigElementType.BONE])
    require(bone_count == 342, 'Actual MetaHuman hierarchy changed; audit required')
    require(controls_before <= {str(k.name) for k in h.get_all_keys()}, 'Bone import removed a rig control or curve')
    ed.set_preview_mesh(mesh, True)
    graph = ed.get_default_model()
    changed = []
    if kind == 'feet':
        # Every missing IK reference in the current source is a read, never a bone write.
        # The animated-toe path stays selected by UseIKBoneTargets=false. Remapping
        # also makes non-short-circuited early-out comparisons and debug reads valid.
        source_data = rig_snapshot(source)
        for desc in source_data['graphs']:
            relative = desc['name'].split(':', 1)[1]
            target_graph = unreal.load_object(None, ed.get_path_name() + ':' + relative)
            # Runtime editor assets have a different nested path; feet is legacy BP.
            require(target_graph is not None, 'Cannot find copied function graph: ' + relative)
            for node in desc['nodes']:
                for pin in node['pins']:
                    old = pin['value']
                    if 'Name="ik_' not in old:
                        continue
                    require(node['class'] == 'RigVMFunctionReferenceNode' or 'GetTransform' in node['name'],
                            'Unexpected IK bone write/reference; manual review needed: ' + node['name'])
                    new = old.replace('"ik_foot_l"', '"foot_l"').replace('"ik_foot_r"', '"foot_r"').replace('"ik_foot_root"', '"root"')
                    require('Name="ik_' not in new, 'Unmapped external IK bone')
                    set_pin(ed, target_graph, pin['path'], new)
                    changed.append(pin['path'])
        for side, suffix in [('l', ''), ('r', '_1')]:
            thigh = h.get_global_transform(bone_key('thigh_' + side), True)
            ball = h.get_global_transform(bone_key('ball_' + side), True)
            foot = h.get_global_transform(bone_key('foot_' + side), True)
            primary = normalized(h.get_local_transform(bone_key('calf_' + side), True).translation)
            forward_local = unreal.MathLibrary.inverse_transform_direction(thigh, unreal.Vector(0, 1, 0))
            dot = sum(a * b for a, b in zip(vec(primary), vec(forward_local)))
            secondary = normalized(forward_local - primary * dot)
            toe_direction = ball.translation - foot.translation
            toe_direction.z = 0.0
            toe_forward = normalized(unreal.MathLibrary.inverse_transform_direction(ball, normalized(toe_direction)))
            toe_up = normalized(unreal.MathLibrary.inverse_transform_direction(ball, unreal.Vector(0, 0, 1)))
            values = {'Solve Leg IK' + suffix + '.Primary Axis': primary,
                      'Solve Leg IK' + suffix + '.Secondary Axis': secondary,
                      'Update Leg Controls' + suffix + '.Toe Forward Axis': toe_forward,
                      'Update Leg Controls' + suffix + '.Toe Up Axis': toe_up,
                      'Update Foot Post Solve' + suffix + '.Toe Forward Axis': toe_forward}
            for path, value in values.items():
                set_pin(ed, graph, path, vector_text(value))
                changed.append(path)
        # Correct the source left-construction typo: both source legs named r floor null.
        set_pin(ed, graph, 'Init Leg.Smoothed Floor Null', '(Type=Null,Name="smoothed_floor_l_null")')
        # The source Reset helper repeats the same left/right copy error.
        reset_graph = unreal.find_object(None, ed.get_path_name() + ':RigVMFunctionLibrary.Reset.New Function_ContainedGraph')
        require(reset_graph is not None, 'Cannot find feet Reset graph')
        set_pin(ed, reset_graph, 'RigUnit_SetTransform_2.Item', '(Type=Null,Name="smoothed_floor_l_null")')
        cdo = unreal.get_default_object(ed.get_rig_vm_host_class())
        for name, value in {'UseIKBoneTargets': False, 'DebugDraw': False, 'DoRaycast': True,
                            'Enable Foot Pinning': False}.items():
            cdo.set_editor_property(name, value)
    else:
        set_pin(ed, graph, 'PBIK_1.Debug.bDrawDebug', 'false')
        # Hand controls are unparented and receive mesh-component-space targets.
        for side in ['l', 'r']:
            key = unreal.RigElementKey(unreal.RigElementType.CONTROL, 'hand_' + side + '_target')
            require(str(h.get_first_parent(key).name) in ['', 'None'], 'Paired hand target must remain unparented')
            initial = h.get_global_transform(bone_key('hand_' + side), True)
            h.set_global_transform(key, initial, True, True, False, False)
            h.set_global_transform(key, initial, False, True, False, False)
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    # RigVM compilation does not compile an AnimBlueprint or invoke Kismet GC.
    ed.set_auto_vm_recompile(True)
    ed.recompile_vm()
    require(EAL.save_asset(target, False), 'Failed to save owned rig: ' + target)
    return {'target': target, 'bone_count': bone_count, 'pins_changed': changed}


def author():
    require_editor_idle()
    require(not (OUTPUT / 'authored.json').exists(), 'Rigs already authored; verify instead of replacing later tuning')
    baseline = read('prepared')
    require(file_hash(BODY) == baseline['body_sha256'], 'Body changed since audit')
    for kind, (source, target) in RIGS.items():
        require(file_hash(source) == baseline['source'][kind]['sha256'], 'Source changed since audit')
        created = read('created') if (OUTPUT / 'created.json').exists() else {}
        require(file_hash(target) == baseline['target_hashes'][target] or
                (created.get(target) == source and EAL.get_metadata_tag(load(target), OWNER_KEY) == OWNER),
                'Target changed since prepare')
    mesh = load(BODY)
    result = {key: author_rig(key, source, target, mesh) for key, (source, target) in RIGS.items()}
    write('authored', result)
    return result


def graph_author():
    require_editor_idle()
    require(hasattr(unreal, 'AZ_ProceduralRigEditorUtils'), 'Build/restart to load native rig authoring bridge first')
    require(not (OUTPUT / 'graph-authored.json').exists(), 'Graph already authored; verify rather than duplicate nodes')
    for _, target in RIGS.values():
        require(EAL.get_metadata_tag(load(target), OWNER_KEY) == OWNER, 'Rig not prepared by this setup')
    rows = list(AG.list_anim_graph_nodes(ANIM_BP))
    require(not any('Class=AnimGraphNode_ControlRig ' in x for x in rows), 'Existing procedural nodes need review')
    history = [re.search(r'GUID=([A-Fa-f0-9-]+)', x).group(1) for x in rows if 'Class=AnimGraphNode_PoseSearchHistoryCollector ' in x]
    offset = [re.search(r'GUID=([A-Fa-f0-9-]+)', x).group(1) for x in rows if 'Class=AnimGraphNode_OffsetRootBone ' in x]
    require(len(history) == len(offset) == 1, 'Unexpected final pose graph topology')
    original = list(BG.list_function_nodes(ANIM_BP, 'AnimGraph'))
    history_index = next((i for i, line in enumerate(original)
                          if line.startswith('[' + history[0].replace('-', '')[:8] + ']')), -1)
    require(history_index >= 0 and history_index + 1 < len(original) and
            '<-' + offset[0].replace('-', '')[:8] + '.Pose' in original[history_index + 1],
            'PoseHistory no longer directly follows OffsetRootBone; preserve the new final pose path')
    write('graph-before', original)
    created = {}
    for kind, bindings, constants, x in [('feet', FEET_BINDINGS, FEET_CONSTANTS, 2800),
                                         ('fullbody', HAND_BINDINGS, {}, 3300)]:
        exposed = [name for name in list(bindings) + list(constants) if name != 'Alpha']
        callback = 'OnProceduralFeetBecameRelevant' if kind == 'feet' else unreal.Name('None')
        guid = unreal.AZ_ProceduralRigEditorUtils.add_control_rig_node(ANIM_BP, RIGS[kind][1], exposed, x, 0, callback)
        require(guid, 'Failed to create rig node ' + kind)
        require(AG.set_anim_node_tag(ANIM_BP, guid, 'AZProceduralFeet' if kind == 'feet' else 'AZPairedHands'), 'Cannot tag rig')
        for name, value in {'bSetRefPoseFromSkeleton': 'True', 'bTransferInputPose': 'True',
                            'bTransferInputCurves': 'True', 'bTransferPoseInGlobalSpace': 'False',
                            'bResetInputPoseToInitial': 'True', 'LODThreshold': '2'}.items():
            require(AG.set_anim_node_property(ANIM_BP, guid, name, value), 'Cannot configure node ' + name)
        getters = {}
        for i, (pin, native) in enumerate(bindings.items()):
            getter = unreal.AZ_ProceduralRigEditorUtils.connect_control_rig_input(ANIM_BP, guid, pin, native, x - 250, 180 + 70 * i)
            require(getter, 'Cannot wire native snapshot ' + native + ' -> ' + pin)
            getters[pin] = getter
        for name, value in constants.items():
            require(BG.set_pin_default_value(ANIM_BP, 'AnimGraph', guid, name, value), 'Cannot set rig input ' + name)
        created[kind] = {'guid': guid, 'getters': getters}
    # Change the live output path only after both rigs are configured completely.
    require(AG.connect_pose_link(ANIM_BP, offset[0], created['feet']['guid'], 'Source'), 'Feet source connection failed')
    require(AG.connect_pose_link(ANIM_BP, created['feet']['guid'], created['fullbody']['guid'], 'Source'), 'Hands source connection failed')
    require(AG.connect_pose_link(ANIM_BP, created['fullbody']['guid'], history[0], 'Source'), 'History connection failed')
    created['history'] = history[0]
    created['previous_source'] = offset[0]
    created['requires_native_compile_and_save'] = True
    write('graph-authored', created)
    return created


def verify():
    baseline = read('prepared')
    result = {}
    for kind, (source, target) in RIGS.items():
        require(file_hash(source) == baseline['source'][kind]['sha256'], 'Imported source asset changed')
        data = rig_snapshot(target)
        require(data['owner'] == OWNER, 'Owned-rig metadata missing')
        require(len(data['bones']) == 342 and not any(x.startswith('ik_') for x in data['bones']), 'Wrong rig hierarchy')
        require(not any('Name="ik_' in p['value'] for g in data['graphs'] for n in g['nodes'] for p in n['pins']), 'Missing IK bone reference remains')
        result[kind] = data
    result['hero_graph'] = list(BG.list_function_nodes(ANIM_BP, 'AnimGraph'))
    write('verified', result)
    return {'rigs': {k: {'bones': len(v['bones']), 'sha256': v['sha256']} for k, v in result.items() if k != 'hero_graph'},
            'graph_authored': (OUTPUT / 'graph-authored.json').exists(), 'requires_user_gameplay_check': True}


def main(stage='audit'):
    try:
        require(stage in ['audit', 'prepare', 'author', 'graph', 'verify'], 'Unknown stage')
        result = {'audit': audit, 'prepare': prepare, 'author': author, 'graph': graph_author, 'verify': verify}[stage]()
        print('AZ_PROCEDURAL_RIGS ' + stage + ' ' + json.dumps(result))
        return result
    finally:
        gc.collect()
