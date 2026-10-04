import os, sys, json
os.environ.setdefault("NATGRIP_SIDE", "l")
from PIL import Image
from ql import *
import left_solve, place2, skin, render, render2, secs_hand
import hand as H
p = tuple(float(v) for v in sys.argv[1].split(",")); tag = sys.argv[2]
r = left_solve.solve(p)
C = place2.corr(p[0], p[1], p[2], tuple(p[3:])); hand = place2.hand_new(C)
locs = {b: T(v[3:7], v[0:3]) for b, v in r["locs"].items()}
W = world = left_solve.world(hand, locs)
pts = skin.skin(W)
secs_hand.BC.update({b: render.FCOL[f] for f in H.FING for b in H.chain(f)}); secs_hand.BC[H.HAND] = (120, 70, 30)
a = render2.draw_skin("side", (-16, 28, -4, 22), 20, pts, "left " + tag)
b = render2.draw_skin("top", (-16, 28, -10, 12), 20, pts)
c = render2.draw_skin("front", (-10, 12, -4, 22), 20, pts)
render.sheet([a, b, c], "left_%s.png" % tag)
ims = []
for yc in (0.0, 3.0, 6.0, 9.0, 12.0):
    n = "ls_%s_%d.png" % (tag, int(yc + 100)); secs_hand.sec_with_hand(pts, yc, n, x0=-9, x1=10, z0=-2, z1=22, sc=22); ims.append(Image.open(n))
render.sheet(ims, "left_%s_sec.png" % tag)
D = hand * H.HAND_W.inv()
json.dump({"p": p, "hand_in_weapon": list(hand.t) + list(hand.q), "locals": {b: list(t.q) for b, t in locs.items()},
           "report": {k: v for k, v in r.items() if k not in ("locs",)}}, open("left_grip_%s.json" % tag, "w"), indent=1, default=str)
print(tag, "palm %.2f" % r["palm"], {f: (r[f][0], [round(x) for x in r[f][1]], [round(x, 2) for x in r[f][2]]) if r[f] else None for f in ("index", "middle", "ring", "pinky")}, "thumb", r["thumb"] and (r["thumb"][1], round(r["thumb"][2], 2)))
