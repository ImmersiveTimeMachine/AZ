"""Natural trigger finger (user, M16 PIE 2026-09-30: "the finger is a bit crooked").
fit_index only minimised the pad-to-trigger distance, and a pad on a point leaves one degree of freedom free: it took
MCP -5.5 (knuckle bent BACK) + PIP 58 + DIP 41 = a hook. A real trigger finger closes like any finger: every joint
flexed, MCP:PIP about 0.5-1, the tip joint following the middle one (DIP ~ 0.6-0.7 PIP), the pad's palm side facing
the pull direction (rearward) and the side angle small. This searches the whole family of poses that put the pad on
the trigger and picks the most natural one.
Env as rsolve.py (NATGRIP_WEAPON, NATGRIP_TRIGGER, ...) + NATGRIP_PULL="x,y,z" (trigger pull direction, weapon space)."""
import os, sys, json, math
from multiprocessing import Pool
from grasp3 import *
import place2
import rsolve
PULL = norm(tuple(float(v) for v in os.environ.get("NATGRIP_PULL", "0,-1,0").split(",")))
F = "index"


def metrics(hand, x):
    phi, a0, a1, a2 = x
    W = world(hand, finger_locals(F, 0, phi, a0, a1, a2))
    err = length(sub(pad_point(W, F), rsolve.TRIG))
    n = W[chain(F)[-1]].vec((0.0, 1.0, 0.0))            # palm side of the tip phalanx (flexion moves toward local +Y)
    face = dot(n, PULL)                                 # the pad presses the trigger back: its palm side faces the pull direction
    pen = max(pens(W, F))
    return err, face, pen, W


def unnatural(x):
    phi, a0, a1, a2 = x
    r = a2 / max(a1, 1.0)
    c = ((r - 0.65) / 0.15) ** 2                        # DIP follows PIP
    c += (max(0.0, 0.5 * a1 - a0) / 10.0) ** 2          # the knuckle closes too (no hook: MCP >= ~half of PIP)
    c += (max(0.0, a0 - 1.1 * a1) / 15.0) ** 2          # ... and not a flat "pointing" knuckle-only bend
    c += (phi / 12.0) ** 2
    return c


def cost(hand, x):
    err, face, pen, _ = metrics(hand, x)
    return (err / 0.08) ** 2 + 60.0 * max(0.0, pen) ** 2 * 100 + unnatural(x) + 3.0 * (1.0 - face)


def refine(hand, x):
    lo = [-20, -15, 0, 0]; hi = [20, 90, 105, 80]
    c = cost(hand, x)
    for step in (4.0, 2.0, 1.0, 0.5, 0.25):
        imp = True
        while imp:
            imp = False
            for k in range(4):
                for s in (-step, step):
                    y = list(x); y[k] = min(hi[k], max(lo[k], y[k] + s)); cy = cost(hand, y)
                    if cy < c - 1e-7:
                        x, c, imp = y, cy, True
            # paired moves along the family (MCP up, PIP down) so the search can slide along the solution curve
            for s in (-step, step):
                for kk in ((1, 2), (1, 3), (2, 3)):
                    y = list(x); y[kk[0]] += s; y[kk[1]] -= s
                    y = [min(hi[k], max(lo[k], y[k])) for k in range(4)]
                    cy = cost(hand, y)
                    if cy < c - 1e-7:
                        x, c, imp = y, cy, True
    return x, c


def job(args):
    hand_l, x0 = args
    hand = T.f7(hand_l)
    x, c = refine(hand, list(x0))
    err, face, pen, _ = metrics(hand, x)
    return c, x, err, face, pen, unnatural(x)


def solve_index(hand):
    seeds = []
    for phi in (-10, -5, 0, 5, 10):
        for a0 in range(0, 76, 10):
            for a1 in range(10, 101, 15):
                seeds.append((list(hand.t) + list(hand.q), (phi, a0, a1, 0.65 * a1)))
    with Pool(12) as pool:
        res = pool.map(job, seeds, chunksize=4)
    res.sort(key=lambda r: r[0])
    return res


if __name__ == "__main__":
    p = tuple(float(v) for v in sys.argv[1].split(","))
    hand = place2.hand_new(place2.corr(p[0], p[1], p[2], tuple(p[3:])))
    cur = [6.5, -5.5, 58.5, 41.0]
    e, f_, pn, _ = metrics(hand, cur)
    print("current  phi %+5.1f a %s  err %.3f face %.2f pen %+.3f unnatural %.2f" % (cur[0], cur[1:], e, f_, pn, unnatural(cur)))
    res = solve_index(hand)
    seen = []
    for c, x, err, face, pen, un in res:
        if any(max(abs(x[k] - s[k]) for k in range(4)) < 4 for s in seen):
            continue
        seen.append(x)
        print("c %6.2f phi %+5.1f a [%5.1f %5.1f %5.1f]  err %.3f face %.2f pen %+.3f unnatural %.2f" % (c, x[0], x[1], x[2], x[3], err, face, pen, un))
        if len(seen) >= 12:
            break
    json.dump({"best": res[0][1], "p": p}, open("index_nat_%s.json" % sys.argv[2], "w"))
