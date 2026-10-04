import sys
from PIL import Image, ImageDraw
from hand import *
import skin, render
BC = {"hand_r": (120, 60, 20)}
for f in FING:
    for b in chain(f): BC[b] = render.FCOL[f]
def sec_with_hand(pts, yc, name, x0=-7, x1=7, z0=-1, z1=17, sc=26, halfw=0.35):
    views.section(yc, x0, x1, z0, z1, sc, name)
    im = Image.open(name); dr = ImageDraw.Draw(im); Hh = int((z1 - z0) * sc)
    P = lambda x, z: ((x - x0) * sc, Hh - (z - z0) * sc)
    for b, p in pts:
        if abs(p[1] - yc) < halfw:
            u, v = P(p[0], p[2]); c = BC[b]
            dr.rectangle([u - 1, v - 1, u + 1, v + 1], fill=c)
    im.save(name)
if __name__ == "__main__":
    L = clip_local(); W = fk(L); pts = skin.skin(W)
    ims = []
    for yc in (-31.0, -29.0, -27.0, -25.0, -23.0, -21.0):
        n = "hs_%d.png" % int(-yc); sec_with_hand(pts, yc, n); ims.append(Image.open(n))
    render.sheet(ims, "clip_sections.png")
