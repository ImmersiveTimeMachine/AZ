"""Left support hand: fingers solved one after another, each lying ALONGSIDE the previous one (skin gap ~1.5 mm along
the middle phalanges, similar flexion - a hand closes its fingers together), resting on the forend."""
import os, sys, json, math
os.environ.setdefault("NATGRIP_SIDE", "l")
from multiprocessing import Pool
from grasp3 import *
import place2, left_solve, finger_gaps
import hand as H
P = tuple(float(v) for v in (sys.argv[1] if len(sys.argv) > 1 else "0,0,20,-0.5,0,0").split(","))
C = place2.corr(P[0], P[1], P[2], tuple(P[3:])); HAND = place2.hand_new(C)
def evaluate(args):
    f, phi, prev_locs, prev_a = args
    best = None
    others = []
    Wp = world(HAND, prev_locs)
    for g in ("index", "middle", "ring", "pinky"):
        if any(b in prev_locs for b in H.chain(g)[1:]): others += segs(Wp, g)
    prev_f = {"middle": "index", "ring": "middle", "pinky": "ring"}[f]
    ps = segs(Wp, prev_f)
    a0 = LIM_ABS["mcp"][0]
    while a0 <= 90.0:
        a1 = 0.0
        while a1 <= 105.0:
            a2 = 0.7 * a1
            locs = dict(prev_locs); locs.update(finger_locals(f, CUP[f] * max(0, a0) / 90, phi, a0, a1, a2))
            W = world(HAND, locs)
            if max(pens(W, f)) <= 0:
                s_ = segs(W, f)
                # adjacency: surface gap between the middle phalanges (and the distal ones) of the two fingers
                gm = seg_seg(s_[1][0], s_[1][1], ps[1][0], ps[1][1]) - s_[1][2] - ps[1][2]
                gd = seg_seg(s_[2][0], s_[2][1], ps[2][0], ps[2][1]) - s_[2][2] - ps[2][2]
                gp = seg_seg(s_[0][0], s_[0][1], ps[0][0], ps[0][1]) - s_[0][2] - ps[0][2]
                if min(gm, gd, gp) >= -0.05:
                    g = [link_gap(W, l) for l in chain(f)[1:]]
                    touch = sum(1.0 for x in g[:2] if x < 0.3) + (0.5 if g[2] < 0.3 else 0.0)
                    adj = ((gm - 0.15) ** 2 + 0.5 * (gd - 0.15) ** 2) * 6.0
                    sim = ((a0 - prev_a[0]) ** 2 + (a1 - prev_a[1]) ** 2) / 1200.0
                    dn = sum((x - y) ** 2 for x, y in zip((a0, a1), NATURAL[:2])) / 2400.0
                    s = touch - adj - sim - dn - abs(phi) / 40.0
                    if best is None or s > best[0]:
                        best = (s, (a0, a1, a2), phi, g, (gp, gm, gd))
            a1 += 3.0
        a0 += 3.0
    return best
if __name__ == "__main__":
    # index first: resting on the forend (flood fill from open, the most natural touching shape)
    best = None
    for phi in (-10.0, -5.0, 0.0, 5.0, 10.0):
        r = solve_relaxed(HAND, "index", phi, [])
        if not r: continue
        a, g, _ = r
        sc = sum(1.0 for v in g if v < 0.3) - abs(phi) / 20.0 - sum((a[m] - NATURAL[m]) ** 2 for m in range(3)) / 1800.0
        if best is None or sc > best[0]: best = (sc, phi, a, g)
    locs = dict(finger_locals("index", 0.0, best[1], *best[2]))
    prev_a = best[2]
    print("index  phi %+5.1f a=%s wood gaps=%s" % (best[1], [round(v) for v in best[2]], [round(v, 2) for v in best[3]]))
    out = {"index": {"phi": best[1], "a": best[2], "gaps": best[3]}}
    for f in ("middle", "ring", "pinky"):
        with Pool(13) as pool:
            res = pool.map(evaluate, [(f, phi, locs, prev_a) for phi in [x * 2.5 for x in range(-10, 11)]])
        res = [r for r in res if r]
        best = max(res, key=lambda r: r[0])
        s, a, phi, g, adj = best
        print("%-6s phi %+5.1f a=%s wood gaps=%s neighbour gaps prox/mid/dist=%s" % (f, phi, [round(v) for v in a], [round(v, 2) for v in g], [round(v, 2) for v in adj]))
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
        prev_a = a; out[f] = {"phi": phi, "a": a, "gaps": g, "adj": adj}
    W = world(HAND, locs)
    others = []
    for g_ in ("index", "middle", "ring", "pinky"): others += segs(W, g_)
    tb = left_solve.thumb_left(HAND, others)
    abd, fl, m, ip = tb[1]
    locs.update(thumb_locals(H.clip_local(), m, ip, (abd, fl)))
    out["thumb"] = {"cmc": (abd, fl), "mcp": m, "ip": ip, "pad_gap": tb[2]}
    print("thumb", out["thumb"])
    json.dump({"hand_in_weapon": list(HAND.t) + list(HAND.q), "locals": {b: list(t.q) for b, t in locs.items()}, "p": P, "report": out},
              open("left_adj.json", "w"), indent=1, default=str)
    finger_gaps.report(HAND, locs, "adjacent solve")
