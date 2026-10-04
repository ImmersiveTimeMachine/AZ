"""Small vector / quaternion helpers for the switch-arm solver (UE conventions: FQuat (x, y, z, w), Hamilton product,
child CS = parent CS * child local -> rotation parent (x) child)."""
import math


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def lerp(a, b, t): return add(a, mul(sub(b, a), t))


def normalize(a, fallback=(0.0, 0.0, 1.0)):
    n = length(a)
    return mul(a, 1.0 / n) if n > 1e-9 else fallback


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q): return (-q[0], -q[1], -q[2], q[3])


def qnorm(q):
    n = math.sqrt(sum(x * x for x in q))
    return tuple(x / n for x in q)


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return (r[0], r[1], r[2])


def qaxis(axis, angle):
    s = math.sin(angle / 2.0)
    a = normalize(axis)
    return (a[0] * s, a[1] * s, a[2] * s, math.cos(angle / 2.0))


def qslerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0.0:
        b = tuple(-x for x in b)
        d = -d
    if d > 0.9995:
        return qnorm(tuple(x + (y - x) * t for x, y in zip(a, b)))
    th = math.acos(min(1.0, d))
    s = math.sin(th)
    wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
    return tuple(wa * x + wb * y for x, y in zip(a, b))


def qangle(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return 2.0 * math.acos(min(1.0, d))


def qbetween(u, v):
    """Shortest rotation taking direction u onto direction v."""
    u, v = normalize(u), normalize(v)
    c = dot(u, v)
    if c < -0.999999:
        perp = normalize(cross(u, (1.0, 0.0, 0.0)) if abs(u[0]) < 0.9 else cross(u, (0.0, 1.0, 0.0)))
        return (perp[0], perp[1], perp[2], 0.0)
    ax = cross(u, v)
    return qnorm((ax[0], ax[1], ax[2], 1.0 + c))


def rotator_quat(pitch, yaw, roll):
    """FRotator::Quaternion (degrees)."""
    p, y, r = (math.radians(x) / 2.0 for x in (pitch, yaw, roll))
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return (cr * sp * sy - sr * cp * cy, -cr * sp * cy - sr * cp * sy, cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy)


class Xf:
    """Rigid transform (rotation quat, translation)."""
    __slots__ = ("q", "t")

    def __init__(self, q, t):
        self.q, self.t = q, t

    @staticmethod
    def f7(v):
        return Xf((v[3], v[4], v[5], v[6]), (v[0], v[1], v[2]))

    def point(self, p): return add(self.t, qrot(self.q, p))
    def vector(self, v): return qrot(self.q, v)
    def local_point(self, p): return qrot(qinv(self.q), sub(p, self.t))

    def __mul__(self, parent):
        """UE order: (self * parent) = self expressed in parent's space, applied after parent."""
        return Xf(qmul(parent.q, self.q), parent.point(self.t))


def closest_seg_seg(p1, q1, p2, q2):
    """Closest points between segments [p1,q1] and [p2,q2] (Ericson). Returns (distance, point on 1, point on 2)."""
    d1, d2, r = sub(q1, p1), sub(q2, p2), sub(p1, p2)
    a, e, f = dot(d1, d1), dot(d2, d2), dot(d2, r)
    if a <= 1e-9 and e <= 1e-9:
        return length(r), p1, p2
    if a <= 1e-9:
        s, t = 0.0, max(0.0, min(1.0, f / e))
    else:
        c = dot(d1, r)
        if e <= 1e-9:
            t, s = 0.0, max(0.0, min(1.0, -c / a))
        else:
            b = dot(d1, d2)
            den = a * e - b * b
            s = max(0.0, min(1.0, (b * f - c * e) / den)) if den > 1e-9 else 0.0
            t = (b * s + f) / e
            if t < 0.0:
                t, s = 0.0, max(0.0, min(1.0, -c / a))
            elif t > 1.0:
                t, s = 1.0, max(0.0, min(1.0, (b - c) / a))
    c1, c2 = add(p1, mul(d1, s)), add(p2, mul(d2, t))
    return length(sub(c1, c2)), c1, c2


def smoothstep(e0, e1, x):
    if e1 <= e0:
        return 1.0 if x >= e1 else 0.0
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)
