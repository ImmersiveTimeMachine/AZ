"""Perspective views of the support hand on the weapon: python left_view3d.py out.png [solve.json] [title]
(NATGRIP_SIDE=l, NATGRIP_WEAPON, NATGRIP_LEFT_PICK). Without solve.json: the clip hold as it is."""
import sys, json
from PIL import Image
from ql import *
import view3d as V, hand as H, skin
CAMS = (((34, 10, -4), (2, 24, 9)), ((10, 52, -12), (0, 24, 9)), ((-6, 18, -26), (0, 24, 9)), ((-26, 6, 22), (0, 24, 10)))
def points(d=None):
    L = H.clip_local(); hand = H.HAND_W
    if d:
        hand = T.f7(d["hand_in_weapon"])
        L.update({b: T(q, skin.MREF[b].t) for b, q in d["locals"].items()})
    return skin.skin(H.fk(L, hand))
def sheet(pts, title, out):
    ims = [V.draw(V.camera(c, l), pts, "%s  cam %d" % (title, i), box_y=(-2.0, 50.0)) for i, (c, l) in enumerate(CAMS)]
    W = sum(i.width for i in ims); o = Image.new("RGB", (W, ims[0].height)); x = 0
    for i in ims: o.paste(i, (x, 0)); x += i.width
    o.resize((W // 2, ims[0].height // 2)).save(out)
if __name__ == "__main__":
    d = json.load(open(sys.argv[2])) if len(sys.argv) > 2 and sys.argv[2].endswith(".json") else None
    sheet(points(d), sys.argv[3] if len(sys.argv) > 3 else sys.argv[1], sys.argv[1]); print("saved", sys.argv[1])
