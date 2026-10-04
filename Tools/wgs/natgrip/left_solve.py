import os, sys, json, math, itertools
os.environ.setdefault("NATGRIP_SIDE", "l")
from multiprocessing import Pool
from grasp3 import *
import place2, skin, views
import hand as H
Lc = H.clip_local()
def palm(C):
    return place2.palm_worst(C)
def thumb_left(hand, others):
    """Support-hand thumb: pad resting on the forend's side, pointing forward along it (+y), small CMC change."""
    best = None
    for abd in range(-40, 41, 10):
        for fl in range(-40, 41, 10):
            for m in range(0, 61, 5):
                for k in (0.6, 0.8, 1.0):
                    L = thumb_locals(Lc, m, k * m, (abd, fl))
                    W = world(hand, L)
                    p = max(link_pen(W, H.chain("thumb")[0]) - 0.3, link_pen(W, H.chain("thumb")[1]), link_pen(W, H.chain("thumb")[2]))
                    if p > 0 or finger_overlap(segs(W, "thumb"), others) > 0: continue
                    g3 = link_gap(W, H.chain("thumb")[2]); g2 = link_gap(W, H.chain("thumb")[1])
                    t2, t3 = W[H.chain("thumb")[1]].t, W[H.chain("thumb")[2]].t
                    fwd = norm(sub(t3, t2))[1]                    # along the forend (+y)
                    s = -abs(g3 - 0.05) * 5 - max(0.0, g2 - 0.4) * 1.0 + 1.0 * fwd - (abd * abd + fl * fl) / 3000.0 - ((m - 20) ** 2) / 3000.0
                    if best is None or s > best[0]: best = (s, (abd, fl, m, k * m), g3)
    return best
def solve(p, verbose=False):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
    hand = place2.hand_new(C)
    rec = {"p": p, "palm": palm(C)}
    locs = {}; others = []; wrap = 0.0
    for f in ("index", "middle", "ring", "pinky"):
        best = None
        for phi in (-10.0, -5.0, 0.0, 5.0, 10.0):
            r = solve_relaxed(hand, f, phi, others)
            if r is None: continue
            a, g, Wf = r
            s = sum(1.0 for v in g if v < 0.3) - abs(phi) / 20.0 - sum((a[m] - NATURAL[m]) ** 2 for m in range(3)) / 1800.0
            if best is None or s > best[0]: best = (s, phi, a, g)
        if best is None:
            rec[f] = None; wrap -= 3; continue
        rec[f] = best[1:]; wrap += best[0]
        locs.update(finger_locals(f, CUP[f] * max(0, best[2][0]) / 90, best[1], *best[2]))
        W = world(hand, locs); others = others + segs(W, f)
    tb = thumb_left(hand, others)
    rec["thumb"] = tb
    if tb:
        abd, fl, m, ip = tb[1]; locs.update(thumb_locals(Lc, m, ip, (abd, fl)))
    else:
        for b in H.chain("thumb"): locs[b] = Lc[b]
    rec["J"] = (max(0.0, -rec["palm"] - 0.2) * 4) ** 2 - wrap + (4.0 if not tb else 3.0 * abs(tb[2] - 0.05)) \
        + (abs(p[0]) + abs(p[1]) + abs(p[2])) / 40.0 + math.sqrt(p[3] ** 2 + p[4] ** 2 + p[5] ** 2) / 2.0
    rec["locs"] = {b: list(t.t) + list(t.q) for b, t in locs.items()}
    return rec
if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "one":
        p = tuple(float(v) for v in sys.argv[2].split(","))
        r = solve(p, True)
        print(json.dumps({k: v for k, v in r.items() if k != "locs"}, indent=1, default=str))
        json.dump(r, open("left_%s.json" % sys.argv[3], "w"), default=str)
    elif mode == "search":
        grid = list(itertools.product((-6, 0, 6), (-6, 0, 6), (-6, 0, 6), (-0.5, 0.0, 0.5), (-1.0, 0.0, 1.0), (-1.0, -0.5, 0.0)))
        with Pool(12) as pool:
            pw = pool.map(place2.job, grid, chunksize=8)
        pre = [p for p, w in pw if w >= -0.35]
        print("grid", len(grid), "palm ok", len(pre)); sys.stdout.flush()
        with Pool(12) as pool:
            res = pool.map(solve, pre, chunksize=2)
        res.sort(key=lambda r: r["J"])
        json.dump(res[:20], open("left_search.json", "w"), default=str)
        for r in res[:10]:
            print("J=%.2f p=%s palm %.2f | %s | thumb %s" % (r["J"], r["p"], r["palm"],
                  " ".join("%s:phi%+.0f %s g%s" % (f[0], r[f][0], [round(v) for v in r[f][1]], [round(v, 1) for v in r[f][2]]) if r[f] else f[0] + ":x" for f in ("index", "middle", "ring", "pinky")),
                  r["thumb"] and ([round(v) for v in r["thumb"][1]], round(r["thumb"][2], 2))))
