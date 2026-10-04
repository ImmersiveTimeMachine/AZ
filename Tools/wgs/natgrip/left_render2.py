import os, sys, json
os.environ.setdefault("NATGRIP_SIDE", "l")
from PIL import Image
from ql import *
import skin, render, render2, secs_hand, views
import hand as H
from grasp3 import world
d = json.load(open(sys.argv[1])); tag = sys.argv[2]
hand = T.f7(d["hand_in_weapon"])
locs = {b: T(q, skin.MREF[b].t) for b, q in d["locals"].items()}
W = world(hand, locs); pts = skin.skin(W)
secs_hand.BC.update({b: render.FCOL[f] for f in H.FING for b in H.chain(f)}); secs_hand.BC[H.HAND] = (120, 70, 30)
a = render2.draw_skin("side", (-16, 28, -4, 22), 20, pts, "left " + tag)
# view from the fingers' side (-x): project (y, z) mirrored -> reuse side view of points with x < 0 only
b = render2.draw_skin("top", (-16, 28, -10, 12), 20, pts)
c = render2.draw_skin("front", (-10, 12, -4, 22), 20, pts)
render.sheet([a, b, c], "left_%s.png" % tag)
print("min skin field", round(min(views.field(*p) for _, p in pts), 2))
