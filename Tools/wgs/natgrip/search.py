import sys, json, math, itertools, time
from multiprocessing import Pool
from grasp3 import *
import grasp
import place2, bend as BB, cand
import hand as H
TRIG = views.socks["I"]
Lc = H.clip_local()
def thumb_solve2(hand, others):
    best = None
    for abd in range(-30, 31, 10):
        for fl in range(-20, 41, 10):
            for m in range(0, 61, 10):
                L = thumb_locals(Lc, m, 0.8 * m, (abd, fl))
                W = world(hand, L)
                p1 = link_pen(W, "thumb_01_r") - 0.35
                p = max(p1, link_pen(W, "thumb_02_r"), link_pen(W, "thumb_03_r"))
                if p > 0 or finger_overlap(segs(W, "thumb"), others) > 0: continue
                g = min(link_gap(W, b) for b in ("thumb_02_r", "thumb_03_r"))
                s = -abs(g - 0.05) * 3 - (abd * abd + fl * fl) / 900.0 - ((m - 25) ** 2) / 2500.0
                if best is None or s > best[0]: best = (s, (abd, fl, m, 0.8 * m), g)
    return best
def evaluate(p):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
    hand = place2.hand_new(C)
    rec = {"p": p, "bend": BB.bend(C), "palm": place2.palm_worst(C)}
    x, e, pen = cand.fit_index(hand)
    rec["index"] = (x, e, pen)
    locs = dict(finger_locals("index", 0, *x)); W = world(hand, locs); others = segs(W, "index")
    wrap = 0.0
    for f in ("middle", "ring", "pinky"):
        r = solve_relaxed(hand, f, 0.0, others)
        if r is None:
            rec[f] = None; wrap -= 3; continue
        a, g, Wf = r
        th = cand.through_loop(Wf, f)
        rec[f] = (a, g, th)
        wrap += (1.0 if th else -1.0) + sum(0.5 for v in g[:2] if v < 0.3)
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, 0.0, *a)); W = world(hand, locs); others = others + segs(W, f)
    tb = thumb_solve2(hand, others)
    rec["thumb"] = tb
    J = (max(0.0, rec["bend"] - 40.0) / 10.0) ** 2 + (max(0.0, -rec["palm"] - 0.3) * 3) ** 2 + (e / 0.5) ** 2 + (0 if tb else 4.0) - wrap \
        + (abs(p[0]) + abs(p[1]) + abs(p[2])) / 60.0 + math.sqrt(p[3] ** 2 + p[4] ** 2 + p[5] ** 2) / 4.0
    rec["J"] = J
    return rec
if __name__ == "__main__":
    grid = list(itertools.product((-24, -18, -12), (-20, -12, -5, 0), (0, 8), (-2, -1, 0), (0, 1, 2), (-2, -1, 0)))
    pre = []
    for p in grid:
        C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
        b = BB.bend(C)
        if b > 55: continue
        pre.append(p)
    print("grid", len(grid), "after bend filter", len(pre)); sys.stdout.flush()
    with Pool(12) as pool:
        pw = pool.map(place2.job, pre, chunksize=8)
    pre = [p for p, w in pw if w >= -0.5]
    print("after palm filter", len(pre)); sys.stdout.flush()
    t = time.time()
    with Pool(12) as pool:
        res = pool.map(evaluate, pre, chunksize=2)
    print("evaluated in %.0fs" % (time.time() - t))
    res.sort(key=lambda r: r["J"])
    json.dump(res, open("search.json", "w"))
    for r in res[:12]:
        print("J=%.2f p=%s bend %.1f palm %.2f idx err %.2f | %s | thumb %s" % (r["J"], r["p"], r["bend"], r["palm"], r["index"][1],
              " ".join("%s:%s%s" % (f[0], "T" if r[f] and r[f][2] else "-", [round(v, 1) for v in r[f][1][:2]] if r[f] else "x") for f in ("middle", "ring", "pinky")),
              r["thumb"] and ([round(v) for v in r["thumb"][1]], round(r["thumb"][2], 2))))
