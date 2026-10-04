"""Fine EXACT signed-distance lattice around the right-hand grip (thin iron - the lever loop, the trigger - is lost in
the 0.5 cm game lattice: e.g. the loop bar centre reads +0.06 cm there, exactly -0.15). Narrow band: nodes where the
coarse field says |d| > BAND keep the coarse value, the rest get the exact per-part distance (geom.Weapon).
    python fine_field.py  -> Saved/wgs/fine_field_right.json
"""
import json, math, os, sys, time
from multiprocessing import Pool
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from geom import Weapon  # noqa: E402
import views  # noqa: E402
REGION = os.environ.get("NATGRIP_SIDE", "r")
WEAPON = os.environ.get("NATGRIP_WEAPON", "winchester")
PARTS = views._PARTS
OUT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/fine_field_%s%s.json" % ("right" if REGION == "r" else "left", "" if WEAPON == "winchester" else "_" + WEAPON)
_BOX = {("winchester", "r"): ((-8.0, -35.0, -1.0), (6.0, -14.0, 18.0)), ("winchester", "l"): ((-9.0, -12.0, 0.0), (9.0, 26.0, 20.0))}
LO, HI = _BOX.get((WEAPON, REGION)) or (tuple(float(v) for v in os.environ["NATGRIP_BOX_LO"].split(",")), tuple(float(v) for v in os.environ["NATGRIP_BOX_HI"].split(",")))
H, BAND = float(os.environ.get("NATGRIP_FINE_H", "0.15")), 1.3
N = [int(math.ceil((HI[k] - LO[k]) / H)) + 1 for k in range(3)]
_w = None
def _init():
    global _w
    _w = Weapon.load(PARTS)
def _slab(i):
    x = LO[0] + i * H
    out = []
    for j in range(N[1]):
        y = LO[1] + j * H
        for k in range(N[2]):
            z = LO[2] + k * H
            c = views.coarse_field(x, y, z)
            out.append(round(c, 3) if abs(c) > BAND else round(_w.sdf((x, y, z), max_dist=3.0), 3))
    return out
if __name__ == "__main__":
    t = time.time()
    n = int(os.environ.get("NATGRIP_POOL", max(2, os.cpu_count() - 2)))
    if n <= 1:                         # single process (parallel workers crashed on this machine, 2026-09-30)
        _init(); slabs = [_slab(i) for i in range(N[0])]
    else:
        with Pool(n, initializer=_init) as pool:
            slabs = pool.map(_slab, range(N[0]))
    json.dump({"origin": LO, "spacing": H, "dims": N, "d": [v for s in slabs for v in s]}, open(OUT, "w"))
    print("fine field %s nodes in %.0fs -> %s" % (N, time.time() - t, OUT))
