import math
from PIL import Image, ImageDraw, ImageFont
from ql import *
import views
FCOL = {"thumb": (200, 0, 200), "index": (230, 30, 30), "middle": (0, 150, 0), "ring": (0, 90, 230), "pinky": (230, 140, 0)}
RAD = {"thumb": (1.0, 0.85, 0.75), "index": (0.85, 0.75, 0.65), "middle": (0.85, 0.75, 0.65), "ring": (0.8, 0.72, 0.62), "pinky": (0.72, 0.65, 0.58)}
font = ImageFont.load_default()
def proj(view):
    # returns (u, v) axes: side = (y, z), top = (y, x), front = (x, z)
    return {"side": (1, 2), "top": (1, 0), "front": (0, 2)}[view]
def weapon_layer(dr, view, P, box, parts=views.parts, cols=views.COL, xcut=None):
    u, v = proj(view)
    for n, p in parts.items():
        V, Tt = p["verts"], p["tris"]
        c = tuple(int(200 + 0.2 * (x - 200)) if False else x for x in cols[n])
        lc = tuple(min(255, int(x * 0.35 + 255 * 0.65)) for x in cols[n])
        for i in range(0, len(Tt), 3):
            a, b, cc = V[Tt[i]], V[Tt[i + 1]], V[Tt[i + 2]]
            if max(a[1], b[1], cc[1]) < box[0] - 5 or min(a[1], b[1], cc[1]) > box[1] + 5:
                continue
            dr.polygon([P(a[u], a[v]), P(b[u], b[v]), P(cc[u], cc[v])], outline=lc)
def hand_layer(dr, P, view, W, local, chains, tipfn, width_sc):
    u, v = proj(view)
    for f, ch in chains.items():
        pts = [W[b].t for b in ch] + [tipfn(f)]
        # skip metacarpal->MCP draw thin
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            w = 2 if i == 0 and f != "thumb" else max(2, int(RAD[f][min(2, i - (0 if f == "thumb" else 1))] * 2 * width_sc * 0.5))
            dr.line([P(a[u], a[v]), P(b[u], b[v])], fill=FCOL[f], width=w)
        for p in pts[1:]:
            x, y = P(p[u], p[v]); dr.ellipse([x - 3, y - 3, x + 3, y + 3], outline=(0, 0, 0))
def draw(view, box, sc, W, local, chains, tipfn, name, extra=None, title=""):
    u0, u1, v0, v1 = box
    Wd, Hh = int((u1 - u0) * sc), int((v1 - v0) * sc)
    im = Image.new("RGB", (Wd, Hh + 14), (255, 255, 255)); dr = ImageDraw.Draw(im)
    P = lambda a, b: ((a - u0) * sc, Hh - (b - v0) * sc)
    weapon_layer(dr, view, P, (u0, u1) if view != "front" else (-40, 0))
    hand_layer(dr, P, view, W, local, chains, tipfn, sc)
    if extra:
        u, v = proj(view)
        for k, s in extra.items():
            x, y = P(s[u], s[v]); dr.rectangle([x - 3, y - 3, x + 3, y + 3], fill=(0, 0, 0)); dr.text((x + 4, y - 11), k, fill=(0, 0, 0), font=font)
    dr.text((4, Hh), "%s %s" % (view, title), fill=(0, 0, 0), font=font)
    return im
def sheet(ims, name):
    Wt = sum(i.width for i in ims) + 10 * (len(ims) - 1); Ht = max(i.height for i in ims)
    out = Image.new("RGB", (Wt, Ht), (255, 255, 255)); x = 0
    for i in ims:
        out.paste(i, (x, 0)); x += i.width + 10
    out.save(name)
