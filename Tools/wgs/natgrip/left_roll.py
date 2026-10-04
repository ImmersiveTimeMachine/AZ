import os, sys, json, math, itertools
os.environ.setdefault("NATGRIP_SIDE", "l")
from multiprocessing import Pool
from grasp3 import *
import place2, left_solve
import hand as H
Lc = H.clip_local()
def thumb_fast(hand, others):
    best = None
    for abd in range(-40, 41, 20):
        for fl in range(-40, 41, 20):
            for m in range(0, 61, 10):
                L = thumb_locals(Lc, m, 0.8 * m, (abd, fl))
                W = world(hand, L)
                p = max(link_pen(W, H.chain("thumb")[0]) - 0.3, link_pen(W, H.chain("thumb")[1]), link_pen(W, H.chain("thumb")[2]))
                if p > 0 or finger_overlap(segs(W, "thumb"), others) > 0: continue
                g3 = link_gap(W, H.chain("thumb")[2])
                s = -abs(g3 - 0.05) * 5 - (abd * abd + fl * fl) / 3000.0
                if best is None or s > best[0]: best = (s, (abd, fl, m, 0.8 * m), g3)
    return best
def job(p):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
    pw = place2.palm_worst(C)
    if pw < -0.35: return None
    hand = place2.hand_new(C)
    locs = {}; others = []; wrap = 0.0; fin = {}
    for f in ("index", "middle", "ring", "pinky"):
        r = solve_relaxed(hand, f, 0.0, others)
        if r is None: return None
        a, g, W = r
        fin[f] = (a, g)
        wrap += 1.5 * sum(1.0 for v in g[:2] if v < 0.3) + (0.5 if g[2] < 0.3 else 0.0) - (g[0] + g[1]) * 0.3
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, 0.0, *a)); W = world(hand, locs); others = others + segs(W, f)
    tb = thumb_fast(hand, others)
    J = -wrap + (max(0.0, -pw - 0.2) * 4) ** 2 + (4.0 if not tb else 3.0 * abs(tb[2] - 0.05)) + abs(p[2]) / 40.0 + math.sqrt(p[3] ** 2 + p[5] ** 2) / 2.0
    return {"p": p, "palm": pw, "fingers": fin, "thumb": tb, "J": J}
if __name__ == "__main__":
    grid = list(itertools.product((0,), (0,), (-20, -15, -10, -5, 0, 5, 10, 15, 20), (-1.0, -0.5, 0.0, 0.5, 1.0), (0.0,), (-1.0, -0.5, 0.0)))
    with Pool(12) as pool:
        res = [r for r in pool.map(job, grid) if r]
    res.sort(key=lambda r: r["J"])
    json.dump(res[:15], open("left_roll.json", "w"), default=str)
    print("evaluated", len(res))
    for r in res[:10]:
        print("J=%.2f p=%s palm %.2f | %s | thumb %s" % (r["J"], r["p"], r["palm"],
              " ".join("%s:%s g%s" % (f[0], [round(v) for v in r["fingers"][f][0]], [round(v, 2) for v in r["fingers"][f][1]]) for f in ("index", "middle", "ring", "pinky")),
              r["thumb"] and ([round(v) for v in r["thumb"][1]], round(r["thumb"][2], 2))))
