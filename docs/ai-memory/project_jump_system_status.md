---
name: project_jump_system_status
description: "v2 jump system status — RM in-place + moving-jump MVP (chooser), and the known issues driving next steps (run lands-to-stop → MM Land2Run; uneven-terrain float-then-drop → gravity-governed descent)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# v2 Jump System — status & next steps (2026-06-02)

## Mechanism (current)
- Jump fires off the GAS jump edge → `UAZ_LocomotionStateMachine` returns `TransitionToInAir`, held for the chosen clip's length, then settles to loco/idle.
- **`UAZ_PawnMovementMode_RMAction`** drives the capsule: gravity-free, floor-snap-free, follows the clip's full XYZ root motion (OverrideAll). One mode for all jumps (and later vault/mantle).
- MP: sim proxies mirror the replicated `RMAction` MovementMode (not the one-shot edge); the AnimInstance does NOT queue the RM move / QueueNextMode on sim proxies (they follow the replicated arc). Committed `bc20e53`.

## Anim assets (`/Game/Assets/RM_Movement/`)
- In-place: `RTG_RM_Jump_place_ALL` (+`_short`), `RTG_RM_JumpIdleStart`, `RTG_RM_JumpIdleLand`/`…Hard`/`…2Walk`.
- Walk/Run moving (per takeoff foot LU/RU): one-shot `RTG_RM_Jump_{walk,run}_{lu,ru}_ALL`; componentized `…JumpWalkStart_{LU,RU}` / `…JumpRunStart_{LU,RU}` → `…_Land` (to stop) / `…_Land2Walk` / `…_Land2Run` (continue).
- Foot convention (matches the stop/turn rows): `bLeftFootDown=True → "RU"` clip, `False → "LU"` (left foot down ⇒ right foot up).

## Moving-jump MVP — DONE (chooser-only), in CHT_v2_CharacterAnimations
- Added a **`bIsMoving` bool column** bound to `ChooserContext.bIsMoving` (ctx 0). Place row → `bIsMoving=False` (standing-only); 4 moving rows (`TransitionToInAir` + `bIsMoving=True` + `Gait`×foot) → `…_{walk,run}_{ru,lu}_ALL`; 1 moving fallback (`Gait` Any) → `place_ALL` (sprint/edge). Jumps are `bUseMM=False` (no MM on takeoff — only the landing should use MM).
- **Chooser column reorder happened again:** live layout is now `… 7 bMovingTransition | 8 bJustLanded | 9 bIsMoving | 10 OutputStruct` (OutputStruct trails inputs). Data moved with columns; matching intact. RE-INSPECT before any by-index write. See [[feedback_chooser_column_reorder]].
- Re-bound `bLeftFootDown` (col 4) to ctx 0 — the documented "always-_LU" context-index fix (foot was reading always-left). VERIFY in PIE that `ru`/`lu` now alternates.
- Decision: gate place-vs-moving on `bIsMoving` (input intent) — user accepted; NOT latching pre-jump idle-state for now.

## KNOWN ISSUES → next steps
> **DECISION 2026-06-02:** jumps pivot RM→**PHYSICS** (launch + Falling mode + cosmetic anim + MM land) — it fixes all the below with less code, and physics is the only thing that adapts to unknown terrain height. Full step list in [[project_physics_jump_plan]]. The descent/landing reasoning below is the rationale that led there.
1. **Run `…_ALL` lands-to-STOP** (walk is OK). The one-shot run clips are jump-to-stop; continuing the run hitches. Fix = componentize the run jump (Start → air → **`Land2Run`**) and select the landing via **Motion Matching** (landings vary by touchdown pose; MM over a `Land2Run`/`Land2Walk`/`Land2Stop` PoseSearch DB blends seamlessly into the loop). User leans MM. Needs: a land SM phase + `PSD_v2_JumpLandings` + chooser land row (`bUseMM=True`).
2. **Uneven terrain "float-then-drop"** (found 2026-06-02): jump from height X, land where ground Y < X → the baked RM keeps the capsule at ~X for the whole clip, then it drops to Y only when RMAction → Walking floor-snaps. Pure-RM jumps assume flat ground and can't adapt to terrain height. **Fix direction:** the DESCENT must be **gravity + floor-governed**, not baked RM — i.e. RM drives takeoff/pose (up), then gravity pulls the capsule to the ACTUAL floor, and the **land is triggered by real floor contact** (not the clip's baked end). This converges with (1): the componentized jump should be **takeoff(RM up) → gravity-fall + floor detect → floor-contact-triggered MM land**, which fixes BOTH the lands-to-stop and the uneven-terrain issues. (This is the same reason GASP uses physics arcs for jumps; our RM-moving clips are still usable for the pose/takeoff + warped/blended.)

See [[project_v2_locomotion_progress]], [[project_locomotion_sm_refactor_plan]], [[project_root_motion_mode]].
