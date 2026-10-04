"""Offline SWITCH-ARM solver: bakes the left arm of a weapon-switch clip so that it is correct over the legs it is
really played on (user decision 2026-10-03: corrections live in the animation, not in per-frame runtime fixes).

Input  - a recording of the situation (az.Weapon.RecordSwitch 1 in PIE): the AZ Weapon Grip node's INPUT pose every frame
         of one switch phase = the switch clip's upper body over the live legs (after foot placement, real layering),
         Saved/NaturalGrip/SwitchRecordings/<clip>_<time>.json.
         The body: PHYS_MHC_Hero exported as T3D (Saved/NaturalGrip/SwitchArm/PHYS_MHC_Hero.t3d).
Solve  - per frame the hand target is what the runtime grip does (the clip hand blended onto the weapon's grip by the
         recorded grip weight); then the arm is kept >= MARGIN from the thigh / knee / calf / foot by ELBOW SWIVEL about
         the shoulder->hand line and, only while the hand is not on the gun, a small HAND OFFSET. The whole trajectory is
         solved at once (dynamic programming over feasible candidates, then smoothing + re-verification), so the
         correction is minimal, continuous and does not flip sides.
Output - Saved/NaturalGrip/SwitchArm/<clip>.solve.json: per recorded frame the corrected component-space arm and the new
         LOCAL rotations of upperarm_l / lowerarm_l / hand_l (relative to the recorded parents) + a report.

python switch_arm_solve.py <recording.json> [more recordings of the same clip ...]
"""
import sys, os, json, math, re, glob
from sa_math import *

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ARM_DIR = os.path.join(ROOT, "Saved", "NaturalGrip", "SwitchArm")

MARGIN = 1.0                 # cm kept between the arm and the leg surfaces
ARM_SCALE = 0.85             # Physics Asset arm capsules include sleeve / simulation margin
UPPER_R = 5.770294 * ARM_SCALE
FORE_R = 5.164654 * ARM_SCALE
ELBOW_R = 5.164654 * ARM_SCALE
HAND_R = 4.0                 # hand_l box 17.2 x 6.0 x 10.3 as a capsule along hand X
HAND_X = (1.9, 13.1)
SWIVEL_STEP = 4.0            # deg
SWIVEL_MAX = 84.0
OFFSET_STEP = 1.0            # cm
OFFSET_MAX = 14.0
TRANSITION_WEIGHT = 40.0     # DP: cost of changing the correction between frames (per cm of elbow / hand travel)^2 / dt
SMOOTH_SIGMA_S = 0.06        # final smoothing (s)

LEG_PREFIX = ("thigh_", "calf_", "foot_", "ball_")


# ---------------------------------------------------------------- body (Physics Asset)
def load_bodies(path=os.path.join(ARM_DIR, "PHYS_MHC_Hero.t3d")):
    pending, bodies = None, {}
    for line in open(path, encoding="utf-8", errors="ignore"):
        s = line.strip()
        if s.startswith("AggGeom="):
            pending = s
        m = re.match(r'BoneName="([^"]+)"', s)
        if m and pending:
            bodies[m.group(1)] = parse_geom(pending)
            pending = None
    return bodies


def _num(s, key):
    m = re.search(key + r"=([-\d.eE]+)", s)
    return float(m.group(1)) if m else 0.0


def parse_geom(s):
    elems = []
    for kind, body in re.findall(r"(SphylElems|BoxElems|SphereElems)=\((.*?\))\)\)", s):
        starts = [m.start() for m in re.finditer(r"\(Center=", body)]
        for si, st in enumerate(starts):
            e = body[st + 1:(starts[si + 1] if si + 1 < len(starts) else len(body))]
            c = re.search(r"Center=\(X=([-\d.eE]+),Y=([-\d.eE]+),Z=([-\d.eE]+)\)", e)
            center = tuple(float(x) for x in c.groups()) if c else (0.0, 0.0, 0.0)
            r = re.search(r"Rotation=\(Pitch=([-\d.eE]+),Yaw=([-\d.eE]+),Roll=([-\d.eE]+)\)", e)
            q = rotator_quat(*(float(x) for x in r.groups())) if r else (0.0, 0.0, 0.0, 1.0)
            rest = e[c.end():] if c else e
            if kind == "SphylElems":
                elems.append(("capsule", center, q, _num(rest, "Radius"), _num(rest, "Length")))
            elif kind == "SphereElems":
                elems.append(("capsule", center, q, _num(rest, "Radius"), 0.0))
            else:
                ext = (_num(rest, r"(?<![A-Za-z])X"), _num(rest, r"(?<![A-Za-z])Y"), _num(rest, r"(?<![A-Za-z])Z"))
                long_axis = max(range(3), key=lambda i: ext[i])
                radius = max(ext[i] for i in range(3) if i != long_axis) / 2.0
                axis = [0.0, 0.0, 0.0]
                axis[long_axis] = 1.0
                half = max(0.0, ext[long_axis] / 2.0 - radius)
                elems.append(("box", center, q, radius, 2.0 * half, tuple(axis)))
    return elems


def obstacles_cs(bodies, bones, prefixes):
    out = []
    for name, elems in bodies.items():
        if not name.startswith(prefixes) or name not in bones:
            continue
        x = Xf.f7(bones[name])
        for e in elems:
            center, q, radius, L = e[1], e[2], e[3], e[4]
            axis = qrot(q, e[5] if e[0] == "box" else (0.0, 0.0, 1.0))
            a = x.point(sub(center, mul(axis, L / 2.0)))
            b = x.point(add(center, mul(axis, L / 2.0)))
            out.append((name, a, b, radius))
    return out


# ---------------------------------------------------------------- arm
def two_bone(S, P, la, lb, side):
    """Elbow for shoulder S, wrist P, bone lengths, preferred side (unit, perpendicular to S->P). Returns (E, P)."""
    d_vec = sub(P, S)
    d = length(d_vec)
    u = normalize(d_vec)
    d = max(abs(la - lb) + 1e-3, min(la + lb - 1e-3, d))
    P = add(S, mul(u, d))
    x = (la * la - lb * lb + d * d) / (2.0 * d)
    h = math.sqrt(max(0.0, la * la - x * x))
    return add(add(S, mul(u, x)), mul(side, h)), P


def perp(v, u):
    return normalize(sub(v, mul(u, dot(v, u))), fallback=normalize(cross(u, (0.0, 0.0, 1.0))))


def arm_parts(S, E, P, hand_q):
    hx = qrot(hand_q, (1.0, 0.0, 0.0))
    return (("upperarm", S, E, UPPER_R), ("elbow", E, E, ELBOW_R), ("forearm", E, P, FORE_R),
            ("hand", add(P, mul(hx, HAND_X[0])), add(P, mul(hx, HAND_X[1])), HAND_R))


def clearance(parts, obstacles, skip_upper=False):
    worst = (1e9, None, None, None)
    for pn, a, b, r in parts:
        if skip_upper and pn == "upperarm":
            continue
        for on, oa, ob, orad in obstacles:
            d, c1, c2 = closest_seg_seg(a, b, oa, ob)
            gap = d - r - orad
            if gap < worst[0]:
                worst = (gap, pn, on, normalize(sub(c1, c2)))
    return worst


# ---------------------------------------------------------------- recording
def load_recording(path):
    d = json.load(open(path))
    d["path"] = path
    return d


def grip_curve(clip):
    curves = json.load(open(os.path.join(ARM_DIR, "clip_grip_curves.json")))
    return curves.get(clip)


def settled_grip(recs):
    """The left hand's grip relative to az_weapon_r once the weapon hangs settled in the hand (a holster recording
    starts with it settled; a draw recording's first frame is mid socket blend)."""
    for path in sorted(glob.glob(os.path.join(ROOT, "Saved", "NaturalGrip", "SwitchRecordings", "*Return_To*.json"))):
        g = json.load(open(path))["leftHandInWeaponBone"]
        return Xf.f7(g)
    return Xf.f7(recs[0]["leftHandInWeaponBone"])


# ---------------------------------------------------------------- per-frame setup and candidates
class Frame:
    pass


def frame_setup(f, bodies, grip_in_bone):
    B = f["bones"]
    F = Frame()
    F.t = f["clipTime"]
    F.alpha = f.get("alpha", 0.0)
    F.S = tuple(B["upperarm_l"][0:3])
    F.E0 = tuple(B["lowerarm_l"][0:3])
    F.H0 = tuple(B["hand_l"][0:3])
    F.q_clav = Xf.f7(B["clavicle_l"]).q
    F.q_upper0 = Xf.f7(B["upperarm_l"]).q
    F.q_lower0 = Xf.f7(B["lowerarm_l"]).q
    F.q_hand0 = Xf.f7(B["hand_l"]).q
    F.la = length(sub(F.E0, F.S))
    F.lb = length(sub(F.H0, F.E0))
    grip = grip_in_bone * Xf.f7(B["az_weapon_r"])
    F.P0 = lerp(F.H0, grip.t, F.alpha)
    F.R = qslerp(F.q_hand0, grip.q, F.alpha)
    F.legs = obstacles_cs(bodies, B, LEG_PREFIX)
    F.torso = obstacles_cs(bodies, B, ("pelvis", "spine_"))
    u = normalize(sub(F.P0, F.S))
    F.side0 = perp(sub(F.E0, F.S), u)
    return F


def config(F, swivel_deg, offset):
    P = add(F.P0, mul(offset, 1.0 - F.alpha))
    u = normalize(sub(P, F.S))
    side = qrot(qaxis(u, math.radians(swivel_deg)), perp(F.side0, u))
    E, P = two_bone(F.S, P, F.la, F.lb, side)
    return E, P


def evaluate(F, swivel_deg, offset):
    E, P = config(F, swivel_deg, offset)
    parts = arm_parts(F.S, E, P, F.R)
    gap = clearance(parts, F.legs)
    return E, P, gap


def candidates(F, base_gap):
    """Feasible (swivel, offset) states of this frame with their static cost. The offset directions: the way out of the
    nearest leg at the base pose, straight up, and away from the body."""
    out = []
    dirs = [(0.0, 0.0, 0.0)]
    if F.alpha < 0.999:
        n = base_gap[3] if base_gap[3] else (0.0, 0.0, 1.0)
        lateral = normalize(sub(F.P0, F.torso[0][1])) if F.torso else (0.0, 0.0, 1.0)
        dirs = [normalize(n), (0.0, 0.0, 1.0), normalize((lateral[0], lateral[1], 0.0))]
    E0, P0b = config(F, 0.0, (0.0, 0.0, 0.0))
    steps = [0.0] + [s * OFFSET_STEP for s in range(1, int(OFFSET_MAX / OFFSET_STEP) + 1)]
    sw = [k * SWIVEL_STEP for k in range(-int(SWIVEL_MAX / SWIVEL_STEP), int(SWIVEL_MAX / SWIVEL_STEP) + 1)]
    for di, dvec in enumerate(dirs):
        for mag in (steps if di or F.alpha < 0.999 else [0.0]):
            if di == 0 and mag > 0.0:
                pass
            off = mul(dvec, mag)
            for s in sw:
                E, P, gap = evaluate(F, s, off)
                if gap[0] >= MARGIN:
                    cost = length(sub(E, E0)) ** 2 + 2.0 * length(sub(P, P0b)) ** 2
                    out.append((cost, s, off, E, P))
            if dvec == (0.0, 0.0, 0.0):
                break
    out.sort(key=lambda c: c[0])
    return out[:80]


# ---------------------------------------------------------------- whole-trajectory solve
def solve(frames, log=print):
    n = len(frames)
    base = [evaluate(F, 0.0, (0.0, 0.0, 0.0)) for F in frames]
    cands = []
    for i, F in enumerate(frames):
        if base[i][2][0] >= MARGIN:
            # no penetration: keep the base, but allow small states so the trajectory can ramp in / out smoothly
            c = [(0.0, 0.0, (0.0, 0.0, 0.0), base[i][0], base[i][1])]
            c += [x for x in candidates(F, base[i][2]) if x[0] > 0.0][:40]
        else:
            c = candidates(F, base[i][2])
            if not c:
                log("  frame %d t=%.3f: NO feasible correction within limits (gap %.1f cm %s vs %s)" % (i, F.t, base[i][2][0], base[i][2][1], base[i][2][2]))
                c = [(0.0, 0.0, (0.0, 0.0, 0.0), base[i][0], base[i][1])]
        cands.append(c)
    # dynamic programming (Viterbi): static cost + transition cost (elbow / hand travel between frames)
    acc = [[c[0] for c in cands[0]]]
    back = [[-1] * len(cands[0])]
    for i in range(1, n):
        dt = max(1e-3, frames[i].t - frames[i - 1].t)
        row, brow = [], []
        for c in cands[i]:
            best, arg = 1e18, 0
            for j, p in enumerate(cands[i - 1]):
                trans = (length(sub(c[3], p[3])) ** 2 + length(sub(c[4], p[4])) ** 2) / dt
                v = acc[i - 1][j] + TRANSITION_WEIGHT * trans * dt * dt * 60.0
                if v < best:
                    best, arg = v, j
            row.append(best + c[0])
            brow.append(arg)
        acc.append(row)
        back.append(brow)
    k = min(range(len(acc[-1])), key=lambda j: acc[-1][j])
    path = [0] * n
    for i in range(n - 1, -1, -1):
        path[i] = k
        k = back[i][k]
    sw = [cands[i][path[i]][1] for i in range(n)]
    off = [cands[i][path[i]][2] for i in range(n)]
    # smoothing + re-verification: smooth the parameters, then where a frame is no longer clear, pull it back toward
    # its DP state and smooth again
    def gauss(vals, is_vec):
        out = []
        for i in range(n):
            wsum, acc_v = 0.0, (0.0, 0.0, 0.0) if is_vec else 0.0
            for j in range(n):
                w = math.exp(-0.5 * ((frames[j].t - frames[i].t) / SMOOTH_SIGMA_S) ** 2)
                if w < 1e-4:
                    continue
                wsum += w
                acc_v = add(acc_v, mul(vals[j], w)) if is_vec else acc_v + w * vals[j]
            out.append(mul(acc_v, 1.0 / wsum) if is_vec else acc_v / wsum)
        return out
    target_sw, target_off = sw[:], off[:]
    for it in range(8):
        sm_sw, sm_off = gauss(sw, False), gauss(off, True)
        bad = 0
        for i, F in enumerate(frames):
            gap = evaluate(F, sm_sw[i], sm_off[i])[2][0]
            if gap < MARGIN - 0.05 and base[i][2][0] < MARGIN + 3.0:
                bad += 1
                # push this frame's parameters further toward (and beyond) its feasible DP state
                sw[i] = sw[i] + 0.6 * (target_sw[i] - sm_sw[i])
                off[i] = add(off[i], mul(sub(target_off[i], sm_off[i]), 0.6))
        if not bad:
            break
    sw, off = gauss(sw, False), gauss(off, True)
    result = []
    for i, F in enumerate(frames):
        E, P, gap = evaluate(F, sw[i], off[i])
        result.append((sw[i], off[i], E, P, gap, base[i]))
    return result


def local_rotations(F, E, P):
    """New CS rotations of the three bones (swing from the recorded ones) and their LOCAL rotations relative to the
    recorded parents (clavicle_l / the new upperarm_l / the new lowerarm_l)."""
    q_upper = qnorm(qmul(qbetween(sub(F.E0, F.S), sub(E, F.S)), F.q_upper0))
    fore_old = qrot(qbetween(sub(F.E0, F.S), sub(E, F.S)), sub(F.H0, F.E0))
    q_lower = qnorm(qmul(qbetween(fore_old, sub(P, E)), qmul(qbetween(sub(F.E0, F.S), sub(E, F.S)), F.q_lower0)))
    q_hand = F.R
    return {
        "upperarm_l": qnorm(qmul(qinv(F.q_clav), q_upper)),
        "lowerarm_l": qnorm(qmul(qinv(q_upper), q_lower)),
        "hand_l": qnorm(qmul(qinv(q_lower), q_hand)),
        "cs": {"upperarm_l": q_upper, "lowerarm_l": q_lower, "hand_l": q_hand},
    }


def main(paths):
    recs = [load_recording(p) for p in paths]
    clip = recs[0]["clip"]
    bodies = load_bodies()
    grip_in_bone = settled_grip(recs)
    rec = recs[-1]
    frames = [frame_setup(f, bodies, grip_in_bone) for f in rec["frames"]]
    print("%s (%s, %d frames %.3f..%.3f s)  legs: %d obstacle shapes" % (clip, os.path.basename(rec["path"]), len(frames),
          frames[0].t, frames[-1].t, len(frames[0].legs)))
    res = solve(frames)
    out_frames = []
    worst_before, worst_after, max_sw, max_off, max_elbow = 1e9, 1e9, 0.0, 0.0, 0.0
    for F, (s, o, E, P, gap, base) in zip(frames, res):
        rots = local_rotations(F, E, P)
        worst_before = min(worst_before, base[2][0])
        worst_after = min(worst_after, gap[0])
        max_sw = max(max_sw, abs(s))
        max_off = max(max_off, length(o) * (1.0 - F.alpha))
        max_elbow = max(max_elbow, length(sub(E, base[0])))
        out_frames.append({
            "clipTime": F.t, "alpha": F.alpha, "swivelDeg": s, "handOffset": list(mul(o, 1.0 - F.alpha)),
            "gapBefore": base[2][0], "gapAfter": gap[0], "partAfter": gap[1], "legAfter": gap[2],
            "shoulder": list(F.S), "elbowBefore": list(base[0]), "elbowAfter": list(E), "wristBefore": list(F.P0), "wristAfter": list(P),
            "local": {b: list(rots[b]) for b in ("upperarm_l", "lowerarm_l", "hand_l")},
            "cs": {b: list(v) for b, v in rots["cs"].items()},
        })
    # speeds of the corrected elbow / wrist vs the recorded ones
    def peak_speed(key):
        v = 0.0
        for a, b in zip(out_frames, out_frames[1:]):
            dt = max(1e-3, b["clipTime"] - a["clipTime"])
            v = max(v, length(sub(b[key], a[key])) / dt)
        return v
    report = {"clip": clip, "recording": os.path.basename(rec["path"]), "frames": len(frames),
              "worstGapBefore": worst_before, "worstGapAfter": worst_after, "maxSwivelDeg": max_sw, "maxHandOffset": max_off,
              "maxElbowMove": max_elbow, "peakElbowSpeedBefore": peak_speed("elbowBefore"), "peakElbowSpeedAfter": peak_speed("elbowAfter"),
              "peakWristSpeedBefore": peak_speed("wristBefore"), "peakWristSpeedAfter": peak_speed("wristAfter"), "margin": MARGIN}
    os.makedirs(ARM_DIR, exist_ok=True)
    path = os.path.join(ARM_DIR, clip + ".solve.json")
    json.dump({"report": report, "gripInBone": [*grip_in_bone.t, *grip_in_bone.q], "frames": out_frames}, open(path, "w"), indent=1)
    for k, v in report.items():
        print("  %-22s %s" % (k, ("%.2f" % v) if isinstance(v, float) else v))
    print("  ->", path)
    return path


if __name__ == "__main__":
    main(sys.argv[1:])
