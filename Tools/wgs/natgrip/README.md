# Natural right-hand grasp - offline solver (Winchester pilot, 2026-09-29)

Prototype that replaced the "fit the clip's fingers" math. Plain CPython + PIL, run from this folder.

Inputs (dumped from the editor with the dump_*.py scripts, Unreal Python):
- `Saved/wgs/win_hand_live.json` - hand_r + finger locals of Rifle01 clips in Winchester space (dump_hand_live.py)
- `Saved/wgs/hand_r_verts.json`  - hero right-hand skin vertices (mesh bind pose) + mesh ref locals (dump_hand_verts.py)
- `Saved/wgs/win_wrist.json`      - forearm/hand for the wrist-bend metric (dump_wrist.py)
- `Saved/wgs/gf_winchester.json`  - the weapon's signed-distance field (in-game asset dump)

Model:
- MetaHuman right fingers: flexion = bone local -Z (verified: mocap flexion axes, sign test), MCP side = local +Y.
- 4 segments per finger: ring/pinky metacarpal cup coupled to MCP flexion; MCP (flex + side), PIP, DIP hinges only
  (DIP = 0.7 PIP in the search), limits MCP -15..90, PIP 0..105, DIP 0..80 (absolute, ref pose = 20/12/4 ...).
- Collision = the real skin vertices, rigidly bound to the nearest bone segment, against the field (soft tissue 1.5 mm,
  thenar 3.5 mm).
- Placement (hand re-grip): rotation about the middle knuckle + translation in weapon space, scored by wrist bend,
  palm clearance, index pad on the trigger, fingers through the lever loop and touching, thumb pad over the wrist.
- Fingers: flood fill of the free (MCP, PIP) configurations reachable from open, pick the relaxed power-grasp shape
  touching the gun; index = pad-to-trigger fit; thumb = CMC/MCP/IP search, pad on the wood, tip over the top.

Run: `python search2.py` (placement search, ~12 min) -> `python final.py "-16,0,0,-2.5,-0.5,-2.0" B` writes
nat_grip_B.json (+ renders); copy it to Saved/wgs/nat_grip.json; in the editor run Tools/wgs/nat_grip_preview.py.
Result B: wrist bend 43 deg (clip 63), palm clear 0.6 cm (clip 2 cm INSIDE the stock), index pad 0.1 mm off the trigger
target, middle/ring/pinky through the lever loop, thumb over the wrist (pad gap 0.6 mm), no finger overlaps.

## Natural trigger finger (M16 PIE review 2026-09-30, "the finger is a bit crooked")
fit_index only minimised pad-to-trigger distance; a pad on a point leaves one free DOF and it took MCP -5 + PIP 58 +
DIP 41 (knuckle bent back, hook). At that re-grip NO natural index shape reached the trigger (1.2-2.7 cm short), so the
placement changes too: `place_nat.py` (natural index set x placements near the applied one, palm contact, smallest
change; run `slice 0 1` single-process - 12 parallel CPython processes crashed with impossible TypeErrors / "Executing
a cache" here), `bend_m16.py` (wrist bend per W2 clip, Saved/wgs/m16_wrist.json), `rsolve3.py` (rsolve2 with
`index_nat.refine`; NATGRIP_POOL=1 = no worker processes), `view3d.py` / `compare3d.py` (perspective before/after).
Applied N2: p=(15,-5,0,-2,-1,-1.5), index 22/38/25 phi 2, wrist bend aim 24->21, relaxed 31->18.
