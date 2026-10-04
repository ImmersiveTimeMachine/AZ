import os, sys, json
from PIL import Image
from ql import *
import hand as H, skin, views, render, render2, secs_hand
from collections import defaultdict
tag = sys.argv[1]
box = [float(v) for v in sys.argv[2].split(",")]          # y0,y1,z0,z1 (side view)
xr = [float(v) for v in sys.argv[3].split(",")]           # x0,x1
ys = [float(v) for v in sys.argv[4].split(",")]           # section planes
locs_file = sys.argv[5] if len(sys.argv) > 5 else None
if locs_file:
    d = json.load(open(locs_file)); hand = T.f7(d["hand_in_weapon"]) if "hand_in_weapon" in d else H.HAND_W
    L = {b: T(q, skin.MREF[b].t) for b, q in d["locals"].items()}
    base = H.clip_local(); base.update(L); L = base
else:
    hand = H.HAND_W; L = H.clip_local()
W = H.fk(L, hand); pts = skin.skin(W)
worst = defaultdict(lambda: 9.0)
for b, p in pts: worst[b] = min(worst[b], views.field(*p))
print(tag, "hand in weapon", [round(c, 2) for c in hand.t])
print("  skin min field per bone:", {b.replace("_" + H.SIDE, ""): round(v, 2) for b, v in worst.items() if v < 0.3})
secs_hand.BC.update({b: render.FCOL[f] for f in H.FING for b in H.chain(f)}); secs_hand.BC[H.HAND] = (120, 70, 30)
render2.views.socks = {}
a = render2.draw_skin("side", (box[0], box[1], box[2], box[3]), 18, pts, tag)
b = render2.draw_skin("top", (box[0], box[1], xr[0], xr[1]), 18, pts)
c = render2.draw_skin("front", (xr[0], xr[1], box[2], box[3]), 18, pts)
render.sheet([a, b, c], "%s.png" % tag)
ims = []
for yc in ys:
    n = "wv_%s_%d.png" % (tag, int(yc * 10)); secs_hand.sec_with_hand(pts, yc, n, x0=xr[0], x1=xr[1], z0=box[2], z1=box[3], sc=20); ims.append(Image.open(n))
render.sheet(ims, "%s_sec.png" % tag)
