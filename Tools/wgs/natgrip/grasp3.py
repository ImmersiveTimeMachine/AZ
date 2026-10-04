"""v3: relaxed natural hook (target synergy pose), finger-finger clearance, thumb solve."""
import math
from collections import deque
from grasp2 import *
RAD = {"index": (1.0, 0.85, 0.68), "middle": (1.0, 0.85, 0.7), "ring": (0.93, 0.78, 0.64), "pinky": (0.84, 0.7, 0.6), "thumb": (1.3, 1.0, 0.9)}
NATURAL = (58.0, 85.0, 58.0)   # relaxed power-grasp shape (MCP, PIP, DIP abs deg)

def segs(W, f):
    ch = chain(f)[1:] if f != "thumb" else chain(f)
    pts = [W[b].t for b in ch] + [W[ch[-1]].pos(mul(MREF[ch[-1]].t, 0.9))]
    return [(pts[i], pts[i + 1], RAD[f][min(i, 2)]) for i in range(len(pts) - 1)][-3:]

def seg_seg(p1, q1, p2, q2):
    d1, d2, r = sub(q1, p1), sub(q2, p2), sub(p1, p2)
    a, e, f = dot(d1, d1), dot(d2, d2), dot(d2, r)
    c, b = dot(d1, r), dot(d1, d2)
    den = a * e - b * b
    s = max(0.0, min(1.0, (b * f - c * e) / den)) if den > 1e-9 else 0.0
    t = (b * s + f) / e if e > 1e-9 else 0.0
    if t < 0: t, s = 0.0, max(0.0, min(1.0, -c / a))
    elif t > 1: t, s = 1.0, max(0.0, min(1.0, (b - c) / a))
    return length(sub(add(p1, mul(d1, s)), add(p2, mul(d2, t))))

def finger_overlap(sa, others):
    worst = 0.0
    for (p, q, r) in sa:
        for (p2, q2, r2) in others:
            worst = max(worst, (r + r2 - 0.25) - seg_seg(p, q, p2, q2))
    return worst

def solve_relaxed(hand, f, phi, others=(), step=3.0, dip_ratio=0.7):
    lo0, hi0 = LIM_ABS["mcp"]; lo1, hi1 = LIM_ABS["pip"]
    n0 = int((hi0 - lo0) / step) + 1; n1 = int((hi1 - lo1) / step) + 1
    def cfg(i, j):
        a0 = lo0 + i * step; a1 = lo1 + j * step
        return a0, a1, min(LIM_ABS["dip"][1], dip_ratio * a1)
    cache = {}
    def ev(i, j):
        k = (i, j)
        if k not in cache:
            a = cfg(i, j)
            W = world(hand, finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
            ok = max(pens(W, f)) <= 0 and finger_overlap(segs(W, f), others) <= 0
            cache[k] = (ok, W)
        return cache[k]
    s = (int(round((0 - lo0) / step)), 0)
    if not ev(*s)[0]:
        cands = sorted(((i - s[0]) ** 2 + (j - s[1]) ** 2, i, j) for i in range(n0) for j in range(n1))
        s = next(((i, j) for _, i, j in cands if ev(i, j)[0]), None)
        if s is None: return None
    seen = {s}; q = deque([s])
    while q:
        i, j = q.popleft()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ni, nj = i + di, j + dj
            if 0 <= ni < n0 and 0 <= nj < n1 and (ni, nj) not in seen and ev(ni, nj)[0]:
                seen.add((ni, nj)); q.append((ni, nj))
    def score(k):
        a = cfg(*k)
        W = ev(*k)[1]
        g = [link_gap(W, l) for l in chain(f)[1:]]
        touch = sum(1.0 for x in g[:2] if x < 0.3)          # proximal / middle phalanx resting on the gun
        dn = sum((a[m] - NATURAL[m]) ** 2 for m in range(3)) / 900.0
        return touch * 1.0 - dn
    best = max(seen, key=score)
    a = cfg(*best); W = ev(*best)[1]
    return a, [link_gap(W, l) for l in chain(f)[1:]], W

def thumb_solve(hand, base_locals, others):
    """CMC (abd, flex) + MCP + IP(coupled 0.8): thumb pad resting on the gun, most natural (small CMC change)."""
    best = None
    for abd in range(-30, 31, 6):
        for fl in range(-24, 37, 6):
            for m in range(-10, 61, 5):
                ip = 0.8 * m
                L = thumb_locals(base_locals, m, ip, (abd, fl))
                W = world(hand, L)
                p = max(link_pen(W, b) for b in chain("thumb"))
                if p > 0: continue
                if finger_overlap(segs(W, "thumb"), others) > 0: continue
                g = min(link_gap(W, b) for b in chain("thumb")[1:])
                s = -abs(g - 0.05) * 3 - (abd * abd + fl * fl) / 900.0 - ((m - 25) ** 2) / 2500.0
                if best is None or s > best[0]: best = (s, (abd, fl, m, ip), g)
    return best


NAT_BY_FINGER = {"middle": (58.0, 85.0), "ring": (52.0, 76.0), "pinky": (50.0, 72.0)}   # user 2026-09-29: ring/pinky less bent
PIP_MAX = {"middle": 105.0, "ring": 88.0, "pinky": 85.0}


def solve_through(hand, f, phi, others=(), step=3.0, ratios=(0.45, 0.6, 0.75, 0.9), through=None):
    """Fingers that live INSIDE the lever loop (threaded, not closed into it from straight - that path crosses the
    iron): every free configuration that passes the loop is a candidate; pick the most natural one resting on the gun.
    Free = no skin vertex in the weapon (exact fine field) and no overlap with the fingers already placed."""
    best = None
    lo0, hi0 = LIM_ABS["mcp"]; lo1, hi1 = LIM_ABS["pip"]
    hi1 = min(hi1, PIP_MAX.get(f, hi1))
    nat = NAT_BY_FINGER.get(f, (NATURAL[0], NATURAL[1]))
    a0 = lo0
    while a0 <= hi0 + 1e-6:
        a1 = lo1
        while a1 <= hi1 + 1e-6:
            for r in ratios:
                a2 = min(LIM_ABS["dip"][1], r * a1)
                W = world(hand, finger_locals(f, CUP[f] * max(0, a0) / 90, phi, a0, a1, a2))
                if through is not None and not through(W, f):
                    continue
                if max(pens(W, f)) > 0 or finger_overlap(segs(W, f), others) > 0:
                    continue
                g = [link_gap(W, l) for l in chain(f)[1:]]
                touch = sum(1.0 for x in g[:2] if x < 0.3) + (0.5 if g[2] < 0.3 else 0.0)
                dn = ((a0 - nat[0]) ** 2 + (a1 - nat[1]) ** 2) / 900.0 + ((r - 0.7) ** 2) * 4.0
                s = touch - dn
                if best is None or s > best[0]:
                    best = (s, (a0, a1, a2), g, W)
            a1 += step
        a0 += step
    if best is None:
        return None
    return best[1], best[2], best[3]
