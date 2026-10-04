from PIL import Image, ImageDraw, ImageFont
from ql import *
import views, render
font = ImageFont.load_default()
BC = {}
def bcol(b):
    for f, c in render.FCOL.items():
        if b.startswith(f): return c
    return (120, 70, 30)
def draw_skin(view, box, sc, pts, title="", marks=None, depth_sort=True):
    u0, u1, v0, v1 = box
    Wd, Hh = int((u1 - u0) * sc), int((v1 - v0) * sc)
    im = Image.new("RGB", (Wd, Hh + 14), (255, 255, 255)); dr = ImageDraw.Draw(im)
    P = lambda a, b: ((a - u0) * sc, Hh - (b - v0) * sc)
    render.weapon_layer(dr, view, P, (u0, u1) if view != "front" else (-40, 0) if views.WEAPON == "winchester" else (-12, 8))
    u, v = render.proj(view)
    for b, p in pts:
        d = views.field(*p)
        x, y = P(p[u], p[v])
        c = (0, 0, 0) if d < -0.1 else bcol(b)
        r = 2 if d < -0.1 else 1
        dr.rectangle([x - r, y - r, x + r, y + r], fill=c)
    if marks:
        for k, s in marks.items():
            x, y = P(s[u], s[v]); dr.rectangle([x - 3, y - 3, x + 3, y + 3], outline=(0, 0, 0)); dr.text((x + 4, y - 11), k, fill=(0, 0, 0), font=font)
    dr.text((4, Hh), "%s %s  (black = skin inside the gun)" % (view, title), fill=(0, 0, 0), font=font)
    return im
def three(pts, name, title=""):
    m = {k: views.socks[k] for k in ("I", "M", "R", "P") if k in views.socks}
    a = draw_skin("side", (-38, -12, -2, 18), 24, pts, title, m)
    b = draw_skin("top", (-38, -12, -8, 8), 24, pts, "", m)
    c = draw_skin("front", (-8, 8, -2, 18), 24, pts, "", m)
    render.sheet([a, b, c], name)
if __name__ == "__main__":
    from hand import *
    import skin
    L = clip_local(); W = fk(L)
    three(skin.skin(W), "clip_skin.png", "clip hand as-is")
