---
name: project_crouch_start_loop_flap_2026-10-04
description: "★★ Winchester crouch start<->loop flip: DIAGNOSED + FIXED 2026-10-04 (borrowed crouch turn starts, 5 Winchester PSDs moved to the Master schema, SM pivot block). Built; user PIE check pending."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-05T02:51:22.420Z
---

**Symptom.** Log 2026-10-05 02:10 UTC, frozen in `Saved/Diagnostics/CrouchStartLoopFlap/`.
- Crouched walk with the Winchester. After a stop and a stick reversal, the SM went start (SM=3, `AZ_RTG_MH_Rifle_Crouch_WalkFwdStart`) -> loop (SM=2) -> start again one frame later, 3-4 times.
- Several loop picks logged `[v2 MMFallback] MotionMatch returned NOTHING` (cost FLT_MAX), a pop to frame 0.

**Cause 1 (the flip):** a moving reversal (`|PendingStartAngleDeg| >= 135`) makes the SM pivot through a 135/180 turn start. The crouch rifle set (RifleAnimsetPro) has no turn starts.
1. CHT_v2 row 505 (Winchester crouch start) had StartDirection = Any, so the straight start played.
2. `FLayeredMove_RootMotionAttribute` is OverrideAll (it applies root-motion rotation too), so the capsule did not turn during the start.
3. The start ended still reversed, so the SM pivoted again.

**Cause 2 (NOTHING):** 5 Winchester PSDs (Crouch, RunAim, RunRelaxed, WalkRelaxed, Sprint) held SK_AZ_Master clips but used PSS_v2_SurvivalMan_Loco (SKEL_SurvivalMan). Only WalkAim had been moved before.

**Fix (2026-10-04):**
1. **Chooser.** `Tools/diagnostics/add_winchester_crouch_turn_starts.py` (backup `AZ_Backups/2026-10-04_WinchesterCrouchTurnStarts_024939`).
   - Rows 531-536 are copies of 505: StartDirection L90/R90/L135/R135/L180/R180, MovementDirection Any.
   - Clips: `AnimPro_Crouch_WalkFwdStart90_L/R`, and `180_L/R` for both 135 and 180. The Winchester has bUpperBodyFromAimPoseInTransitions, so the arms stay on its crouch aim pose.
   - Row 505 narrowed to StartDirection == Fwd.
2. **Databases.** `Tools/diagnostics/fix_winchester_db_schemas.py` (backup `AZ_Backups/2026-10-04_WinchesterSchemaAll_024744`): the 5 PSDs now use `PSS_WIN_Master_Loco`. Membership is unchanged, no mirrored entries.
3. **Code.** `UAZ_LocomotionStateMachine::bPivotBlockedUntilAligned`. A start or pivot that ends still `>= ReversalAngleDeg` blocks further moving pivots until `|angle| < 90` (the facing spring turns in the loop). It is cleared on any not-moving / air / traversal / suppressed frame.
   - This is the generic safety net for sets without turn starts. The protected pistol crouch has none either, and unarmed / Winchester-standing have no 135 rows.
   - Header change: built with the editor closed.

**Not changed:** the protected M16 / pistol / unarmed rows and clips. The M16 crouch already has W2 turn rows.

**How to apply:** if crouch turns still look wrong, check which row fires (v2 Pick, c10) before adding more rows. The 135 bucket is mapped to the 180 clip on purpose.

**PIE 2026-10-05 03:06 UTC (after the fix):**
- No flip, no `MMFallback` in the whole session. Crouch turn starts fire as designed.
- **New side effect, the user saw it:** at the END of a crouched start, the arms (with the rifle) dipped for 1-2 frames and came back up.
- **Cause:** the borrowed unarmed turn-start clip shows through. `TransitionLockAlpha` eased out at 12/s the moment the SM left the transition, but the blend stack cross-fades out of the borrowed clip over 0.2 s. About 1/3 of the unarmed arms showed for ~0.1 s.
- **Fix:** member `BorrowedTransitionHoldUntil`. The lock is held at 1 for 0.25 s after a borrowed transition ends, rises at 30/s and falls at 12/s. Header change, closed-editor build.
