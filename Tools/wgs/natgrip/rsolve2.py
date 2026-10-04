"""Trigger-hand grasp v2 (user review of the M16, 2026-09-29: "fingers not pulled in", "thumb sticks up"):
  * fingers CLOSE LIKE A HAND, joint after joint to contact: the base joint until the proximal phalanx rests on the grip,
    then the middle joint until the middle phalanx rests, then the tip joint until the pad rests -> every phalanx lies
    on the grip; the side (MCP) angle is chosen so the finger lies against the previous one;
  * the thumb: both of its phalanges resting on the weapon, tip past the grip centre on the far side and not above
    NATGRIP_THUMB_ZMAX (under the receiver), small CMC change.
Env as rsolve.py (NATGRIP_TRIGGER, NATGRIP_GRIP_X, NATGRIP_THUMB_SIDE) + NATGRIP_THUMB_ZMAX."""
import os, sys, json, math
from grasp3 import *
import place2, skin, views
import hand as H
import rsolve
CONTACT = 0.08          # a phalanx "rests" when its skin is within this of the surface (cm)
THUMB_ZMAX = float(os.environ.get("NATGRIP_THUMB_ZMAX", "99"))
WRAP_FRONT = float(os.environ.get("NATGRIP_WRAP_FRONT_Y", "99"))   # the grip's front face (weapon y): wrapped pads end behind it
Lc = rsolve.Lc


def finger_W(hand, locs, f, phi, a):
    L = dict(locs); L.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
    return world(hand, L)


def close_to_contact(hand, locs, f, phi, others):
    """Joint after joint: MCP until the proximal rests, PIP (DIP following at 0.5) until the middle rests, DIP until
    the pad rests. A joint also stops when any phalanx would enter the weapon or touch another finger."""
    links = chain(f)[1:]
    a = [0.0, 0.0, 0.0]

    def blocked(a):
        W = finger_W(hand, locs, f, phi, a)
        return max(pens(W, f)) > 0 or finger_overlap(segs(W, f), others) > 0, W
    b, W = blocked(a)
    tries = 0
    while b and tries < 15:                                  # open (extend) until free
        a = [a[0] - 2.0, max(0.0, a[1] - 3.0), max(0.0, a[2] - 2.0)]; b, W = blocked(a); tries += 1
        if a[0] < LIM_ABS["mcp"][0]:
            return None
    if b:
        return None
    for joint, link_i, step in ((0, 0, 1.0), (1, 1, 1.0), (2, 2, 1.0)):
        hi = (LIM_ABS["mcp"], LIM_ABS["pip"], LIM_ABS["dip"])[joint][1]
        while a[joint] + step <= hi:
            t = list(a); t[joint] += step
            if joint == 1:
                t[2] = min(LIM_ABS["dip"][1], t[2] + 0.5 * step)     # the tip follows while the middle joint closes
            bt, Wt = blocked(t)
            if bt:
                break
            a, W = t, Wt
            if link_gap(W, links[link_i]) <= CONTACT:
                break
    g = [link_gap(W, l) for l in links]
    return a, g, W


def close_synergy(hand, locs, f, phi, others, rates=(1.0, 1.1, 0.75), step=1.0):
    """All joints close together (a hand's natural synergy); when a phalanx comes to rest on the weapon the joints
    that carry it stop and the ones beyond it keep closing (it wraps). A step that would push any phalanx into the
    weapon or into another finger stops the joints carrying that phalanx."""
    links = chain(f)[1:]
    lim = (LIM_ABS["mcp"], LIM_ABS["pip"], LIM_ABS["dip"])

    def state(a):
        W = finger_W(hand, locs, f, phi, a)
        p = pens(W, f)
        ov = finger_overlap(segs(W, f), others) > 0
        return W, p, ov
    a = [0.0, 0.0, 0.0]
    W, p, ov = state(a)
    tries = 0
    while (max(p) > 0 or ov) and tries < 15:
        a = [a[0] - 2.0, max(0.0, a[1] - 3.0), max(0.0, a[2] - 2.0)]; W, p, ov = state(a); tries += 1
    if max(p) > 0 or ov:
        return None
    locked = [False, False, False]
    for _ in range(300):
        if all(locked):
            break
        t = list(a)
        for j in range(3):
            if not locked[j]:
                t[j] = min(lim[j][1], a[j] + rates[j] * step)
                if t[j] >= lim[j][1]:
                    locked[j] = True
        Wt, pt, ovt = state(t)
        bad = [k for k in range(3) if pt[k] > 0]
        if ovt and not bad:
            bad = [2]
        if bad:
            k = min(bad)
            for j in range(k + 1):
                locked[j] = True
            continue                       # retry the remaining joints from the last free state
        a, W = t, Wt
        for k in range(3):
            if link_gap(W, links[k]) <= CONTACT:
                for j in range(k + 1):
                    locked[j] = True
    g = [link_gap(W, l) for l in links]
    return a, g, W


def _opt_job(args):
    hand_l, locs_l, f, phi, prev_f = args
    hand = T.f7(hand_l); locs = {b: T.f7(v) for b, v in locs_l.items()}
    Wp = world(hand, locs); ps = segs(Wp, prev_f)
    others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if g_ not in (f, prev_f) and any(b in locs for b in H.chain(g_)[1:]):
            others += segs(Wp, g_)
    tight = prev_f != "index"
    links = chain(f)[1:]
    best = None
    a0 = LIM_ABS["mcp"][0]
    while a0 <= 90.0:
        a1 = 0.0
        while a1 <= 105.0:
            for r in (0.5, 0.65, 0.8, 0.95):
                a = (a0, a1, min(LIM_ABS["dip"][1], r * a1))
                W = finger_W(hand, locs, f, phi, a)
                if max(pens(W, f)) > 0:
                    continue
                s_ = segs(W, f)
                if finger_overlap(s_, others) > 0:
                    continue
                gm = seg_seg(s_[1][0], s_[1][1], ps[1][0], ps[1][1]) - s_[1][2] - ps[1][2]
                gp = seg_seg(s_[0][0], s_[0][1], ps[0][0], ps[0][1]) - s_[0][2] - ps[0][2]
                gd = seg_seg(s_[2][0], s_[2][1], ps[2][0], ps[2][1]) - s_[2][2] - ps[2][2]
                if min(gm, gp, gd) < -0.05:
                    continue
                g = [link_gap(W, l) for l in links]
                rest = sum(w * max(0.0, x - CONTACT) for w, x in zip((1.0, 1.5, 1.0), g))
                adj = ((gm - 0.15) ** 2 + 0.5 * (gd - 0.15) ** 2) * (6.0 if tight else 0.3)
                # WRAP: the pad must come round to the far side of the grip (a straight finger along the near side
                # also "rests", user 2026-09-29: "fingers not pulled in")
                tip = W[links[2]].pos(mul(MREF[links[2]].t, 0.6))
                wrap = (tip[0] - rsolve.GRIP_X) * rsolve.THUMB_SIDE
                behind = WRAP_FRONT - tip[1]              # the pad has come back behind the grip's front face
                around = 2.0 * min(wrap, 1.2) + 2.0 * min(max(behind, -2.0), 1.0)
                s = -rest * 3.0 - adj - abs(phi) / 30.0 - ((r - 0.7) ** 2) * 2.0 + around
                if best is None or s > best[0]:
                    best = (s, a, phi, g, (gp, gm, gd))
            a1 += 3.0
        a0 += 3.0
    return best


def next_finger_opt(hand, locs, f, prev_f, phis):
    from multiprocessing import Pool
    args = [(list(hand.t) + list(hand.q), {b: list(t.t) + list(t.q) for b, t in locs.items()}, f, phi, prev_f) for phi in phis]
    n = int(os.environ.get("NATGRIP_POOL", "13"))      # 1 = no worker processes (parallel runs crashed CPython here)
    if n <= 1:
        res = [r for r in map(_opt_job, args) if r]
    else:
        with Pool(min(n, len(args))) as pool:
            res = [r for r in pool.map(_opt_job, args) if r]
    return max(res, key=lambda r: r[0]) if res else None


def next_finger(hand, locs, f, prev_f, phis):
    Wp = world(hand, locs); ps = segs(Wp, prev_f)
    others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if g_ not in (f, prev_f) and any(b in locs for b in H.chain(g_)[1:]):
            others += segs(Wp, g_)
    tight = prev_f != "index"
    best = None
    for phi in phis:
        r = close_synergy(hand, locs, f, phi, others)
        if r is None:
            continue
        a, g, W = r
        s_ = segs(W, f)
        gm = seg_seg(s_[1][0], s_[1][1], ps[1][0], ps[1][1]) - s_[1][2] - ps[1][2]
        gp = seg_seg(s_[0][0], s_[0][1], ps[0][0], ps[0][1]) - s_[0][2] - ps[0][2]
        if min(gm, gp) < -0.05:
            continue
        rest = sum(1.0 for v in g if v <= CONTACT + 0.05)
        adj = ((gm - 0.15) ** 2) * (6.0 if tight else 0.3)
        s = 2.0 * rest - adj - abs(phi) / 30.0
        if best is None or s > best[0]:
            best = (s, a, phi, g, (gp, gm))
    return best


def thumb_rest(hand, others):
    best = None
    t0, t1, t2 = H.chain("thumb")
    for abd in range(-60, 31, 10):
        for fl in range(-40, 41, 10):
            for m in range(0, 61, 5):
                for k in (0.5, 0.8, 1.1):
                    L = thumb_locals(Lc, m, k * m, (abd, fl))
                    W = world(hand, L)
                    if max(link_pen(W, t0) - 0.3, link_pen(W, t1), link_pen(W, t2)) > 0 or finger_overlap(segs(W, "thumb"), others) > 0:
                        continue
                    g1, g2 = link_gap(W, t1), link_gap(W, t2)
                    tip = W[t2].pos(mul(MREF[t2].t, 0.9))
                    far = (tip[0] - rsolve.GRIP_X) * rsolve.THUMB_SIDE > 0
                    low = tip[2] <= THUMB_ZMAX
                    s = -abs(g2 - 0.05) * 5 - max(0.0, g1 - 0.2) * 2 + (1.5 if far else 0.0) + (1.0 if low else -2.0) \
                        - (abd * abd + fl * fl) / 3000.0
                    if best is None or s > best[0]:
                        best = (s, (abd, fl, m, k * m), (g1, g2), far, low)
    return best


def solve(p):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:])); hand = place2.hand_new(C)
    rec = {"p": p, "palm": place2.palm_worst(C)}
    x, e, pen = rsolve.fit_index(hand, fine=True)
    rec["index"] = {"phi": x[0], "a": x[1:], "err": e, "pen": pen}
    locs = dict(finger_locals("index", 0, *x)); prev_f = "index"
    for f in ("middle", "ring", "pinky"):
        b = next_finger_opt(hand, locs, f, prev_f, phis=[v * 2.5 for v in range(-6, 11)])
        if b is None:
            rec[f] = None; continue
        s, a, phi, g, adj = b
        rec[f] = {"phi": phi, "a": a, "gaps": g, "adj": adj}
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a)); prev_f = f
    W = world(hand, locs); others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if any(b_ in locs for b_ in H.chain(g_)[1:]):
            others += segs(W, g_)
    tb = thumb_rest(hand, others)
    rec["thumb"] = tb and {"cmc": tb[1][:2], "mcp": tb[1][2], "ip": tb[1][3], "gaps": tb[2], "far": tb[3], "low": tb[4]}
    if tb:
        abd, fl, m, ip = tb[1]; locs.update(thumb_locals(Lc, m, ip, (abd, fl)))
    rec["hand_in_weapon"] = list(hand.t) + list(hand.q)
    rec["locals"] = {b: list(t.q) for b, t in locs.items()}
    return rec


if __name__ == "__main__":
    p = tuple(float(v) for v in sys.argv[1].split(","))
    r = solve(p)
    json.dump(r, open("rsolve2_%s_%s.json" % (views.WEAPON, sys.argv[2]), "w"), indent=1, default=str)
    print("palm %.2f index err %.2f pen %.2f a %s" % (r["palm"], r["index"]["err"], r["index"]["pen"], [round(v) for v in r["index"]["a"]]))
    for f in ("middle", "ring", "pinky"):
        v = r[f]
        print("  %-6s %s" % (f, v and "phi %+.1f a %s gaps %s adj %s" % (v["phi"], [round(x) for x in v["a"]], [round(x, 2) for x in v["gaps"]], [round(x, 2) for x in v["adj"]])))
    print("  thumb", r["thumb"])
