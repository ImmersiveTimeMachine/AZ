import math, json, sys
from grasp import *
from hand import HAND_W
from multiprocessing import Pool
PALMB = [b for b in GROUP if b == "hand_r" or b.endswith("metacarpal_r") or b == "thumb_01_r"]
# palm verts in weapon space at the current placement (metacarpals = ref, thumb_01 = clip)
import hand as H
L = H.clip_local(); Wc = H.fk(L)
PTS = []
for b, o in skin.BIND:
    if b.endswith("metacarpal_r") or (b == "hand_r" and o[0] < 0.5):
        PTS.append(Wc[b].pos(o))
PTS = PTS[::3]
def ev(d):
    return (min(views.field(*add(p, d)) for p in PTS), d)
if __name__ == "__main__":
    grid = [(dx * 0.5, dy * 0.5, dz * 0.5) for dx in range(-9, 3) for dy in range(-6, 7) for dz in range(-9, 3)]
    with Pool(12) as pool:
        res = pool.map(ev, grid, chunksize=20)
    ok = [(math.sqrt(sum(c * c for c in d)), d, w) for w, d in res if w >= -0.3]
    ok.sort()
    print("palm verts", len(PTS), "feasible", len(ok))
    for r in ok[:12]: print("  |d|=%.2f d=%s palm worst %.2f" % r)
    json.dump([[m, list(d), w] for m, d, w in ok], open("place_ok.json", "w"))
