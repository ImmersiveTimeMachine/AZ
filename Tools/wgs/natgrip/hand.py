import json, math, os
from ql import *
import views
SIDE = os.environ.get("NATGRIP_SIDE", "r")          # r = the trigger hand, l = the support hand
HAND = "hand_" + SIDE
WEAPON = os.environ.get("NATGRIP_WEAPON", "winchester")
_HOLD = None
if SIDE == "r" and WEAPON != "winchester":
    # hand bone structure from the Winchester dump (same skeleton); the hold = the weapon's medoid clip frame
    D = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live.json"))
    BONES, PARENT, REF = D["bones"], D["parents"], {b: T.f7(v) for b, v in D["ref_local"].items()}
    _HOLD = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/%s_hold_pick.json" % WEAPON))
    HAND_W = T.f7(_HOLD["weapon_in_hand"]).inv()      # hand_r in weapon space
elif SIDE == "r":
    D = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live.json"))
    BONES, PARENT, REF = D["bones"], D["parents"], {b: T.f7(v) for b, v in D["ref_local"].items()}
    HAND_W = T.f7(D["clips"]["AZ_MST_Rifle01_St_Aim_CC"]["frames"][0]["hand_in_weapon"])
elif WEAPON != "winchester":
    # support hand of another weapon: bone structure from the Winchester dump (same skeleton); the hold = a medoid clip
    # frame of the weapon's own clips (Saved/wgs/<weapon>_left_picks.json, NATGRIP_LEFT_PICK = aim | relaxed)
    D = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live_l.json"))
    BONES, PARENT, REF = D["bones"], D["parents"], {b: T.f7(v) for b, v in D["ref_local"].items()}
    _HOLD = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/%s_left_picks.json" % WEAPON))[os.environ.get("NATGRIP_LEFT_PICK", "aim")]
    HAND_W = T.f7(_HOLD["hand_in_weapon"])
else:
    D = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live_l.json"))
    BONES, PARENT, REF = D["bones"], D["parents"], {b: T.f7(v) for b, v in D["ref_local"].items()}
    HAND_W = T.f7(D["hand_in_weapon"])                # hand_l is IK'd onto the LeftHandGrip socket
FING = ("thumb", "index", "middle", "ring", "pinky")
def chain(f):
    return ([] if f == "thumb" else ["%s_metacarpal_%s" % (f, SIDE)]) + ["%s_0%d_%s" % (f, i, SIDE) for i in (1, 2, 3)]
def fk(local, hand=HAND_W):
    W = {HAND: hand}
    for b in BONES[1:]:
        W[b] = local[b] * W[PARENT[b]]
    return W
def tip(W, local, f, ext=0.85):
    b3 = chain(f)[-1]
    return W[b3].pos(mul(local[b3].t, ext))
def clip_local(clip="AZ_MST_Rifle01_St_Aim_CC", k=0):
    if _HOLD is not None:
        return {b: T.f7(v) for b, v in _HOLD["local"].items()}
    if SIDE == "r":
        return {b: T.f7(v) for b, v in D["clips"][clip]["frames"][k]["local"].items()}
    return {b: T.f7(v) for b, v in D["clip_local"].items()}
