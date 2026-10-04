"""Trigger-hand grasp v3 = rsolve2 with a NATURAL index (index_nat: every joint flexed, DIP following PIP, pad facing
the pull) on a placement from place_nat.py.  python rsolve3.py "yaw,pitch,roll,dx,dy,dz" "phi,mcp,pip,dip" TAG
Env as rsolve2.py (NATGRIP_TRIGGER, NATGRIP_GRIP_X, NATGRIP_THUMB_SIDE, NATGRIP_THUMB_ZMAX, NATGRIP_WRAP_FRONT_Y)."""
import sys, json
from grasp3 import *
import place2, views
import hand as H
import index_nat as I
import rsolve, rsolve2


def solve(p, x0):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:])); hand = place2.hand_new(C)
    rec = {"p": p, "palm": place2.palm_worst(C)}
    x, c = I.refine(hand, list(x0))
    err, face, pen, _ = I.metrics(hand, x)
    rec["index"] = {"phi": x[0], "a": x[1:], "err": err, "face": face, "pen": pen, "unnatural": I.unnatural(x)}
    locs = dict(finger_locals("index", 0, *x)); prev_f = "index"
    for f in ("middle", "ring", "pinky"):
        b = rsolve2.next_finger_opt(hand, locs, f, prev_f, phis=[v * 2.5 for v in range(-6, 11)])
        if b is None:
            rec[f] = None; continue
        s, a, phi, g, adj = b
        rec[f] = {"phi": phi, "a": a, "gaps": g, "adj": adj}
        locs.update(finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a)); prev_f = f
    W = world(hand, locs); others = []
    for g_ in ("index", "middle", "ring", "pinky"):
        if any(b_ in locs for b_ in H.chain(g_)[1:]):
            others += segs(W, g_)
    tb = rsolve2.thumb_rest(hand, others)
    rec["thumb"] = tb and {"cmc": tb[1][:2], "mcp": tb[1][2], "ip": tb[1][3], "gaps": tb[2], "far": tb[3], "low": tb[4]}
    if tb:
        abd, fl, m, ip = tb[1]; locs.update(thumb_locals(rsolve.Lc, m, ip, (abd, fl)))
    rec["hand_in_weapon"] = list(hand.t) + list(hand.q)
    rec["locals"] = {b: list(t.q) for b, t in locs.items()}
    return rec


if __name__ == "__main__":
    p = tuple(float(v) for v in sys.argv[1].split(","))
    x0 = [float(v) for v in sys.argv[2].split(",")]
    r = solve(p, x0)
    json.dump(r, open("rsolve3_%s_%s.json" % (views.WEAPON, sys.argv[3]), "w"), indent=1, default=str)
    i = r["index"]
    print("palm %.2f index phi %+.1f a %s err %.3f face %.2f pen %+.3f unnatural %.2f" % (
        r["palm"], i["phi"], [round(v, 1) for v in i["a"]], i["err"], i["face"], i["pen"], i["unnatural"]))
    for f in ("middle", "ring", "pinky"):
        v = r[f]
        print("  %-6s %s" % (f, v and "phi %+.1f a %s gaps %s adj %s" % (v["phi"], [round(x) for x in v["a"]], [round(x, 2) for x in v["gaps"]], [round(x, 2) for x in v["adj"]])))
    print("  thumb", r["thumb"])
