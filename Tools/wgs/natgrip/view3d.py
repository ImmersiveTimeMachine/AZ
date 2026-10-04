"""Perspective view of the hand on the weapon (painter's order, flat shading) - to compare with in-game screenshots.
python view3d.py out.png solve.json [index_override.json] [cam=x,y,z] [look=x,y,z]
Weapon space is UE's (left-handed): camera right = up x forward, up = forward x right."""
import sys, json, math
from PIL import Image, ImageDraw, ImageFont
from ql import *
import hand as H, skin, views, render
from grasp import finger_locals

W_, H_ = 900, 700
FOC = 1100.0


def camera(cam, look):
    d = norm(sub(look, cam))
    r = norm(cross((0.0, 0.0, 1.0), d))
    u = cross(d, r)
    return cam, d, r, u


def proj(C, p):
    cam, d, r, u = C
    v = sub(p, cam); z = dot(v, d)
    return (W_ / 2 + FOC * dot(v, r) / z, H_ / 2 - FOC * dot(v, u) / z, z)


def shade(c, k):
    return tuple(int(max(0, min(255, x * k))) for x in c)


def hand_points(d, index=None):
    hand = T.f7(d["hand_in_weapon"])
    L = H.clip_local()
    L.update({b: T(q, skin.MREF[b].t) for b, q in d["locals"].items()})
    if index:
        L.update(finger_locals("index", 0, *index))
    return skin.skin(H.fk(L, hand))


def draw(C, pts, title, box_y=(-14.0, 14.0)):
    im = Image.new("RGB", (W_, H_), (235, 235, 235)); dr = ImageDraw.Draw(im)
    items = []
    for n, p in views.parts.items():
        V, Tt = p["verts"], p["tris"]
        col = views.COL[n]
        for i in range(0, len(Tt), 3):
            a, b, c = V[Tt[i]], V[Tt[i + 1]], V[Tt[i + 2]]
            if max(a[1], b[1], c[1]) < box_y[0] or min(a[1], b[1], c[1]) > box_y[1]:
                continue
            nrm = norm(cross(sub(b, a), sub(c, a)))
            k = 0.45 + 0.55 * abs(dot(nrm, C[1]))
            pa, pb, pc = proj(C, a), proj(C, b), proj(C, c)
            if min(pa[2], pb[2], pc[2]) <= 1:
                continue
            items.append(((pa[2] + pb[2] + pc[2]) / 3, 0, [(pa[0], pa[1]), (pb[0], pb[1]), (pc[0], pc[1])], shade(col, k * 0.8)))
    fc = {}
    for f in H.FING:
        for b in H.chain(f):
            fc[b] = render.FCOL[f]
    for b, p in pts:
        x, y, z = proj(C, p)
        col = fc.get(b, (205, 160, 130))
        if b in fc:
            col = tuple(int(0.55 * c + 0.45 * s) for c, s in zip(col, (225, 180, 150)))
        items.append((z, 1, (x, y, 0.2 * FOC / z), col))
    items.sort(key=lambda it: -it[0])
    zmin = min(it[0] for it in items if it[1] == 1); zmax = max(it[0] for it in items if it[1] == 1)
    for z, kind, g, col in items:
        if kind == 0:
            dr.polygon(g, fill=col)
        else:
            k = 1.05 - 0.35 * (z - zmin) / max(1e-6, zmax - zmin)
            x, y, r = g
            dr.ellipse([x - r, y - r, x + r, y + r], fill=shade(col, k))
    dr.text((8, 8), title, fill=(0, 0, 0), font=ImageFont.load_default())
    return im


if __name__ == "__main__":
    out, solve = sys.argv[1], json.load(open(sys.argv[2]))
    idx = None
    args = sys.argv[3:]
    if args and args[0].endswith(".json"):
        idx = json.load(open(args[0]))["best"]; args = args[1:]
    kv = dict(a.split("=") for a in args)
    cam = tuple(float(v) for v in kv.get("cam", "-30,-14,-8").split(","))
    look = tuple(float(v) for v in kv.get("look", "-3,1,1").split(","))
    C = camera(cam, look)
    draw(C, hand_points(solve, idx), kv.get("title", out)).save(out)
    print("saved", out)
