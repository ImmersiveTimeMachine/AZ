---
name: project_punch_contact_next_session
description: "★★★ START HERE next session (built 2026-09-01, UNTESTED): bounded back-step + warp windows on all four jab montages + heavy hit-window trim, plus the pose-selected Chalkie reactions (tested green). Exact PIE script and pass/fail lines inside; loose ends: delete the duplicate AddGameplayEventNotify in AZ_PoseSearchUtils, decide the heavy's future, light-tier reactions, commit."
metadata:
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-01T04:34:52.595Z
---

# Punch contact — state at end of 2026-09-01, everything below the line is UNTESTED

Last committed: `ebda88b` (PSI catch + pose-selected reactions). Everything here is in the working tree.
Build: `Result: Succeeded` on the final CLI build (editor closed). Read
[[project_motion_warping]] and [[reference_punch_reaction_content_inventory]] BEFORE changing anything —
both were updated with tonight's measurements and the two mistakes made by not reading them first
([[feedback_inform_before_proposing]]).

## What is live and untested

| change | where | value |
|---|---|---|
| bounded back-step | `UAZ_GA_MeleeAttack` clamp | `dest = Min(standoff, gap + MaxBackstepDistance)`; in-place clips also `Max(dest, gap − MaxInPlaceApproachDistance)`; both default **15** |
| warp windows, standing jabs | `AM_Fists_Punch_L/R` | 0 → 0.168 / 0.184 (contact), HermiteCubic (in-place → lerp branch) |
| warp windows, moving punches | `AM_Fists_Punch_Move_L/R` | 0 → 0.275 / 0.300, speed clamp 2.0 (70cm travel → scale/shear branch) |
| stand-off | `BP_GA_Punch_L` / `_R` CDOs | **120** / **130** (derived from the clips that warp; compiled + verified on fresh instances) |
| heavy hit window | `AM_Fists_Punch_Heavy_L` | 0.25→1.95 (1.70s!) trimmed to **0.25→0.55**; cancel window 1.70→2.64 intact |

## The PIE script (one run, three cases) and the lines that decide it

1. **Standing jab at close range** (walk up, stop, punch — the case that was always wrong):
   `[MeleeWarp] hero clip=AM_Fists_Punch_R gap=60 standoff=130 -> dest=75 (back 15cm, inPlace=1)`
   Knuckle 78 + 15 back = fist ~3cm past the capsule surface instead of 48. Watch the retreat: it must
   read as a weight-shift, not a slide. If it slides → lower `MaxBackstepDistance`, don't touch windows.
2. **Standing jab from far** (stand at 200, punch): `dest = gap − 15`, `inPlace=1`, `fwd 15cm` — a
   correction, NOT a dash. If the hero lunges more than ~15cm on a standing jab the in-place bound failed.
3. **Moving punch** (walk into the Chalkie punching): `[MeleeRM] … clip=AM_Fists_Punch_Move_R
   travel=70.1cm warpWindow=live->0.30s drive=YES` (was `none`), `inPlace=0`, and NO overshoot at close
   range — the Min side still holds for travelling clips. From range it should close to 120/130.
4. Heavy (left, > 110cm): hit window now 0.30s — should land once, not grind. Its warp window (0→1.65)
   is UNCHANGED and still spreads the approach across the walk-to-idle tail; that clip's fate is a
   separate decision (see below).
Failure signatures: "magnetic"/snappy → window too short or clamp too loose (lower clamp first); whiff →
stand-off too far (L toward 117, R toward 125); overshoot at close range on a MOVING punch → the Min
side is not behaving as the comment says → STOP and re-model (two-strike).

## Loose ends, in priority order
1. **Delete the duplicate** `UAZ_PoseSearchUtils::AddGameplayEventNotify` — `AZ_MontageUtils` has the
   real one (FName-based). Needs a closed-editor build; the beats it stamped are already on the clips.
2. **The heavy clip's role.** `Heavy2Idle` punches at 0.34 then walks 160cm; memory's 2026-08-02 verdict
   ("poor primary attack — give it its own input/ability at range") still stands. Options: trim the
   montage to ~0.9s (segment end drag; no script utility exists — `BuildSectionedMontage` takes whole
   sequences), or commission a PAIRED heavy take for PSIA (the ONE legitimate PSIA strike candidate —
   GASP's shove is 3.33s/1.33s, victim origin 85.6 ≈ our catch's 83.4).
3. **Light-tier reactions**: every hit currently plays a full 30–96cm knockback, so a jab launches the
   Chalkie out of jab range. The pack ships the same 16 knockbacks as `_IPC` in-place clips — that is the
   light tier for free. Second PSD, selected by damage magnitude, same `TrySelectReactionByPose`.
4. Commit. Untracked caveat unchanged: `Content/Zombie_01/` is gitignored, so the `bEnableRootMotion`
   flags on `Zombie_Atk_KnockBack_2..5` live only on this machine.
5. Pre-existing dirty files NOT part of any of this (leave or triage): the seven `PSD_v2_*`,
   `BP_CMC_Hero`, `BP_AZ_GA_Crouch`, `AZ_ABP_MoverAnimInstance`, `Tools/cmc_jump_notify_fixup.py`.

Related: [[project_motion_warping]], [[reference_punch_reaction_content_inventory]],
[[feedback_inform_before_proposing]], [[project_grab_grapple_design]], [[feedback_stop_the_patch_loop]].
