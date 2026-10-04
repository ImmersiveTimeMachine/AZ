import json, os
from ql import *
SIDE = os.environ.get("NATGRIP_SIDE", "r")
HAND = "hand_" + SIDE
V = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/hand_%s_verts.json" % SIDE))
MREF = {b: T.f7(v) for b, v in V["ref_local_mesh"].items()}
BCS = {b: T.f7(v) for b, v in V["bone_hand_space"].items()}
BCS[HAND] = T()
FING = ("thumb", "index", "middle", "ring", "pinky")
def chain(f): return ([] if f == "thumb" else ["%s_metacarpal_%s" % (f, SIDE)]) + ["%s_0%d_%s" % (f, i, SIDE) for i in (1, 2, 3)]
SEGS = []  # (bone, a, b) in hand space
for f in FING:
    ch = chain(f)
    prev = HAND
    for i, b in enumerate(ch):
        a = BCS[b].t
        e = BCS[ch[i + 1]].t if i + 1 < len(ch) else BCS[b].pos(mul(MREF[b].t, 0.9))
        SEGS.append((b, a, e))
    # wrist->metacarpal base segments belong to hand
    SEGS.append((HAND, (0, 0, 0), BCS[ch[0]].t))
def segd(p, a, b):
    ab = sub(b, a); t = max(0.0, min(1.0, dot(sub(p, a), ab) / max(1e-9, dot(ab, ab))))
    return length(sub(p, add(a, mul(ab, t))))
BIND = []  # (bone, local offset)
for v in V["verts_hand_space"]:
    best = min(SEGS, key=lambda s: segd(v, s[1], s[2]))
    BIND.append((best[0], BCS[best[0]].ipos(v)))
def skin(W):
    """W: bone -> transform in target space (must include hand_r)."""
    return [(b, W[b].pos(o)) for b, o in BIND]
