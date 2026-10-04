# @Description: AZ_Grip_L of the M16 clips from where each clip's left hand is: 1 on the handguard, 0 off it (curves only, motion untouched)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/set_left_curve_m16.py").read(), {"CHUNK": 0, "CHUNK_SIZE": 200, "DRY": True})
# The M16 support hand is IK'd onto the weapon's LeftHandGrip (solved natural grasp, Tools/wgs/natgrip/lsolve_m16.py)
# wherever the clip itself holds the handguard. Where the clip's hand leaves the gun (reload, holster, swap, unjam,
# melee, death, the no-weapon idles) the curve fades to 0 so the clip's own hand plays. On = the clip's hand_l within
# R_ON cm of the handguard axis (and along the handguard), off beyond R_OFF; sampled at FPS, 3-tap smoothed.
# The M16 clips are MetaHuman-native (no az_weapon_r track): weapon = RightHandM16Socket (m16_hold_pick.json) * hand_r.
# APPROACH (2026-10-03): where the hand flies onto the handguard (a draw, the end of a reload) the curve used to rise
# only once the clip's hand had stopped - the IK then visibly slid it the last 8 cm / 34 deg onto the solved grasp
# ("as if it searches for its place"). Now, on the rising edge only, the weight also follows the distance to the IK
# TARGET (the solved hand, Saved/NaturalGrip/NGP_M16_Left/support_final.json): 0 at T_OFF cm, 1 at T_ON cm, from the
# frame the hand starts closing in. Release edges and everything else stay as they were. With APPROACH a clip is
# rewritten only when its old curve is reproduced exactly and the approach changes it.
import json
import math
import unreal
FOLDER = globals().get("FOLDER", "/Game/AZ/Assets/M16/Riffle_RTG_MH")
CHUNK = int(globals().get("CHUNK", 0))
CHUNK_SIZE = int(globals().get("CHUNK_SIZE", 200))
DRY = bool(globals().get("DRY", True))
FPS = 15.0
MIN_ON, RAMP = 0.35, 0.15
AX = (-1.1, 11.85)                  # handguard axis (x, z) in weapon space, along +y
R_ON, R_OFF = 12.5, 16.0
Y_LO, Y_HI, Y_RAMP = 5.0, 45.0, 3.0
APPROACH = bool(globals().get("APPROACH", True))
T_ON, T_OFF = 12.0, 30.0            # distance to the IK target (cm): full weight / none
APPROACH_MAX = 0.4                  # s: how far back before the grab the approach may start
ONLY = globals().get("ONLY")        # optional list of clip names
APE = unreal.AnimPoseExtensions
AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
WS = unreal.AnimPoseSpaces.WORLD
FLOAT = unreal.RawCurveTrackTypes.RCT_FLOAT
v = json.load(open("C:/UnrealEngine/Games/AZ/Saved/wgs/m16_hold_pick.json"))["weapon_in_hand"]
S = unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))
h = json.load(open("C:/UnrealEngine/Games/AZ/Saved/NaturalGrip/NGP_M16_Left/support_final.json"))["hand_in_weapon"]
TARGET = (h[0], h[1], h[2])


def on_value(p):
    r = math.hypot(p.x - AX[0], p.z - AX[1])
    a = min(1.0, max(0.0, (R_OFF - r) / (R_OFF - R_ON)))
    b = min(1.0, max(0.0, (p.y - (Y_LO - Y_RAMP)) / Y_RAMP), max(0.0, ((Y_HI + Y_RAMP) - p.y) / Y_RAMP))
    return a * b


ar = unreal.AssetRegistryHelpers.get_asset_registry()
paths = sorted(str(a.package_name) for a in ar.get_assets_by_path(FOLDER, recursive=True)
               if str(a.asset_class_path.asset_name) == "AnimSequence")
total = len(paths)
if ONLY:
    paths = [p for p in paths if p.rsplit("/", 1)[1] in ONLY]
paths = paths[CHUNK * CHUNK_SIZE:(CHUNK + 1) * CHUNK_SIZE]
opts = unreal.AnimPoseEvaluationOptions()
const, animated, failed = 0, [], []
changed, mismatch = [], []
for p in paths:
    seq = unreal.load_asset(p)
    L = max(seq.get_play_length(), 0.001)
    n = max(2, int(L * FPS) + 1)
    times = [L * k / (n - 1) for k in range(n)]
    vals, dist = [], []
    for t in times:
        pose = APE.get_anim_pose_at_time(seq, t, opts)
        wpn = unreal.MathLibrary.compose_transforms(S, APE.get_bone_pose(pose, "hand_r", WS))
        rel = unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(pose, "hand_l", WS), wpn)
        vals.append(on_value(rel.translation))
        q = rel.translation
        dist.append(math.sqrt((q.x - TARGET[0]) ** 2 + (q.y - TARGET[1]) ** 2 + (q.z - TARGET[2]) ** 2))
    vals = [(vals[max(0, i - 1)] + vals[i] + vals[min(n - 1, i + 1)]) / 3.0 for i in range(n)]
    # no brief grabs: a hand only passing the handguard mid-reload (slapping the magazine, the charging handle) must not
    # be snapped onto the grip - "on" runs shorter than MIN_ON s inside the clip are dropped, edges ramp over RAMP s
    on = [v > 0.5 for v in vals]
    i = 0
    while i < n:
        j = i
        while j < n and on[j] == on[i]:
            j += 1
        if on[i] and 0 < i and j < n and times[j - 1] - times[i] < MIN_ON:
            for k in range(i, j):
                on[k] = False
        i = j
    mask = [1.0 if o else 0.0 for o in on]
    for k in range(n):
        if not on[k]:
            continue
        d = min([abs(times[k] - times[m]) for m in range(n) if not on[m]] or [9.0])
        mask[k] = min(1.0, d / RAMP) if d < RAMP else 1.0
    vals = [min(a, b) if b < 1.0 else a for a, b in zip(vals, mask)]
    old_vals, times_all = list(vals), list(times)
    holster = "Return_To_MOB" in p or ("Holster" in p and "Unholster" not in p)
    if APPROACH and not holster:                 # a holster's grip is off once the gun is on the back
        # every grab (an on-run after an off stretch): the rising edge also follows the distance to the IK target, up
        # to the run's own peak (a hand that only hovers at the handguard keeps its lower weight); after the frame where
        # the old curve reaches that peak nothing changes, so release edges stay as they were
        i = 0
        while i < n:
            j = i
            while j < n and on[j] == on[i]:
                j += 1
            if on[i] and i > 0:
                peak = max(vals[i:j])
                r = i
                while r < j - 1 and vals[r] < peak - 1e-3:
                    r += 1
                s0 = i - 1                       # walk back while the hand is closing in on the target
                while (s0 > 0 and not on[s0 - 1] and dist[s0 - 1] > dist[s0] + 1.0 and dist[s0 - 1] < T_OFF
                       and times[i] - times[s0 - 1] <= APPROACH_MAX):
                    s0 -= 1
                tw = [min(1.0, max(0.0, (T_OFF - d) / (T_OFF - T_ON))) for d in dist]
                base = min(tw[s0 - 1] if s0 > 0 else 0.0, 0.999)   # stopped inside T_OFF: the ramp starts from there
                prev = vals[s0 - 1] if s0 > 0 else 0.0
                for k in range(s0, r + 1):
                    w = max(0.0, (tw[k] - base) / (1.0 - base)) * peak
                    vals[k] = min(peak, max(vals[k], w, prev))
                    prev = vals[k]
            i = j
    new_vals = list(vals)
    if min(vals) > 0.995:
        times, vals = [0.0, L], [1.0, 1.0]; const += 1
    else:
        animated.append((p.rsplit("/", 1)[1], round(min(vals), 2), round(sum(vals) / n, 2)))
    if APPROACH:
        # write only a clip whose current curve is exactly the old rule's (nothing tuned by hand / another script)
        # and which the approach changes
        if not AL.does_curve_exist(seq, "AZ_Grip_L", FLOAT):
            continue
        kt, kv = AL.get_float_keys(seq, "AZ_Grip_L")
        olds = ([1.0, 1.0] if min(old_vals) > 0.995 else [round(x, 4) for x in old_vals])
        oldt = [0.0, L] if min(old_vals) > 0.995 else times_all
        same = len(kt) == len(oldt) and all(abs(a - b) < 1e-3 for a, b in zip(kt, oldt)) and all(abs(a - b) < 2e-3 for a, b in zip(kv, olds))
        delta = max(abs(a - b) for a, b in zip(new_vals, old_vals))
        if delta < 0.02:
            continue
        name = p.rsplit("/", 1)[1]
        if not same:
            mismatch.append(name)
            continue
        changed.append((name, round(delta, 2), [(round(t, 3), round(a, 2), round(b, 2)) for t, a, b in zip(times_all, old_vals, new_vals) if abs(a - b) > 0.01]))
    if DRY:
        continue
    if not APPROACH and min(vals) > 0.995 and AL.does_curve_exist(seq, "AZ_Grip_L", FLOAT) and min(AL.get_float_keys(seq, "AZ_Grip_L")[1]) > 0.995:
        continue                                   # already constant 1: nothing to write
    if AL.does_curve_exist(seq, "AZ_Grip_L", FLOAT):
        AL.remove_curve(seq, "AZ_Grip_L", False)
    AL.add_curve(seq, "AZ_Grip_L", FLOAT, False)
    AL.add_float_curve_keys(seq, "AZ_Grip_L", times, [round(x, 4) for x in vals])
    if not EAL.save_asset(p, only_if_is_dirty=False):
        failed.append(p)
print("[set_left_curve_m16] chunk %d (%d of %d) dry=%s: constant 1 = %d, animated = %d, failed %s"
      % (CHUNK, len(paths), total, DRY, const, len(animated), failed[:5]))
if APPROACH:
    print("  approach changes %d clips; skipped (current curve is not the old rule's) %d: %s" % (len(changed), len(mismatch), mismatch[:20]))
    for c in changed:
        if globals().get("BRIEF"):
            print("   ", c[0], c[1], "%.2f-%.2f s" % (c[2][0][0], c[2][-1][0]))
            continue
        print("   ", c[0], "max delta", c[1])
        print("      (t, old, new):", c[2])
else:
    for a in animated:
        print("   ", a)
import gc
gc.collect()
