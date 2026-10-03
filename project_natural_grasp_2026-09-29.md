---
name: project-natural-grasp-2026-09-29
description: "Winchester right-hand grasp redesign - why the finger math looked crooked (palm 2 cm in the stock, retarget-broken fingers, pistol-grip hand) and the anatomical re-grip + baked-grasp solution"
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-01T05:10:42.674Z
---

**2026-09-29, user asked for a deep re-think of the finger math ("не тупое айкей").** Root causes found, all verified
with data (Tools/wgs/natgrip/, README there):
1. The weapon field was CORRECT (mesh sections vs field match; lever-loop opening free). Winchester parts:
   ChargerBase = the big-loop LEVER (top bar 0.5 cm under the wrist, loop interior ~9 x 6 cm), BasePart = trigger
   (+barrel), ShutterDet = hammer, StockBase = stock (wrist = tall oval 3.6 x 5-6.5 cm, straight, NO pistol grip).
2. Right hand relative to the gun is CONSTANT in every Rifle01 clip (az_weapon_r == hand_r, child with identity);
   in Shoot_Winch the hand rides the lever (fingers belong INSIDE the loop), so solve the grasp ONCE per weapon.
3. With the user-tuned RightHandWinchesterSocket the palm/heel skin is up to 2.1 cm INSIDE the stock; the pack
   hand is a pistol-grip "handshake" hand (knuckle line ~vertical) -> fingers can only curl horizontally.
4. Retargeted clip fingers are anatomically broken: MCP twists 20-36 deg, PIP bends sideways, ring DIP hyperextended
   30 deg; the old solver used them as the base + axes from bone x palm-normal + collision samples 2.4 cm apart
   (thin lever bar slips through) -> crooked.
5. Clip wrist bend (forearm vs hand) = 63 deg.
MetaHuman finger rig: flexion = bone local -Z, MCP side = local +Y, ref pose already curled (index 20/12/4 ...);
real phalanx radii ~1.0/0.85/0.68 cm (old node used 0.85/0.75/0.65).

**Chosen solution (result B, previewed as /Game/AZ/Assets/Master/GripPreview/AZ_MST_Rifle01_St_Idle00_NatGrip):**
hand re-grip = yaw -16 deg about the middle knuckle + (-2.5, -0.5, -2.0) cm weapon space (hand-space D in
Saved/wgs/nat_grip.json, wrist moves 5.1 cm, gun STAYS) -> wrist bend 43, palm clear 0.6 cm; anatomical fingers
(index pad on the trigger 0.1 mm, M/R/P through the loop 63/69/48, 60/93/65, 66/84/59, thumb over the wrist
MCP/IP 20/20, pad gap 0.6 mm). Preview actors via Tools/wgs/nat_grip_preview.py (level NOT saved).

**APPLIED 2026-09-29 (both hands, built, flags on in AZ_BP_Winchester; PIE check pending):** right = re-grip in
Body Clearance + baked fingers; ring/pinky re-solved on an EXACT fine field (the 0.5 cm game lattice misses the thin
lever iron: loop bar centre +0.06 vs exact -0.15) and threaded-through-the-loop search (closing from straight crosses
the bar). Left = LeftHandGrip socket re-placed (roll 20 deg about the barrel, -0.5 x, -0.3 z; old value in
AZ_Backups/2026-09-29_natgrip_left) + baked fingers solved one after another lying ALONGSIDE the previous one (user
spotted 1.8 cm gaps: the MH ref pose spread kept by independent solves); wrist bends L 31->19, R 64->43 deg.
Tools: Tools/wgs/natgrip (NATGRIP_SIDE=l|r, fine_field.py, left_adjacent.py, final.py), nat_grip_preview.py,
apply_nat_grip.py.

**M16 APPLIED (right hand only, 2026-09-29 late):** the M16 hung on middle_01_r (gun turned 23-27 deg with the finger
in NW/RAP/swap clips) -> own socket RightHandM16Socket on az_weapon_r (medoid W2 hold); re-grip ~1.6 cm / 17 deg;
AS_Grip_M16; AZ_BP_Rifle sockets + GripPose + flags; curves AZ_Grip_R=1 / AZ_Grip_L=0 in all 923 clips of
/Game/AZ/Assets/M16/Riffle_RTG_MH (backup AZ_Backups/2026-09-29_natgrip_m16). Left hand NOT done: the W2 left hand
slides 3-6 cm off LeftHandGrip/LeftHandGripAim between relaxed and aim -> needs a per-state or runtime left grasp.
Body Clearance now evaluates with a re-grip even without stock markers (push only with markers) - Live Coding .cpp.
**Solver v2 (Tools/wgs/natgrip/rsolve2.py, user review "fingers not pulled in, thumb sticks up"):** every phalanx must
REST on the grip (optimization over MCP/PIP/DIP, DIP free), pad must come AROUND (far side of the grip centre AND back
behind the front face - a straight finger along the guard/side also "rests"), neighbour adjacency; thumb: both
phalanges resting, tip on the far side, not above the receiver. Closure simulations (joint-by-joint, synergy+locking)
failed: tips touch the guard first. Generic tools: weapon_skm_dump.py (skeletal weapon -> JSON; do NOT run heavy
GeometryScript while another session runs editor Python - froze the editor once), skm_parts.py, add_hand_socket.py,
add_grip_curves.py, nat_grip_preview.py (SOCKET/BEFORE_SOCKET/WEAPON/DST/LABEL; MH-native clips without az_weapon_r
-> the preview hangs the gun on hand_r with S*D^-1).

**M16 index fix (2026-09-30, user PIE: "палец чуть-чуть кривой"):** the pad-on-trigger fit had a free DOF and
picked a hook (MCP -5/PIP 58/DIP 41); at the approved re-grip no natural index reached the trigger, so the PLACEMENT
was re-searched with a natural index (Tools/wgs/natgrip place_nat.py + rsolve3.py): hand 1.4 cm higher/outward,
index 22/38/25, wrist bend lower. Applied to AS_Grip_M16 + AZ_BP_Rifle correction (backup AZ_Backups/2026-09-30_m16_index),
user PIE: works ("правой рукой можно заканчивать"). ★ LESSON (user: "сегодняшний урок про правую руку"): a contact
target leaves free DOFs and a contact-only fit picks hooks/claws -> ALWAYS search the hand PLACEMENT together with
natural finger shapes (every joint flexed, DIP ~0.65 PIP, small side angle) + palm contact + wrist bend; for BOTH hands.
**M16 LEFT HAND APPLIED 2026-10-01** (Tools/wgs/natgrip/lsolve_m16.py, L2 p=(0,10,15,1,0,0) around the medoid AIM
clip hold; LeftHandGrip on M16_Skeleton (old in AZ_Backups/2026-10-01_m16_left), AS_Grip_M16 left fingers,
bBakedLeftHandGrasp; AZ_Grip_L: 871 clips 1, 52 animated (reload/holster/swap/death/melee, brief grabs <0.35 s removed;
Tools/wgs/set_left_curve_m16.py). 76 unarmed AnimPro clips used in M16 transitions have NO AZ_Grip curves - user
declined adding them (dismissed). ★ NEXT (user PIE 2026-10-01, screenshot): left hand only SUPPORTS from below,
thumb and hand do not grip ("when shooting you hold it firmly"). Likely causes: (1) search anchored on the clip's
cradle hold + "smallest change"; (2) thumb rule copied from the Winchester forend (lies along, pointing forward) - no
thumb OPPOSITION rule; (3) wrap only 106-149 deg, proximal phalanges 4-6 mm off - not a power grasp; (4) maybe a
runtime mismatch (in-game fingers point along the barrel, render shows them up the far side) - verify first.
Plan: verify runtime vs solve; power-grasp objective (handguard seated in the hand, palm+proximals touching, wrap
>=180 deg, thumb pad opposite the finger pads = closure); wider rotation about the handguard axis.
Machine note: 12 parallel CPython solver processes produced impossible TypeErrors + "Fatal Python error: Executing a
cache" (single process fine) - run the solvers single-process (NATGRIP_POOL=1, place_nat `slice 0 1`).

**Why:** no finger math can look natural while the palm is inside the wood and the base fingers are twisted.
**How to apply:** runtime plan pending user review of the preview: AAZ_Weapon RightHandGripCorrection (hand-local)
+ bBakedRightHandGrasp -> markers; Body Clearance applies the re-grip first (two-bone IK, weight = ClearanceAlpha x
AZ_Grip_R, output az_weapon_r so the gun stays); Grip node uses the grip pose for the right hand as-is (incl. ring/
pinky metacarpal cup); write the solved locals into AS_Grip_Winchester. Related: [[project-weapon-arm-solver-2026-09-28]],
[[project-winchester-integration-audit-2026-09-26]].
