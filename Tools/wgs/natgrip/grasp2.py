"""Anatomical grasp v2: reachable most-closed configuration (flood fill over MCP x PIP, DIP coupled then refined)."""
import math
from collections import deque
from grasp import *

def shifted(hand, d):
    return T(hand.q, add(hand.t, d))

def pens(W, f):
    return [link_pen(W, l) for l in chain(f)[1:]]

def solve_long_finger(hand, f, phi, step=3.0, dip_ratio=0.7, start=(0.0, 0.0)):
    lo0, hi0 = LIM_ABS["mcp"]; lo1, hi1 = LIM_ABS["pip"]
    n0 = int((hi0 - lo0) / step) + 1; n1 = int((hi1 - lo1) / step) + 1
    def cfg(i, j):
        a0 = lo0 + i * step; a1 = lo1 + j * step; a2 = min(LIM_ABS["dip"][1], dip_ratio * a1)
        return a0, a1, a2
    cache = {}
    def free(i, j):
        k = (i, j)
        if k not in cache:
            a = cfg(i, j)
            W = world(hand, finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
            cache[k] = max(pens(W, f)) <= 0
        return cache[k]
    s = (int(round((start[0] - lo0) / step)), int(round((start[1] - lo1) / step)))
    if not free(*s):
        # nearest free start along the open direction
        found = None
        for r in range(1, 8):
            for i in range(max(0, s[0] - r), min(n0, s[0] + 1)):
                for j in range(max(0, s[1] - r), min(n1, s[1] + 1)):
                    if free(i, j): found = (i, j); break
                if found: break
            if found: break
        if not found: return None
        s = found
    seen = {s}; q = deque([s])
    while q:
        i, j = q.popleft()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ni, nj = i + di, j + dj
            if 0 <= ni < n0 and 0 <= nj < n1 and (ni, nj) not in seen and free(ni, nj):
                seen.add((ni, nj)); q.append((ni, nj))
    # most closed = max total flexion (MCP + PIP + DIP), ties -> more balanced
    best = max(seen, key=lambda k: (sum(cfg(*k)) - 0.15 * abs(cfg(*k)[0] - cfg(*k)[1])))
    a0, a1, a2 = cfg(*best)
    # refine DIP: close further while free
    while a2 + 1.0 <= LIM_ABS["dip"][1]:
        W = world(hand, finger_locals(f, CUP[f] * max(0, a0) / 90, phi, a0, a1, a2 + 1.0))
        if max(pens(W, f)) > 0: break
        a2 += 1.0
    W = world(hand, finger_locals(f, CUP[f] * max(0, a0) / 90, phi, a0, a1, a2))
    gaps = [link_gap(W, l) for l in chain(f)[1:]]
    return (a0, a1, a2), gaps, len(seen)
