import sys, json, math
from grasp3 import *
import place2, render, render2, skin, secs_hand, cand, bend as BB, search2
import hand as H
from PIL import Image
p = tuple(float(v) for v in sys.argv[1].split(","))
tag = sys.argv[2]
C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
hand = place2.hand_new(C)
x, e, pen = cand.fit_index(hand)
locs = dict(finger_locals("index", 0, *x)); W = world(hand, locs); others = segs(W, "index")
report = {"p": p, "bend": BB.bend(C), "palm": place2.palm_worst(C), "index": {"phi": x[0], "a": x[1:], "err": e, "pen": pen}}
for f in ("middle", "ring", "pinky"):
    best = None
    for phi in (-10.0, -5.0, 0.0, 5.0, 10.0):
        r = solve_through(hand, f, phi, others, through=cand.through_loop)
        if r is None: continue
        a, g, Wf = r
        th = cand.through_loop(Wf, f)
        s = (1.0 if th else -1.0) + sum(0.5 for v in g[:2] if v < 0.3) - abs(phi) / 20.0
        if best is None or s > best[0]: best = (s, phi, a, g, th)
    report[f] = {"phi": best[1], "a": best[2], "gaps": best[3], "through": best[4]}
    locs.update(finger_locals(f, CUP[f] * max(0, best[2][0]) / 90, best[1], *best[2])); W = world(hand, locs); others = others + segs(W, f)
import thumb4
tb = thumb4.thumb_solve4(hand, others, True)
abd, fl, m, ip = tb[1]
locs.update(thumb_locals(H.clip_local(), m, ip, (abd, fl)))
report["thumb"] = {"cmc_abd": abd, "cmc_flex": fl, "mcp": m, "ip": ip, "pad_gap": tb[2], "tip": tb[3]}
W = world(hand, locs)
# per-link final check
chk = {}
for f in FING:
    for b in chain(f):
        chk[b] = [round(link_gap(W, b), 2), round(link_pen(W, b) + TOL, 2)]
report["links_gap_pen"] = chk
# inter-finger clearance
fs = ["index", "middle", "ring", "pinky", "thumb"]
ov = {}
for i in range(len(fs)):
    for j in range(i + 1, len(fs)):
        ov[fs[i] + "-" + fs[j]] = round(finger_overlap(segs(W, fs[i]), segs(W, fs[j])), 2)
report["finger_overlap(>0 bad)"] = ov
# correction in hand space: HandNew = D * HandClip
D = hand * H.HAND_W.inv()
qd = D.q
report["correction_hand_space"] = {"t": [round(c, 3) for c in D.t], "angle_deg": round(ang_of(qd), 1)}
out = {"correction": list(D.t) + list(D.q), "locals": {b: list(t.q) for b, t in locs.items()}, "report": report}
json.dump(out, open("nat_grip_%s.json" % tag, "w"), indent=1)
print(json.dumps(report, indent=1))
pts = skin.skin(W)
render2.three(pts, "final_%s.png" % tag, "p=%s" % (p,))
ims = []
for yc in (-31.0, -28.0, -25.5, -23.0, -21.0, -18.5):
    n = "fs_%s_%d.png" % (tag, int(-yc)); secs_hand.sec_with_hand(pts, yc, n); ims.append(Image.open(n))
render.sheet(ims, "final_%s_sec.png" % tag)
