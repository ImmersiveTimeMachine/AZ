"""Adaptive grasp solver v2 - a hand closes on the weapon the way a human (or an underactuated robot hand) does.

v1 (az_grip_solve.py) started from the mocap fingers, which are already INSIDE a thicker weapon, and pushed them out
by opening -> straight fingers and a hand hanging under the forearm (user screenshot 2026-09-26). v2:

  1. PALM = skin, not bone capsules: points on the palm surface (from the wrist to every knuckle + the thenar pad),
     offset PALM_T from the bone lines towards the palm side. The hand (orientation from the mocap) is moved along
     the palm normal until that skin just touches the weapon.
  2. FINGERS start from the OPEN hand (the skeleton's reference pose) and close together (synergy ratios per joint)
     about each joint's own flexion axis, measured from the mocap (ref -> mocap rotation of that joint).
     Underactuated rule: when a phalanx would go inside the weapon (beyond SOFT skin compression) the joints that
     drive it stop, the joints beyond it keep closing -> each phalanx ends on the surface or at its joint limit.
  3. The thumb closes the same way about its own mocap axes.

Input  Saved/<IN> (Tools/az_grip_export.py with ref_local).  Output Saved/<OUT> (same layout as az_grip_solve.py, read
by Tools/az_grip_apply.py MODE assets / preview).
    python az_grip_solve2.py [in.json] [out.json]
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from az_grip_solve import Surface, add, sub, mul, dot, cross, length, unit, qmul, qinv, qrot, qaxis, qnorm  # noqa: E402

SAVED = r"C:/UnrealEngine/Games/AZ/Saved"
IN = os.path.join(SAVED, sys.argv[1] if len(sys.argv) > 1 else "az_grip_reference_v2.json")
OUT = os.path.join(SAVED, sys.argv[2] if len(sys.argv) > 2 else "az_grip_solution.json")
PALM_T = 1.0                              # palm skin below the metacarpal lines (cm)
SOFT = 0.12                               # skin compression allowed at a contact (cm)
FINGER_R = (0.8, 0.72, 0.62)              # proximal / middle / distal phalanx radius (cm)
THUMB_R = (0.9, 0.8, 0.7)
LIMITS = {"finger": (95.0, 105.0, 80.0), "thumb": (60.0, 65.0, 80.0)}     # max closing from the open hand (deg)
RATIO = {"finger": (1.0, 1.0, 0.75), "thumb": (1.0, 1.0, 0.8)}
STEP = 1.5                                # closing step (deg)
SEG_W = (0.6, 1.0, 1.4)                   # how much each phalanx (proximal, middle, distal) should rest on the weapon
FINGERS = ("index", "middle", "ring", "pinky")


def axis_angle(q):
    x, y, z, w = q
    if w < 0:
        x, y, z, w = -x, -y, -z, -w
    ang = 2.0 * math.acos(max(-1.0, min(1.0, w)))
    s = math.sqrt(max(0.0, 1.0 - w * w))
    return ((x / s, y / s, z / s) if s > 1e-6 else (0.0, 0.0, 0.0)), math.degrees(ang)


def main():
    R = json.load(open(IN))
    M = {b: (tuple(v[:3]), tuple(v[3:7])) for b, v in R["model"].items()}
    L = {b: (tuple(v[:3]), tuple(v[3:7])) for b, v in R["local"].items()}
    RL = {b: (tuple(v[:3]), tuple(v[3:7])) for b, v in R["ref_local"].items()}
    # AZ_GRIP_LEFT_HAND="x,y,z,qx,qy,qz,qw": user-tuned LeftHandGrip (hand_l in weapon-model space). The whole left
    # hand (hand_l + its finger subtree, locals unchanged) is moved rigidly onto it before anything is solved.
    left_override = os.environ.get("AZ_GRIP_LEFT_HAND")
    if left_override:
        tv = [float(x) for x in left_override.split(",")]
        tp, tq = tuple(tv[:3]), qnorm(tuple(tv[3:7]))
        hp, hq = M["hand_l"]
        dq = qmul(tq, qinv(hq))
        for b in list(M):
            if b.endswith("_l") and any(k in b for k in ("hand_l", "thumb", "index", "middle", "ring", "pinky")):
                p, q = M[b]
                M[b] = (add(tp, qrot(dq, sub(p, hp))), qnorm(qmul(dq, q)))
    hand_pts =[M[b][0] for b in M if b[-2:] in ("_l", "_r") and any(k in b for k in ("hand", "thumb", "index", "middle", "ring", "pinky"))]
    S = Surface(R["verts"], R["tris"], hand_pts)
    lines, pose_local = [], {}
    finger_tips = {}                      # fingertip pad per finger, weapon-model space -> Grip_<L|R>_<Finger> markers
    moves = {}

    def chain_names(f, sd):
        return ["%s_0%d_%s" % (f, k, sd) for k in (1, 2, 3)]

    def parent_of(f, sd):
        return "hand_" + sd if f == "thumb" else "%s_metacarpal_%s" % (f, sd)

    def fk(parent_tf, locs_t, rots):
        pos, rot = parent_tf
        pts, wr = [], []
        for t, r in zip(locs_t, rots):
            pos = add(pos, qrot(rot, t))
            rot = qmul(rot, r)
            pts.append(pos)
            wr.append(rot)
        pts.append(add(pts[2], qrot(wr[2], mul(locs_t[2], 0.85))))          # fingertip
        return pts, wr

    def seg_min(pts, si, radii):
        a, b = pts[si], pts[si + 1]
        return min(S.sdist(add(a, mul(sub(b, a), k / 4.0))) - radii[si] for k in range(5))

    # sanity: our FK from the clip locals must land on the exported model-space joints
    fk_err = 0.0
    for sd in ("l", "r"):
        for f in ("thumb",) + FINGERS:
            names = chain_names(f, sd)
            pts, _ = fk(M[parent_of(f, sd)], [L[n][0] for n in names], [L[n][1] for n in names])
            fk_err = max(fk_err, max(length(sub(p, M[n][0])) for p, n in zip(pts[:3], names)))
    lines.append("FK check vs exported joints: max %.3f cm" % fk_err)

    for sd in ("l", "r"):
        # ---------------- palm onto the surface
        hand = M["hand_" + sd][0]
        knuck = {f: M["%s_01_%s" % (f, sd)][0] for f in FINGERS}
        n0 = unit(cross(sub(knuck["index"], knuck["pinky"]), sub(knuck["middle"], hand)))
        tips = []
        for f in FINGERS:
            names = chain_names(f, sd)
            pts, _ = fk(M[parent_of(f, sd)], [L[n][0] for n in names], [L[n][1] for n in names])
            tips.append(sub(pts[3], knuck[f]))
        avg_tip = mul(tips[0], 0.0)
        for t in tips:
            avg_tip = add(avg_tip, t)
        N = n0 if dot(n0, avg_tip) > 0 else mul(n0, -1.0)                   # palm side = where curled fingers go
        skin = [add(add(hand, mul(sub(knuck[f], hand), t)), mul(N, PALM_T)) for f in FINGERS for t in (0.25, 0.5, 0.75, 0.95)]
        th1 = M["thumb_01_" + sd][0]
        skin += [add(add(hand, mul(sub(th1, hand), 0.6)), mul(N, PALM_T)), add(mul(add(th1, M["thumb_02_" + sd][0]), 0.5), mul(N, PALM_T))]

        def palm_min(s):
            return min(S.sdist(add(p, mul(N, s))) for p in skin)

        lo_s, hi_s = -6.0, 6.0
        if palm_min(0.0) < 0:
            hi_s = 0.0
        else:
            lo_s = 0.0
        # palm_min decreases as s grows (moving into the weapon); find palm_min(s) = 0
        for _ in range(30):
            mid = 0.5 * (lo_s + hi_s)
            if palm_min(mid) > 0:
                lo_s = mid
            else:
                hi_s = mid
        s_move = lo_s
        if os.environ.get("AZ_GRIP_KEEP_HANDS") == "1":
            s_move = 0.0                                                   # hand placement is user-tuned: fingers only
        d = mul(N, s_move)
        moves[sd] = d
        lines.append("%s palm: moved %+.2f cm along the palm normal (skin clearance before %+.2f, after %+.2f)"
                     % ("LEFT " if sd == "l" else "RIGHT", s_move, palm_min(0.0), palm_min(s_move)))
        moved = lambda tf: (add(tf[0], d), tf[1])

        # ---------------- fingers + thumb close from the open hand
        for f in ("thumb",) + FINGERS:
            kind = "thumb" if f == "thumb" else "finger"
            radii = THUMB_R if f == "thumb" else FINGER_R
            names = chain_names(f, sd)
            parent_tf = moved(M[parent_of(f, sd)])
            locs_t = [L[n][0] for n in names]
            open_r = [RL[n][1] for n in names]
            axes, amounts = [], []
            for n, o in zip(names, open_r):
                ax, ang = axis_angle(qmul(qinv(o), L[n][1]))                # ref -> mocap, in the bone's frame
                axes.append(ax)
                amounts.append(ang)
            for k in range(3):                                              # weak joints borrow a neighbour's axis
                if amounts[k] < 6.0 or length(axes[k]) < 0.5:
                    donors = [j for j in (k + 1, k - 1, k + 2, k - 2) if 0 <= j < 3 and amounts[j] >= 6.0]
                    axes[k] = axes[donors[0]] if donors else (0.0, 0.0, 1.0)
            lim, ratio = LIMITS[kind], RATIO[kind]

            def pose(th):
                rots = [qmul(open_r[k], qaxis(axes[k], math.radians(th[k]))) for k in range(3)]
                return fk(parent_tf, locs_t, rots), rots

            # WRAP search: base joint a, middle b, last = RATIO * b. Every phalanx should rest on the weapon (a tip-only
            # touch left fingers floating 1.3-1.7 cm), nothing inside beyond SOFT, and the curl stays near the mocap's.
            mocap_a, mocap_b = amounts[0], amounts[1]

            def cost(a, b):
                th_ = [a, b, b * ratio[2]]
                (pp, _), _r = pose(th_)
                c = [seg_min(pp, s, radii) for s in range(3)]
                pen = sum(max(0.0, -SOFT - x) ** 2 for x in c)
                gap = sum(w * max(0.0, x - 0.05) ** 2 for w, x in zip(SEG_W, c))
                return 400.0 * pen + 3.0 * gap + 0.0002 * ((a - mocap_a) ** 2 + (b - mocap_b) ** 2), th_

            best = None
            for a in range(-20, int(lim[0]) + 1, 8):
                for b in range(-10, int(lim[1]) + 1, 8):
                    e, th_ = cost(float(a), float(b))
                    if best is None or e < best[0]:
                        best = (e, th_)
            for step in (4.0, 2.0, 1.0):
                improved = True
                while improved:
                    improved = False
                    a0, b0 = best[1][0], best[1][1]
                    for da, db in ((step, 0), (-step, 0), (0, step), (0, -step), (step, step), (-step, -step), (step, -step), (-step, step)):
                        a, b = max(-25.0, min(lim[0], a0 + da)), max(-15.0, min(lim[1], b0 + db))
                        e, th_ = cost(a, b)
                        if e < best[0] - 1e-9:
                            best, improved = (e, th_), True
            th = best[1]
            (pts, _), rots = pose(th)
            finger_tips["%s_%s" % (sd, f)] = list(pts[3])
            segs = [seg_min(pts, s, radii) for s in range(3)]
            lines.append("%s %-6s closed %4.0f/%4.0f/%4.0f deg | phalanx clearance %+.2f/%+.2f/%+.2f cm"
                         % (sd, f, th[0], th[1], th[2], segs[0], segs[1], segs[2]))
            for n, r_ in zip(names, rots):
                pose_local[n] = list(qnorm(r_))
    # ---------------- outputs
    hl = M["hand_l"]
    left_grip = list(add(hl[0], moves["l"])) + list(hl[1])
    sr = R["socket_rel"]
    Sp, Sq = tuple(sr[:3]), tuple(sr[3:7])
    new_loc = add(Sp, qrot(Sq, mul(moves["r"], -1.0)))                       # right hand moved +d == weapon moved -d
    out = {"weapon_mesh": R["weapon_mesh"], "socket": R["socket"], "clip": R["clip"], "frame": R["frame"], "solver": "v2",
           "socket_new": list(new_loc) + list(Sq), "socket_old": list(sr), "right_shift_model": list(moves["r"]),
           "left_hand_grip": left_grip, "pose_local": pose_local, "finger_tips": finger_tips, "report": lines}
    json.dump(out, open(OUT, "w"), indent=1)
    print("\n".join(lines))


if __name__ == "__main__":
    main()
