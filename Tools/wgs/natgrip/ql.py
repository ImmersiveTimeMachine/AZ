import math
def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz)
def qinv(q): return (-q[0], -q[1], -q[2], q[3])
def qrot(q, v):
    x, y, z, w = q
    t = (2*(y*v[2]-z*v[1]), 2*(z*v[0]-x*v[2]), 2*(x*v[1]-y*v[0]))
    return (v[0] + w*t[0] + (y*t[2]-z*t[1]), v[1] + w*t[1] + (z*t[0]-x*t[2]), v[2] + w*t[2] + (x*t[1]-y*t[0]))
def qaxis(axis, ang):
    s = math.sin(ang/2); return (axis[0]*s, axis[1]*s, axis[2]*s, math.cos(ang/2))
def qnorm(q):
    n = math.sqrt(sum(c*c for c in q)); return tuple(c/n for c in q)
def add(a, b): return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def mul(a, s): return (a[0]*s, a[1]*s, a[2]*s)
def dot(a, b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def length(a): return math.sqrt(dot(a, a))
def norm(a):
    l = length(a); return mul(a, 1.0/l) if l > 1e-12 else (0.0, 0.0, 0.0)
class T(object):
    """UE FTransform (no scale): A*B = apply A then B."""
    __slots__ = ("q", "t")
    def __init__(self, q=(0, 0, 0, 1), t=(0, 0, 0)): self.q, self.t = tuple(q), tuple(t)
    @staticmethod
    def f7(a): return T(a[3:7], a[0:3])
    def __mul__(self, p):  # self (child local) * p (parent)
        return T(qnorm(qmul(p.q, self.q)), add(qrot(p.q, self.t), p.t))
    def inv(self):
        qi = qinv(self.q); return T(qi, mul(qrot(qi, self.t), -1))
    def pos(self, v): return add(qrot(self.q, v), self.t)
    def vec(self, v): return qrot(self.q, v)
    def ipos(self, v): return qrot(qinv(self.q), sub(v, self.t))
    def ivec(self, v): return qrot(qinv(self.q), v)
def ang_of(q):
    w = max(-1.0, min(1.0, abs(q[3]))); return 2*math.degrees(math.acos(w))
