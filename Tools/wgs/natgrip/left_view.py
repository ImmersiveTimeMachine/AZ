import sys, os
os.environ.setdefault("NATGRIP_SIDE", "l")
from PIL import Image, ImageDraw, ImageFont
from ql import *
import hand as H, skin, views, render, render2, secs_hand
from collections import defaultdict
tag = sys.argv[1] if len(sys.argv) > 1 else "clip"
L = H.clip_local(); W = H.fk(L)
pts = skin.skin(W)
worst = defaultdict(lambda: 9.0)
for b, p in pts:
    worst[b] = min(worst[b], views.field(*p))
for b in [H.HAND] + [x for f in H.FING for x in H.chain(f)]:
    print("%-20s skin min field d = %6.2f" % (b, worst[b]))
hw = H.HAND_W
print("hand_l in weapon:", [round(c, 2) for c in hw.t], "knuckles:", {f: [round(c, 1) for c in W[H.chain(f)[1]].t] for f in ("index", "middle", "ring", "pinky")})
print("thumb tip", [round(c, 1) for c in H.tip(W, L, "thumb")])
secs_hand.BC.update({b: render.FCOL[f] for f in H.FING for b in H.chain(f)}); secs_hand.BC[H.HAND] = (120, 70, 30)
a = render2.draw_skin("side", (-16, 28, -4, 22), 20, pts, "left hand " + tag)
b = render2.draw_skin("top", (-16, 28, -10, 12), 20, pts)
c = render2.draw_skin("front", (-10, 12, -4, 22), 20, pts)
render.sheet([a, b, c], "left_%s.png" % tag)
ims = []
for yc in (-4.0, 0.0, 4.0, 8.0, 12.0, 16.0):
    n = "ls_%s_%d.png" % (tag, int(yc + 100)); secs_hand.sec_with_hand(pts, yc, n, x0=-9, x1=10, z0=-2, z1=22, sc=22); ims.append(Image.open(n))
render.sheet(ims, "left_%s_sec.png" % tag)
