---
name: project_m16_sprint_turns_2026-10-05
description: "★★ M16 sprint turn -> left hand reaching = UNARMED turn-start rows (CHT_v2 88-93 never excluded the M16; Sept fix covered only moving-180). Fixed 2026-10-05 by data; check the c9 column of shared rows before adding weapon rows."
metadata:
  type: project
---

**Symptom (user, came back twice):** sprinting with the M16 and turning, the left hand reaches for the rifle.

**Cause:** the M16 matched the shared unarmed sprint turn starts.
- Rows 88-93 (L90 / R90 / L135 / R135, from-rest 180) had c9 (inverted, non-exact) = `Weapon.Rifle.Winchester`, so they excluded only the Winchester.
- The 2026-09-16 fix (`docs/design-briefs/weapon-moving-180-status.md`) handled only moving 180 (rows 94-97 + 403-406).
- The 2026-09-27 Winchester work added the Winchester-only exclusion to rows 77-102.

**Fix 2026-10-05 (data):**
- Rows 88-93 c9 = `Weapon.Rifle`.
- Rows 537-548 = M16 run turn rows 195-200 / 213-218 with Gait Sprint.
- Script: `Tools/diagnostics/fix_m16_sprint_turn_starts.py`. Backup: `AZ_Backups/2026-10-05_M16SprintTurnStarts_161447`.

**How to apply:**
- For any "weapon pose wrong in a transition" report: look at the [v2 Pick] clip first. An AnimPro_* clip under a weapon means a shared row leaked.
- Then read c8 (exact include), c9 (NOT, non-exact) and c19/c20 (pistol) of the matching row with `get_cell_gameplay_tags_on_sub`. DumpChooserFullTree does not print tags.
- When adding a child weapon tag, exclude the PARENT tag on shared rows, not just the child.

Related: [[feedback_chooser_row_add_tag_columns]], [[project_winchester_integration_audit_2026-09-26]].

**Second pass 2026-10-05:**
- **Stops.** Sprint stops (shared 85-87) leaked too: every reversal passed through `AnimPro_RunFwdStop`. Now c9 = Weapon.Rifle. M16 rows 549/550 = RAP `AZ_RTG_MH_Rifle_SprintStop_RU/LU` (root motion on). Rows 551/552 = M16 sprint straight starts.
- **OPEN:** M16 sprint jumps are unarmed. M16 jump rows 280-303 exclude Movement.Sprinting (a back-carry leftover), so shared rows 98-101 and M16 300/301 play AnimPro jumps. Fix separately (three-phase jump).

**Third pass 2026-10-05:** M16 moving reversals (run + sprint, 180 and 135) now use the rifle RUNNING pivot `MocupOnline_Rifle_RunFwdTurn180_{L,R}_{LU,RU}` (332 -> 5 -> 414 cm/s; AZ_Grip_L = 1 added).
- Before, they used the W2 from-REST turn start (5 -> 349), so a sprint dropped to ~0 and then re-accelerated: 'slow then abrupt fast'.
- Rows: 276-279 / 403-406 are RU, 553-560 LU. The 135 rows are from-rest only; moving copies are 561-576.
- Script `Tools/diagnostics/fix_m16_moving_pivots.py`; backup `AZ_Backups/2026-10-05_M16MovingPivots_163830`.
- **Next levers:** loop gait by speed after a pivot; keyboard SOCD (last key wins).

**Fourth pass 2026-10-05:** the user wanted the left hand RELEASED during the pivot.
- The constant AZ_Grip_L = 1 glued the hand to the M16 handguard while the pivot's arms hold a different rifle shape.
- The pivot curves are now 1 -> 0 (first 0.15 s), 0 through the turn, then 0 -> 1 (last 0.3 s). Script `Tools/diagnostics/m16_pivot_left_hand_release.py`.

**FINDING to verify next (likely a general cause of 'abrupt' transitions).** On a stop or pivot entered from an IN-PLACE loop, the capsule speed collapses for the first frames.
- Log 16:46:04: SprintStop picked at spd 538, and 22 ms later spd = 15.
- Hypothesis: `FLayeredMove_RootMotionAttribute` (OverrideAll) reads the BLENDED root-motion attribute. The outgoing in-place loop contributes 0, so the capsule velocity follows the incoming clip's blend weight (about 0 -> 1 over the 0.2 s cross-fade) instead of keeping its speed.
- Check: per-frame capsule speed vs the transition clip's blend weight on a run stop. If confirmed, scale the RM by 1/weight, or take the transition clip's own RM, while it blends in.

**Fifth pass 2026-10-05 (user-approved approach):** BAKED M16 running pivots `.../M16/Riffle_RTG_MH/Pivots/AZ_M16_RunPivot180_[Aim_]{L,R}_{LU,RU}`.
- Built by the new C++ `UAZNaturalGripLibrary::GraftUpperBody` (AZNaturalGrip, NGUpperBodyGraft.cpp).
- Body (momentum pivot): AnimPro_RunFwdTurn180_*. Arms: W2 jog loop (or aim jog loop), relative to the chest. Spine: a stance delta to the W2 jog.
- All 32 M16 moving-pivot rows point to them. Reusable for the Winchester / pistol.
- Trap: AnimPro clips are at odd frame rates (263.8 fps), so the output keeps its own rate and the body is resampled.
- Trap: saving new anim assets under the Rider debugger can hit a render-thread ensure in FAppTime::Get (thumbnail). The editor looks hung; resume via xdebug.

**PIE 2026-10-05: user "everything works great".**
- Log: 7 run 180 pivots (L/R, LU/RU, relaxed) hand back to W2_Jog_F_Loop at MM cost 0.01-0.04. The usual exit is 0.09-0.18, so the arms match the loop seamlessly.
- One sprint pivot exits to SprintLoop. Sprint-loop costs are always ~1e4 (single-loop DB), so that is not a regression.
- Nuance: on sprint the reversal passed through zero input for a moment (keyboard), so SprintStop played 0.08 s before the pivot. This is the known SOCD item.
- Not covered by the log: aim pivots, 135 pivots.
