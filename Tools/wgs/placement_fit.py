"""WGS task 1.4 (placement) - find where the Winchester should sit in the RIGHT hand so the fingers can wrap it.

    python placement_fit.py           -> Saved/wgs/placement_fit.json (best hand-in-weapon transform + top list)

The right hand carries the gun (RightHandWinchesterSocket on az_weapon_r). Moving the gun in the hand == moving the
hand relative to the gun, so the search runs in weapon space: candidate = the current hand transform rotated about
the wrist (weapon X / Y / Z, small angles) and translated. For each candidate the fingers close on the surface
(grasp_solve.solve_hand, on a precomputed signed-distance lattice for speed) and the candidate is scored:
  palm skin inside the weapon (heavy), palm not touching, fingers index..pinky not wrapped (closed with a contact),
  thumb not touching, distance from the user's placement. Coarse grid, then coordinate descent with step halving.
"""
import json
import math
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import Weapon, SDFGrid                                              # noqa: E402
from handfk import qaxis, qmul, qnorm, add, sub, mul, dot                       # noqa: E402
import grasp_solve                                                              # noqa: E402

PARTS = r"C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json"
EXPORT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/hands_export.json"
CACHE = r"C:/UnrealEngine/Games/AZ/Saved/wgs/sdf_lattice_right.json"
LOCK_ROT = "--lock-rot" in sys.argv
OUT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/placement_fit%s.json" % ("_norot" if LOCK_ROT else "")
LO, HI, H = (-8.0, -46.0, -16.0), (8.0, -8.0, 20.0), 0.75
PALM_OFF, SKIN = 1.0, 0.25
FINGERS4 = ("index", "middle", "ring", "pinky")

_weapon = None


def _init():
    global _weapon
    _weapon = Weapon.load(PARTS)


def _row(i):
    n = [int(math.ceil((HI[k] - LO[k]) / H)) + 1 for k in range(3)]
    return [_weapon.sdf((LO[0] + i * H, LO[1] + j * H, LO[2] + k * H)) for j in range(n[1]) for k in range(n[2])]


def build_lattice():
    n = [int(math.ceil((HI[k] - LO[k]) / H)) + 1 for k in range(3)]
    try:
        d = json.load(open(CACHE))
        if d["lo"] == list(LO) and d["hi"] == list(HI) and d["h"] == H:
            return SDFGrid(LO, H, d["n"], d["v"])
    except (IOError, OSError, ValueError, KeyError):
        pass
    t = time.time()
    with Pool(max(2, os.cpu_count() - 2), initializer=_init) as pool:
        rows = pool.map(_row, range(n[0]))
    v = [x for r in rows for x in r]
    json.dump({"lo": list(LO), "hi": list(HI), "h": H, "n": n, "v": v}, open(CACHE, "w"))
    print("lattice %s nodes built in %.1fs" % (n, time.time() - t))
    return SDFGrid(LO, H, n, v)


def candidate(base, p):
    """base = (pos, quat) of hand_r in weapon space; p = (dx, dy, dz, rx, ry, rz)."""
    pos, q = base
    r = qnorm(qmul(qaxis((0, 0, 1), p[5]), qmul(qaxis((0, 1, 0), p[4]), qaxis((1, 0, 0), p[3]))))
    return list(add(pos, (p[0], p[1], p[2]))) + list(qnorm(qmul(r, q)))


def score(grid, rec, base, p, step=3.0):
    hand_ws = candidate(base, p)
    _solved, rep, n, h = grasp_solve.solve_hand(grid, rec, "r", hand_ws=hand_ws, step=step)
    side = "r"
    wrist = h.pos("hand_r")
    palm = []
    for f in FINGERS4:
        k = h.pos("%s_01_%s" % (f, side))
        for t in (0.35, 0.7, 1.0):
            palm.append(add(add(wrist, mul(sub(k, wrist), t)), mul(n, PALM_OFF)))
    palm.append(add(add(wrist, mul(sub(h.pos("thumb_02_r"), wrist), 0.5)), mul(n, PALM_OFF)))
    pc = [grid.sdf(x) - SKIN for x in palm]
    pen = sum(max(0.0, -c) ** 2 for c in pc)
    gap = max(0.0, min(pc) - 0.3)
    cost = 50.0 * pen + 10.0 * gap * gap
    wrapped = 0
    for f in FINGERS4:
        a, cl = rep[f]["angles"], rep[f]["clearance_end"]
        closed = a[0] + a[1]
        touching = min(cl[1:]) <= 0.35
        if touching and closed >= 30.0:
            cost -= min(closed, 160.0) / 160.0
            wrapped += 1
        elif closed >= 150.0:
            cost += 0.3
        else:
            cost += 1.0 + max(0.0, -a[0]) / 30.0
    if min(rep["thumb"]["clearance_end"]) <= 0.35:
        cost -= 0.3
    cost += 0.01 * (p[0] ** 2 + p[1] ** 2 + p[2] ** 2) + 0.002 * (p[3] ** 2 + p[4] ** 2 + p[5] ** 2)
    return cost, {"wrapped": wrapped, "palm_pen": round(pen, 3), "palm_min": round(min(pc), 2),
                  "fingers": {f: rep[f]["angles"] for f in ("thumb",) + FINGERS4}}


def main():
    grid = build_lattice()
    E = json.load(open(EXPORT))
    rec = E["hands"]["r"]
    b = rec["mocap"]["hand_ws"]
    base = (tuple(b[:3]), tuple(b[3:7]))
    t = time.time()
    c0, info0 = score(grid, rec, base, (0, 0, 0, 0, 0, 0))
    print("current placement: cost %.3f %s  (%.2fs per candidate)" % (c0, info0, time.time() - t))
    results = []
    t = time.time()
    for dz in (1.0, 0.0, -1.5, -3.0, -4.5, -6.0, -7.5, -9.0):
        for dy in (-4.0, -2.0, 0.0, 2.0, 4.0):
            for dx in (-1.0, 0.0, 1.0):
                for rx in ((0.0,) if LOCK_ROT else (-15.0, 0.0, 15.0)):
                    p = (dx, dy, dz, rx, 0.0, 0.0)
                    c, info = score(grid, rec, base, p)
                    results.append((c, p, info))
    results.sort(key=lambda r: r[0])
    print("coarse grid %d candidates in %.0fs; best:" % (len(results), time.time() - t))
    for c, p, info in results[:5]:
        print("  %.3f %s %s" % (c, p, info))
    best_c, best_p, best_i = results[0]
    best_p = list(best_p)
    steps = [1.0, 1.0, 1.0, 6.0, 6.0, 6.0]
    for _ in range(4):
        improved = True
        while improved:
            improved = False
            for k in range(3 if LOCK_ROT else 6):
                for s in (-1, 1):
                    trial = list(best_p)
                    trial[k] += s * steps[k]
                    if k >= 3 and abs(trial[k]) > 20.0:
                        continue
                    c, info = score(grid, rec, base, tuple(trial))
                    if c < best_c - 1e-4:
                        best_c, best_p, best_i, improved = c, trial, info, True
        steps = [x * 0.5 for x in steps]
    c_fine, info_fine = score(grid, rec, base, tuple(best_p), step=1.5)
    print("refined: cost %.3f p %s %s" % (c_fine, [round(x, 2) for x in best_p], info_fine))
    out = {"base_hand_ws": b, "best": {"p": best_p, "cost": c_fine, "info": info_fine, "hand_ws": candidate(base, best_p)},
           "current": {"cost": c0, "info": info0},
           "top": [{"p": list(p), "cost": c, "info": i} for c, p, i in results[:10]]}
    json.dump(out, open(OUT, "w"), indent=1)
    print("written", OUT)


if __name__ == "__main__":
    main()
