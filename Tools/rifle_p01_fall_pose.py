# rifle_p01_fall_pose.py -- regenerates the P01 fall clips.
#
# The clips it writes live under Content/AZ/Assets/RTG (GITIGNORED), so this script is the
# source of truth for them. Run from the editor:
#
#   ns = {}; exec(open(r'Tools/rifle_p01_fall_pose.py').read(), ns)
#   IPC = '/Game/AZ/Assets/RTG/Riffle_P01/Stand/Jump/IPC/'
#   ns['run'](SRC=IPC+'Riffle_P_W2_Stand_Relaxed_Jump_Air_IPC', DST=IPC+'Riffle_P_W2_Stand_Relaxed_Fall_v2',
#             LIFT_DEG=30.0, LIFT_AMP=2.5, ELBOW_UP=10.0, ELBOW_OUT=11.0, CLAV=7.0, SPINE=-6.0, HEAD=-10.0,
#             THIGH_L=22.0, THIGH_R=13.0, CALF_L=-28.0, CALF_R=-19.0,
#             LEG_AMP=6.0, CALF_AMP=7.0, SPINE_AMP=1.8, HEAD_AMP=2.2,
#             TWIST=6.0, ROLL=3.5, SPLAY=7.0, RAMP_FRAMES=6, CYCLES=3.0, LOOP=False)
#   ns['run'](SRC=IPC+'Riffle_P_W2_Stand_Aim_Jump_Air_IPC', DST=IPC+'Riffle_P_W2_Stand_Aim_Fall_v2',
#             ARMS=False, SPINE=-4.0, HEAD=-4.0, THIGH_L=17.0, THIGH_R=11.0, CALF_L=-22.0, CALF_R=-17.0,
#             LEG_AMP=4.5, CALF_AMP=5.5, SPINE_AMP=1.2, HEAD_AMP=1.2,
#             TWIST=4.0, ROLL=2.5, SPLAY=5.0, RAMP_FRAMES=6, CYCLES=3.0, LOOP=False)
#
# Then re-point CHT_v2 rows 302/303 if the names changed, and SAVE (verify by file mtime --
# EditorAssetLibrary.save_loaded_asset can return False without saving; save_asset(path) works).
# Full rationale: docs/design-briefs/rifle-p01-jump-fall-handoff.md
# Authors a falling pose (+ a seamlessly looping idle-air motion) onto a copy of a rifle air clip.
#
# GRIP SAFETY: the rifle is held in BOTH hands, so the two hands must keep their exact
# relative transform. Every frame applies ONE rigid rotation to both hand transforms about a
# chest pivot, then two-bone IK solves each arm to follow. Hand-to-hand transform is therefore
# preserved by construction, at any lift value.
#
# LOOP SAFETY: all oscillation uses phase = 2*pi*f/(N-1), so frame 0 and frame N-1 evaluate to
# the same value and the cycle wraps without a pop.
import unreal, math


def run(SRC, DST,
        LIFT_DEG=38.0, ELBOW_UP=12.0, ELBOW_OUT=16.0, LIFT_AMP=3.0,
        CLAV=8.0, SPINE=-7.0, HEAD=-12.0,
        THIGH_L=26.0, THIGH_R=18.0, CALF_L=-34.0, CALF_R=-26.0,
        LEG_AMP=7.0, CALF_AMP=8.0, SPINE_AMP=2.0, HEAD_AMP=2.5,
        RAMP_FRAMES=18, LOOP=False, ELBOW_WOBBLE=2.0, ARMS=True,
        TWIST=5.0, ROLL=3.0, SPLAY=6.0, CYCLES=1.0, SAVE=True):

    src = unreal.load_asset(SRC)
    if not unreal.EditorAssetLibrary.does_asset_exist(DST):
        unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
    dst = unreal.load_asset(DST)
    assert dst is not None, 'could not create/load ' + DST
    # N.B. we deliberately do NOT delete+recreate: once the chooser references DST the delete
    # fails silently. Every track we touch is rewritten from SRC below, so this is idempotent.
    opts = unreal.AnimPoseEvaluationOptions()
    W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
    N = dst.get_editor_property('number_of_sampled_keys')

    def qmul(a, b):
        ax, ay, az, aw = a; bx, by, bz, bw = b
        return (aw*bx+ax*bw+ay*bz-az*by, aw*by-ax*bz+ay*bw+az*bx,
                aw*bz+ax*by-ay*bx+az*bw, aw*bw-ax*bx-ay*by-az*bz)
    def qconj(q): return (-q[0], -q[1], -q[2], q[3])
    def qaxis(ax, deg):
        r = math.radians(deg)*0.5; s = math.sin(r)
        return (ax[0]*s, ax[1]*s, ax[2]*s, math.cos(r))
    def qrotv(q, v):
        x, y, z, w = q; vx, vy, vz = v
        tx = 2*(y*vz-z*vy); ty = 2*(z*vx-x*vz); tz = 2*(x*vy-y*vx)
        return (vx+w*tx+(y*tz-z*ty), vy+w*ty+(z*tx-x*tz), vz+w*tz+(x*ty-y*tx))
    def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
    def add(a, b): return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
    def mul(a, s): return (a[0]*s, a[1]*s, a[2]*s)
    def dot(a, b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
    def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
    def length(a): return math.sqrt(dot(a, a))
    def norm(a):
        l = length(a); return (0, 0, 0) if l < 1e-9 else (a[0]/l, a[1]/l, a[2]/l)
    def q_between(a, b):
        a = norm(a); b = norm(b); d = max(-1.0, min(1.0, dot(a, b)))
        if d > 0.999999: return (0, 0, 0, 1)
        if d < -0.999999:
            ax = cross(a, (1, 0, 0))
            if length(ax) < 1e-6: ax = cross(a, (0, 1, 0))
            return qaxis(norm(ax), 180.0)
        return qaxis(norm(cross(a, b)), math.degrees(math.acos(d)))
    def V(t): return (t.x, t.y, t.z)
    def Q(t): return (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)

    X = (1.0, 0.0, 0.0)
    ARM_CHAINS = [('upperarm_r', 'lowerarm_r', 'hand_r', -1.0),
                  ('upperarm_l', 'lowerarm_l', 'hand_l', +1.0)]
    PI = math.pi
    # (bone, base_deg, amplitude_deg, phase_offset)  -- legs run in OPPOSITE phase = air tread
    # (bone, axis, base_deg, amp_deg, phase) -- MULTIPLE entries per bone compose.
    # axis is in COMPONENT space: X = lateral (flexion), Y = roll/splay, Z = twist.
    Y = (0.0, 1.0, 0.0); Z = (0.0, 0.0, 1.0)
    EXTRA = ([('clavicle_r', X, CLAV,       1.5, 0.0),
              ('clavicle_l', X, CLAV*0.75,  1.5, PI)] if ARMS else []) + [
             ('spine_02', X, SPINE,        SPINE_AMP, PI*0.5),
             ('spine_03', X, SPINE*0.75,   SPINE_AMP*0.6, PI*0.5),
             ('spine_02', Z, TWIST,        TWIST*0.4, PI*0.25),   # torso twist -> not frontal
             ('spine_03', Z, TWIST*0.6,    TWIST*0.3, PI*0.25),
             ('spine_02', Y, ROLL,         ROLL*0.5, PI*0.7),     # slight side lean
             ('neck_01',  X, HEAD*0.6,     HEAD_AMP*0.6, PI*0.33),
             ('head',     X, HEAD,         HEAD_AMP, PI*0.33),
             ('head',     Z, -TWIST*0.8,   TWIST*0.3, PI*0.25),   # head counter-turns the torso
             ('thigh_l',  X, THIGH_L,      LEG_AMP, 0.0),
             ('thigh_r',  X, THIGH_R,      LEG_AMP, PI),
             ('thigh_l',  Y, -SPLAY,       SPLAY*0.4, PI*0.5),    # knees apart, not tram-lined
             ('thigh_r',  Y, SPLAY*0.7,    SPLAY*0.4, PI*1.5),
             ('calf_l',   X, CALF_L,       CALF_AMP, 0.0),
             ('calf_r',   X, CALF_R,       CALF_AMP, PI)]

    tracks = {}; clamped = 0
    def put(b, p, r, s):
        t = tracks.setdefault(b, {'p': [], 'r': [], 's': []})
        t['p'].append(p); t['r'].append(unreal.Quat(*r)); t['s'].append(s)

    for f in range(N):
        # REACTION RAMP: w=0 at frame 0 (pose identical to the source) easing to w=1 by
        # RAMP_FRAMES, so the clip PERFORMS the fall reaction instead of starting in it.
        if RAMP_FRAMES > 0:
            t = min(1.0, f/float(RAMP_FRAMES)); w = t*t*(3.0-2.0*t)
        else:
            w = 1.0
        # CYCLES>1 packs more tread into the SHORT window the clip actually gets in game
        # (measured: the air state lasts ~0.63s, so a 2.0s-period wobble is invisible).
        ph = 2.0*PI*CYCLES*f/float(N-1)
        # Read the SOURCE every frame, never dst: dst is what we are writing, and if the
        # delete/duplicate above silently failed (it does once the asset is referenced by the
        # chooser) reading dst would stack this pose on top of the previous run's output.
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(src, f, opts)
        def wt(b): return unreal.AnimPoseExtensions.get_bone_pose(pose, b, W)
        def lt(b): return unreal.AnimPoseExtensions.get_bone_pose(pose, b, LOC)
        P = mul(add(V(wt('upperarm_r').translation), V(wt('upperarm_l').translation)), 0.5)
        R = qaxis(X, w*(LIFT_DEG + LIFT_AMP*math.sin(ph)))
        # ARMS=False leaves the arm tracks completely untouched, so the two-hand rifle grip is
        # bit-identical to the source (the IK round-trip alone perturbs it ~1.6cm on the aim clip).
        if not ARMS:
            # Restore the arm chain + clavicles to the SOURCE verbatim. Required because we no
            # longer delete/recreate DST: any track we skip would otherwise keep a previous run's
            # edit. Writing source values makes the result independent of DST's prior state.
            for bname in ('upperarm_r','lowerarm_r','hand_r','upperarm_l','lowerarm_l','hand_l',
                          'clavicle_r','clavicle_l'):
                t = lt(bname)
                put(bname, t.translation, Q(t), t.scale3d)
        for ub, lb, hb, side in (ARM_CHAINS if ARMS else []):
            Uwt, Lwt, Hwt = wt(ub), wt(lb), wt(hb)
            S, E, H = V(Uwt.translation), V(Lwt.translation), V(Hwt.translation)
            Uw, Lw, Hw = Q(Uwt), Q(Lwt), Q(Hwt)
            L1, L2 = length(sub(E, S)), length(sub(H, E))
            Ht = add(P, qrotv(R, sub(H, P)))
            eu = w*(ELBOW_UP + ELBOW_WOBBLE*math.sin(ph + (0.0 if side < 0 else PI)))
            Et = add(add(P, qrotv(R, sub(E, P))), (side*ELBOW_OUT*w, 0.0, eu))
            v = sub(Ht, S); raw = length(v)
            if raw > (L1+L2)*0.999: clamped += 1
            d = max(min(raw, (L1+L2)*0.999), 1e-4); n = norm(v)
            cos_a = max(-1.0, min(1.0, (L1*L1 + d*d - L2*L2)/(2*L1*d)))
            a = math.acos(cos_a)
            u = sub(Et, S); up = sub(u, mul(n, dot(u, n)))
            if length(up) < 1e-5:
                up = cross(n, (0, 0, 1))
                if length(up) < 1e-5: up = cross(n, (0, 1, 0))
            up = norm(up)
            En = add(S, mul(add(mul(n, math.cos(a)), mul(up, math.sin(a))), L1))
            Uw2 = qmul(q_between(sub(E, S), sub(En, S)), Uw)
            Lw2 = qmul(q_between(sub(H, E), sub(Ht, En)), Lw)
            Hw2 = qmul(R, Hw)
            Pc = qmul(Uw, qconj(Q(lt(ub))))
            put(ub, lt(ub).translation, qmul(qconj(Pc), Uw2), lt(ub).scale3d)
            put(lb, lt(lb).translation, qmul(qconj(Uw2), Lw2), lt(lb).scale3d)
            put(hb, lt(hb).translation, qmul(qconj(Lw2), Hw2), lt(hb).scale3d)
        acc = {}
        for bone, axis, base, amp, po in EXTRA:
            deg = w*(base + amp*math.sin(ph + po))
            acc[bone] = qmul(qaxis(axis, deg), acc.get(bone, (0.0, 0.0, 0.0, 1.0)))
        for bone, D in acc.items():
            Bw, Bl = Q(wt(bone)), Q(lt(bone))
            Pw = qmul(Bw, qconj(Bl))
            Dl = qmul(qmul(qconj(Pw), D), Pw)
            put(bone, lt(bone).translation, qmul(Dl, Bl), lt(bone).scale3d)

    dst.controller.open_bracket('fall pose + air motion')
    for b, d in tracks.items():
        dst.controller.set_bone_track_keys(unreal.Name(b), d['p'], d['r'], d['s'], True)
    dst.controller.close_bracket()

    # ---------------- verification ----------------
    def g(p, b): return V(unreal.AnimPoseExtensions.get_bone_pose(p, b, W).translation)
    out = ['lift=%.0f(+-%.0f) elbowUp=%.0f elbowOut=%.0f | IK clamped %d/%d'
           % (LIFT_DEG, LIFT_AMP, ELBOW_UP, ELBOW_OUT, clamped, N*2)]

    d0 = max(length(sub(g(unreal.AnimPoseExtensions.get_anim_pose_at_frame(dst,0,opts), bb),
                        g(unreal.AnimPoseExtensions.get_anim_pose_at_frame(src,0,opts), bb)))
              for bb in ('hand_r','hand_l','foot_l','head'))
    out.append('ENTRY : frame 0 deviation from ORIGINAL = %.3f cm (should be ~0)' % d0)
    worst = 0.0
    for f in range(0, N, 5):
        po = unreal.AnimPoseExtensions.get_anim_pose_at_frame(src, f, opts)
        pn = unreal.AnimPoseExtensions.get_anim_pose_at_frame(dst, f, opts)
        e = length(sub(g(pn, 'hand_l'), g(pn, 'hand_r'))) - length(sub(g(po, 'hand_l'), g(po, 'hand_r')))
        worst = max(worst, abs(e))
    out.append('GRIP  : worst hand-to-hand error = %.4f cm  (rifle stays attached)' % worst)

    # loop seam: frame 0 vs last frame, ours vs the source's own seam
    p0s = unreal.AnimPoseExtensions.get_anim_pose_at_frame(src, 0, opts)
    pLs = unreal.AnimPoseExtensions.get_anim_pose_at_frame(src, N-1, opts)
    p0n = unreal.AnimPoseExtensions.get_anim_pose_at_frame(dst, 0, opts)
    pLn = unreal.AnimPoseExtensions.get_anim_pose_at_frame(dst, N-1, opts)
    for b in ('hand_r', 'foot_l', 'head'):
        so = length(sub(g(p0s, b), g(pLs, b))); sn = length(sub(g(p0n, b), g(pLn, b)))
        out.append('LOOP  : %-7s seam src=%.3f cm  new=%.3f cm' % (b, so, sn))

    # how much motion is there now, vs before
    for b in ('hand_r', 'foot_l', 'foot_r', 'lowerarm_r'):
        zo = [g(unreal.AnimPoseExtensions.get_anim_pose_at_frame(src, f, opts), b)[2] for f in range(0, N, 3)]
        zn = [g(unreal.AnimPoseExtensions.get_anim_pose_at_frame(dst, f, opts), b)[2] for f in range(0, N, 3)]
        out.append('MOTION: %-11s dZ over clip  src=%5.1f cm -> new=%5.1f cm' % (b, max(zo)-min(zo), max(zn)-min(zn)))

    dst.set_editor_property('loop', LOOP)
    out.append('LOOP FLAG: %s   (ramped clips cannot loop -- holds the settled fall pose)' % LOOP)
    if SAVE:
        out.append('SAVED: %s' % unreal.EditorAssetLibrary.save_asset(DST, only_if_is_dirty=False))
    return '\n'.join(out)
