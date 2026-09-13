---
name: project_aim_tip_next_session_2026-09-12
description: "★★★ 2026-09-12 aim turn-in-place SHIPPED and ON: rifle AZ_RTG_MH_W2_*_Aim_Turn_In_Place_*_Loop_IPC loops for the lower body under the aim lock, capsule yaw-rate clamp 360 deg/s (user-tuned, feet skate accepted), enter 45 / exit 6; pistol picks its own TurnL/R_90Loop. Telemetry CVars now default 0 (az.Aim.Debug 1 to see [v2 Aim]). Open follow-ups: per-weapon rate, discrete RM turns."
metadata:
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-12T04:09:58.235Z
---

# Aim turn-in-place — where the next session starts (user, 2026-09-11 23:59)

## STATUS 2026-09-12 (after the session): DONE, user: "OK IT WORKS", then "360 for me works"
- `bAimTurnInPlaceEnabled` = true on the MHC pawn's walking mode, `AimTurnInPlaceRateDegPerSec` = **360**
  (user tuned from 67 -> 120 -> 360; the loop still plays at 1.0, so the feet skate during the turn and the
  user accepts that for the speed), enter 45, exit 6. Saved in `AZ_BP_PawnMoverHero_MHC` 14:49.
- Log proof of the mechanism (rate 67 run): 142 frames in SM 10/11, bodyYawRate pinned at exactly 67, picks =
  `Stand_Aim_Turn_In_Place_{L,R}_Loop_IPC`; the PISTOL picks `AS_Pistol_TurnL/R_90Loop` (rows gated correctly).
- Telemetry defaults flipped to 0 the same day (`az.Aim.Debug`, `az.Cam.Debug`, `az.Facing.Debug`,
  `az.TipRate.Debug`, new `az.Lean.Debug`): `[v2 Facing]` logged ~60x/frame from inside GenerateWalkMove
  (4,400 lines/s) and caused the user's in-game lag. Enable per test in the PIE console.
- Open follow-ups if the skate bothers later: (1) per-weapon rate = profile `AimTurnInPlaceClipRateDegPerSec`
  (rifle 67 / pistol 90) x one multiplier on the walking mode; (2) discrete RM turn clips for big swings
  (pistol has 90/180, rifle has none).

**The instruction, verbatim intent:** "we have turn anims, so we can use `AZ_RTG_MH_W2_*_Aim_Turn_In_Place`
for the lower body only, when we are in aim, not in relaxed — remember, tomorrow we start from this."

## What is already built (all committed in `b640892`, all OFF by one switch)
- `UAZ_PawnMovementMode_Walking::bAimTurnInPlaceEnabled` (default false) gates BOTH the pawn latch and the SM
  entry (thresholds + switch travel in `FAZ_LocoSMInputs` from the walking mode — one source of truth).
- SM states `IdleTurnLeft/Right` (not-moving switch): enter at `AimTurnInPlaceEnterDeg` (35), exit at
  `ExitDeg` (6) or aim end, 0.2 s idle dwell, move-out -> plain forward start (never bucketed while aiming).
- Pawn `ProduceInput` aim branch: latch `bAimTurningInPlace`, ships `FAZ_MoverCustomInputs::AimTurnYawRateLimit`
  (= `AimTurnInPlaceRateDegPerSec`, pawn BP currently 240) — the mode clamps the yaw rate after the spring;
  the turn FINISHES after the aim button is released (`IsAimTurningInPlace()` ORed into `bIsAiming` on the
  anim side, so the clip + aim idle persist through the tail).
- Chooser rows 304-307 (`CHT_v2`, copies of the unarmed rows 77/78): Stand/Crouch x L/R ->
  `AZ_RTG_MH_W2_{Stand,Crouch}_Aim_Turn_In_Place_{L,R}_Loop_IPC`, `loop=True`. Rows 77/78 pinned to
  bIsAiming=False; other agent's pistol rows 397-400 (`AS_Pistol_TurnL/R_90Loop`) come after.
- `GetWeaponLoopPlayRate` is pinned to 1.0 by another agent (user rule: play rate 1, no fast-motion).
- Upper-body lock (LayeredBoneBlend spine_01 depth 4, mesh-space, weight = AimAlpha) already replaces
  everything above the pelvis with the aim idle while aiming — so ANY turn clip is automatically
  "lower body only" under it. That is the lever the user is pointing at.
- Aim ENTRY is solved separately and must stay: `AimMaxYawRateDegPerSec` 540 (0.3 s snap), AO yaw range
  fade 30..60, camera interp 18, rifle `AimBlendInSpeed` 20. TIP is for camera DRIFT while already aiming.

## Facts that constrain the design (measured)
- The loops are authored at 45 deg / 0.67 s = **67 deg/s** (stand and crouch, L and R). Use the **_IPC** twins:
  the non-IPC ones carry the 45 deg in the pelvis and POP at every loop seam when the capsule also turns.
- Lateral pelvis sway per step: rifle loop 3 cm (fine at 1x), pistol loop 16 cm. Above ~1.25x play rate the
  hips visibly rock; at 3.6x it was the user's "sway". At play rate 1 the capsule rate must be ~67 for
  planted feet; faster = mild slide (accepted by most TPS during a turn the camera is moving with).
- The pistol set also has DISCRETE root-motion turns (`AZ_Pistol_TurnL/R_91` = 90 deg / 1.0 s,
  `TurnL/R_181` = 180 deg / 1.33-1.43 s, metahuman_base_skel) — usable as LEGS under the rifle lock via the
  existing RM bridge (`bPendingTransitionRMMove`) if the loops are not enough.
- Any per-tick decision must ride `FAZ_MoverCustomInputs`, never a mode member (the MM trajectory
  predictor runs `GenerateWalkMove` ~60x/frame). Never compile the pawn BP from Python while a hero
  preview ticks (hung the editor); the user compiles BPs.

## Suggested first hour
1. Build (the `[v2 Aim]` telemetry is committed but not yet compiled), then flip `bAimTurnInPlaceEnabled`
   on the pawn's walking mode by hand, set `AimTurnInPlaceRateDegPerSec` = 67 (the clip rate), enter 45.
2. Verify in PIE + `[v2 Aim]`/`[v2 Facing]`/`[v2 Pick]`: SM=10/11 only while aiming at rest, the aim idle
   torso stays on top (lock), relaxed never turns.
3. Judge speed vs feet; if 67 is too slow, the options are (a) rate 90-100 with slight slide, (b) the
   pistol discrete RM turns as legs, (c) authored fast rifle turn clips.

Related: [[project_rifle_aim_upper_body_lock_2026-09-10]], [[feedback_mover_mode_state_not_rollback_safe]],
[[feedback_parallel_build_header_edit_corruption]].
