import os, sys, json
from ql import *
import skin, views
import hand as H
from grasp3 import segs, seg_seg, RAD, world
import place2
def skin_gap(W, fa, fb):
    """closest skin-to-skin distance between two fingers (phalanges only), cm; <0 = overlap"""
    pa = [(b, W[b].pos(o)) for b, o in skin.BIND if b in H.chain(fa)[1:]][::2]
    pb = [(b, W[b].pos(o)) for b, o in skin.BIND if b in H.chain(fb)[1:]][::2]
    best = 9.0; which = None
    for ba, a in pa:
        for bb, b in pb:
            d = length(sub(a, b))
            if d < best: best, which = d, (ba, bb)
    return best, which
def report(hand, locs, label):
    W = world(hand, locs)
    out = []
    for fa, fb in (("index", "middle"), ("middle", "ring"), ("ring", "pinky")):
        d, w = skin_gap(W, fa, fb)
        # also the gap between the middle phalanges' surfaces along their length (capsules)
        sa, sb = segs(W, fa), segs(W, fb)
        mid = seg_seg(sa[1][0], sa[1][1], sb[1][0], sb[1][1]) - sa[1][2] - sb[1][2]
        dist = seg_seg(sa[2][0], sa[2][1], sb[2][0], sb[2][1]) - sa[2][2] - sb[2][2]
        out.append("%s-%s: nearest skin %.2f cm, middle phalanges %.2f, distal %.2f" % (fa, fb, d, mid, dist))
    print(label); [print("   ", o) for o in out]
if __name__ == "__main__":
    side = os.environ.get("NATGRIP_SIDE", "r")
    if side == "l":
        d = json.load(open("C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip_left.json"))
        hand = T.f7(d["hand_in_weapon"])
    else:
        d = json.load(open("C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip.json"))
        hand = place2.hand_new(place2.corr(-16, 0, 0, (-2.5, -0.5, -2.0)))
    locs = {b: T(q, skin.MREF[b].t) for b, q in d["locals"].items()}
    report(hand, locs, "solved " + side)
    # reference: the MetaHuman ref pose spread
    W0 = world(T(), {})
    report(T(), {}, "MH reference pose (relaxed, for comparison)")
