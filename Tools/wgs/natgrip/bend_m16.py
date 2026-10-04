"""Wrist bend (forearm vs hand, deg) of the M16 trigger hand for a placement, per W2 clip (elbow kept where the clip
has it; the hand-to-weapon relation is the same rigid socket in every clip)."""
import json, math
from ql import *
import place2
import hand as H
CL = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/m16_wrist.json"))
L = H.clip_local()
ELB = {}
for n, d in CL.items():
    hw = T.f7(d["hand_r"]); ELB[n] = H.HAND_W.pos(hw.ipos(T.f7(d["lowerarm_r"]).t))   # elbow in weapon space
def bend(p, clip):
    Hn = place2.hand_new(place2.corr(p[0], p[1], p[2], tuple(p[3:])))
    W = H.fk(L, Hn)
    wrist, mid = W["hand_r"].t, W["middle_01_r"].t
    return math.degrees(math.acos(max(-1, min(1, dot(norm(sub(wrist, ELB[clip])), norm(sub(mid, wrist)))))))
if __name__ == "__main__":
    import sys
    ps = [(0, 0, 0, 0, 0, 0), (10, -10, 10, -1, -1, -1)] + [tuple(float(v) for v in a.split(",")) for a in sys.argv[1:]]
    for c in ("AZ_RTG_MH_W2_Stand_Aim_Idle", "AZ_RTG_MH_W2_Stand_Relaxed_Idle", "AZ_RTG_MH_W2_CrouchWalk_Aim_BL_BkPd_Loop"):
        print(c[13:], "  ".join("%s: %.0f" % (list(p), bend(p, c)) for p in ps))
