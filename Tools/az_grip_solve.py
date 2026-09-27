"""Weapon grip pilot - solve a hand grip on the weapon's real surface (offline, plain Python 3, no Unreal).

Input  Saved/az_grip_reference.json (Tools/az_grip_export.py): bones + weapon triangles in weapon-model space.
Output Saved/az_grip_solution.json:
   left_hand_grip  : hand_l target transform in weapon-model space (-> the weapon mesh socket LeftHandGrip)
   pose_local      : finger bone LOCAL rotations of the solved grip (both hands) (-> the grip pose asset)
   report lines

Method (units cm):
  * surface query: nearest-triangle distance + inside/outside by vertical ray parity (winding-free; the whole-weapon
    meshes are merged parts, normals are not trusted);
  * hand = palm capsules (hand bone -> each finger base, radius PALM_R) + finger capsules (proximal/middle/distal,
    radii FINGER_R); clearance = surface distance - radius (negative = inside the weapon);
  * LEFT hand (IK-placed at runtime): translated along the palm -> surface direction until the palm rests on the
    surface (PALM_GAP); the RIGHT hand is the attach reference and is not moved;
  * every finger (both hands): rotated about the knuckle line (thumb: its own curl axis) - 0.5 / 1 / 1 of the angle at
    its _01 / _02 / _03 joints - closing or opening until its closest capsule point touches (CONTACT band).
"""
import json
import math
import os
import sys
from collections import defaultdict

SAVED = r"C:/UnrealEngine/Games/AZ/Saved"
IN = os.path.join(SAVED, sys.argv[1] if len(sys.argv) > 1 else "az_grip_reference.json")
OUT = os.path.join(SAVED, sys.argv[2] if len(sys.argv) > 2 else "az_grip_solution.json")
PALM_R, PALM_GAP = 1.1, 0.1
FINGER_R = (0.8, 0.72, 0.62)
CONTACT = (0.02, 0.25)
JOINT_OPEN, JOINT_CLOSE = 30, 70    # per-joint sweep (deg): open / close
PEN_TOL = 0.05                     # allowed skin overlap (cm)
SPLIT = (0.5, 1.0, 1.0)
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
HAND_PARTS = ("hand_", "thumb", "index", "middle", "ring", "pinky")
# Right hand slide along the weapon before the palm rest (weapon-model cm). Winchester: the reference hold had the
# palm ~3 cm behind the receiver and the thumb tip on the receiver top; the pack gun is held ~5-6 cm behind it with
# the thumb wrapped over the wrist (user 2026-09-26) -> slide the hand 3 cm back onto the wrist.
# Roll of the weapon about its barrel so it sits upright like the mocap gun (deg, measured by comparing the pack
# gun's up axis with ours in the hand frame; Winchester -18.5, user 2026-09-26 'сильно повернуто влево').
ROLL_DEG = float(os.environ.get("AZ_GRIP_ROLL", "-18.5"))
BARREL_AXIS = (0.0, 1.0, 0.0)          # weapon-model forward axis
RIGHT_SLIDE = tuple(float(x) for x in os.environ.get("AZ_GRIP_RIGHT_SLIDE", "0,-3,0").split(","))


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def unit(a):
    n = length(a)
    return mul(a, 1.0 / n) if n > 1e-12 else (0.0, 0.0, 0.0)
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def qinv(a): return (-a[0], -a[1], -a[2], a[3])
def qrot(a, p):
    r = qmul(qmul(a, (p[0], p[1], p[2], 0.0)), qinv(a))
    return (r[0], r[1], r[2])
def qaxis(axis, ang):
    s = math.sin(ang / 2)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(ang / 2))
def qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


def closest_on_tri(p, a, b, c):
    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    d1, d2 = dot(ab, ap), dot(ac, ap)
    if d1 <= 0 and d2 <= 0: return a
    bp = sub(p, b); d3, d4 = dot(ab, bp), dot(ac, bp)
    if d3 >= 0 and d4 <= d3: return b
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0: return add(a, mul(ab, d1 / (d1 - d3)))
    cp = sub(p, c); d5, d6 = dot(ab, cp), dot(ac, cp)
    if d6 >= 0 and d5 <= d6: return c
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0: return add(a, mul(ac, d2 / (d2 - d6)))
    va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
        return add(b, mul(sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))))
    den = 1.0 / (va + vb + vc)
    return add(a, add(mul(ab, vb * den), mul(ac, vc * den)))


class Surface(object):
    def __init__(self, verts, tris, near_pts, margin=16.0, cell=2.0):
        V = [tuple(v) for v in verts]
        T = [(V[tris[i]], V[tris[i + 1]], V[tris[i + 2]]) for i in range(0, len(tris), 3)]
        lo = tuple(min(p[i] for p in near_pts) - margin for i in range(3))
        hi = tuple(max(p[i] for p in near_pts) + margin for i in range(3))
        self.near = [t for t in T if all(min(t[0][i], t[1][i], t[2][i]) <= hi[i] and max(t[0][i], t[1][i], t[2][i]) >= lo[i]
                                         for i in range(3))]
        self.cell, self.xy = cell, defaultdict(list)
        for a, b, c in T:
            for i in range(int(math.floor(min(a[0], b[0], c[0]) / cell)), int(math.floor(max(a[0], b[0], c[0]) / cell)) + 1):
                for j in range(int(math.floor(min(a[1], b[1], c[1]) / cell)), int(math.floor(max(a[1], b[1], c[1]) / cell)) + 1):
                    self.xy[(i, j)].append((a, b, c))

    def _crossings(self, p, up):
        n = 0
        for a, b, c in self.xy.get((int(math.floor(p[0] / self.cell)), int(math.floor(p[1] / self.cell))), ()):
            d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(d) < 1e-12:
                continue
            l1 = ((b[1] - c[1]) * (p[0] - c[0]) + (c[0] - b[0]) * (p[1] - c[1])) / d
            l2 = ((c[1] - a[1]) * (p[0] - c[0]) + (a[0] - c[0]) * (p[1] - c[1])) / d
            l3 = 1 - l1 - l2
            if l1 < 0 or l2 < 0 or l3 < 0:
                continue
            z = l1 * a[2] + l2 * b[2] + l3 * c[2]
            if (z > p[2]) if up else (z < p[2]):
                n += 1
        return n

    def inside(self, p):
        u, d = self._crossings(p, True) % 2, self._crossings(p, False) % 2
        return u == 1 if u == d else (u == 1 and d == 1)

    def nearest(self, p):
        best, bq = 1e18, None
        for a, b, c in self.near:
            q = closest_on_tri(p, a, b, c)
            d = length(sub(p, q))
            if d < best:
                best, bq = d, q
        return best, bq

    def sdist(self, p):
        d, _ = self.nearest(p)
        return -d if self.inside(p) else d


def main():
    R = json.load(open(IN))
    M = {b: (tuple(v[:3]), tuple(v[3:7])) for b, v in R["model"].items()}
    L = {b: (tuple(v[:3]), tuple(v[3:7])) for b, v in R["local"].items()}
    hand_pts = [M[b][0] for b in M if b.endswith("_l") or b.endswith("_r")
                if any(k in b for k in ("hand", "thumb", "index", "middle", "ring", "pinky"))]
    S = Surface(R["verts"], R["tris"], hand_pts)
    lines, pose_local = [], {}
    right_shift = (0.0, 0.0, 0.0)

    def chain(f, sd):
        return ["%s_0%d_%s" % (f, k, sd) for k in (1, 2, 3)]

    def palm_samples(bones, sd):
        hb = bones["hand_" + sd][0]
        pts = []
        for f in ("index", "middle", "ring", "pinky"):
            k = bones["%s_01_%s" % (f, sd)][0]
            pts += [add(hb, mul(sub(k, hb), t)) for t in (0.35, 0.65, 0.9)]
        return pts

    def palm_clear(bones, sd):
        return min(S.sdist(p) - PALM_R for p in palm_samples(bones, sd))

    def finger_pts(pos):
        p1, p2, p3 = pos
        tip = add(p3, mul(sub(p3, p2), 0.85))
        segs = ((p1, p2, FINGER_R[0]), (p2, p3, FINGER_R[1]), (p3, tip, FINGER_R[2]))
        return [(add(a, mul(sub(b, a), k / 4.0)), r) for a, b, r in segs for k in range(5)]

    def finger_clear(pos):
        return min(S.sdist(p) - r for p, r in finger_pts(pos))

    def seg_clear(pts, first):
        """Min clearance of the finger segments first..2 (0 proximal, 1 middle, 2 distal) of a 4-point chain."""
        best = 1e9
        for si in range(first, 3):
            a, b = pts[si], pts[si + 1]
            for k in range(5):
                best = min(best, S.sdist(add(a, mul(sub(b, a), k / 4.0))) - FINGER_R[si])
        return best

    def rotate_joint(pts, rots, k, axis, theta):
        """Turn joint k (world axis through pts[k]) by theta; everything after it follows."""
        q = qaxis(axis, theta)
        out = list(pts)
        for j in range(k + 1, 4):
            out[j] = add(pts[k], qrot(q, sub(pts[j], pts[k])))
        rr = list(rots)
        for j in range(k, 3):
            rr[j] = qmul(q, rots[j])
        return out, rr

    def rotate_chain(pos, rot, axis, theta):
        """Rotate the chain by theta*SPLIT[k] at each joint about the (world) axis carried by the chain."""
        axl = [qrot(qinv(r), axis) for r in rot]
        pts = list(pos) + [add(pos[2], mul(sub(pos[2], pos[1]), 0.85))]
        rots = list(rot)
        for k in range(3):
            qw = qmul(rots[k], qmul(qaxis(axl[k], theta * SPLIT[k]), qinv(rots[k])))
            for j in range(k + 1, 4):
                pts[j] = add(pts[k], qrot(qw, sub(pts[j], pts[k])))
            for j in range(k, 3):
                rots[j] = qmul(qw, rots[j])
        return pts[:3], rots

    # roll: the weapon turns by ROLL_DEG about its barrel through the wrist centre next to the right palm ==
    # both hands turn by -ROLL_DEG about that axis in weapon-model space
    pr = mul(add(M["hand_r"][0], M["middle_01_r"][0]), 0.5)
    near_pts = [tuple(v) for v in R["verts"] if abs(v[1] - pr[1]) < 3.0]
    pivot = tuple(sum(v[i] for v in near_pts) / len(near_pts) for i in range(3)) if near_pts else pr
    q_roll = qaxis(BARREL_AXIS, -math.radians(ROLL_DEG))
    M = {b: ((add(pivot, qrot(q_roll, sub(t[0], pivot))), qmul(q_roll, t[1])) if (b[-2:] in ("_l", "_r") and any(k in b for k in HAND_PARTS)) else t)
         for b, t in M.items()}
    lines.append("weapon rolled %+.1f deg about its barrel (pivot %s)" % (ROLL_DEG, tuple(round(x, 1) for x in pivot)))
    for sd in ("l", "r"):
        bones = dict(M)
        shift = (0.0, 0.0, 0.0)
        if sd == "r" and length(RIGHT_SLIDE) > 0:
            shift = RIGHT_SLIDE
            bones = {b: (add(t[0], shift), t[1]) if (b.endswith("_r") and any(k in b for k in HAND_PARTS)) else t
                     for b, t in bones.items()}
        for _ in range(5):
            pc = palm_clear(bones, sd)
            centre = mul(add(bones["hand_" + sd][0], bones["middle_01_" + sd][0]), 0.5)
            _, q = S.nearest(centre)
            v = unit(sub(q, centre))                       # palm -> surface
            if S.inside(centre):
                v = mul(v, -1.0)
            move = PALM_GAP - pc                            # >0: back off, <0: come closer
            if abs(move) < 0.03 or (sd == "r" and 0.0 <= pc <= 0.4):
                break
            d = mul(v, -move)
            shift = add(shift, d)
            bones = {b: (add(t[0], d), t[1]) if (b.endswith("_" + sd) and any(k in b for k in HAND_PARTS)) else t
                     for b, t in bones.items()}
        if sd == "l":
            lines.append("left hand moved %.2f cm to rest the palm on the weapon (palm clearance %.2f -> %.2f cm)"
                         % (length(shift), palm_clear(M, sd), palm_clear(bones, sd)))
        else:
            # the right hand is where the weapon is attached: moving it relative to the weapon = moving the weapon
            # by -shift in its own model space -> a new attach socket (S' = T(-shift) then S)
            right_shift = shift
            lines.append("right hand: slid %s cm + palm rest -> weapon moved %.2f cm in the hand (palm clearance %.2f -> %.2f cm)"
                         % (RIGHT_SLIDE, length(shift), palm_clear(M, sd), palm_clear(bones, sd)))
        knuck = unit(sub(bones["index_01_" + sd][0], bones["pinky_01_" + sd][0]))
        hand_p = bones["hand_" + sd][0]
        for f in FINGERS:
            names = chain(f, sd)
            pos = [bones[n][0] for n in names]
            rots = [bones[n][1] for n in names]
            pts0 = pos + [add(pos[2], mul(sub(pos[2], pos[1]), 0.85))]
            c0 = seg_clear(pts0, 0)
            # Anatomical grasp: two parameters per finger - a = base joint (_01), b = middle joint (_02) with the last
            # joint coupled (_03 = 0.7 b), inside human-like limits and as close to the mocap as possible. Cost: any
            # part inside the weapon (heavy) + the fingertip not resting on the surface + distance from the mocap.
            if f == "thumb":
                _, q = S.nearest(pts0[3])
                axis = unit(cross(sub(pts0[3], pts0[0]), sub(q, pts0[3])))      # swing the thumb tip towards the wood
                if S.inside(pts0[3]):
                    axis = mul(axis, -1.0)
                close = 1.0
            else:
                axis = knuck
                tst, _ = rotate_joint(pts0, rots, 0, axis, math.radians(10))
                close = 1.0 if length(sub(tst[3], hand_p)) < length(sub(pts0[3], hand_p)) else -1.0

            def pose_ab(a, b):
                p_, r_ = rotate_joint(pts0, rots, 0, axis, close * math.radians(a))
                p_, r_ = rotate_joint(p_, r_, 1, axis, close * math.radians(b))
                return rotate_joint(p_, r_, 2, axis, close * math.radians(0.7 * b))

            def cost(a, b):
                p_, r_ = pose_ab(a, b)
                pen, tip = 0.0, 1e9
                for si in range(3):
                    s0, s1 = p_[si], p_[si + 1]
                    for k in range(5):
                        c = S.sdist(add(s0, mul(sub(s1, s0), k / 4.0))) - FINGER_R[si]
                        if c < -PEN_TOL:
                            pen += (c + PEN_TOL) ** 2
                        if si == 2:
                            tip = min(tip, c)
                return 100.0 * pen + 2.0 * max(0.0, tip - CONTACT[1]) ** 2 + 0.00015 * (a * a + b * b), p_, r_, tip

            if length(axis) < 1e-6:
                best = (0.0, 0.0) + cost(0.0, 0.0)
            else:
                grid = [(a, b) for a in range(-30, 46, 15) for b in range(-20, 61, 10)]
                best = min(((a, b) + cost(a, b) for a, b in grid), key=lambda c: c[2])
                for step_a, step_b in ((5.0, 5.0), (2.0, 2.0)):
                    ca, cb = best[0], best[1]
                    for da in (-step_a, 0.0, step_a):
                        for db in (-step_b, 0.0, step_b):
                            a, b = max(-30.0, min(45.0, ca + da)), max(-20.0, min(60.0, cb + db))
                            c = (a, b) + cost(a, b)
                            if c[2] < best[2]:
                                best = c
            a, b, _, pts, rots, tip = best
            c1 = seg_clear(pts, 0)
            lines.append("%s %-6s base %+4.0f / middle %+4.0f / last %+4.0f deg (+ = close) | clearance %+.2f -> %+.2f cm, tip %+.2f"
                         % (sd, f, a, b, 0.7 * b, c0, c1, tip))
            parent0 = "hand_" + sd if f == "thumb" else "%s_metacarpal_%s" % (f, sd)
            par_rot = M[parent0][1]
            for n, r_ in zip(names, rots):
                pose_local[n] = list(qnorm(qmul(qinv(par_rot), r_)))
                par_rot = r_
        if sd == "l":
            left_grip = [bones["hand_l"][0][0], bones["hand_l"][0][1], bones["hand_l"][0][2]] + list(bones["hand_l"][1])
    # new attach socket: model point p -> hand space was S(p); with the hand moved by +d in model space it is S(p - d)
    sr = R["socket_rel"]
    Sp, Sq = tuple(sr[:3]), tuple(sr[3:7])
    qi = qinv(q_roll)
    new_loc = add(Sp, qrot(Sq, sub(pivot, qrot(qi, add(right_shift, pivot)))))
    new_rot = qnorm(qmul(Sq, qi))
    out = {"weapon_mesh": R["weapon_mesh"], "socket": R["socket"], "clip": R["clip"], "frame": R["frame"],
           "socket_new": list(new_loc) + list(new_rot), "roll_deg": ROLL_DEG, "socket_old": list(sr), "right_shift_model": list(right_shift),
           "left_hand_grip": left_grip, "pose_local": pose_local, "report": lines}
    json.dump(out, open(OUT, "w"), indent=1)
    print("\n".join(lines))


if __name__ == "__main__":
    main()
