---
name: project-riflemega-retarget-2026-09-25
description: "★★★ 2026-09-25: all 446 RifleMega mocap clips retargeted onto the hero MetaHuman (/Game/AZ/Assets/RifleMega/**/AZ_RTG_MH_*) with Tools/riflemega_retarget.py - exact FK + spine/feet/left-hand/head post-process; pack specifics for wiring the 'винтовка' (no starts/stops, gun moves in reloads, 3 styles). Throwables resume AFTER the rifle."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-25T17:26:27.605Z
---

**User order (2026-09-25):** move every clip of `/Game/RifleMega_MocapAnimPack/AnimationsFBX` onto our MetaHuman "аккуратно, очень качественно"; the rifle (винтовка) must work like the автомат (M16); throwables
(stone/bottle, see [[project-throwable-resume-2026-09-17]]) come back after the rifle.

**Done.** 446/446 clips, same subfolders under `/Game/AZ/Assets/RifleMega/`, prefix `AZ_RTG_MH_`, saved (gitignored
Content/AZ/Assets). Retargeter `RTG_RifleMega_UE4_to_MetaHuman` + source rig `IK_RifleMega_Src_UE4` (tracked, in
`/Game/AZ/Blueprints/Animation/Retarget/`), target pose `MH_AlignedToRifleMega` (numeric alignment, 61 bones).
Pipeline + measurement: `Tools/riflemega_retarget.py` (MODE all/post/measure, FIRST/LAST slices).
Final numbers vs the original mocap, all 446: torso lean 0.0, sole contact 0.0, left-hand grip 0.0, head gaze 0.0,
exact bones (hands/fingers/right arm/clavicles) 0.0; IK limbs ≤14° arms / ≤21° legs (jumps in the air); planted
slide 0.07 cm/frame (source 0.03). Overlay check in the viewport (source mannequin vs MH full body) coincided.

**Pack facts.** UE4 Mannequin skeleton + one rigid weapon bone under `ik_hand_gun` (`RifleMesh` / `Rifle_Auto_Mesh` /
`Rifle_DoubleBarrel_Mesh` / `Rifle_ShotGun_Mesh` / `Rifle_Winch_Mesh`) — 5 skeletons identical except that bone name;
rest pose = UE4 Manny except feet 2.84°. No curves. 3 standing styles (Rifle01/02/03; sprint only 01 and 03) + crouch:
8-dir walk/run loops (root-motion AND `_IPC`), circle strafes, turns in place, St↔Cr and style↔style transitions,
hits/deaths, dodges, jumps, grenade, pickups, stun, melee, take/hide (draw/holster), patrol set, 17-pose aim sets.
**No locomotion starts/stops/pivots** (the M16 MM set relies on 122 of them). In reloads the gun moves up to ~20 cm
relative to hand_r (the hero has no `ik_hand_gun` bone — bake the gun path to curves if the pack reloads are used).
Pump/lever/break parts are not animated (guns are one rigid piece).

**Traps paid for today:**
- A 90-clip batch retarget + saves in ONE MCP call deadlocked the editor (CPU time frozen; call outlived the 300 s MCP
  timeout). Process one clip end-to-end per iteration, ≤40 clips per call, progress written per clip.
- Batch retarget with `overwrite_existing_files` over loaded assets is ~10x slower than creating new (0.7 → 6–11 s/clip).
- Two-bone IK must be **minimal swing + in-plane bend**; rebuilding the limb frame from a pole spun near-straight
  legs/arms 90–105° about their own axis (knee sideways). Detected only by per-bone orientation vs source, not by
  contact metrics.
- Foot contact must use heel/toe SOLE points in the foot frame (from each rest pose), not "ankle rest height" —
  the latter breaks in lying poses (stun/death).
- The aligned pose copies UE4 bone AXES: feet and head need rest-relative orientation (flat stays flat, gaze stays
  straight). Hands stay aligned (user's palm verdict, [[feedback_ik_retargeter_exact_transfer]]).
- `unreal.Rotator(a, b, c)` is (roll, pitch, yaw) — use keywords. The body mesh `SKM_MHC_Hero_BodyMesh` renders only
  hands (clothing-masked); preview with `/Game/AZ/Metahuman/MHC_Hero_FullBody`.
- Struct arrays again: FK chain settings must be rebuilt as fresh `RetargetFKChainSettings` in a new `unreal.Array`.

**Next (not started):** wire the rifle like the M16 — pick style (recommended Rifle01: has sprint), decide starts/stops
source, aim offset from the 17 poses (additive vs `_Aim_CC`, `ABPT_ANIM_FRAME` trap in
[[project_rifle_mh_native_migration_2026-09-09]]), weapon socket for the pack grip, reload gun-path curves.
