import math, json, itertools
from multiprocessing import Pool
from ql import *
import views, skin
import hand as H
L = H.clip_local(); Wc = H.fk(L)
PIVOT = Wc[H.chain("middle")[1]].t
FWD_SIGN = -1.0 if H.SIDE == "r" else 1.0      # fingers along -X (right) / +X (left) in hand space
PALM = [(b, o) for b, o in skin.BIND if b.endswith("metacarpal_" + H.SIDE) or (b == H.HAND and o[0] * FWD_SIGN > -0.5)][::3]
def corr(yaw, pitch, roll, d):
    """rotation about weapon axes at PIVOT then translation; returns transform C with HandNew = HandClip * C"""
    q = qmul(qaxis((0, 0, 1), math.radians(yaw)), qmul(qaxis((1, 0, 0), math.radians(pitch)), qaxis((0, 1, 0), math.radians(roll))))
    t = add(sub(PIVOT, qrot(q, PIVOT)), d)
    return T(q, t)
def hand_new(C):
    return H.HAND_W * C
def palm_worst(C):
    Hn = hand_new(C)
    W = {H.HAND: Hn}
    for b in H.BONES[1:]:
        W[b] = L[b] * W[H.PARENT[b]]
    return min(views.field(*W[b].pos(o)) for b, o in PALM)
def job(p):
    yaw, pitch, roll, dx, dy, dz = p
    C = corr(yaw, pitch, roll, (dx, dy, dz))
    return p, palm_worst(C)
if __name__ == "__main__":
    grid = list(itertools.product((0, -8, -16, -24), (0, -10, -20, -30), (0, -10, 10), (0, -1, -2, -3), (-1, 0, 1, 2), (0, -1, -2, -3)))
    with Pool(12) as pool:
        res = pool.map(job, grid, chunksize=16)
    ok = [(p, w) for p, w in res if w >= -0.4]
    print("grid", len(grid), "palm-ok", len(ok))
    json.dump(ok, open("place2_ok.json", "w"))
    cost = lambda p: abs(p[0]) / 10 + abs(p[1]) / 10 + abs(p[2]) / 10 + math.sqrt(p[3] ** 2 + p[4] ** 2 + p[5] ** 2) / 1.5
    for p, w in sorted(ok, key=lambda r: cost(r[0]))[:15]:
        print("  yaw %4d pitch %4d roll %4d d=(%d %d %d)  palm %.2f" % (*p, w))
