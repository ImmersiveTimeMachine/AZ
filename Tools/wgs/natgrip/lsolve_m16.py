"""M16 support (left) hand: a natural grasp on the handguard.
Lesson of the right hand (2026-09-30): a contact target leaves free degrees of freedom and a contact-only fit picks
hooks, so the hand PLACEMENT is searched together with natural finger shapes, palm contact and the wrist bend.
  * placement = rotation about the clip hand's middle knuckle + translation (place2.corr), around the medoid AIM hold
    of the M16 clips (Saved/wgs/m16_left_picks.json);
  * fingers: every phalanx resting on the handguard, the pad come round the handguard axis (wrap angle), DIP ~0.65 PIP,
    MCP not left straight under a bent PIP (no hook), neighbours lying together (skin gap ~1.5 mm), small side angle;
  * thumb: pad on the near side pointing forward along the handguard (left_solve.thumb_left);
  * wrist bend (forearm vs hand) with the clips' elbows in aim AND relaxed (the one target serves both).
python lsolve_m16.py stage1                -> lsolve_stage1.json  (placements by palm/bend/change, cheap)
python lsolve_m16.py stage2 i n            -> lsolve_stage2_i.json (coarse fingers for slice i of n)
python lsolve_m16.py final "p" TAG         -> lsolve_m16_TAG.json (fine solve, for the editor)"""
import os, sys, json, math, itertools
os.environ.setdefault("NATGRIP_SIDE", "l")
os.environ.setdefault("NATGRIP_WEAPON", "m16")
from grasp3 import *
import place2, left_solve, finger_gaps
import hand as H
AX = (-1.1, 11.85)                   # handguard axis (x, z); it runs along +y
CONTACT = 0.08
PICKS = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/m16_left_picks.json"))
ELB = {k: tuple(PICKS[k]["elbow"]) for k in ("aim", "relaxed")}
PALM_C = None


def corr_hand(p):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
    return C, place2.hand_new(C)


def angle(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, dot(norm(a), norm(b))))))


def bend(hand, elbow):
    W = world(hand, {})
    return angle(sub(hand.t, elbow), sub(W[chain("middle")[1]].t, hand.t))


def around(p):
    return math.degrees(math.atan2(p[2] - AX[1], p[0] - AX[0]))


def palm_angle(hand):
    """Angle of the palm centre around the handguard axis."""
    c = (0.0, 0.0, 0.0); W = world(hand, {})
    for b, o in place2.PALM:
        c = add(c, W[b].pos(o))
    return around(mul(c, 1.0 / len(place2.PALM)))


def wrap_deg(W, f, pa):
    b3 = chain(f)[-1]
    d = around(W[b3].pos(mul(MREF[b3].t, 0.6))) - pa
    return abs((d + 180.0) % 360.0 - 180.0)


def finger_W(hand, locs, f, phi, a):
    L = dict(locs); L.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a))
    return world(hand, L)


def finger_opt(hand, locs, f, prev_f, pa, phis, step, ratios):
    links = chain(f)[1:]
    Wp = world(hand, locs)
    ps = segs(Wp, prev_f) if prev_f else None
    others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if g_ not in (f, prev_f) and any(b in locs for b in H.chain(g_)[1:]):
            others += segs(Wp, g_)
    best = None
    for phi in phis:
        a0 = LIM_ABS["mcp"][0]
        while a0 <= 90.0:
            a1 = 0.0
            while a1 <= 105.0:
                for r in ratios:
                    a = (a0, a1, min(LIM_ABS["dip"][1], r * a1))
                    W = finger_W(hand, locs, f, phi, a)
                    if max(pens(W, f)) > 0:
                        continue
                    s_ = segs(W, f)
                    if others and finger_overlap(s_, others) > 0:
                        continue
                    adj, gg = 0.0, None
                    if ps:
                        gp = seg_seg(s_[0][0], s_[0][1], ps[0][0], ps[0][1]) - s_[0][2] - ps[0][2]
                        gm = seg_seg(s_[1][0], s_[1][1], ps[1][0], ps[1][1]) - s_[1][2] - ps[1][2]
                        gd = seg_seg(s_[2][0], s_[2][1], ps[2][0], ps[2][1]) - s_[2][2] - ps[2][2]
                        if min(gp, gm, gd) < -0.05:
                            continue
                        adj = ((gm - 0.15) ** 2 + 0.5 * (gd - 0.15) ** 2) * 6.0
                        gg = (gp, gm, gd)
                    g = [link_gap(W, l) for l in links]
                    rest = sum(w * max(0.0, x - CONTACT) for w, x in zip((1.0, 1.5, 1.0), g))
                    wr = wrap_deg(W, f, pa)
                    # natural closing: no hook (MCP straight under a bent PIP) and no flat knuckle-only bend (a
                    # straight finger bent at the MCP alone - first stage-2 run picked middle 89/0/0)
                    nat = ((r - 0.65) / 0.15) ** 2 * 0.5 + (max(0.0, 0.5 * a1 - a0) / 15.0) ** 2 \
                        + (max(0.0, a0 - a1 - 15.0) / 12.0) ** 2
                    s = -3.0 * rest - adj - abs(phi) / 30.0 - nat + min(wr, 160.0) / 40.0
                    if best is None or s > best[0]:
                        best = (s, a, phi, g, gg, wr)
                a1 += step
            a0 += step
    return best


def solve_fingers(hand, phis, step, ratios):
    pa = palm_angle(hand)
    locs, rec, total, prev = {}, {}, 0.0, None
    for f in ("index", "middle", "ring", "pinky"):
        b = finger_opt(hand, locs, f, prev, pa, phis, step, ratios)
        if b is None:
            rec[f] = None; total -= 6.0; continue
        s, a, phi, g, gg, wr = b
        rec[f] = {"phi": phi, "a": a, "gaps": g, "adj": gg, "wrap": wr}
        total += s
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a)); prev = f
    W = world(hand, locs); others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if any(b_ in locs for b_ in H.chain(g_)[1:]):
            others += segs(W, g_)
    tb = left_solve.thumb_left(hand, others)
    if tb:
        abd, fl, m, ip = tb[1]; locs.update(thumb_locals(H.clip_local(), m, ip, (abd, fl)))
        rec["thumb"] = {"cmc": (abd, fl), "mcp": m, "ip": ip, "pad_gap": tb[2]}
        total += -3.0 * abs(tb[2] - 0.05)
    else:
        rec["thumb"] = None; total -= 4.0
    rec["palm_angle"] = pa
    return total, rec, locs


def stage1_eval(p):
    C, hand = corr_hand(p)
    palm = place2.palm_worst(C)
    if palm < -0.1 or palm > 0.6:
        return None
    ba, br = bend(hand, ELB["aim"]), bend(hand, ELB["relaxed"])
    dev = math.sqrt(sum(v * v for v in p[:3])) / 10.0 + math.sqrt(sum(v * v for v in p[3:]))
    J = dev + max(0.0, ba - 25.0) / 5.0 + max(0.0, br - 30.0) / 5.0 + abs(palm - 0.15) * 3.0
    return {"p": list(p), "palm": palm, "bend": [ba, br], "dev": dev, "J1": J}


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "stage1":
        C0, h0 = corr_hand((0, 0, 0, 0, 0, 0))
        print("clip aim hold: palm %.2f bend aim %.0f relaxed %.0f" % (place2.palm_worst(C0), bend(h0, ELB["aim"]), bend(h0, ELB["relaxed"])))
        grid = itertools.product((-20, -10, 0, 10, 20), (-20, -10, 0, 10, 20), (-30, -15, 0, 15, 30), (-2, -1, 0, 1, 2), (-2, -1, 0, 1, 2), (-2, -1, 0, 1, 2))
        res = [r for r in (stage1_eval(p) for p in grid) if r]
        res.sort(key=lambda r: r["J1"])
        json.dump(res[:400], open("lsolve_stage1.json", "w"))
        print("ok", len(res))
        for r in res[:15]:
            print("J1 %.2f p %s palm %+.2f bend %s dev %.2f" % (r["J1"], r["p"], r["palm"], [round(v) for v in r["bend"]], r["dev"]))
    elif mode == "stage2":
        i, n = int(sys.argv[2]), int(sys.argv[3])
        cands = json.load(open("lsolve_stage1.json"))[:int(os.environ.get("NATGRIP_TOP", "60"))][i::n]
        out = []
        for r in cands:
            C, hand = corr_hand(r["p"])
            total, rec, _ = solve_fingers(hand, (-5.0, 0.0, 5.0, 10.0), float(os.environ.get("NATGRIP_STEP", "8")), (0.55, 0.75))
            r["fingers"] = rec; r["F"] = total; r["J2"] = r["J1"] - total
            out.append(r)
            print("J2 %.2f p %s F %.2f %s" % (r["J2"], r["p"], total, " ".join("%s:%s w%.0f" % (f[0], [round(v) for v in rec[f]["a"]], rec[f]["wrap"]) if rec[f] else f[0] + ":x" for f in ("index", "middle", "ring", "pinky"))))
            sys.stdout.flush()
        json.dump(out, open("lsolve_stage2_%d.json" % i, "w"), default=str)
    elif mode == "final":
        p = [float(v) for v in sys.argv[2].split(",")]
        C, hand = corr_hand(p)
        total, rec, locs = solve_fingers(hand, [v * 2.5 for v in range(-4, 7)], 3.0, (0.5, 0.65, 0.8, 0.95))
        rec["palm"] = place2.palm_worst(C); rec["bend"] = [bend(hand, ELB["aim"]), bend(hand, ELB["relaxed"])]
        json.dump({"p": p, "F": total, "report": rec, "hand_in_weapon": list(hand.t) + list(hand.q),
                   "locals": {b: list(t.q) for b, t in locs.items()}}, open("lsolve_m16_%s.json" % sys.argv[3], "w"), indent=1, default=str)
        print(json.dumps(rec, indent=1, default=str))
        finger_gaps.report(hand, locs, "M16 support hand " + sys.argv[3])
