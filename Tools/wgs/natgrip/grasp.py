"""Anatomical grasp prototype (MetaHuman right hand on the Winchester)."""
import math
from ql import *
import views, skin
from hand import D, BONES, PARENT, FING, chain, HAND_W
import hand as H
MREF = skin.MREF
FIELD = views.field
Z_FLEX = (0.0, 0.0, -1.0)   # flexion axis (bone local), verified on the rig
Y_ABD = (0.0, 1.0, 0.0)     # MCP side axis (~palm normal), + = toward the index side
REF_ABS = {"index": (20, 12, 4), "middle": (24, 20, 4), "ring": (18, 25, 5), "pinky": (12, 20, 4)}
LIM_ABS = {"mcp": (-15.0, 90.0), "pip": (0.0, 105.0), "dip": (0.0, 80.0)}
CUP = {"index": 0.0, "middle": 0.0, "ring": 8.0, "pinky": 16.0}
TOL = 0.05   # skin may touch, not pierce (0.15 let the fingers sink into the thin lever iron)
# vertex groups per bone (hand-space bind offsets)
GROUP = {}
for b, o in skin.BIND:
    GROUP.setdefault(b, []).append(o)
for b in GROUP:
    GROUP[b] = GROUP[b][::int(__import__('os').environ.get('NATGRIP_THIN', '2'))]  # thin out for speed (final: 1)

def Q(axis, deg): return qaxis(axis, math.radians(deg))

def finger_locals(f, cup, phi, a0, a1, a2):
    """Absolute anatomical angles (deg) -> local transforms; ref pose = REF_ABS."""
    ch = chain(f); out = {}
    if f == "thumb":
        raise ValueError
    r = REF_ABS[f]
    m, p1, p2, p3 = ch
    out[m] = T(qmul(MREF[m].q, Q(Z_FLEX, cup)), MREF[m].t)
    out[p1] = T(qmul(qmul(MREF[p1].q, Q(Y_ABD, phi)), Q(Z_FLEX, a0 - r[0])), MREF[p1].t)
    out[p2] = T(qmul(MREF[p2].q, Q(Z_FLEX, a1 - r[1])), MREF[p2].t)
    out[p3] = T(qmul(MREF[p3].q, Q(Z_FLEX, a2 - r[2])), MREF[p3].t)
    return out

def thumb_locals(base, a1, a2, cmc=None):
    """thumb: CMC = base rotation (optionally extra (abd, flex)); MCP/IP flex about -Z relative to ref."""
    t1, t2, t3 = chain("thumb")
    q1 = base[t1].q
    if cmc:
        q1 = qmul(qmul(q1, Q(Y_ABD, cmc[0])), Q(Z_FLEX, cmc[1]))
    return {t1: T(q1, MREF[t1].t), t2: T(qmul(MREF[t2].q, Q(Z_FLEX, a1)), MREF[t2].t), t3: T(qmul(MREF[t3].q, Q(Z_FLEX, a2)), MREF[t3].t)}

def world(hand, locs):
    """FK of the given local set (bones missing from locs use MREF)."""
    W = {H.HAND: hand}
    for b in BONES[1:]:
        W[b] = locs.get(b, MREF[b]) * W[PARENT[b]]
    return W

def link_pen(W, bone):
    """Deepest penetration (cm, > 0 = inside beyond TOL) of the bone's skin verts."""
    Wb = W[bone]; worst = -9.0
    for o in GROUP.get(bone, ()):
        d = FIELD(*Wb.pos(o))
        if -d > worst: worst = -d
    return worst - TOL

def link_gap(W, bone):
    Wb = W[bone]
    return min(FIELD(*Wb.pos(o)) for o in GROUP.get(bone, ((0, 0, 0),)))

def close_finger(hand, f, phi, rates=(1.0, 1.1, 0.75), step=1.0, start=None):
    """Human-like closing: all joints close together; a phalanx that touches stops itself and the joints before it,
    the joints after it keep closing (wrap). Returns (angles, contact flags)."""
    ch = chain(f); links = ch[1:]
    lo = [LIM_ABS["mcp"][0], LIM_ABS["pip"][0], LIM_ABS["dip"][0]]
    hi = [LIM_ABS["mcp"][1], LIM_ABS["pip"][1], LIM_ABS["dip"][1]]
    a = list(start) if start else [0.0, 0.0, 0.0]
    cup = lambda a0: CUP[f] * max(0.0, a0) / 90.0
    def W_of(a):
        return world(hand, finger_locals(f, cup(a[0]), phi, *a))
    # open until free (straight fingers can still touch the side of the gun)
    W = W_of(a)
    tries = 0
    while any(link_pen(W, l) > 0 for l in links) and tries < 20:
        a = [max(lo[0], a[0] - 1.5), max(lo[1], a[1] - 1.5), max(lo[2], a[2] - 1.0)]
        W = W_of(a); tries += 1
    locked = [False, False, False]
    contact = [False, False, False]
    for it in range(400):
        if all(locked): break
        trial = list(a)
        for j in range(3):
            if not locked[j]:
                trial[j] = min(hi[j], a[j] + rates[j] * step)
                if trial[j] >= hi[j]: locked[j] = True
        W = W_of(trial)
        hit = -1
        for k in range(3):
            if link_pen(W, links[k]) > 0:
                hit = k
        if hit >= 0:
            # stop joints 0..hit (they drive the touching link); later joints go on
            for j in range(hit + 1):
                locked[j] = True
            contact[hit] = True
            # keep the unlocked joints' progress only if it stays free
            t2 = list(a)
            for j in range(hit + 1, 3):
                t2[j] = trial[j]
            if not any(link_pen(W_of(t2), l) > 0 for l in links):
                a = t2
            continue
        a = trial
    return a, contact

def pad_point(W, f):
    b3 = chain(f)[-1]
    return W[b3].pos(add(mul(MREF[b3].t, 0.55), (0.0, 0.35, 0.0)))
