import sys, json, math, time
from multiprocessing import Pool
from grasp2 import *
import place2, render2, skin
import hand as H
TRIG = views.socks["I"]
LOOP_X = -0.45
def through_loop(W, f):
    pts = [W[b].t for b in chain(f)[1:]] + [W[chain(f)[-1]].pos(mul(MREF[chain(f)[-1]].t, 0.9))]
    for a, b in zip(pts, pts[1:]):
        if (a[0] - LOOP_X) * (b[0] - LOOP_X) < 0:
            t = (LOOP_X - a[0]) / (b[0] - a[0]); p = add(a, mul(sub(b, a), t))
            if -29.0 < p[1] < -20.8 and 1.8 < p[2] < 7.2: return True
    return False
def fit_long(hand, f):
    best = None
    for phi in (-12.0, -6.0, 0.0, 6.0, 12.0):
        r = solve_long_finger(hand, f, phi)
        if r is None: continue
        a, g, n = r
        W = world(hand, finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
        th = through_loop(W, f)
        touch = sum(1 for x in g if x < 0.3)
        s = (10 if th else 0) + 2 * touch + sum(a) / 60.0 - abs(phi) / 12.0
        if best is None or s > best[0]: best = (s, phi, a, g, th)
    return best
def fit_index(hand):
    f = "index"; best = None
    for phi in range(-20, 21, 4):
        for a0 in range(-15, 91, 5):
            for a1 in range(0, 106, 5):
                a2 = 0.7 * a1
                W = world(hand, finger_locals(f, 0, phi, a0, a1, a2))
                e = length(sub(pad_point(W, f), TRIG))
                if best is None or e < best[0]: best = (e, [phi, a0, a1, a2])
    # refine with collision (coordinate descent)
    x = best[1]
    def cost(x):
        W = world(hand, finger_locals(f, 0, *x))
        e = length(sub(pad_point(W, f), TRIG)) ** 2
        p = sum(max(0.0, v) ** 2 for v in pens(W, f)) * 50
        nat = 0.0004 * ((x[3] - 0.7 * x[2]) ** 2 + 0.5 * x[0] ** 2)
        return e + p + nat
    c = cost(x)
    lo = [-20, -15, 0, 0]; hi = [20, 90, 105, 80]
    for step in (4.0, 2.0, 1.0, 0.5):
        improved = True
        while improved:
            improved = False
            for k in range(4):
                for s in (-step, step):
                    y = list(x); y[k] = min(hi[k], max(lo[k], y[k] + s)); cy = cost(y)
                    if cy < c - 1e-6: x, c, improved = y, cy, True
    W = world(hand, finger_locals(f, 0, *x))
    return x, length(sub(pad_point(W, f), TRIG)), max(pens(W, f))
def evaluate(p):
    yaw, pitch, roll, dx, dy, dz = p
    C = place2.corr(yaw, pitch, roll, (dx, dy, dz))
    hand = place2.hand_new(C)
    out = {"p": p, "palm": place2.palm_worst(C), "fingers": {}}
    for f in ("middle", "ring", "pinky"):
        b = fit_long(hand, f)
        out["fingers"][f] = None if b is None else {"phi": b[1], "a": b[2], "gaps": b[3], "through": b[4]}
    x, e, pen = fit_index(hand)
    out["fingers"]["index"] = {"phi": x[0], "a": x[1:], "err": e, "pen": pen}
    return out
CANDS = [(0, 0, 0, 0, 0, 0), (0, -20, 0, -3, 0, 0), (-8, -20, 0, -2, 0, 0), (-8, -30, 0, -2, 0, 0), (0, -30, 0, -3, 0, -1),
         (-8, -20, 0, -2, 0, -1), (-16, -20, 0, -2, 0, 0), (-8, -10, 0, -2, 0, 0), (0, -20, 0, -3, 1, 0), (-8, -30, 0, -2, 1, -1)]
if __name__ == "__main__":
    with Pool(10) as pool:
        res = pool.map(evaluate, CANDS)
    json.dump(res, open("cands.json", "w"))
    for r in res:
        fs = r["fingers"]
        print("p=%s palm=%.2f | idx err=%.2f pen=%.2f a=%s | %s" % (r["p"], r["palm"], fs["index"]["err"], fs["index"]["pen"],
              [round(v) for v in fs["index"]["a"]], " | ".join("%s phi%+d a=%s thr=%s gaps=%s" % (f[:3], fs[f]["phi"], [round(v) for v in fs[f]["a"]], fs[f]["through"], [round(g, 1) for g in fs[f]["gaps"]]) for f in ("middle", "ring", "pinky") if fs[f])))
