"""RifleMega mocap pack (UE4 Mannequin + weapon bone) -> hero MetaHuman, native clips.

Pipeline per clip (all measured against the ORIGINAL mocap, never against another retarget):
  1. IK Retargeter `RTG_RifleMega_UE4_to_MetaHuman` - FK only, numerically aligned target pose
     `MH_AlignedToRifleMega`: every 1:1 bone gets the source's world orientation exactly (0.00 deg measured).
  2. Post-process, per frame, written back into the clip's bone tracks:
     a. Spine: the 3-bone UE4 spine vs the 5-bone MetaHuman spine leaves the torso ~4 deg further back than the
        mocap. Each MetaHuman spine segment is swung onto the source spine curve at the same arc-length
        fraction, then the whole spine so pelvis->neck_01 equals the source (same method as
        Tools/rifle_mh_spine_fix.py, which the user accepted for the M16 set). Clavicles keep their world
        orientation; the neck rides the chest; the head keeps the mocap's gaze (relative to rest).
     b. Feet: foot/ball/head orientation is taken RELATIVE TO REST (flat mocap foot -> flat MetaHuman foot; the
        aligned pose copies the UE4 bone axes, which tilts the MetaHuman sole ~1.5 deg). Ankle targets come
        from the source's lowest SOLE point (heel or toe, from each skeleton's rest geometry, valid for any foot
        orientation incl. lying): horizontal = relative to root, scaled by the pelvis-height ratio (the same
        ratio the retargeter applies to root motion, so planted feet stay planted); vertical = the source's
        height. Two-bone leg IK; if a target is out of reach the pelvis is lowered just enough (smoothed).
     c. Left hand: the MetaHuman's different arm proportions move the left hand ~5 cm off the handguard. Its
        target is the source's hand_l expressed in the hand_r frame (the weapon is rigid in the right hand),
        blended back to FK when the hands are far apart (hand off the weapon).
     Two-bone IK = minimal swing + in-plane bend (twist-free; a pole-rebuilt frame spun straight limbs 90+ deg).
  3. Flags: loop by name (cycles) - reloads/turns also start and end on one pose; root motion when the root
     actually moves.

MODE:
  'retarget'  - batch retarget NAMES (or every clip) into /Game/AZ/Assets/RifleMega/<same subfolders>
  'post'      - post-process NAMES (or every clip) in place and save
  'measure'   - write quality numbers for NAMES to REPORT
  'all'       - retarget + post + save for NAMES (or every clip), in chunks
"""
import collections
import math
import time
import warnings

import unreal

warnings.simplefilter('ignore')

MODE = globals().get('MODE', 'measure')
NAMES = globals().get('NAMES', None)
REPORT = globals().get('REPORT', r'C:\UnrealEngine\Games\AZ\Saved\riflemega_report.txt')

# Overridable per run (2026-09-28: the same pipeline moves RifleAnimsetPro starts / stops / jumps / turns).
SRC_ROOT = globals().get('SRC_ROOT', '/Game/RifleMega_MocapAnimPack/AnimationsFBX/')
DST_ROOT = globals().get('DST_ROOT', '/Game/AZ/Assets/RifleMega/')
REST_SKEL = globals().get('REST_SKEL', 'Rifle_Mannequin_A_Skeleton')     # source mesh whose rest pose feeds the post
PREFIX = 'AZ_RTG_MH_'
RTG_PATH = '/Game/AZ/Blueprints/Animation/Retarget/RTG_RifleMega_UE4_to_MetaHuman'
HERO_PATH = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
_M = '/Game/RifleMega_MocapAnimPack/Demo/Models/'
MESH_BY_SKEL = {
    'Rifle_Mannequin_A_Skeleton': _M + 'Character/Mesh/Rifle_Mannequin_A',
    'Rifle_Auto_Mannequin_A_Skeleton': _M + 'forShootingReloading/Character_Automatic/Mesh/Rifle_Auto_Mannequin_A',
    'Rifle_DB_Mannequin_A_Skeleton': _M + 'forShootingReloading/Character_DoubleBarrel/Mesh/Rifle_DB_Mannequin_A',
    'Rifle_SG_Mannequin_A_Skeleton': _M + 'forShootingReloading/Character_ShotGun/Mesh/Rifle_SG_Mannequin_A',
    'Rifle_Winch_Mannequin_A_Skeleton': _M + 'forShootingReloading/Character_Winchester/Mesh/Rifle_Winch_Mannequin_A',
    'UE4_Mannequin_Skeleton': '/Game/RifleAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin'}

APE = unreal.AnimPoseExtensions
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
EAL = unreal.EditorAssetLibrary

# Target bones the post-process reads (parents always listed before children).
T_BONES = ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'neck_02', 'head',
           'clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l', 'clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r',
           'thigh_l', 'calf_l', 'foot_l', 'ball_l', 'thigh_r', 'calf_r', 'foot_r', 'ball_r']
S_BONES = ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'head', 'hand_l', 'hand_r',
           'thigh_l', 'thigh_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r']
MH_SPINE = ['spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01']
UE_SPINE = ['spine_01', 'spine_02', 'spine_03', 'neck_01']
WRITTEN = ['pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'head', 'clavicle_l', 'clavicle_r',
           'upperarm_l', 'lowerarm_l', 'hand_l', 'thigh_l', 'calf_l', 'foot_l', 'ball_l',
           'thigh_r', 'calf_r', 'foot_r', 'ball_r']
GRIP_FULL, GRIP_NONE = 50.0, 75.0      # cm between the hands: left-hand weapon IK full / off
REACH = 0.998                          # never straighten a limb completely (knee/elbow snap)


# ---------------------------------------------------------------- math (pure Python, UE conventions)
def v(x): return (x.x, x.y, x.z)
def q(x): return (x.x, x.y, x.z, x.w)
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def lerp(a, b, t): return add(a, mul(sub(b, a), t))


def unit(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(a): return (-a[0], -a[1], -a[2], a[3])


def qrot(a, p):
    r = qmul(qmul(a, (p[0], p[1], p[2], 0.0)), qinv(a))
    return (r[0], r[1], r[2])


def qnorm(a):
    n = math.sqrt(sum(c * c for c in a))
    return tuple(c / n for c in a)


def qang(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return math.degrees(2.0 * math.acos(min(1.0, d)))


def between(a, b):
    """Shortest rotation taking unit vector a onto unit vector b."""
    d = max(-1.0, min(1.0, dot(a, b)))
    axis = cross(a, b)
    n = length(axis)
    if n < 1e-9:
        return (0.0, 0.0, 0.0, 1.0)
    half = math.acos(d) * 0.5
    s = math.sin(half) / n
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


def quat_from_axes(x, y, z):
    """Rotation whose local X/Y/Z axes land on the given orthonormal world directions."""
    m00, m01, m02 = x[0], y[0], z[0]
    m10, m11, m12 = x[1], y[1], z[1]
    m20, m21, m22 = x[2], y[2], z[2]
    tr = m00 + m11 + m22
    if tr > 0.0:
        s = 0.5 / math.sqrt(tr + 1.0)
        return qnorm(((m21 - m12) * s, (m02 - m20) * s, (m10 - m01) * s, 0.25 / s))
    if m00 > m11 and m00 > m22:
        s = 2.0 * math.sqrt(1.0 + m00 - m11 - m22)
        return qnorm((0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s))
    if m11 > m22:
        s = 2.0 * math.sqrt(1.0 + m11 - m00 - m22)
        return qnorm(((m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s))
    s = 2.0 * math.sqrt(1.0 + m22 - m00 - m11)
    return qnorm(((m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s))


def frame(x_axis, normal):
    x = unit(x_axis)
    z = unit(sub(normal, mul(x, dot(normal, x))))
    y = cross(z, x)
    return quat_from_axes(x, y, z)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def yaw_quat(angle): return (0.0, 0.0, math.sin(angle * 0.5), math.cos(angle * 0.5))


# ---------------------------------------------------------------- skeleton helpers
def rest_of(mesh, bones):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -5000))
    c = a.skeletal_mesh_component
    c.set_skinned_asset_and_update(mesh)
    out = {}
    for b in bones:
        t = c.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT)
        out[b] = (v(t.translation), q(t.rotation))
    parents = {b: str(c.get_parent_bone(b)) for b in bones}
    a.destroy_actor()
    return out, parents


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property('optional_skeletal_mesh', mesh)
    o.set_editor_property('should_retarget', False)
    o.set_editor_property('extract_root_motion', False)
    return o


class Ctx(object):
    """Rest data shared by every clip."""

    def __init__(self):
        self.hero = unreal.load_asset(HERO_PATH)
        self.src_mesh = unreal.load_asset(MESH_BY_SKEL[REST_SKEL])
        rest_bones = ['pelvis', 'head', 'foot_l', 'foot_r', 'ball_l', 'ball_r']
        self.t_rest, _ = rest_of(self.hero, rest_bones)
        self.s_rest, _ = rest_of(self.src_mesh, rest_bones)
        _, self.parent = rest_of(self.hero, T_BONES)
        self.k = self.t_rest['pelvis'][0][2] / self.s_rest['pelvis'][0][2]     # retargeter's pelvis-height ratio
        self.s_sole = {side: sole_points(self.s_rest, side) for side in ('l', 'r')}
        self.t_sole = {side: sole_points(self.t_rest, side) for side in ('l', 'r')}
        self.meshes = {}

    def source_mesh(self, anim):
        skel = anim.get_editor_property('skeleton').get_name()
        if skel not in self.meshes:
            self.meshes[skel] = unreal.load_asset(MESH_BY_SKEL[skel])
        return self.meshes[skel]


def fk(local, parent):
    world = {}
    for b in T_BONES:
        lp, lr = local[b]
        p = parent.get(b)
        if p in world:
            pp, pr = world[p]
            world[b] = (add(pp, qrot(pr, lp)), qmul(pr, lr))
        else:
            world[b] = (lp, lr)
    return world


def hip_heading(pos):
    lat = sub(pos['thigh_r'], pos['thigh_l'])
    return math.atan2(lat[1], lat[0])


def fractions(lengths):
    total, acc, out = sum(lengths), 0.0, [0.0]
    for seg in lengths:
        acc += seg
        out.append(acc / total)
    return out


def curve_point(points, fracs, u):
    for i in range(len(points) - 1):
        if u <= fracs[i + 1] or i == len(points) - 2:
            span = max(1e-9, fracs[i + 1] - fracs[i])
            return lerp(points[i], points[i + 1], (u - fracs[i]) / span)


def two_bone(a, b, c, target, fallback_pole):
    """Upper/lower world-rotation deltas that put the chain end on target.

    Twist-free by construction: (1) the MINIMAL swing of the whole limb that points its end at the target, then
    (2) a bend strictly inside the limb's own plane. The earlier version rebuilt the limb frame from a pole and
    spun near-straight limbs about their own axis by up to 105 deg (the pole of a straight limb is noise).
    The bend side is the FK knee/elbow side; for a limb that is almost straight it fades to fallback_pole.
    """
    upper, lower = length(sub(b, a)), length(sub(c, b))
    to_t = sub(target, a)
    d = max(abs(upper - lower) + 0.01, min((upper + lower) * REACH, length(to_t)))
    direction = unit(to_t)
    swing = between(unit(sub(c, a)), direction)
    b1 = qrot(swing, sub(b, a))                               # swung upper bone, relative to a
    perp = sub(b1, mul(direction, dot(b1, direction)))
    fb = sub(fallback_pole, mul(direction, dot(fallback_pole, direction)))
    w = smoothstep(0.5, 3.0, length(perp))                    # how well the FK limb defines its own bend side
    pole = unit(add(mul(unit(perp), w), mul(unit(fb), 1.0 - w))) if length(perp) > 1e-6 else unit(fb)
    cos_a = max(-1.0, min(1.0, (upper * upper + d * d - lower * lower) / (2.0 * upper * d)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    b_new = mul(add(mul(direction, cos_a), mul(pole, sin_a)), upper)
    r_upper = qmul(between(unit(b1), unit(b_new)), swing)
    c_new = mul(direction, d)
    r_lower = between(unit(qrot(r_upper, sub(c, b))), unit(sub(c_new, b_new)))
    return r_upper, r_lower, length(to_t) - d


# ---------------------------------------------------------------- inventory
def inventory():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    rows = []
    for a in registry.get_assets_by_path(SRC_ROOT[:-1], recursive=True):
        if str(a.asset_class_path.asset_name) != 'AnimSequence':
            continue
        # Relative sub-folder; clips directly in SRC_ROOT have none. (2026-09-28: 'DST_ROOT + sub_path + /' built
        # '/Game/.../RifleAnimsetPro//AZ_RTG_MH_x' for a flat source folder -> engine fatal error on load, editor crash.)
        sub_path = str(a.package_path)[len(SRC_ROOT.rstrip('/')):].strip('/')
        folder = DST_ROOT.rstrip('/') + ('/' + sub_path if sub_path else '')
        name = str(a.asset_name)
        dst = folder + '/' + PREFIX + name
        if '//' in dst or '//' in folder:
            raise RuntimeError('bad destination path ' + dst)
        rows.append((name, str(a.package_name), folder, dst, a.get_tag_value('Skeleton').split('.')[-1].rstrip("'")))
        ASSET_DATA[name] = a
    rows.sort()
    return rows


ASSET_DATA = {}


def select(rows):
    if not NAMES:
        return rows
    wanted = set(NAMES)
    return [r for r in rows if r[0] in wanted]


# ---------------------------------------------------------------- 1. retarget
def retarget(rows):
    rtg = unreal.load_asset(RTG_PATH)
    hero = unreal.load_asset(HERO_PATH)
    groups = collections.defaultdict(list)
    for name, src, folder, dst, skel in rows:
        groups[(folder, skel)].append(ASSET_DATA[name])
    made = 0
    for (folder, skel), assets in sorted(groups.items()):
        inp = unreal.IKRetargetBatchOperationInputs()
        inp.set_editor_property('assets_to_retarget', assets)
        inp.set_editor_property('source_mesh', unreal.load_asset(MESH_BY_SKEL[skel]))
        inp.set_editor_property('target_mesh', hero)
        inp.set_editor_property('ik_retarget_asset', rtg)
        inp.set_editor_property('prefix', PREFIX)
        inp.set_editor_property('target_path', folder)
        inp.set_editor_property('use_source_path', False)
        inp.set_editor_property('include_referenced_assets', False)
        inp.set_editor_property('overwrite_existing_files', True)
        made += len(unreal.IKRetargetBatchOperation.run_batch_retarget(inp))
    return made


# ---------------------------------------------------------------- 2. post-process
def source_frames(ctx, source, keys, length_s):
    so = opts(ctx.source_mesh(source))
    out = []
    for f in range(keys):
        t = min(length_s * f / max(1, keys - 1), source.get_play_length())
        pose = APE.get_anim_pose_at_time(source, t, so)
        row = {}
        for b in S_BONES:
            x = APE.get_bone_pose(pose, b, W)
            row[b] = (v(x.translation), q(x.rotation))
        out.append(row)
    return out


def target_frames(ctx, target, keys):
    to = opts(ctx.hero)
    out = []
    for f in range(keys):
        pose = APE.get_anim_pose_at_frame(target, f, to)
        local = {}
        for b in T_BONES:
            t = APE.get_bone_pose(pose, b, LOC)
            local[b] = (v(t.translation), q(t.rotation))
        out.append(local)
    return out


def sole_points(rest, side):
    """Heel and toe contact points in the FOOT bone's frame: the ground points under the ankle and under the
    ball in the rest pose (both meshes rest standing flat on z=0)."""
    ankle_p, ankle_r = rest['foot_' + side]
    ball_p = rest['ball_' + side][0]
    heel = qrot(qinv(ankle_r), sub((ankle_p[0], ankle_p[1], 0.0), ankle_p))
    toe = qrot(qinv(ankle_r), sub((ball_p[0], ball_p[1], 0.0), ankle_p))
    return heel, toe


def ankle_target(ctx, s, t_world, side, foot_rot):
    """Ankle target for one leg, from the source mocap.

    The source's LOWEST sole point (heel or toe, from each skeleton's own rest geometry, so it holds for any foot
    orientation - standing, rolling, lying on the back) is reproduced by the target's lowest sole point: a flat
    foot stays flat, a rolling foot pivots on the toe, a heel strike lands on the heel, nothing sinks into the
    floor. Horizontally that contact point is the source's, relative to root, scaled by the retargeter's ratio,
    so a planted point stays planted under the scaled root motion.
    """
    k = ctx.k
    s_root_p, s_root_r = s['root']
    t_root_p, t_root_r = t_world['root']
    rel = qmul(t_root_r, qinv(s_root_r))
    s_heel_l, s_toe_l = ctx.s_sole[side]
    t_heel_l, t_toe_l = ctx.t_sole[side]
    ankle_s, rot_s = s['foot_' + side]
    heel_s, toe_s = add(ankle_s, qrot(rot_s, s_heel_l)), add(ankle_s, qrot(rot_s, s_toe_l))
    oh, ot = qrot(foot_rot, t_heel_l), qrot(foot_rot, t_toe_l)
    w_src = smoothstep(-0.5, 0.5, heel_s[2] - toe_s[2])       # 1 = the source stands on its toe
    w_tgt = smoothstep(-0.5, 0.5, oh[2] - ot[2])              # 1 = the target's toe is its lowest point
    contact_s = lerp(heel_s, toe_s, w_src)
    offset_t = lerp(oh, ot, w_src)
    h = qrot(rel, mul(sub(contact_s, s_root_p), k))
    z = min(heel_s[2], toe_s[2]) * k - (oh[2] + (ot[2] - oh[2]) * w_tgt)
    return (t_root_p[0] + h[0] - offset_t[0], t_root_p[1] + h[1] - offset_t[1], z)


def rest_relative(ctx, s_rot, bone):
    """World rotation for a target bone such that it sits on the source's pose relative to each one's rest."""
    return qmul(qmul(s_rot, qinv(ctx.s_rest[bone][1])), ctx.t_rest[bone][1])


def post(ctx, source, target):
    keys = unreal.AnimationLibrary.get_num_keys(target)
    length_s = target.get_play_length()
    src = source_frames(ctx, source, keys, length_s)
    tgt = target_frames(ctx, target, keys)
    parent = ctx.parent
    # self-check: our FK must reproduce the engine's world pose
    pose0 = APE.get_anim_pose_at_frame(target, 0, opts(ctx.hero))
    w0 = fk(tgt[0], parent)
    err = max(length(sub(w0[b][0], v(APE.get_bone_pose(pose0, b, W).translation))) for b in T_BONES)
    if err > 0.05:
        raise RuntimeError('FK mismatch %.3f cm on %s' % (err, target.get_name()))

    # pass 1: ankle targets and the pelvis drop needed to reach them
    plans, need = [], []
    for f in range(keys):
        world = fk(tgt[f], parent)
        s = src[f]
        drop = 0.0
        legs = {}
        for side in ('l', 'r'):
            foot_rot = rest_relative(ctx, s['foot_' + side][1], 'foot_' + side)
            ankle = ankle_target(ctx, s, world, side, foot_rot)
            legs[side] = (ankle, foot_rot)
            hip = world['thigh_' + side][0]
            reach = (length(tgt[f]['calf_' + side][0]) + length(tgt[f]['foot_' + side][0])) * REACH
            dx, dy, dz = ankle[0] - hip[0], ankle[1] - hip[1], hip[2] - ankle[2]
            h2 = dx * dx + dy * dy
            if dx * dx + dy * dy + dz * dz > reach * reach and reach * reach > h2:
                drop = max(drop, dz - math.sqrt(reach * reach - h2))
        plans.append(legs)
        need.append(drop)
    # smooth the drop: widen (moving max) then soften (moving average), wrapping for loops
    looped = is_loop(src)
    n = len(need)

    def at(arr, i):
        return arr[i % n] if looped else arr[max(0, min(n - 1, i))]

    widened = [max(at(need, i + j) for j in range(-3, 4)) for i in range(n)]
    drops = [sum(at(widened, i + j) for j in range(-3, 4)) / 7.0 for i in range(n)]

    # pass 2: apply
    tracks = {b: ([], [], []) for b in WRITTEN}
    worst_miss = 0.0
    for f in range(keys):
        local = dict(tgt[f])
        s = src[f]
        root_p, root_r = local['root']
        # pelvis drop (world Z) expressed in the root frame
        pp, pr = local['pelvis']
        local['pelvis'] = (add(pp, qrot(qinv(root_r), (0.0, 0.0, -drops[f]))), pr)
        world = fk(local, parent)

        # a. spine
        pos = {b: world[b][0] for b in T_BONES}
        s_pos = {b: s[b][0] for b in S_BONES}
        facing = yaw_quat(hip_heading(pos) - hip_heading(s_pos))
        seg = [length(local[b][0]) for b in MH_SPINE[1:]]
        fr = fractions(seg)
        pts = [s_pos[b] for b in UE_SPINE]
        ue_fr = fractions([length(sub(pts[i + 1], pts[i])) for i in range(len(pts) - 1)])
        dirs = [qrot(facing, unit(sub(curve_point(pts, ue_fr, fr[i + 1]), curve_point(pts, ue_fr, fr[i])))) for i in range(5)]
        spine_root = pos['spine_01']
        neck = spine_root
        for d_, l_ in zip(dirs, seg):
            neck = add(neck, mul(d_, l_))
        src_lean = qrot(facing, unit(sub(s_pos['neck_01'], s_pos['pelvis'])))
        goal_neck = add(pos['pelvis'], mul(src_lean, length(sub(neck, pos['pelvis']))))
        whole = between(unit(sub(neck, spine_root)), unit(sub(goal_neck, spine_root)))
        dirs = [qrot(whole, d_) for d_ in dirs]
        held = {b: world[b][1] for b in ('clavicle_l', 'clavicle_r')}
        par = world['pelvis'][1]
        for (bone, child), goal in zip(zip(MH_SPINE[:-1], MH_SPINE[1:]), dirs):
            cur = qmul(par, local[bone][1])
            new = qmul(between(unit(qrot(cur, local[child][0])), goal), cur)
            local[bone] = (local[bone][0], qmul(qinv(par), new))
            par = new
        for b in ('clavicle_l', 'clavicle_r'):
            local[b] = (local[b][0], qmul(qinv(par), held[b]))
        world = fk(local, parent)
        # the neck rides the straightened chest, the head keeps the mocap's gaze (relative to rest, so a
        # straight-ahead look stays straight ahead on the MetaHuman's own head axes)
        local['head'] = (local['head'][0], qmul(qinv(world['neck_02'][1]), rest_relative(ctx, s['head'][1], 'head')))
        world = fk(local, parent)

        # c. left hand on the weapon, relative to the right hand
        hr_t, hr_s, hl_s = world['hand_r'], s['hand_r'], s['hand_l']
        rel_rot = qmul(hr_t[1], qinv(hr_s[1]))
        grip_pos = add(hr_t[0], qrot(rel_rot, sub(hl_s[0], hr_s[0])))
        grip_rot = qmul(rel_rot, hl_s[1])
        w_grip = 1.0 - smoothstep(GRIP_FULL, GRIP_NONE, length(sub(hl_s[0], hr_s[0])))
        goal = lerp(world['hand_l'][0], grip_pos, w_grip)
        ru, rl, _ = two_bone(world['upperarm_l'][0], world['lowerarm_l'][0], world['hand_l'][0], goal, (0.0, -1.0, -1.0))
        up_w = qmul(ru, world['upperarm_l'][1])
        lo_w = qmul(rl, qmul(ru, world['lowerarm_l'][1]))
        local['upperarm_l'] = (local['upperarm_l'][0], qmul(qinv(world['clavicle_l'][1]), up_w))
        local['lowerarm_l'] = (local['lowerarm_l'][0], qmul(qinv(up_w), lo_w))
        local['hand_l'] = (local['hand_l'][0], qmul(qinv(lo_w), grip_rot))

        # b. legs and feet
        for side in ('l', 'r'):
            ankle, foot_rot = plans[f][side]
            th, ca, fo = world['thigh_' + side], world['calf_' + side], world['foot_' + side]
            toes = sub(world['ball_' + side][0], fo[0])      # a straight knee bends where the foot points
            ru, rl, miss = two_bone(th[0], ca[0], fo[0], ankle, unit(toes))
            worst_miss = max(worst_miss, miss)
            th_w = qmul(ru, th[1])
            ca_w = qmul(rl, qmul(ru, ca[1]))
            ball_rot = rest_relative(ctx, s['ball_' + side][1], 'ball_' + side)
            local['thigh_' + side] = (local['thigh_' + side][0], qmul(qinv(world['pelvis'][1]), th_w))
            local['calf_' + side] = (local['calf_' + side][0], qmul(qinv(th_w), ca_w))
            local['foot_' + side] = (local['foot_' + side][0], qmul(qinv(ca_w), foot_rot))
            local['ball_' + side] = (local['ball_' + side][0], qmul(qinv(foot_rot), ball_rot))

        for b in WRITTEN:
            p_, r_ = local[b]
            tracks[b][0].append(unreal.Vector(*p_))
            tracks[b][1].append(unreal.Quat(*qnorm(r_)))
            tracks[b][2].append(unreal.Vector(1.0, 1.0, 1.0))

    target.modify()
    controller = target.controller
    controller.open_bracket('RifleMega post-process (spine, feet, left hand)', False)
    try:
        for b in WRITTEN:
            p_, r_, s_ = tracks[b]
            if not controller.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                raise RuntimeError('set_bone_track_keys failed on %s / %s' % (target.get_name(), b))
    finally:
        controller.close_bracket(False)
    return max(drops), worst_miss, looped


LOOP_WORDS = ('_Idle', '_Walk', '_Run', '_Sprint', 'Circle', 'Cirlce', '_Loop')
NOT_LOOP_WORDS = ('_to_', 'Jump', 'Turn', 'Start', 'End', 'Stop', 'Land')


def is_cycle_name(name):
    # Explicit cycles first: turn-in-place loops ('..._90Loop') and the platformer fall loop (RifleAnimsetPro).
    if name.endswith('Loop') or name.endswith('_Fall'):
        return True
    return any(w in name for w in LOOP_WORDS) and not any(w in name for w in NOT_LOOP_WORDS)


def is_loop(src):
    """First and last source frames match (root-relative) - the pose side of a cycle."""
    a, b = src[0], src[-1]
    ra, rb = a['root'], b['root']
    worst = 0.0
    for bone in ('pelvis', 'hand_l', 'hand_r', 'foot_l', 'foot_r', 'neck_01'):
        pa = qrot(qinv(ra[1]), sub(a[bone][0], ra[0]))
        pb = qrot(qinv(rb[1]), sub(b[bone][0], rb[0]))
        worst = max(worst, length(sub(pa, pb)))
    return worst < 1.5 and len(src) > 8


def flags(ctx, source, target):
    so = opts(ctx.source_mesh(source))
    p0 = APE.get_anim_pose_at_time(source, 0.0, so)
    p1 = APE.get_anim_pose_at_time(source, source.get_play_length(), so)
    r0, r1 = APE.get_bone_pose(p0, 'root', W), APE.get_bone_pose(p1, 'root', W)
    moved = length(sub(v(r1.translation), v(r0.translation))) > 1.0 or qang(q(r0.rotation), q(r1.rotation)) > 1.0
    cycle = is_cycle_name(source.get_name())
    target.set_editor_property('loop', bool(cycle))
    target.set_editor_property('enable_root_motion', bool(moved))
    return moved, cycle


# ---------------------------------------------------------------- 3. measure
def measure(ctx, source, target):
    so, to = opts(ctx.source_mesh(source)), opts(ctx.hero)
    keys = unreal.AnimationLibrary.get_num_keys(target)
    length_s = target.get_play_length()
    lean, fl_a, slide, src_slide, grip, orient = [], [], [], [], [], {}
    prev = None
    for f in range(keys):
        t = length_s * f / max(1, keys - 1)
        ps = APE.get_anim_pose_at_time(source, min(t, source.get_play_length()), so)
        pt = APE.get_anim_pose_at_frame(target, f, to)
        g = lambda p, b: APE.get_bone_pose(p, b, W)

        def lean_of(p):
            lat = sub(v(g(p, 'thigh_r').translation), v(g(p, 'thigh_l').translation))
            h = math.atan2(lat[1], lat[0])
            vec = sub(v(g(p, 'neck_01').translation), v(g(p, 'pelvis').translation))
            fwd = vec[0] * math.sin(h) - vec[1] * math.cos(h)
            return math.degrees(math.atan2(fwd, vec[2]))
        lean.append(lean_of(pt) - lean_of(ps))
        cur = {}
        for side in ('l', 'r'):
            fs, ft = g(ps, 'foot_' + side), g(pt, 'foot_' + side)
            sa = add(v(fs.translation), qrot(q(fs.rotation), ctx.s_sole[side][0]))     # heel
            sb = add(v(fs.translation), qrot(q(fs.rotation), ctx.s_sole[side][1]))     # toe
            ta = add(v(ft.translation), qrot(q(ft.rotation), ctx.t_sole[side][0]))
            tb = add(v(ft.translation), qrot(q(ft.rotation), ctx.t_sole[side][1]))
            ra, rb, ha, hb = sa[2], sb[2], ta[2], tb[2]
            toe = rb < ra
            cur[side] = (sb if toe else sa, tb if toe else ta, toe)
            if min(ra, rb) < 0.7:     # source foot in ground contact
                fl_a.append(min(ha, hb) - min(ra, rb) * ctx.k)
                if prev and prev[side][2] == toe:
                    s_move = math.hypot(cur[side][0][0] - prev[side][0][0], cur[side][0][1] - prev[side][0][1])
                    t_move = math.hypot(cur[side][1][0] - prev[side][1][0], cur[side][1][1] - prev[side][1][1])
                    if s_move < 0.3:
                        slide.append(t_move)
                        src_slide.append(s_move)
        prev = cur
        hls, hrs, hlt, hrt = g(ps, 'hand_l'), g(ps, 'hand_r'), g(pt, 'hand_l'), g(pt, 'hand_r')
        dist = length(sub(v(hls.translation), v(hrs.translation)))
        if dist < GRIP_FULL:
            a_ = qrot(qinv(q(hrs.rotation)), sub(v(hls.translation), v(hrs.translation)))
            b_ = qrot(qinv(q(hrt.rotation)), sub(v(hlt.translation), v(hrt.translation)))
            grip.append(length(sub(a_, b_)))
        # exact bones should read 0; IK bones a few degrees (a flipped elbow/knee would read ~180)
        for b in ('hand_r', 'hand_l', 'index_02_r', 'middle_02_l', 'upperarm_r', 'clavicle_l',
                  'upperarm_l', 'lowerarm_l', 'thigh_l', 'calf_l', 'thigh_r', 'calf_r', 'head'):
            want = q(g(ps, b).rotation)
            if b == 'head':
                want = rest_relative(ctx, want, 'head')
            orient[b] = max(orient.get(b, 0.0), qang(want, q(g(pt, b).rotation)))

    def rng(a):
        return '%.1f..%.1f' % (min(a), max(a)) if a else '-'
    return ('lean %s | contact float %s | planted slide/frame %s | grip err max %s | orient %s' %
            (rng(lean), rng(fl_a), ('%.2f (src %.2f) max %.2f' % (sum(slide) / len(slide), sum(src_slide) / len(src_slide), max(slide))) if slide else '-',
             ('%.1f' % max(grip)) if grip else '-', ' '.join('%s %.1f' % kv for kv in orient.items())))


# ---------------------------------------------------------------- driver
def run():
    rows = select(inventory())
    if MODE in ('retarget', 'post', 'all'):
        rows = rows[globals().get('FIRST', 0):globals().get('LAST', len(rows))]
    ctx = Ctx()
    lines = ['MODE %s  clips %d  k %.4f' % (MODE, len(rows), ctx.k)]
    started = time.time()
    if MODE == 'retarget':
        lines.append('retargeted %d' % retarget(rows))
    if MODE in ('post', 'all'):
        # ONE clip end to end at a time (retarget -> post -> save), progress written per clip. A 90-clip batch
        # retarget followed by the saves hung the editor on 2026-09-25 (dozens of fresh sequences with async
        # compression in flight, and a call that outlived the 300 s MCP timeout).
        for row in rows:
            name, src, folder, dst, skel = row
            try:
                if MODE == 'all':
                    retarget([row])
                source, target = unreal.load_asset(src), unreal.load_asset(dst)
                if not target:
                    raise RuntimeError('missing ' + dst)
                drop, miss, looped = post(ctx, source, target)
                moved, cycle = flags(ctx, source, target)
                saved = EAL.save_asset(dst, only_if_is_dirty=False)
                warn = '  <- cycle name but ends differ' if cycle and not looped else ''
                line = '%-40s drop %.1f miss %.1f loop %s rm %s saved %s%s' % (name, drop, miss, cycle, moved, saved, warn)
            except Exception as e:     # keep going; report the clip
                line = 'FAIL %s: %s' % (name, e)
            open(REPORT, 'a').write(line + '\n')
    if MODE == 'measure':
        rows = rows[globals().get('FIRST', 0):globals().get('LAST', len(rows))]
        for name, src, folder, dst, skel in rows:
            source, target = unreal.load_asset(src), unreal.load_asset(dst)
            lines.append('%-36s %s' % (name, measure(ctx, source, target) if target else 'MISSING'))
    lines.append('took %.1f s' % (time.time() - started))
    open(REPORT, 'a').write('\n'.join(lines) + '\n')
    print('riflemega %s done: %d clips, %.1f s' % (MODE, len(rows), time.time() - started))


run()
