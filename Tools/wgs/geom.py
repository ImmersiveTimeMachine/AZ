"""WGS task 1.2 - weapon geometry queries (pure Python, runs in CPython and in UE Python).

Signed distance to a weapon built from separate parts (Saved/wgs/weapons/<weapon>_parts.json, verts in cm, weapon
model space). Per part: exact nearest-triangle distance through a uniform grid of cells (the "lattice": every cell
lists the triangles whose box touches it), and an inside test by the GENERALISED WINDING NUMBER of that part alone.
Why per part + winding number: 6 of the 8 Winchester parts are open meshes (boundary edges) and several overlap; the
old ray-parity test over the merged mesh counted crossings of two overlapping parts as "outside" and broke on open
edges, so the old grasp solver stopped fingers in the wrong places. The union SDF = min over parts.

    from geom import Weapon
    w = Weapon.load(r"C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json")
    d = w.sdf((x, y, z))        # cm, < 0 inside
"""
import json
import math

FOUR_PI = 4.0 * math.pi


def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(_dot(a, a))


def closest_on_triangle(p, a, b, c):
    """Ericson, Real-Time Collision Detection 5.1.5."""
    ab, ac, ap = _sub(b, a), _sub(c, a), _sub(p, a)
    d1, d2 = _dot(ab, ap), _dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        return a
    bp = _sub(p, b)
    d3, d4 = _dot(ab, bp), _dot(ac, bp)
    if d3 >= 0 and d4 <= d3:
        return b
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0:
        v = d1 / (d1 - d3)
        return (a[0] + v * ab[0], a[1] + v * ab[1], a[2] + v * ab[2])
    cp = _sub(p, c)
    d5, d6 = _dot(ab, cp), _dot(ac, cp)
    if d6 >= 0 and d5 <= d6:
        return c
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0:
        w = d2 / (d2 - d6)
        return (a[0] + w * ac[0], a[1] + w * ac[1], a[2] + w * ac[2])
    va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return (b[0] + w * (c[0] - b[0]), b[1] + w * (c[1] - b[1]), b[2] + w * (c[2] - b[2]))
    denom = 1.0 / (va + vb + vc)
    v, w = vb * denom, vc * denom
    return (a[0] + ab[0] * v + ac[0] * w, a[1] + ab[1] * v + ac[1] * w, a[2] + ab[2] * v + ac[2] * w)


class Part(object):
    def __init__(self, name, verts, tris, cell=1.5):
        self.name = name
        self.V = [tuple(v) for v in verts]
        self.T = [(self.V[tris[i]], self.V[tris[i + 1]], self.V[tris[i + 2]]) for i in range(0, len(tris), 3)]
        self.cell = cell
        self.grid = {}
        for t in self.T:
            lo = [int(math.floor(min(t[0][k], t[1][k], t[2][k]) / cell)) for k in range(3)]
            hi = [int(math.floor(max(t[0][k], t[1][k], t[2][k]) / cell)) for k in range(3)]
            for i in range(lo[0], hi[0] + 1):
                for j in range(lo[1], hi[1] + 1):
                    for k in range(lo[2], hi[2] + 1):
                        self.grid.setdefault((i, j, k), []).append(t)
        self.lo = tuple(min(v[k] for v in self.V) for k in range(3))
        self.hi = tuple(max(v[k] for v in self.V) for k in range(3))

    def box_distance(self, p):
        d = [max(self.lo[k] - p[k], 0.0, p[k] - self.hi[k]) for k in range(3)]
        return _len(d)

    def distance(self, p, max_dist):
        """Unsigned distance, exact within max_dist; returns max_dist when nothing is closer."""
        if self.box_distance(p) >= max_dist:
            return max_dist
        c = self.cell
        ci = [int(math.floor(p[k] / c)) for k in range(3)]
        best = max_dist
        seen = set()
        rings = int(math.ceil(max_dist / c))
        for r in range(rings + 1):
            if r > 0 and (r - 1) * c > best:
                break
            for i in range(ci[0] - r, ci[0] + r + 1):
                for j in range(ci[1] - r, ci[1] + r + 1):
                    for k in range(ci[2] - r, ci[2] + r + 1):
                        if max(abs(i - ci[0]), abs(j - ci[1]), abs(k - ci[2])) != r:
                            continue
                        for t in self.grid.get((i, j, k), ()):
                            key = id(t)
                            if key in seen:
                                continue
                            seen.add(key)
                            q = closest_on_triangle(p, t[0], t[1], t[2])
                            d = _len(_sub(p, q))
                            if d < best:
                                best = d
        return best

    def winding(self, p):
        """Generalised winding number (Jacobson 2013): ~1 inside, ~0 outside, robust to holes in the surface."""
        total = 0.0
        for a, b, c in self.T:
            A, B, C = _sub(a, p), _sub(b, p), _sub(c, p)
            la, lb, lc = _len(A), _len(B), _len(C)
            if la < 1e-9 or lb < 1e-9 or lc < 1e-9:
                return 0.5
            num = _dot(A, _cross(B, C))
            den = la * lb * lc + _dot(A, B) * lc + _dot(A, C) * lb + _dot(B, C) * la
            total += 2.0 * math.atan2(num, den)
        return total / FOUR_PI


class Weapon(object):
    def __init__(self, parts):
        self.parts = parts

    @staticmethod
    def load(path, cell=1.5):
        data = json.load(open(path))
        return Weapon([Part(n, v["verts"], v["tris"], cell) for n, v in data.items()])

    def sdf(self, p, max_dist=6.0):
        """Signed distance (cm) to the union of the parts; < 0 inside. Exact where |d| < max_dist.
        The winding test (the expensive part) runs only for parts closer than 2.5 cm, nearest first, and stops at the
        first part that contains the point (no Winchester part is thicker than 5 cm)."""
        dists = sorted((part.distance(p, max_dist), i) for i, part in enumerate(self.parts))
        for d, i in dists:
            if d >= 2.5:
                break
            if abs(self.parts[i].winding(p)) > 0.5:
                return -d
        return dists[0][0]

    def lattice(self, lo, hi, h, cache=None):
        """Precomputed signed-distance lattice over the box [lo, hi] (spacing h cm), trilinear queries. Cached as JSON."""
        if cache:
            try:
                data = json.load(open(cache))
                if data["lo"] == list(lo) and data["hi"] == list(hi) and data["h"] == h:
                    return SDFGrid(lo, h, data["n"], data["v"])
            except (IOError, OSError, ValueError, KeyError):
                pass
        n = [int(math.ceil((hi[k] - lo[k]) / h)) + 1 for k in range(3)]
        v = [self.sdf((lo[0] + i * h, lo[1] + j * h, lo[2] + k * h))
             for i in range(n[0]) for j in range(n[1]) for k in range(n[2])]
        if cache:
            json.dump({"lo": list(lo), "hi": list(hi), "h": h, "n": n, "v": v}, open(cache, "w"))
        return SDFGrid(lo, h, n, v)

    def nearest_part(self, p, max_dist=6.0):
        best, name = max_dist, None
        for part in self.parts:
            d = part.distance(p, max_dist)
            if d < best:
                best, name = d, part.name
        return name, best


class SDFGrid(object):
    """Trilinear lookup into a precomputed signed-distance lattice (Weapon.lattice). Outside the box: exact fallback."""

    def __init__(self, lo, h, n, v, fallback=None):
        self.lo, self.h, self.n, self.v, self.fallback = tuple(lo), h, n, v, fallback
        self.nyz, self.nz = n[1] * n[2], n[2]

    def sdf(self, p):
        f = [(p[k] - self.lo[k]) / self.h for k in range(3)]
        i = [int(math.floor(f[k])) for k in range(3)]
        if any(i[k] < 0 or i[k] >= self.n[k] - 1 for k in range(3)):
            return self.fallback.sdf(p) if self.fallback else 6.0
        t = [f[k] - i[k] for k in range(3)]
        v, nyz, nz = self.v, self.nyz, self.nz
        base = i[0] * nyz + i[1] * nz + i[2]
        c = 0.0
        for dx in (0, 1):
            wx = t[0] if dx else 1.0 - t[0]
            for dy in (0, 1):
                wy = t[1] if dy else 1.0 - t[1]
                for dz in (0, 1):
                    wz = t[2] if dz else 1.0 - t[2]
                    c += wx * wy * wz * v[base + dx * nyz + dy * nz + dz]
        return c
