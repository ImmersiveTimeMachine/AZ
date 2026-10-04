"""Bake a weapon's grip field (signed-distance lattice) for the AZ Weapon Grip node (CPython, parallel).

    python bake_grip_field.py [parts.json] [out.json] [spacing_cm] [margin_cm]
    defaults: Saved/wgs/weapons/winchester_parts.json -> Saved/wgs/fields/winchester_field.json, 0.5 cm, 3 cm

The lattice covers the weapon's bounds + margin in the weapon MESH space (the parts share it with SK_Winchester).
Values: signed distance in cm (< 0 inside), computed by geom.Weapon (exact nearest triangle per part, winding-number
inside test). Order: index = (x * ny + y) * nz + z. Then in the editor Tools/wgs/import_grip_field.py writes it into a
UAZ_WeaponGripField asset and assigns it to the weapon BP.
"""
import json
import math
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import Weapon  # noqa: E402

SAVED = r"C:/UnrealEngine/Games/AZ/Saved/wgs"
PARTS = sys.argv[1] if len(sys.argv) > 1 else SAVED + "/weapons/winchester_parts.json"
OUT = sys.argv[2] if len(sys.argv) > 2 else SAVED + "/fields/winchester_field.json"
H = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5
MARGIN = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0

_w = None
_lo = None
_n = None


def _init(lo, n):
    global _w, _lo, _n
    _w, _lo, _n = Weapon.load(PARTS), lo, n


def _slab(i):
    x = _lo[0] + i * H
    return [round(_w.sdf((x, _lo[1] + j * H, _lo[2] + k * H), max_dist=8.0), 3) for j in range(_n[1]) for k in range(_n[2])]


def main():
    w = Weapon.load(PARTS)
    lo = [min(p.lo[k] for p in w.parts) - MARGIN for k in range(3)]
    hi = [max(p.hi[k] for p in w.parts) + MARGIN for k in range(3)]
    n = [int(math.ceil((hi[k] - lo[k]) / H)) + 1 for k in range(3)]
    t = time.time()
    with Pool(max(2, os.cpu_count() - 2), initializer=_init, initargs=(lo, n)) as pool:
        slabs = pool.map(_slab, range(n[0]))
    values = [v for s in slabs for v in s]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({"origin": lo, "spacing": H, "dims": n, "distances": values, "parts": PARTS}, open(OUT, "w"))
    inside = sum(1 for v in values if v < 0)
    print("baked %s nodes (%d total, %d inside) in %.0fs -> %s" % (n, len(values), inside, time.time() - t, OUT))


if __name__ == "__main__":
    main()
