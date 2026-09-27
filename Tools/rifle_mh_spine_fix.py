"""Straighten the leaned-back torso of the MetaHuman-native rifle clips, geometrically, from the UE4 mocap.

The rifle set (/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_*) was batch-retargeted with the default retarget
pose: the torso sits 3-5 deg further back than the original mocap. Copying bone ORIENTATIONS from another
skeleton does not fix it (UE4 / SurvivalMan / MetaHuman bone axes differ: even an "exact 0.0 deg" UE4->MH
retarget leans -4.9 deg vs the source's -1.3). So this matches GEOMETRY instead: per frame, every MetaHuman
spine/neck segment is swung onto the direction of the UE4 source's spine curve at the same arc-length
fraction (3 UE4 spine bones vs 5 MetaHuman ones). Twist stays the clip's own; pelvis untouched (legs and foot
contacts); head and clavicles keep their current world orientation (gaze, rifle grip, aim as tuned).
Additive clips are skipped (their mesh-space deltas carry no absolute lean).

Edits stay IN MEMORY: never save these clips from Python (PoseSearch-indexed clips deadlock the editor on a
Python save) - save from the editor UI. Idempotent enough to re-run: directions come from the source.

MODE: 'pilot' (report only the listed clips), 'referenced' (all game-referenced clips), 'reload' (discard edits).
"""
import collections
import math
import unreal

MODE = 'pilot'
PILOT = ['W2_Walk_F_Loop_IPC', 'W2_Stand_Relaxed_To_Walk_F', 'W2_Walk_F_to_Stand_Relaxed_LU', 'W2_Stand_Relaxed_Idle_IPC',
         'W2_Stand_Relaxed_To_Walk_L90_Fwd', 'W2_Stand_Relaxed_To_Walk_R180_Fwd']

ML, APE = unreal.MathLibrary, unreal.AnimPoseExtensions
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
FOLDER = '/Game/AZ/Assets/M16/Riffle_RTG_MH'
MH_MESH = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
PACKS = [('/Game/Rifle_01/', '/Game/Rifle_01/Character/Mesh/SK_Mannequin'),
         ('/Game/MOCAP/Rifle_01/', '/Game/Rifle_01/Character/Mesh/SK_Mannequin'),
         ('/Game/RifleAnimsetPro/', '/Game/RifleAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin')]
MH_SPINE = ['spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01']   # 5 segments
MH_NECK = ['neck_01', 'neck_02', 'head']                                            # 2 segments
UE_SPINE = ['spine_01', 'spine_02', 'spine_03', 'neck_01']                          # 3 segments
UE_NECK = ['neck_01', 'head']
# Neck and head keep their own local rotations and ride the straightened chest. Re-aiming the neck at the UE4
# neck while holding the head's world orientation made the head bob forward/back like a goose on turns.
SWUNG = MH_SPINE[:-1]                        # bones whose rotation is rewritten from a segment direction
HELD = ['clavicle_l', 'clavicle_r']          # keep current world orientation (rifle grip / aim)


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property('optional_skeletal_mesh', mesh)
    o.set_editor_property('should_retarget', False)
    o.set_editor_property('extract_root_motion', True)
    return o


def qmul(a, b): return ML.multiply_quat_quat(a, b)
def qinv(q): return ML.quat_inversed(q)
def qrot(q, v): return ML.quat_rotate_vector(q, v)


def between(a, b):
    """Shortest rotation taking unit vector a onto unit vector b."""
    d = max(-1.0, min(1.0, a.x * b.x + a.y * b.y + a.z * b.z))
    axis = unreal.Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)
    n = math.sqrt(axis.x ** 2 + axis.y ** 2 + axis.z ** 2)
    if n < 1e-8:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    half = math.acos(d) * 0.5
    s = math.sin(half) / n
    return unreal.Quat(axis.x * s, axis.y * s, axis.z * s, math.cos(half))


def unit(v):
    n = math.sqrt(v.x ** 2 + v.y ** 2 + v.z ** 2)
    return unreal.Vector(v.x / n, v.y / n, v.z / n)


def fractions(lengths):
    total, acc, out = sum(lengths), 0.0, [0.0]
    for length in lengths:
        acc += length
        out.append(acc / total)
    return out


def curve_point(points, fracs, u):
    for i in range(len(points) - 1):
        if u <= fracs[i + 1] or i == len(points) - 2:
            span = max(1e-9, fracs[i + 1] - fracs[i])
            k = (u - fracs[i]) / span
            a, b = points[i], points[i + 1]
            return unreal.Vector(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k)


def hip_heading(pose):
    lat = APE.get_bone_pose(pose, 'thigh_r', W).translation - APE.get_bone_pose(pose, 'thigh_l', W).translation
    return math.atan2(lat.y, lat.x)


def yaw_quat(angle):
    return unreal.Quat(0.0, 0.0, math.sin(angle * 0.5), math.cos(angle * 0.5))


def target_dirs(sp, mh_fracs, ue_names, mh_count, facing):
    pts = [APE.get_bone_pose(sp, b, W).translation for b in ue_names]
    lens = [(pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)]
    ue_fracs = fractions(lens)
    return [qrot(facing, unit(curve_point(pts, ue_fracs, mh_fracs[i + 1]) - curve_point(pts, ue_fracs, mh_fracs[i])))
            for i in range(mh_count)]


def fix(target, source, source_mesh, mh, write=True):
    keys = unreal.AnimationLibrary.get_num_keys(target)
    length = target.get_play_length()
    if abs(length - source.get_play_length()) > 1.0 / 30.0:
        return 'length %.3f vs source %.3f' % (length, source.get_play_length())
    t_opts, s_opts = opts(mh), opts(source_mesh)
    tracks = {b: ([], [], []) for b in SWUNG + HELD}
    for frame in range(keys):
        t = length * frame / max(1, keys - 1)
        tp = APE.get_anim_pose_at_frame(target, frame, t_opts)
        sp = APE.get_anim_pose_at_time(source, min(t, source.get_play_length()), s_opts)
        local = {b: APE.get_bone_pose(tp, b, LOC) for b in MH_SPINE + MH_NECK + ['clavicle_l', 'clavicle_r']}
        spine_fracs = fractions([local[b].translation.length() for b in MH_SPINE[1:]])
        neck_fracs = fractions([local[b].translation.length() for b in MH_NECK[1:]])
        # ★ Turns: the MetaHuman clips carry the turn in the ROOT (extracted here) while the UE4 sources turn the
        # pelvis, so their frames drift apart by up to 180 deg over a pivot. Yaw the source onto the clip's own hip
        # heading every frame, or a forward lean becomes a backward one mid-turn (the 2026-09-23 "twists back").
        facing = yaw_quat(hip_heading(tp) - hip_heading(sp))
        spine_dirs = target_dirs(sp, spine_fracs, UE_SPINE, 5, facing)
        # The pelvis is left alone, so its own tilt still shifts where the spine starts. Swing the whole spine
        # about spine_01 until the overall pelvis->neck_01 direction equals the source's: that is the lean the
        # eye reads, and it removes the ~1 deg the per-segment match alone leaves.
        pelvis_pos = APE.get_bone_pose(tp, 'pelvis', W).translation
        spine_root = APE.get_bone_pose(tp, 'spine_01', W).translation
        neck_pos = spine_root
        for d, b in zip(spine_dirs, MH_SPINE[1:]):
            length_b = local[b].translation.length()
            neck_pos = unreal.Vector(neck_pos.x + d.x * length_b, neck_pos.y + d.y * length_b, neck_pos.z + d.z * length_b)
        src_lean = qrot(facing, unit(APE.get_bone_pose(sp, 'neck_01', W).translation - APE.get_bone_pose(sp, 'pelvis', W).translation))
        reach = (neck_pos - pelvis_pos).length()
        goal_neck = unreal.Vector(pelvis_pos.x + src_lean.x * reach, pelvis_pos.y + src_lean.y * reach, pelvis_pos.z + src_lean.z * reach)
        whole = between(unit(neck_pos - spine_root), unit(goal_neck - spine_root))
        dirs = [qrot(whole, d) for d in spine_dirs]
        chain = list(zip(MH_SPINE[:-1], MH_SPINE[1:]))
        parent = APE.get_bone_pose(tp, 'pelvis', W).rotation
        new_world = {}
        for (bone, child), goal in zip(chain, dirs):
            current = qmul(parent, local[bone].rotation)
            world = qmul(between(unit(qrot(current, local[child].translation)), goal), current)
            new_world[bone] = world
            tracks[bone][0].append(local[bone].translation)
            tracks[bone][1].append(qmul(qinv(parent), world))
            tracks[bone][2].append(local[bone].scale3d)
            parent = world
        for bone, holder in (('clavicle_l', 'spine_05'), ('clavicle_r', 'spine_05')):
            held = APE.get_bone_pose(tp, bone, W).rotation
            tracks[bone][0].append(local[bone].translation)
            tracks[bone][1].append(qmul(qinv(new_world[holder]), held))
            tracks[bone][2].append(local[bone].scale3d)
    if not write:
        return None
    target.modify()   # controller edits alone leave the package clean, and Save All would skip it
    controller = target.controller
    controller.open_bracket('Spine geometric re-target to UE4 mocap (lean-back fix)', False)
    try:
        for bone in SWUNG + HELD:
            p, r, s = tracks[bone]
            if not controller.set_bone_track_keys(unreal.Name(bone), p, r, s, False):
                return 'set_bone_track_keys failed on ' + bone
    finally:
        controller.close_bracket(False)
    return None


def inventory():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    by_name = collections.defaultdict(list)
    for a in registry.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')):
        by_name[str(a.asset_name)].append(a)
    options = unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
    rows = []
    for name, entries in sorted(by_name.items()):
        mh = next((a for a in entries if str(a.package_path) == FOLDER), None)
        if not mh or not name.startswith('AZ_RTG_MH_'):
            continue
        base = name[len('AZ_RTG_MH_'):]
        referenced = any('/Riffle_RTG_MH/' not in str(r) for r in (registry.get_referencers(mh.package_name, options) or []))
        source = mesh = None
        for prefix, pack_mesh in PACKS:
            source = next((a for a in by_name.get(base, []) if str(a.package_path).startswith(prefix)), None)
            if source:
                mesh = pack_mesh
                break
        rows.append((base, mh, source, mesh, referenced))
    return rows


def torso_lean(anim, mesh, n=8):
    """Forward(+)/back(-) lean of pelvis->neck_01 relative to the hips' own facing; returns (mean, worst frame)."""
    o, vals = opts(mesh), []
    for i in range(n + 1):
        pose = APE.get_anim_pose_at_time(anim, anim.get_play_length() * i / n, o)
        h = hip_heading(pose)
        fx, fy = -math.sin(h), math.cos(h)
        v = APE.get_bone_pose(pose, 'neck_01', W).translation - APE.get_bone_pose(pose, 'pelvis', W).translation
        vals.append(math.degrees(math.atan2(v.x * fx + v.y * fy, v.z)))
    return sum(vals) / len(vals), vals


def run():
    mh = unreal.load_asset(MH_MESH)
    rows = inventory()
    if MODE == 'reload':
        # Everything an earlier pass may have touched: clips with a SurvivalMan twin, the referenced set, the pilot.
        names = set(n for n in (unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_class(
            unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence'))) if str(n.asset_name).startswith('Riffle_P_'))
        twins = set(str(n.asset_name)[len('Riffle_P_'):] for n in names)
        packages = [r[1].get_asset().get_outermost() for r in rows if r[0] in twins or r[4] or r[0] in PILOT]
        unreal.EditorLoadingAndSavingUtils.reload_packages(packages)
        print('SPINEGEO reloaded %d packages from disk' % len(packages))
        return
    wanted = [r for r in rows if (r[0] in PILOT if MODE == 'pilot' else r[4])]
    done, skipped = 0, []
    for base, mh_data, source, mesh, _ in wanted:
        target = mh_data.get_asset()
        if target.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            skipped.append((base, 'additive'))
            continue
        if not source:
            skipped.append((base, 'no UE4 source'))
            continue
        src, src_mesh = source.get_asset(), unreal.load_asset(mesh)
        before = torso_lean(target, mh) if MODE == 'pilot' else None
        problem = fix(target, src, src_mesh, mh)
        if problem:
            skipped.append((base, problem))
            continue
        done += 1
        if MODE == 'pilot':
            s, a = torso_lean(src, src_mesh), torso_lean(target, mh)
            worst = max(abs(x - y) for x, y in zip(a[1], s[1]))
            print('SPINEGEO %-34s source=%+.1f  before=%+.1f  after=%+.1f  worst_frame_diff=%.1f' % (base, s[0], before[0], a[0], worst))
    dirty = sum(1 for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name().startswith(FOLDER + '/'))
    print('SPINEGEO %s: fixed=%d skipped=%d dirty_in_folder=%d' % (MODE, done, len(skipped), dirty))
    for base, why in skipped:
        if why != 'additive':
            print('SPINEGEO skip %s: %s' % (base, why))


if __name__ in ('__main__', 'spinegeo'):
    run()
