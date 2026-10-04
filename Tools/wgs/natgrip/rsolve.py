"""Generic trigger-hand grasp (any weapon): index pad on the trigger, the other fingers wrapping the grip one against
the other, the thumb wrapping the far side. Env: NATGRIP_WEAPON, NATGRIP_TRIGGER="x,y,z" (pad centre target),
NATGRIP_GRIP_X (grip centre x: the thumb tip goes past it), NATGRIP_THUMB_SIDE (+1: far side is +x)."""
import os, sys, json, math, itertools
from grasp3 import *
import place2, skin, views
import hand as H
TRIG = tuple(float(v) for v in os.environ["NATGRIP_TRIGGER"].split(","))
GRIP_X = float(os.environ.get("NATGRIP_GRIP_X", "-1.0"))
THUMB_SIDE = float(os.environ.get("NATGRIP_THUMB_SIDE", "1"))
Lc = H.clip_local()


def fit_index(hand, fine=True):
    f = "index"; best = None
    for phi in range(-20, 21, 5):
        for a0 in range(-15, 91, 5):
            for a1 in range(0, 106, 5):
                W = world(hand, finger_locals(f, 0, phi, a0, a1, 0.7 * a1))
                e = length(sub(pad_point(W, f), TRIG))
                if best is None or e < best[0]:
                    best = (e, [phi, a0, a1, 0.7 * a1])
    x = best[1]

    def cost(x):
        W = world(hand, finger_locals(f, 0, *x))
        return length(sub(pad_point(W, f), TRIG)) ** 2 + sum(max(0.0, v) ** 2 for v in pens(W, f)) * 50 \
            + 0.0004 * ((x[3] - 0.7 * x[2]) ** 2 + 0.5 * x[0] ** 2)
    c = cost(x); lo = [-20, -15, 0, 0]; hi = [20, 90, 105, 80]
    for step in ((4.0, 2.0, 1.0, 0.5) if fine else (4.0, 2.0)):
        imp = True
        while imp:
            imp = False
            for k in range(4):
                for s in (-step, step):
                    y = list(x); y[k] = min(hi[k], max(lo[k], y[k] + s)); cy = cost(y)
                    if cy < c - 1e-6:
                        x, c, imp = y, cy, True
    W = world(hand, finger_locals(f, 0, *x))
    return x, length(sub(pad_point(W, f), TRIG)), max(pens(W, f))


def wrap_next(hand, locs, f, prev_f, prev_a, phis=(-10.0, -5.0, 0.0, 5.0, 10.0), step=3.0):
    """Next finger around the grip, lying against prev_f (skin gap ~1.5 mm), resting on the weapon."""
    Wp = world(hand, locs); ps = segs(Wp, prev_f)
    others = []
    for g in ("index", "middle", "ring", "pinky"):
        if g not in (f, prev_f) and any(b in locs for b in H.chain(g)[1:]):
            others += segs(Wp, g)
    tight = prev_f != "index"          # the index is on the trigger: the middle need not hug it
    best = None
    for phi in phis:
        a0 = LIM_ABS["mcp"][0]
        while a0 <= 90.0:
            a1 = 0.0
            while a1 <= 105.0:
                a2 = 0.7 * a1
                L = dict(locs); L.update(finger_locals(f, CUP[f] * max(0, a0) / 90, phi, a0, a1, a2))
                W = world(hand, L)
                if max(pens(W, f)) <= 0:
                    s_ = segs(W, f)
                    if finger_overlap(s_, others) <= 0:
                        gm = seg_seg(s_[1][0], s_[1][1], ps[1][0], ps[1][1]) - s_[1][2] - ps[1][2]
                        gd = seg_seg(s_[2][0], s_[2][1], ps[2][0], ps[2][1]) - s_[2][2] - ps[2][2]
                        gp = seg_seg(s_[0][0], s_[0][1], ps[0][0], ps[0][1]) - s_[0][2] - ps[0][2]
                        if min(gm, gd, gp) >= -0.05:
                            g = [link_gap(W, l) for l in chain(f)[1:]]
                            touch = sum(1.0 for v in g[:2] if v < 0.3) + (0.5 if g[2] < 0.3 else 0.0)
                            adj = ((gm - 0.15) ** 2 + 0.5 * (gd - 0.15) ** 2) * (6.0 if tight else 0.5)
                            sim = ((a0 - prev_a[0]) ** 2 + (a1 - prev_a[1]) ** 2) / (1200.0 if tight else 8000.0)
                            dn = ((a0 - NATURAL[0]) ** 2 + (a1 - NATURAL[1]) ** 2) / 2400.0
                            s = touch - adj - sim - dn - abs(phi) / 40.0
                            if best is None or s > best[0]:
                                best = (s, (a0, a1, a2), phi, g, (gp, gm, gd))
                a1 += step
            a0 += step
    return best


def thumb_wrap(hand, others):
    """Thumb pad resting on the weapon, tip past the grip centre on the far side, small CMC change."""
    best = None
    t0, t1, t2 = H.chain("thumb")
    for abd in range(-60, 31, 10):
        for fl in range(-40, 41, 10):
            for m in range(0, 61, 10):
                for k in (0.6, 0.9):
                    L = thumb_locals(Lc, m, k * m, (abd, fl))
                    W = world(hand, L)
                    if max(link_pen(W, t0) - 0.3, link_pen(W, t1), link_pen(W, t2)) > 0 or finger_overlap(segs(W, "thumb"), others) > 0:
                        continue
                    g3 = link_gap(W, t2)
                    tip = W[t2].pos(mul(MREF[t2].t, 0.9))
                    far = 1.0 if (tip[0] - GRIP_X) * THUMB_SIDE > 0 else 0.0
                    s = -abs(g3 - 0.05) * 5 + 1.5 * far - (abd * abd + fl * fl) / 3000.0 - (max(0, 10 - m) ** 2 + max(0, m - 45) ** 2) / 400.0
                    if best is None or s > best[0]:
                        best = (s, (abd, fl, m, k * m), g3, far)
    return best


def solve(p, full=True):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:])); hand = place2.hand_new(C)
    rec = {"p": p, "palm": place2.palm_worst(C)}
    x, e, pen = fit_index(hand, fine=full)
    rec["index"] = {"phi": x[0], "a": x[1:], "err": e, "pen": pen}
    locs = dict(finger_locals("index", 0, *x)); prev_f, prev_a = "index", tuple(x[1:])
    wrap = 0.0
    for f in ("middle", "ring", "pinky"):
        b = wrap_next(hand, locs, f, prev_f, prev_a,
                      phis=((-10.0, -5.0, 0.0, 5.0, 10.0, 15.0, 20.0) if full else (0.0, 10.0)), step=(3.0 if full else 6.0))
        if b is None:
            rec[f] = None; wrap -= 3; continue
        s, a, phi, g, adj = b
        rec[f] = {"phi": phi, "a": a, "gaps": g, "adj": adj}; wrap += s
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a)); prev_f, prev_a = f, a
    W = world(hand, locs); others = []
    for g in ("index", "middle", "ring", "pinky"):
        if any(b_ in locs for b_ in H.chain(g)[1:]):
            others += segs(W, g)
    tb = thumb_wrap(hand, others) if full else None
    rec["thumb"] = tb and {"cmc": tb[1][:2], "mcp": tb[1][2], "ip": tb[1][3], "pad_gap": tb[2], "far": tb[3]}
    if tb:
        abd, fl, m, ip = tb[1]; locs.update(thumb_locals(Lc, m, ip, (abd, fl)))
    thumb_cost = 0.0 if not full else (4.0 if not tb else 3.0 * abs(tb[2] - 0.05) - tb[3])
    rec["J"] = (max(0.0, -rec["palm"] - 0.3) * 3) ** 2 + (e / 0.4) ** 2 - wrap + thumb_cost \
        + (abs(p[0]) + abs(p[1]) + abs(p[2])) / 40.0 + math.sqrt(p[3] ** 2 + p[4] ** 2 + p[5] ** 2) / 2.0
    rec["hand_in_weapon"] = list(hand.t) + list(hand.q)
    rec["locals"] = {b: list(t.q) for b, t in locs.items()}
    return rec


if __name__ == "__main__":
    from multiprocessing import Pool
    mode = sys.argv[1]
    if mode == "search":
        grid = list(itertools.product((-10, 0, 10), (-10, 0, 10), (-10, 0, 10), (-1.0, 0.0, 1.0), (-1.0, 0.0, 1.0), (-1.0, 0.0, 1.0)))
        with Pool(12) as pool:
            pw = pool.map(place2.job, grid, chunksize=8)
        pre = [p for p, w in pw if w >= -0.45]
        print("grid", len(grid), "palm ok", len(pre)); sys.stdout.flush()
        with Pool(12) as pool:
            res = pool.starmap(solve, [(p, False) for p in pre], chunksize=2)
        res.sort(key=lambda r: r["J"])
        json.dump(res[:15], open("rsolve_%s_search.json" % views.WEAPON, "w"), default=str)
        for r in res[:10]:
            print("J=%.2f p=%s palm %.2f idx %.2f | %s" % (r["J"], r["p"], r["palm"], r["index"]["err"],
                  " ".join("%s:%s" % (f[0], [round(v) for v in r[f]["a"]] if r[f] else "x") for f in ("middle", "ring", "pinky"))))
    elif mode == "one":
        p = tuple(float(v) for v in sys.argv[2].split(","))
        r = solve(p, True)
        json.dump(r, open("rsolve_%s_%s.json" % (views.WEAPON, sys.argv[3]), "w"), indent=1, default=str)
        print(json.dumps({k: v for k, v in r.items() if k not in ("locals",)}, indent=1, default=str))
