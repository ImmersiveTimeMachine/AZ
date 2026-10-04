"""WGS task 1.4 - grasp solver v3: close every finger on the weapon's real surface (CPython).

    python grasp_solve.py          -> Saved/wgs/grasp_solution.json + a report on stdout

Hands are where the AZ Weapon Grip node puts them (Tools/wgs/export_hands.py): the right hand carries the gun
(RightHandWinchesterSocket), the left hand sits on LeftHandGrip. Nothing moves the hands here - only fingers.

Per finger, from an OPEN hand (reference phalanges on the mocap metacarpals / thumb base):
  - hinge axes: for each joint the axis perpendicular to the bone and the palm normal, fixed in the parent's frame,
    signed so that a positive angle curls toward the palm (sign taken from the mocap: its curled fingertips lie on
    the palm side of the open ones);
  - if a phalanx starts inside the weapon, the base joint first opens (extension) until the finger is out;
  - underactuated closing (like a human / an adaptive robot hand): MCP and PIP close together in steps, DIP coupled
    to PIP (0.75); a joint stops for good when its step would push ANY phalanx it moves deeper than SOFT into the
    surface; the other joint keeps closing. Result: each finger ends wrapped on the surface or at its joint limit.
Distances: geom.Weapon (per-part exact triangle distance through a cell grid, winding-number inside test).
"""
import json
import math
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import Weapon                                                     # noqa: E402
from handfk import (Hand, FINGERS, finger_bones, segment_clearance, qaxis, qmul, qinv, qrot, qnorm,   # noqa: E402
                    unit, cross, sub, dot, length)

EXPORT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/hands_export.json"
PARTS = r"C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json"
OUT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/grasp_solution.json"
SOFT = 0.15                         # allowed skin compression at a contact (cm)
STEP = 2.0                          # closing step (deg)
LIMITS = {"finger": (90.0, 105.0, 80.0), "thumb": (50.0, 70.0, 80.0)}     # MCP, PIP, DIP max closing (deg)
EXTEND = -30.0                      # how far a base joint may open to get a finger out of the weapon
DIP_RATIO = 0.75


def palm_normal(h, side, mocap_hand):
    p0, p1, p2 = h.pos("hand_" + side), h.pos("index_01_" + side), h.pos("pinky_01_" + side)
    n = unit(cross(sub(p1, p0), sub(p2, p0)))
    v = sub(mocap_hand.tip("middle"), h.tip("middle"))       # mocap middle is curled, the open one is straight
    return n if dot(v, n) >= 0 else (-n[0], -n[1], -n[2])


def solve_hand(weapon, rec, side, hand_ws=None, step=None):
    global STEP
    if step:
        STEP = step
    if hand_ws is not None:
        rec = dict(rec)
        rec["mocap"] = dict(rec["mocap"])
        rec["mocap"]["hand_ws"] = list(hand_ws)
    start = {}
    for b in rec["bones"][1:]:
        is_phalanx = any(b.endswith("_0%d_%s" % (i, side)) for i in (1, 2, 3))
        thumb_base = b == "thumb_01_" + side
        src = "ref_local" if (is_phalanx and not thumb_base) else "local"
        key = "mocap"
        v = rec[key][src][b]
        # keep the mocap bone translation (bone lengths identical anyway), take the rotation from the chosen source
        start[b] = (tuple(rec[key]["local"][b][:3]), tuple(v[3:7]))
    mocap = Hand(rec, side, "mocap")
    h = Hand(rec, side, "mocap", finger_locals=start)
    n = palm_normal(h, side, mocap)
    base_local = dict(h.local)
    report, solved = {}, {}
    for f in FINGERS:
        joints = finger_bones(f, side)
        if f == "thumb":
            joints = joints[1:]                  # thumb_02 (MCP), thumb_03 (IP); the CMC stays as in the mocap
        axes = []
        for j in joints:
            child = h.pos(joints[joints.index(j) + 1]) if joints.index(j) + 1 < len(joints) else h.tip(f)
            d = unit(sub(child, h.pos(j)))
            a_world = unit(cross(d, n))
            if length(a_world) < 0.5:
                a_world = axes[-1][1] if axes else (1.0, 0.0, 0.0)
            prot = h.world[h.parents[j]][1]
            axes.append((j, a_world, qrot(qinv(prot), a_world)))
        lim = LIMITS["thumb" if f == "thumb" else "finger"]

        def apply(th):
            for (j, _aw, al), t in zip(axes, th):
                h.local[j] = (base_local[j][0], qnorm(qmul(qaxis(al, t), base_local[j][1])))
            h.solve()
            return [segment_clearance(weapon, s) for s in h.segments(f)]

        if f == "thumb":
            th = [0.0, 0.0]

            def coupled(t1, t2):
                return [t1, t2]
        else:
            th = [0.0, 0.0]

            def coupled(t1, t2):
                return [t1, t2, min(DIP_RATIO * t2, lim[2])]
        cl = apply(coupled(*th))
        start_cl = list(cl)
        while min(cl) < -SOFT and th[0] > EXTEND:
            th[0] -= STEP
            cl = apply(coupled(*th))
        stopped = [False, False]
        for _ in range(200):
            moved = False
            for k in (0, 1):
                if stopped[k] or th[k] + STEP > lim[k]:
                    stopped[k] = True
                    continue
                trial = list(th)
                trial[k] += STEP
                c2 = apply(coupled(*trial))
                # a joint moves the phalanges from itself outward: k=0 -> all three, k=1 -> middle + distal
                affected = c2[k:] if f != "thumb" else c2[k + 1:]
                before = cl[k:] if f != "thumb" else cl[k + 1:]
                worse = any(a < -SOFT and a < b - 1e-3 for a, b in zip(affected, before))
                if worse:
                    stopped[k] = True
                else:
                    th, cl, moved = trial, c2, True
            if not moved:
                break
        cl = apply(coupled(*th))
        report[f] = {"angles": [round(x, 1) for x in coupled(*th)], "clearance_start": [round(x, 2) for x in start_cl],
                     "clearance_end": [round(x, 2) for x in cl]}
    for b in rec["bones"][1:]:
        solved[b] = list(h.local[b][0]) + list(h.local[b][1])
    return solved, report, n, h


def main():
    E = json.load(open(EXPORT))
    weapon = Weapon.load(PARTS)
    out = {"export": EXPORT, "bones": {}, "report": {}}
    for side in ("r", "l"):
        solved, report, n, _h = solve_hand(weapon, E["hands"][side], side)
        out["bones"].update(solved)
        out["report"][side] = report
        print("hand", side, "palm normal", [round(x, 2) for x in n])
        for f, r in report.items():
            print("  %-6s angles %-22s clearance start %-24s end %s" % (f, r["angles"], r["clearance_start"], r["clearance_end"]))
    json.dump(out, open(OUT, "w"), indent=1)
    print("written", OUT)


if __name__ == "__main__":
    main()
