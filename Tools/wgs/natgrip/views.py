import json, math
from PIL import Image, ImageDraw, ImageFont
S = r"C:/UnrealEngine/Games/AZ/Saved/wgs"
import os as _os
WEAPON = _os.environ.get("NATGRIP_WEAPON", "winchester")          # winchester | m16 | ...
_PARTS = {"winchester": S + "/weapons/winchester_parts.json"}.get(WEAPON, S + "/weapons/%s_parts.json" % WEAPON)
_FIELD = {"winchester": S + "/gf_winchester.json"}.get(WEAPON, S + "/fields/%s_field.json" % WEAPON)
parts = json.load(open(_PARTS))
gf = json.load(open(_FIELD))
O, H, N = gf["origin"], gf["spacing"], gf["dims"]
D = gf["d"] if "d" in gf else gf["distances"]
_FINE = None
_FF = S + "/fine_field_%s%s.json" % ("left" if _os.environ.get("NATGRIP_SIDE", "r") == "l" else "right", "" if WEAPON == "winchester" else "_" + WEAPON)
if _os.environ.get("NATGRIP_FINE", "1") == "1" and _os.path.exists(_FF):
    _f = json.load(open(_FF))
    _FINE = (_f["origin"], _f["spacing"], _f["dims"], _f["d"])
def _fine(x, y, z):
    FO, FH, FN, FD = _FINE
    f = [(x - FO[0]) / FH, (y - FO[1]) / FH, (z - FO[2]) / FH]
    i = [int(math.floor(v)) for v in f]
    if any(i[k] < 0 or i[k] >= FN[k] - 1 for k in range(3)): return None
    t = [f[k] - i[k] for k in range(3)]
    c = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (t[0] if dx else 1 - t[0]) * (t[1] if dy else 1 - t[1]) * (t[2] if dz else 1 - t[2])
                c += w * FD[((i[0] + dx) * FN[1] + i[1] + dy) * FN[2] + i[2] + dz]
    return c
def field(x, y, z):
    if _FINE is not None:
        v = _fine(x, y, z)
        if v is not None: return v
    return coarse_field(x, y, z)
def coarse_field(x, y, z):
    f = [(x - O[0]) / H, (y - O[1]) / H, (z - O[2]) / H]
    i = [int(math.floor(v)) for v in f]
    if any(i[k] < 0 or i[k] >= N[k] - 1 for k in range(3)): return 10.0
    t = [f[k] - i[k] for k in range(3)]
    c = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (t[0] if dx else 1 - t[0]) * (t[1] if dy else 1 - t[1]) * (t[2] if dz else 1 - t[2])
                c += w * D[((i[0] + dx) * N[1] + i[1] + dy) * N[2] + i[2] + dz]
    return c
COL = {"BasePart": (220, 40, 40), "Winchestere_ElitBase": (40, 120, 220), "ChargerBase": (230, 150, 0),
       "MazzleBase": (120, 120, 120), "Shutter": (150, 0, 200), "ShutterDet": (200, 0, 150),
       "SightPlank": (0, 160, 160), "StockBase": (40, 160, 40)}
_PAL = [(40, 120, 220), (40, 160, 40), (230, 150, 0), (120, 120, 120), (150, 0, 200), (0, 160, 160), (220, 40, 40), (200, 0, 150)]
for _i, _n in enumerate(parts):
    COL.setdefault(_n, _PAL[_i % len(_PAL)])
socks = {"I": (-0.33, -17.95, 8.70), "M": (1.7, -22.4, 9.3), "R": (1.58, -24.4, 8.1), "P": (1.55, -26.3, 7.1)}
font = ImageFont.load_default()

def side(y0, y1, z0, z1, sc, name, xcut=None):
    W, Hh = int((y1 - y0) * sc), int((z1 - z0) * sc)
    im = Image.new("RGB", (W, Hh + 20), (255, 255, 255)); dr = ImageDraw.Draw(im)
    P = lambda y, z: ((y - y0) * sc, Hh - (z - z0) * sc)
    # field inside (d<0) at the x cut plane, light shading
    if xcut is not None:
        step = 0.25
        y = y0
        while y < y1:
            z = z0
            while z < z1:
                d = field(xcut, y, z)
                if d < 0:
                    a = P(y, z + step); dr.rectangle([a[0], a[1], a[0] + step * sc, a[1] + step * sc], fill=(255, 215, 215))
                z += step
            y += step
    for n, p in parts.items():
        V, T = p["verts"], p["tris"]
        for i in range(0, len(T), 3):
            a, b, c = V[T[i]], V[T[i + 1]], V[T[i + 2]]
            if max(a[1], b[1], c[1]) < y0 or min(a[1], b[1], c[1]) > y1: continue
            dr.polygon([P(a[1], a[2]), P(b[1], b[2]), P(c[1], c[2])], outline=COL[n])
    for k, s in socks.items():
        x, y = P(s[1], s[2]); dr.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(0, 0, 0)); dr.text((x + 5, y - 12), k, fill=(0, 0, 0), font=font)
    # grid every 5 cm
    for yy in range(int(math.ceil(y0 / 5)) * 5, int(y1) + 1, 5):
        x, _ = P(yy, z0); dr.line([x, Hh, x, Hh + 6], fill=(0, 0, 0)); dr.text((x + 2, Hh + 6), str(yy), fill=(0, 0, 0), font=font)
    for zz in range(int(math.ceil(z0 / 5)) * 5, int(z1) + 1, 5):
        _, y = P(y0, zz); dr.line([0, y, 6, y], fill=(0, 0, 0)); dr.text((8, y - 6), str(zz), fill=(0, 0, 0), font=font)
    im.save(name)

def section(yc, x0, x1, z0, z1, sc, name):
    W, Hh = int((x1 - x0) * sc), int((z1 - z0) * sc)
    im = Image.new("RGB", (W, Hh + 16), (255, 255, 255)); dr = ImageDraw.Draw(im)
    P = lambda x, z: ((x - x0) * sc, Hh - (z - z0) * sc)
    step = 0.1
    x = x0
    while x < x1:
        z = z0
        while z < z1:
            d = field(x, yc, z)
            col = (255, 200, 200) if d < 0 else ((255, 245, 200) if d < 0.85 else None)
            if col:
                a = P(x, z + step); dr.rectangle([a[0], a[1], a[0] + step * sc, a[1] + step * sc], fill=col)
            z += step
        x += step
    for n, p in parts.items():
        V, T = p["verts"], p["tris"]
        for i in range(0, len(T), 3):
            tri = [V[T[i]], V[T[i + 1]], V[T[i + 2]]]
            pts = []
            for u in range(3):
                a, b = tri[u], tri[(u + 1) % 3]
                if (a[1] - yc) * (b[1] - yc) < 0:
                    t = (yc - a[1]) / (b[1] - a[1]); pts.append((a[0] + t * (b[0] - a[0]), a[2] + t * (b[2] - a[2])))
            if len(pts) == 2:
                dr.line([P(*pts[0]), P(*pts[1])], fill=COL[n], width=2)
    for k, s in socks.items():
        if abs(s[1] - yc) < 1.0:
            x, y = P(s[0], s[2]); dr.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(0, 0, 0)); dr.text((x + 5, y - 12), k, fill=(0, 0, 0), font=font)
    for xx in range(int(math.ceil(x0)), int(x1) + 1):
        a, _ = P(xx, z0); dr.line([a, Hh, a, Hh + (6 if xx % 5 == 0 else 3)], fill=(0, 0, 0))
    for zz in range(int(math.ceil(z0)), int(z1) + 1):
        _, b = P(x0, zz); dr.line([0, b, (6 if zz % 5 == 0 else 3), b], fill=(0, 0, 0))
    dr.text((W - 90, 2), "y=%.1f" % yc, fill=(0, 0, 0), font=font)
    im.save(name)

if __name__ == "__main__":
    side(-40, 0, -5, 19, 22, "side_grip.png", xcut=-0.5)
    for yc in (-17.95, -20.0, -22.4, -24.4, -26.3, -28.5):
        section(yc, -6, 5, -2, 18, 30, "sec_%d.png" % int(round(-yc * 10)))
    print("ok")
