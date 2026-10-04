"""Fingertip markers as the AZ Weapon Grip node will actually produce them (CPython).

    python predict_tips.py   -> Saved/wgs/predicted_tips.json {"Grip_<L|R>_<Finger>": [x, y, z] weapon space} + report

The node keeps the CLIP's bone translations (bone lengths / knuckle offsets) and the clip's metacarpals, and takes
only the rotations of <finger>_01.._03 from the grip pose. The pack clips (AZ_MST_*, retargeted) have hand
proportions that differ from the grip pose asset's by up to 2.4 cm at the knuckles, so markers must be computed with
the clip's translations: hand transform (export) + mocap locals, finger _01.._03 rotations from the grasp solution
(solved fingers) or from the grip pose asset (the others, e.g. the thumbs).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import Weapon                                            # noqa: E402
from handfk import Hand, FINGERS, finger_bones, segment_clearance  # noqa: E402

EXPORT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/hands_export.json"
SOLUTION = r"C:/UnrealEngine/Games/AZ/Saved/wgs/grasp_solution.json"
PARTS = r"C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json"
OUT = r"C:/UnrealEngine/Games/AZ/Saved/wgs/predicted_tips.json"
SOLVED = {"index", "middle", "ring", "pinky"}


def main():
    E = json.load(open(EXPORT))
    sol = json.load(open(SOLUTION))["bones"]
    weapon = Weapon.load(PARTS)
    out, report = {}, {}
    for side, S in (("r", "R"), ("l", "L")):
        rec = E["hands"][side]
        mocap = rec["mocap"]["local"]
        grip = rec["grip"]["local"]
        override = {}
        for f in FINGERS:
            for b in finger_bones(f, side):
                rot = sol[b][3:7] if f in SOLVED else grip[b][3:7]
                override[b] = (tuple(mocap[b][:3]), tuple(rot))
        h = Hand(rec, side, "mocap", finger_locals=override)
        for f, F in zip(FINGERS, ("Thumb", "Index", "Middle", "Ring", "Pinky")):
            out["Grip_%s_%s" % (S, F)] = list(h.tip(f))
            report["%s %s" % (side, f)] = [round(segment_clearance(weapon, s), 2) for s in h.segments(f)]
    json.dump({"tips": out, "clearance": report}, open(OUT, "w"), indent=1)
    for k, v in report.items():
        print("%-9s clearance proximal/middle/distal (cm): %s" % (k, v))
    print("written", OUT)


if __name__ == "__main__":
    main()
