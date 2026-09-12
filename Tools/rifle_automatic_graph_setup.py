# @Description: Author the rifle firing slot before aim offset while preserving the existing animation graph.
"""Audit by default. prepare() creates/wires nodes without compiling or saving.

Compile with the dedicated native BlueprintTools tool after this Python call
returns, then save and verify. No PIE, previews or automated tests.
"""
import gc
import json
import runpy
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
H = runpy.run_path(str(ROOT / 'Tools/rifle_p01_graph_setup.py'), run_name='rifle_graph_helpers')
BP = H['BLUEPRINT']
AG = unreal.AZ_AnimGraphNodeUtils
BG = unreal.AZ_BlueprintNodeUtils
OWNER = 'rifle_automatic_graph_setup:v1'
RECEIPT = ROOT / 'Saved/RifleAutomatic/graph-receipt.json'
CACHE = 'RifleFireBase'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write(report):
    RECEIPT.parent.mkdir(parents=True, exist_ok=True)
    RECEIPT.write_text(json.dumps(report, indent=2), encoding='utf-8')


def audit():
    nodes = H['nodes']()
    anchors = {
        'source': H['choose_anchor'](nodes, ('F6B65611', 'AnimGraphNode_LinkedAnimLayer', 'AdditiveLeans')),
        'target': H['choose_anchor'](nodes, ('F4FF81E2', 'AnimGraphNode_SaveCachedPose', 'AdiativePoses')),
    }
    return dict(owner=OWNER, blueprint=BP, baseline_nodes=nodes, anchors=anchors,
                pins=H['pin_dump']()[1], created={})


def prepare():
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before graph authoring')
    report = json.loads(RECEIPT.read_text(encoding='utf-8')) if RECEIPT.exists() else None
    if report:
        require(report.get('owner') == OWNER and report['blueprint'] == BP, 'Unknown graph receipt')
        require(report['anchors'] == audit()['anchors'], 'Graph anchors changed')
    else:
        report = audit()
        H['check_connection'](report['anchors']['source'], 'Pose', report['anchors']['target'], 'Pose')
        report['backup'] = H['backup_package']()
        write(report)
    for key, cls, x, y in [
        ('cache', 'AnimGraphNode_SaveCachedPose', -1700, 350),
        ('base', 'AnimGraphNode_UseCachedPose', -1500, 350),
        ('slot_base', 'AnimGraphNode_UseCachedPose', -1500, 650),
        ('slot', 'AnimGraphNode_Slot', -1250, 650),
        ('mask', 'AnimGraphNode_LayeredBoneBlend', -950, 350),
    ]:
        if key not in report['created']:
            guid = H['guid'](AG.add_anim_graph_node(BP, '/Script/AnimGraph.' + cls, x, y))
            require(len(guid) == 32, 'Could not create ' + key)
            report['created'][key] = dict(guid=guid, cls=cls)
            write(report)
        H['set_property'](report['created'][key]['guid'], 'NodeComment', OWNER + ': ' + key)
    c = {k:v['guid'] for k,v in report['created'].items()}
    H['set_property'](c['cache'], 'CacheName', CACHE)
    blueprint = unreal.load_asset(BP)
    candidates = [n for n in unreal.ObjectIterator(unreal.AnimGraphNode_SaveCachedPose)
                  if n.get_outer().get_outer() == blueprint and str(n.get_editor_property('cache_name')) == CACHE]
    require(len(candidates) == 1, 'New cache object is ambiguous')
    reference = "/Script/AnimGraph.AnimGraphNode_SaveCachedPose'" + candidates[0].get_path_name() + "'"
    for key in ['base', 'slot_base']:
        H['set_property'](c[key], 'SaveCachedPoseNode', reference)
        H['set_property'](c[key], 'NameOfCache', CACHE)
    H['set_property'](c['slot'], 'SlotName', 'RifleFire')
    H['set_property'](c['slot'], 'bAlwaysUpdateSourcePose', 'True')
    for key, value in {
        'BlendMode':'BranchFilter',
        'LayerSetup':'((BranchFilters=((BoneName="spine_02",BlendDepth=1))))',
        'bMeshSpaceRotationBlend':'True', 'bRootSpaceRotationBlend':'False',
        'bMeshSpaceScaleBlend':'False', 'bBlendRootMotionBasedOnRootBone':'True',
        'CurveBlendOption':'UseBasePose', 'BlendWeights':'(1.0)',
    }.items():
        H['set_property'](c['mask'], key, value)
    require(BG.set_pin_default_value(BP, 'AnimGraph', c['mask'], 'BlendWeights_0', '1.0'), 'Mask weight pin missing')
    edges = [
        (report['anchors']['source'], c['cache'], 'Pose'),
        (c['base'], c['mask'], 'BasePose'),
        (c['slot_base'], c['slot'], 'Source'),
        (c['slot'], c['mask'], 'BlendPoses_0'),
    ]
    for source, target, pin in edges:
        require(AG.connect_pose_link(BP, source, target, pin), 'Cannot connect ' + pin)
    # Last mutation connects the complete masked branch to BOTH downstream aim paths
    # through the existing cache. Original caches, AO, FullBody and graph tail survive.
    require(AG.connect_pose_link(BP, c['mask'], report['anchors']['target'], 'Pose'), 'Cannot connect existing cache')
    report.update(status='wired_requires_native_compile_and_save', cache_reference=reference)
    write(report)
    gc.collect()
    return report


def verify():
    report = json.loads(RECEIPT.read_text(encoding='utf-8'))
    live = H['nodes']()
    require(set(report['baseline_nodes']).issubset(live), 'An original graph node disappeared')
    c = {k:v['guid'] for k,v in report['created'].items()}
    for source, target, pin in [
        (report['anchors']['source'], c['cache'], 'Pose'),
        (c['base'], c['mask'], 'BasePose'),
        (c['slot_base'], c['slot'], 'Source'),
        (c['slot'], c['mask'], 'BlendPoses_0'),
        (c['mask'], report['anchors']['target'], 'Pose'),
    ]:
        H['check_connection'](source, 'Pose', target, pin)
    for key in ['base','slot_base']:
        require(H['properties'](c[key]).get('SaveCachedPoseNode') == report['cache_reference'], 'Wrong cache reference')
    slot = '\n'.join(H['properties'](c['slot']).values())
    mask = '\n'.join(H['properties'](c['mask']).values())
    require('RifleFire' in slot and 'bAlwaysUpdateSourcePose=True' in slot, 'Slot configuration mismatch')
    for token in ['spine_02','BlendDepth=1','CurveBlendOption=UseBasePose','bBlendRootMotionBasedOnRootBone=True']:
        require(token in mask, 'Mask configuration mismatch: '+token)
    require(unreal.load_asset(BP).get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, 'AnimBP not compiled UpToDate')
    require(BP not in [p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()], 'AnimBP not saved')
    report['status'] = 'verified_compiled_saved'
    write(report)
    gc.collect()
    return report
