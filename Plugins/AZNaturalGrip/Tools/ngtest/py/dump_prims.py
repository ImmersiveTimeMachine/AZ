"""Primitive parity dump for the C++ Natural Grip core (Plugins/AZNaturalGrip/Tools/ngtest).
Random inputs + what the Python solver (Tools/wgs/natgrip) computes for them: skin binding, field lookups, placements,
finger / thumb FK with link penetration and gap, pad points, phalanx capsules, segment distances, finger overlap.
The setting comes from the same environment the solver reads (NATGRIP_SIDE / WEAPON / THIN / FINE / LEFT_PICK).
    python dump_prims.py out.json"""
import os
import sys
import json
import random
sys.path.insert(0, r"C:/UnrealEngine/Games/AZ/Tools/wgs/natgrip")
from grasp3 import *  # noqa: E402,F401,F403  (world, finger_locals, thumb_locals, link_pen, link_gap, pens, segs, ...)
import place2  # noqa: E402
import skin  # noqa: E402
import views  # noqa: E402
import hand as H  # noqa: E402

R = random.Random(int(os.environ.get("NG_SEED", "7")))


def f7(t):
    return list(t.t) + list(t.q)


def seglist(S):
    return [list(s[0]) + list(s[1]) + [s[2]] for s in S]


out = {"setting": {"side": H.SIDE, "weapon": H.WEAPON, "thin": int(os.environ.get("NATGRIP_THIN", "2")),
                   "fine": os.environ.get("NATGRIP_FINE", "1") == "1", "left_pick": os.environ.get("NATGRIP_LEFT_PICK", "aim"),
                   "fine_loaded": views._FINE is not None}}
out["bind"] = [[b] + list(o) for b, o in skin.BIND]
out["group_sizes"] = {b: len(v) for b, v in GROUP.items()}
out["pivot"] = list(place2.PIVOT)
out["palm"] = [[b] + list(o) for b, o in place2.PALM]

# field lookups: the coarse box +-2 cm and the fine box +-1 cm
pts = []
O, Hh, N = views.O, views.H, views.N
for _ in range(3000):
    pts.append([O[k] - 2.0 + R.random() * ((N[k] - 1) * Hh + 4.0) for k in range(3)])
if views._FINE is not None:
    FO, FH, FN, _ = views._FINE
    for _ in range(3000):
        pts.append([FO[k] - 1.0 + R.random() * ((FN[k] - 1) * FH + 2.0) for k in range(3)])
out["field"] = [[x, y, z, views.field(x, y, z), views.coarse_field(x, y, z)] for x, y, z in pts]

places = []
for _ in range(60):
    p = [R.uniform(-25, 25), R.uniform(-25, 25), R.uniform(-30, 30), R.uniform(-3, 3), R.uniform(-3, 3), R.uniform(-3, 3)]
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:]))
    places.append({"p": p, "C": f7(C), "hand": f7(place2.hand_new(C)), "palm": place2.palm_worst(C)})
out["place"] = places

fingers = []
for i in range(200):
    f = ("index", "middle", "ring", "pinky")[i % 4]
    pi = i % len(places)
    hand = T.f7(places[pi]["hand"])
    cup, phi = R.uniform(0, 16), R.uniform(-15, 15)
    a = (R.uniform(-15, 90), R.uniform(0, 105), R.uniform(0, 80))
    W = world(hand, finger_locals(f, cup, phi, *a))
    fingers.append({"f": f, "pi": pi, "cup": cup, "phi": phi, "a": list(a), "W": {b: f7(W[b]) for b in H.BONES},
                    "pens": pens(W, f), "gaps": [link_gap(W, l) for l in chain(f)[1:]], "pad": list(pad_point(W, f)),
                    "segs": seglist(segs(W, f))})
out["fingers"] = fingers

Lc = H.clip_local()
thumbs = []
for i in range(100):
    pi = i % len(places)
    hand = T.f7(places[pi]["hand"])
    abd, fl, m, ip = R.uniform(-60, 30), R.uniform(-40, 40), R.uniform(-10, 60), R.uniform(-10, 60)
    W = world(hand, thumb_locals(Lc, m, ip, (abd, fl)))
    ch = chain("thumb")
    thumbs.append({"pi": pi, "cmc": [abd, fl], "m": m, "ip": ip, "W": {b: f7(W[b]) for b in H.BONES},
                   "pen": [link_pen(W, b) for b in ch], "gap": [link_gap(W, b) for b in ch], "segs": seglist(segs(W, "thumb"))})
out["thumbs"] = thumbs

ss = []
for i in range(500):
    p = [[R.uniform(-5, 5) for _ in range(3)] for _ in range(4)]
    if i % 10 == 0:                       # crossing segments (a zero-length FIRST segment divides by zero in Python too)
        p[2] = [p[0][k] + 0.5 * (p[1][k] - p[0][k]) + R.uniform(-0.01, 0.01) for k in range(3)]
    if i % 10 == 1:
        p[3] = list(p[2])                 # degenerate second segment
    if i % 10 == 2:
        p[3] = [p[2][k] + 2.0 * (p[1][k] - p[0][k]) for k in range(3)]   # parallel
    ss.append(p + [seg_seg(*[tuple(v) for v in p])])
out["segseg"] = ss

ov = []
for i in range(100):
    a, b = fingers[i], fingers[(i * 7 + 3) % len(fingers)]
    sa = [(tuple(s[0:3]), tuple(s[3:6]), s[6]) for s in a["segs"]]
    sb = [(tuple(s[0:3]), tuple(s[3:6]), s[6]) for s in b["segs"]]
    ov.append([i, (i * 7 + 3) % len(fingers), finger_overlap(sa, sb)])
out["overlap"] = ov

json.dump(out, open(sys.argv[1], "w"))
print("dump_prims", out["setting"], "bind", len(out["bind"]), "field", len(out["field"]), "fingers", len(fingers))
