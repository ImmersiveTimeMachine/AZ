import json, math
from ql import *
import place2
from hand import *
_d = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/win_wrist.json"))["clips"]["AZ_MST_Rifle01_St_Aim_CC"]
ELBOW = T.f7(_d["lowerarm_r"]["w"]).t
WW = T.f7(D["clips"]["AZ_MST_Rifle01_St_Aim_CC"]["frames"][0]["weapon_ws"])
_L = clip_local()
def bend(C):
    Hn = place2.hand_new(C)
    W = fk(_L, Hn)
    wrist = WW.pos(W["hand_r"].t); mid = WW.pos(W["middle_01_r"].t)
    return math.degrees(math.acos(max(-1, min(1, dot(norm(sub(wrist, ELBOW)), norm(sub(mid, wrist)))))))
if __name__ == "__main__":
    print("clip bend %.1f" % bend(place2.corr(0, 0, 0, (0, 0, 0))))
    ok = json.load(open("place2_ok.json"))
    rows = []
    for p, w in ok:
        b = bend(place2.corr(p[0], p[1], p[2], tuple(p[3:])))
        rows.append((b, p, w))
    rows.sort()
    for b, p, w in rows[:25]: print("bend %.1f p=%s palm %.2f" % (b, p, w))
    json.dump(rows, open("bend_rows.json", "w"))
