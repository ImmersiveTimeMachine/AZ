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

## 2026-09-01 late — first PIE result (45 punches) + Fable adversarial pass

**Clamp: 45/45 lines match `Min(standoff, gap+15)` (+ in-place floor).** Worst contact gap 68 (was 60
routinely). Retreat only ~6 of 15cm landed because the hit window opens at 0.120 while the warp window
ran to 0.167/0.183 (HermiteCubic still easing) and the Chalkie walks in. APPLIED (saved, verified by
dump): `AM_Fists_Punch_L/R` warp window → **0.001–0.120** (= hit-window open), `MaxRotationRate` 180→**720**
on all four jab/move clips. Heavy untouched (can't read back its easing/speed-clamp; separate decision).
PASS LINE: `[MeleeWin] gap` within ~5cm of the `dest` on the matching `[MeleeWarp]` line.

**Rotation residual (user report) = rotation BUDGET = `MaxRotationRate × window`** (verified in
`RootMotionModifier.cpp:595-612`: Slerp spreads over remaining window, ClampedRate caps per-frame step).
Was 180×0.167 = 30° on jabs. Now 86°/197°/215°. Rotation DOES reach the Mover pawn
(`RootMotionAttributeLayeredMove.cpp:162` → AngularVelocityDegrees). Snap risk is the window length, not
the rate; `WarpRotationTimeMultiplier` <1 finishes rotation EARLIER inside the window.

**Dropped clicks — mechanism, verified.** LMB=`Input.Action.PrimaryAttack`→`BP_GA_Punch_L`,
RMB=`SecondaryAttack`→`BP_GA_Punch_R` (separate specs). `UAZ_AbilitySystemComponent::AbilityInputTagHeld`
calls `TryActivateAbility` ONLY on INACTIVE specs (`AZ_AbilitySystemComponent.cpp:~73`); `Pressed` never
activates. So: **same-button mash is blocked for the FULL 0.800s montage** — the GA's own recovery gate
(`AZ_GA_MeleeAttack.cpp:112`, `bRetriggerInstancedAbility=true`) is never consulted; the 0.500 CancelOpen
only helps CROSS-hand (`IsOtherMeleeCommitted`). Nothing latches a refused press. **The input buffer is
DESIGNED, not built**: GA ctor comment (`:35-40` "Same-hand chains arrive with the input buffer, which
must attempt activation on an active spec"), tag doc (`AZ_GameplayTags.cpp:315` "read by the input
buffer"). Only reader of `State.Combat.CancelWindow` today is `TryMovementCancelAttack` (hero cpp:296).
FIX (one closed-editor build): (1) rail: in `AbilityInputTagHeld`, also `TryActivateAbility` on ACTIVE
specs whose ability has `bRetriggerInstancedAbility` → same-hand reaches 0.5s parity via the existing
gate; (2) buffer: latch a refused press ~0.25s, replay when `State.Combat.CancelWindow` rises or the spec
ends. Do NOT move CancelOpen earlier — it sits exactly at hit-window end (0.500) by design.

**Three heavies with `[MeleeWarp]` but no `[MeleeWin]`: explained, not a bug** — hero was grabbed
(23:27:11 +51ms `[Grab] GA_PlayerGrabbed ACTIVE`) or hit (+89ms / +158ms `[HitReact] struck`) inside the
heavy's 0.25s wind-up.

Known imprecisions left in place: `MaxBackstepDistance` bounds the DESTINATION GAP (bFollowComponent),
not displacement — a charging Chalkie can pull a slightly longer retreat; `Move_L/R` warp windows
(0.274/0.299) still end AFTER their hit window opens (0.150) — conservative, their contacts land 0.19–0.30.

**Click fix parts 1+2 BUILT — `Result: Succeeded` 2026-09-02, UNTESTED in PIE, UNCOMMITTED:** `UAZ_AbilitySystemComponent::AbilityInputTagHeld`
now also calls `TryActivateAbility` on ACTIVE specs when the ability is InstancedPerActor +
`bRetriggerInstancedAbility` (flag is protected, no getter, friend not inherited → read via
`FindFProperty<FBoolProperty>`; file-local helper `WantsRetriggerWhileActive`, no header change ⇒
Live-Coding-safe). Engine runs CanActivateAbility BEFORE the retrigger EndAbility, so a refused press is
inert. PASS LINE: mash LMB only → consecutive `[MeleeWarp] … clip=AM_Fists_Punch_L` lines ~0.50s apart
(was 0.80). Holding LMB should auto-chain at 0.5s. Part 2 (latch a refused press ~0.25s, replay on
`State.Combat.CancelWindow` rising / spec end) = new members ⇒ closed-editor build; batch with deleting
the duplicate `UAZ_PoseSearchUtils::AddGameplayEventNotify`.

**Part 2 (buffer) as built:** `UAZ_AbilitySystemComponent` — `InputBufferSeconds` (UPROPERTY, default
**0.5** = the whole committed zone, so ANY click during a swing chains the next; 0 disables),
`BufferedInputTag/Time`, `LatchBufferedInput`, `OnCancelWindowTagChanged` (RegisterGameplayTagEvent on
`State.Combat.CancelWindow`), `OnAnyAbilityEnded` (`OnAbilityEnded` delegate), both → `ScheduleBufferedInputReplay`
(SetTimerForNextTick — never inline, callers are ability code) → `ReplayBufferedInput` → `AbilityInputTagHeld(Tag,
bBufferIfRefused=false)`. ONLY presses whose spec carries `Ability.Combat.Melee` latch (jump/interact never fire
late). A refused replay KEEPS the original latch+timestamp; expiry drops it. `AbilityInputTagHeld` now returns bool.
LOG LINES: `[InputBuffer] <avatar> latched Input.Action.PrimaryAttack` (only on a NEW latch, not per held frame),
`[InputBuffer] … replay … after 0.31s -> activated|refused, holding`, `[InputBuffer] … dropped … (0.72s old)`.
PASS: click at ~0.2s into a jab → `latched` then `replay … after ~0.3s -> activated` and a `[MeleeWarp]` on
the same frame. FAIL: a `replay -> activated` that cuts a swing before its `[MeleeWin] OPEN` (gate order broken
→ revert part 1), or any `[InputBuffer]` line for a non-attack input tag.
**Also in this build:** duplicate `UAZ_PoseSearchUtils::AddGameplayEventNotify` DELETED (+ its two includes);
`AZ_MontageUtils::AddGameplayEventNotify(Montage, FName, …)` is the one that exists.

**2026-09-02 PIE #2:** parts 1+2 PASS (same-button 0.5s cadence; `[InputBuffer] latched → replay after
0.27s -> activated`, `refused, holding` while committed). NEW BUG exposed by back-step + 720°/s: Facing
turns the hero AWAY when `dest` is behind (see [[project_motion_warping]] § Facing). Fix in flight via
Sonnet subagent: `MeleeTarget_Facing` rotation-only twin (GA body-only + AZ_MontageUtils same-target
replace rule + 5 montages re-authored with 2 windows each; heavy keeps 0→1.65 @180). PASS LINE: a
`[MeleeWarp] … back 15cm` followed by `[MeleeHit] BP_GA_Punch_* -> BP_AZ_Chalkie`.
⚠ The Facing split lives in a LIVE CODING patch (AZ_GA_MeleeAttack.cpp + AZ_MontageUtils.cpp) — it dies
on editor restart. Next editor close: CLI build BEFORE reopening, or the hero spins again and the utility
stacks/replaces windows the old way. Cosmetic: the rotation-only window sits on the "MotionWarping" track,
the translation-only on track "1" (editor display only; both fire).

**2026-09-02 PIE #3 (a.MotionWarping.Debug 1):** the split's translation modifier NEVER RAN — engine
de-dups montage warp notifies by exact (Start,End) (`MotionWarpingComponent.cpp:496`); see
[[project_motion_warping]]. FIXED: rotation window now starts +5ms (content, saved, dump-verified).
ALSO: back-step now IN-PLACE ONLY (travelling clips = plain Min; signed SkewWarp scale would moonwalk) —
Live-Coded, `Result: Succeeded` 01:36:30. UNTESTED. Header doc comments on MaxBackstepDistance /
MaxInPlaceApproachDistance still say "any clip" — fix at the next CLI build (comment-only).
PASS LINES (CVar still on): (1) TWO `LogMotionWarping: SkewWarp` lines per frame per clip — rotation one
Dist2D≈gap with Delta==FDelta, translation one Dist2D≈|dest−gap| (~15) shrinking to ~0 across the window
with FDelta≠Delta; (2) `[MeleeWarp] … inPlace=0` never prints `back`; (3) a `[MeleeWarp] … inPlace=1
back 15cm` whose translation lines walk 15→0 = the hero itself stepped back. Proposed, NOT applied:
close-range selection (in-place jab when gap < stand-off even while moving) in `SelectMontage`.

**2026-09-02 PIE #4 — GREEN, verified with a.MotionWarping.Debug (hero's OWN motion):** two SkewWarp
lines every frame. Standing L `gap 84→dest 99`: translation Dist2D 27→24→19→14→6→**0.000** at pos 0.118,
FDelta 3/5/5/7.7/6.2cm vs clip's 0.2-4 (the warp IS the step), dot −0.9995 (dest behind, correct);
rotation line Dist2D = raw gap 72→99, delta==FDelta, dot +0.9995 (faces Chalkie). Standing R `60→75`:
15.3→2.3 by pos 0.107, contact gap 73 (knuckle 78 → 5cm past centre, was 18). Far R `202→187 fwd 15`:
15→9.6 approaching. Heavy at gap 114 (dest=gap): FDelta 0.001-0.006 = lunge collapsed to zero, stood
and swung, hit at 0.27 — no through-body. 0 `back` on inPlace=0. Buffer 10 lines OK.
KNOWN, VISIBLE NOW: the bound is on the destination GAP (bFollowComponent) — vs a closing Chalkie the L
jab retreated 27cm to hold 99, not 15. If it reads as a hop: cap DISPLACEMENT (latch start location →
header member → CLI build). Header doc comments still say "any clip" — fix at that build.
NEXT: commit (5 montages + 6 source files); CLI build before next editor reopen (LC patches die);
optional close-range selection rule; a.MotionWarping.Debug is session-only (not saved).

## COMMITTED 2026-09-02 03:25 — `88af268` (melee) · `28c9672` (grab). PUSHED 04:30 (origin/spike/cmc-backport @ 28c9672).
Melee: warp windows/rate, facing split (+5ms de-dup), back-step in-place-only, same-target replace rule,
input rail retrigger + buffer, duplicate util deleted — all PIE-verified green. Grab: see
[[project_grab_grapple_design]] tail — position FIXED 8/8; facing spring still ~0.25s (open; `[GrabFace]`
diagnostic committed, not yet read from a live catch). ⚠ Several of these are Live Coding patches on top
of the 02:2x CLI build — run a CLI build BEFORE the next editor reopen. Left uncommitted on purpose
(pre-existing, unrelated): the seven PSD_v2_*, BP_CMC_Hero, BP_AZ_GA_Crouch, AZ_ABP_MoverAnimInstance,
Tools/cmc_jump_notify_fixup.py, and the untracked MHC/CMC/Plugins dirs.

## NEXT SESSION: [[project_psia_heavy_strike_plan]] (PSIA heavy on the R key). Its prereqs: CLI build, one [GrabFace] catch, impact frames by eye.
