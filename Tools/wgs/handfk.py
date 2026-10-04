"""Hand forward kinematics in weapon space for the WGS grasp tools (pure Python).

Transforms are (pos, quat) with quat = (x, y, z, w) in UE convention; world = compose(local, parent):
pos = parent.pos + parent.rot * local.pos, rot = parent.rot * local.rot (scale 1).
"""
import math

FINGERS = ("thumb", "index", "middle", "ring", "pinky")
RADII = {"thumb": (1.0, 0.85, 0.75), "other": (0.85, 0.75, 0.65)}   # proximal, middle, distal phalanx (cm)
TIP_EXT = 0.85                                                      # distal end = _03 + 0.85 x (_03 - _02)


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))


def unit(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-12 else (0.0, 0.0, 0.0)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q): return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    u = (q[0], q[1], q[2])
    t = mul(cross(u, v), 2.0)
    return add(add(v, mul(t, q[3])), cross(u, t))


def qaxis(axis, deg):
    h = math.radians(deg) * 0.5
    s = math.sin(h)
    a = unit(axis)
    return (a[0] * s, a[1] * s, a[2] * s, math.cos(h))


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


def compose(local, parent):
    lp, lq = local
    pp, pq = parent
    return add(pp, qrot(pq, lp)), qnorm(qmul(pq, lq))


def tr(v): return (tuple(v[:3]), tuple(v[3:7]))


def finger_bones(f, side):
    return ["%s_0%d_%s" % (f, i, side) for i in (1, 2, 3)]


class Hand(object):
    """World (weapon-space) transforms of one hand's chain from a hand transform + bone locals."""

    def __init__(self, rec, side, locals_key="grip", finger_locals=None):
        self.side = side
        self.bones = rec["bones"]
        self.parents = rec["parents"]
        block = rec[locals_key]
        self.hand = tr(block["hand_ws"])
        self.local = {b: tr(v) for b, v in block["local"].items()}
        if finger_locals:
            self.local.update(finger_locals)
        self.solve()

    def solve(self):
        self.world = {"hand_" + self.side: self.hand}
        for b in self.bones[1:]:
            self.world[b] = compose(self.local[b], self.world[self.parents[b]])

    def pos(self, b): return self.world[b][0]

    def tip(self, f):
        a, b = self.pos("%s_02_%s" % (f, self.side)), self.pos("%s_03_%s" % (f, self.side))
        return add(b, mul(sub(b, a), TIP_EXT))

    def segments(self, f):
        """(start, end, radius) of the proximal, middle and distal phalanx."""
        b1, b2, b3 = (self.pos(x) for x in finger_bones(f, self.side))
        r = RADII["thumb" if f == "thumb" else "other"]
        return [(b1, b2, r[0]), (b2, b3, r[1]), (b3, self.tip(f), r[2])]

    def local_from_world(self, b, world_rot):
        prot = self.world[self.parents[b]][1]
        return qnorm(qmul(qinv(prot), world_rot))


def segment_clearance(weapon, seg, samples=4):
    """Min over sample points of (signed distance - radius); < 0 = the phalanx skin is inside the weapon."""
    a, b, r = seg
    best = 1e9
    for i in range(samples + 1):
        p = add(a, mul(sub(b, a), i / float(samples)))
        best = min(best, weapon.sdf(p) - r)
    return best
