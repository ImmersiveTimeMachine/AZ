# -*- coding: utf-8 -*-
"""
Bake left-handed (mirrored) copies of the throwable animation set.

WHY A BAKE AND NOT THE AnimGraph Mirror NODE
--------------------------------------------
Five of the six throw clips carry root motion, and root motion is extracted from the MONTAGE, not from
the animation graph. A Mirror node in the graph would flip the pose and leave the root delta pointing the
old way, so the character would step the wrong direction while the arms did the right thing. A baked
asset has the mirrored root track inside it, which sidesteps the question entirely.

THE MATH IS A PORT, NOT AN INVENTION
------------------------------------
`FAnimationRuntime::MirrorPose` / `MirrorQuat` / `MirrorVector` from
Engine/Source/Runtime/Engine/Private/Animation/AnimationRuntime.cpp, transcribed 1:1. Mirroring is
authored in OBJECT space, so every local transform is rotated into component space using the reference
pose, mirrored, corrected for the difference in rest orientation between the two paired bones, and
rotated back. Anything less than that leaves wrists and clavicles twisted.
"""
import unreal

SKELETON   = '/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel'
MDT_PATH   = '/Game/AZ/Blueprints/Animation/Throwables/MDT_AZ_MHC_Mirror'
MDT_SOURCE = '/Game/AZ/Blueprints/Animation/AZ_Hero_MDT'   # duplicated only for its find/replace list
ANIM_DIR   = '/Game/AZ/Assets/Throwables/Anims'
SUFFIX     = '_Mirrored'

CLIPS = ['AZ_RTG_MH_Throw_Start', 'AZ_RTG_MH_ThrowLoop', 'AZ_RTG_MH_ThrowCancel',
         'AZ_RTG_MH_ThrowEndClose', 'AZ_RTG_MH_ThrowEndFar', 'AZ_RTG_MH_Throw_CarryIdle_v2']

EAL = unreal.EditorAssetLibrary
AL  = unreal.AnimationLibrary
APE = unreal.AnimPoseExtensions


# ----------------------------------------------------------------------------- mirror primitives
def mirror_vector_x(v):
    return unreal.Vector(-v.x, v.y, v.z)


def mirror_quat_x(q):
    # Mirroring flips both the axis AND the sign of the angle, which collapses to negating the two
    # components perpendicular to the mirror axis. See the derivation in AnimationRuntime.cpp.
    return unreal.Quat(q.x, -q.y, -q.z, q.w)


def q_mul(a, b):
    """a * b in Unreal's convention (apply b first, then a) - matches FQuat::operator*."""
    return unreal.Quat(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z)


def q_inv(q):
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)   # unit quaternions only, which ref rotations are


def q_rotate(q, v):
    """FQuat::RotateVector."""
    qv = unreal.Vector(q.x, q.y, q.z)
    t = qv.cross(v) * 2.0
    return v + t * q.w + qv.cross(t)


def q_unrotate(q, v):
    return q_rotate(q_inv(q), v)


# ----------------------------------------------------------------------------- the mirror data table
def build_pair_rows(bone_names):
    """One row per bone: its partner across the body, or ITSELF for anything on the centre line.

    A bone with no row is left completely untouched by FAnimationRuntime::MirrorPose (the lookup returns
    INDEX_NONE and both branches of the loop fall through), so the spine would keep leaning the old way
    while the arms swapped. Self-rows are what make a centre bone flip its own rotation.
    """
    names = set(bone_names)
    rows, seen = [], set()
    for b in bone_names:
        if b in seen:
            continue
        partner = None
        if b.endswith('_l') and (b[:-2] + '_r') in names:
            partner = b[:-2] + '_r'
        elif b.endswith('_r') and (b[:-2] + '_l') in names:
            partner = b[:-2] + '_l'
        elif b.startswith('l_') and ('r_' + b[2:]) in names:
            partner = 'r_' + b[2:]
        elif b.startswith('r_') and ('l_' + b[2:]) in names:
            partner = 'l_' + b[2:]
        if partner:
            rows.append((b, partner))
            rows.append((partner, b))
            seen.add(b); seen.add(partner)
        else:
            rows.append((b, b))
            seen.add(b)
    return rows


def ensure_mirror_table(bone_names):
    if EAL.does_asset_exist(MDT_PATH):
        t = unreal.load_asset(MDT_PATH)
    else:
        # Duplicated rather than created from the factory: UMirrorDataTableFactory::Skeleton is protected
        # and unreachable from Python, and calling the factory without it opens a modal skeleton picker
        # that would hang this call. The donor is only a carrier for the standard find/replace list.
        t = EAL.duplicate_asset(MDT_SOURCE, MDT_PATH)
        if not t:
            raise RuntimeError('could not duplicate %s -> %s' % (MDT_SOURCE, MDT_PATH))
    t.set_editor_property('skeleton', unreal.load_asset(SKELETON))
    t.set_editor_property('mirror_axis', unreal.AxisType.X)
    t.set_editor_property('mirror_root_motion', True)

    rows = build_pair_rows(bone_names)
    csv = ['---,Name,MirroredName,MirrorEntryType,bEnabled']
    for name, mirrored in rows:
        csv.append('%s,"%s","%s","Bone","True"' % (name, name, mirrored))
    ok = t.fill_from_csv_string("\n".join(csv))
    if not ok:
        raise RuntimeError('fill_from_csv_string rejected the mirror rows')
    pairs = sum(1 for a, b in rows if a != b)
    print('[MDT] %s  rows=%d (%d paired, %d centre)' % (MDT_PATH, len(rows), pairs, len(rows) - pairs))
    return t


# ----------------------------------------------------------------------------- skeleton scaffolding
def build_skeleton_info(seq, bone_names):
    """parent map + COMPONENT-space reference rotations, the two things MirrorPose needs."""
    # FName comparison is case-insensitive but Python's is not, and the two engine calls disagree on
    # casing for a handful of MetaHuman corrective bones ("upperarm_correctiveroot_r" from the track list
    # vs "upperarm_correctiveRoot_r" from the bone path). Everything is resolved through this index so a
    # parent lookup can never miss on capitalisation alone.
    canon = dict((b.lower(), b) for b in bone_names)

    parent = {}
    for b in bone_names:
        chain = [str(x) for x in AL.find_bone_path_to_root(seq, b)]
        # find_bone_path_to_root returns [bone, parent, ..., root]
        parent[b] = canon.get(chain[1].lower()) if len(chain) > 1 else None

    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property('extract_root_motion', False)
    opts.set_editor_property('incorporate_root_motion_into_pose', False)
    opts.set_editor_property('should_retarget', False)
    pose = APE.get_anim_pose_at_frame(seq, 0, opts)

    cs_ref_rot = {}
    for b in bone_names:
        # WORLD here means component space for a pose with no actor transform; this is the reference
        # pose, so the root-motion caveat about WORLD space does not apply.
        xf = APE.get_ref_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD)
        cs_ref_rot[b] = xf.rotation
    return parent, cs_ref_rot


# ----------------------------------------------------------------------------- FAnimationRuntime::MirrorPose
def mirror_transform(src_xf, src_parent, src_bone, tgt_parent, tgt_bone, cs):
    """One entry of the lambda inside FAnimationRuntime::MirrorPose, argument for argument."""
    src_parent_ref = cs[src_parent]
    tgt_parent_ref = cs[tgt_parent]
    src_bone_ref = cs[src_bone]
    tgt_bone_ref = cs[tgt_bone]

    t = src_xf.translation
    t = q_rotate(src_parent_ref, t)
    t = mirror_vector_x(t)
    t = q_unrotate(tgt_parent_ref, t)

    q = src_xf.rotation
    q = q_mul(src_parent_ref, q)
    q = mirror_quat_x(q)
    # The corrective term. Without it a mirrored left wrist inherits the RIGHT wrist's rest orientation
    # and the hand comes out rolled by whatever the two differ by in the bind pose.
    q = q_mul(q, q_mul(q_inv(mirror_quat_x(src_bone_ref)), tgt_bone_ref))
    q = q_mul(q_inv(tgt_parent_ref), q)

    # Assembled field by field rather than through the Rotator constructor: a Quat -> Rotator -> Quat
    # round trip loses precision, and this runs once per bone per frame over the whole set.
    out = unreal.Transform()
    out.translation = t
    out.rotation = q
    out.scale3d = src_xf.scale3d
    return out


def mirror_pose(local, mirror_of, parent, cs, root_bone):
    out = {}

    # The root is special: it has no parent to rotate through, so it mirrors in object space directly.
    # This is also where root MOTION gets mirrored, which is the whole reason for baking.
    rq = mirror_quat_x(local[root_bone].rotation)
    rq = q_mul(rq, q_mul(q_inv(mirror_quat_x(cs[root_bone])), cs[root_bone]))
    root_xf = unreal.Transform()
    root_xf.translation = mirror_vector_x(local[root_bone].translation)
    root_xf.rotation = rq
    root_xf.scale3d = local[root_bone].scale3d
    out[root_bone] = root_xf

    done = {root_bone}
    for tgt in local:
        if tgt in done:
            continue
        src = mirror_of.get(tgt)
        if src is None:
            out[tgt] = local[tgt]        # no row in the table: left exactly as authored
            done.add(tgt)
            continue
        if src == tgt:
            out[tgt] = mirror_transform(local[tgt], parent[tgt], tgt, parent[tgt], tgt, cs)
            done.add(tgt)
        else:
            out[tgt] = mirror_transform(local[src], parent[src], src, parent[tgt], tgt, cs)
            out[src] = mirror_transform(local[tgt], parent[tgt], tgt, parent[src], src, cs)
            done.add(tgt); done.add(src)
    return out


# ----------------------------------------------------------------------------- the bake
def bake(clip_name, bone_names, mirror_of, parent, cs, root_bone):
    src_path = '%s/%s' % (ANIM_DIR, clip_name)
    dst_path = src_path + SUFFIX
    if EAL.does_asset_exist(dst_path):
        EAL.delete_asset(dst_path)
    dst = EAL.duplicate_asset(src_path, dst_path)
    if not dst:
        raise RuntimeError('duplicate failed: ' + dst_path)

    src = unreal.load_asset(src_path)
    num_keys = AL.get_num_keys(src)

    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property('extract_root_motion', False)
    opts.set_editor_property('incorporate_root_motion_into_pose', False)
    opts.set_editor_property('should_retarget', False)

    tracks = dict((b, ([], [], [])) for b in bone_names)
    for frame in range(num_keys):
        pose = APE.get_anim_pose_at_frame(src, frame, opts)
        local = {}
        for b in bone_names:
            local[b] = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
        out = mirror_pose(local, mirror_of, parent, cs, root_bone)
        for b in bone_names:
            xf = out[b]
            pos, rot, scl = tracks[b]
            pos.append(xf.translation)
            rot.append(xf.rotation)
            scl.append(xf.scale3d)

    ctrl = dst.controller
    ctrl.open_bracket('Mirror ' + clip_name, True)
    try:
        for b in bone_names:
            pos, rot, scl = tracks[b]
            ctrl.set_bone_track_keys(b, pos, rot, scl, True)
    finally:
        ctrl.close_bracket(True)

    print('[bake] %-34s -> %s  keys=%d bones=%d' % (clip_name, dst.get_name(), num_keys, len(bone_names)))
    return dst


def run(clips=None):
    clips = clips or CLIPS
    probe = unreal.load_asset('%s/%s' % (ANIM_DIR, clips[0]))
    bone_names = [str(n) for n in AL.get_animation_track_names(probe)]
    root_bone = bone_names[0]

    ensure_mirror_table(bone_names)
    mirror_of = dict(build_pair_rows(bone_names))
    parent, cs = build_skeleton_info(probe, bone_names)
    print('[info] root=%s bones=%d parents_resolved=%d' % (
        root_bone, len(bone_names), sum(1 for b in bone_names if parent[b] or b == root_bone)))

    made = []
    for c in clips:
        made.append(bake(c, bone_names, mirror_of, parent, cs, root_bone))
    print('[done] %d mirrored clips. NOT saved from Python on purpose - use Save All in the editor.' % len(made))
    return made
