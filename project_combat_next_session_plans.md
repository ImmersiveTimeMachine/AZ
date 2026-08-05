---
name: project_combat_next_session_plans
description: "★ NEXT SESSION: two planned combat tasks — (P0) fix the RM travel-gate regression that killed the Chalkie's warp lunge AND its rotation tracking, plus the missing BT warp clamp; then (P1) replace the hand-rolled stagger timer with a duration GameplayEffect. Both Fable-validated 2026-08-03."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-05T00:36:18.226Z
---

# Combat — two planned tasks for next session (planned + Fable-validated 2026-08-03)

Branch `feature/NPC`, HEAD `ae632c3`, tree clean.

**Content-versioning trap (one instance fixed, the general case is NOT).** `.gitignore:65-69` allows
only `Content/AZ/{Blueprints,AI,Camera}` for LFS budget, so tracked montages reference sequences the
repo does not contain. Worse than a missing asset: this codebase decides gameplay by MEASURING clip
data at runtime, so a missing/differently-configured clip takes a different branch SILENTLY instead of
erroring. The grab shove's `bEnableRootMotion` hit exactly this — fixed at `ae632c3` by moving
`RTX_RT_AS_NAAT_Zombie_Grab_To_{Pushed,Kicked}` into `/Game/AZ/Blueprints/Animation/Grab/` (in-editor
rename, never a filesystem move — referencers must be fixed up). **The fist, knockback and hero grab
clips are still in ignored folders.** Widening the ignore is a cost decision, not a technical one; the
alternative is authoring gameplay-critical facts into tracked data (see the `bDriveCapsule`/
`CapsuleMotionPolicy` note in Task 1) so untracked sequences stop being load-bearing.

## ★ TASK 1 (P0, blocks everything else in NPC combat) — the travel-gate regression

**What I broke.** `UAZ_GA_MeleeAttack::ActivateAbility` (~line 272) gates `DriveRootMotion` on the
montage's own measured displacement ≥ `MinRootMotionTravel` (5cm). Correct for the hero's jabs (they
set `bEnableRootMotion` but travel 0.0-0.3cm, and driving them PINS the pawn under OverrideAll).
**Wrong for the Chalkie**: its claw clips are IN-PLACE by design and rely on SkewWarp's
"add translation" branch to synthesise the approach.

**Why it kills more than the lunge.** The motion-warp hook lives INSIDE
`FLayeredMove_RootMotionAttribute::GenerateMove`. No live layered move ⇒ GenerateMove never runs ⇒ the
warp modifier never executes. That kills BOTH the approach translation AND the Facing rotation warp
(same modifier). The legacy facing fallback is disabled behind `!bUseMotionWarping`
(`AZ_BTTask_ZombieAttack.cpp:307`), so the Chalkie starts a swing anywhere in a cos(60°) cone up to
180cm, then stands statue-still and un-tracking while the hero strafes. Reach ≈132cm vs 180cm start ⇒
whiffs outright at range.

**The fix — the discriminator is not "does the clip travel":**
```
bDrive = bClipTravels
      || (montage has a MotionWarping window whose WarpTargetName is CURRENTLY REGISTERED
          on the pawn's UMotionWarpingComponent)
```
API, no new data: `UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(Montage, OutWindows)`
→ each notify's `URootMotionModifier_Warp::WarpTargetName` → `Warping->FindWarpTarget(Name)`.
Ordering already works: the BT registers "AttackTarget" BEFORE `TryActivateAbilityByClass`
(`AZ_BTTask_ZombieAttack.cpp:201` before `:206`); the hero GA registers "MeleeTarget" before the gate.
This keeps the hero fix intact: a windowless jab still skips (no pin); a jab whose warp found no victim
still skips (target removed); a window WITH a live target drives — and then the warp really does
transport, so the pin is no longer pure loss. Count rotation-only windows too (a rooted, rotating swing
is the designed behaviour).
Long-term home: a `bDriveCapsule` field on `FAZ_CombatMontage` — authored beats inferred (arch step B).

**Ship WITH it — the BT warp clamp** (`AZ_BTTask_ZombieAttack.cpp:405-407`). Registers a FIXED
`FVector(WarpApproachDistance,0,0)` with no clamp at all.

★★ **THE CLAMP IS `Min`, NOT `Max`** — I shipped `Max` in the hero (`06336b4`) and it was wrong in BOTH
directions; corrected in a later commit, and the same mistake must not be repeated here.
`LocationOffset.X` is the DESTINATION GAP, not a distance to travel: the engine builds the point as
`Target + normalize(Owner - Target) * X` (engine `RootMotionModifier.cpp:194-196`), so it lands X cm
from the TARGET on the attacker's side, and the attacker moves from its current gap TO X. Bigger X =
further from the target = backwards.
  gap 200, standoff 100 → `Max`=200 = the attacker's own position ⇒ the lunge silently never happens
                        → `Min`=100 ⇒ approaches correctly
  gap  60, standoff 100 → `Max`=100 = 40cm BEHIND ⇒ moonwalk
                        → `Min`= 60 = hold position ⇒ correct
This is why the hero's heavy punch "played but went nowhere" — misdiagnosed at the time as the 1.70s
commitment window. Use planar centre distance; keep the last valid direction inside a near-zero dead
zone.

**Masked on the zombie today** (no drive ⇒ no movement); goes LIVE the moment the gate fix lands, so
ship both together or trade a statue zombie for a moonwalking one.

**Do NOT naively re-clamp every tick** (second reviewer, and it overrides my earlier note): a follow
target already moves each frame, and SkewWarp interpolates from a LATCHED start toward that endpoint —
rewriting the endpoint from the attacker's current position can put it behind the generated trajectory
and produce braking, oscillation or backward correction. Preferred order: (1) latch a non-retreating
translation destination and handle rotation separately; (2) stop translation inside the stand-off
tolerance while rotation continues via its own modifier/fallback; (3) if the endpoint must move, make it
monotonic, add hysteresis, cap endpoint velocity, never allow it behind the attacker; (4) longer term,
separate translation and rotation target names.

**★ DO NOT retune WarpApproachDistance.** It is **100** at HEAD (`AZ_BTTask_ZombieAttack.h:95`,
commit `54d2da0`) — I twice mis-remembered it as 90. At 100 with knuckle reach 87: depth =
87 − (100 − 30) = **17cm**, already at the hero's ~18cm parity target. The proposed 99 was a 1cm
change, below measurement noise. The real asymmetry is per-hand (L-claw ~67 vs R-claw 87-93 across
anim-set variants) — if it offends, that belongs in the per-clip descriptor, not another global.

**The user's "claw goes inside the hero" report cannot currently be caused by the stand-off**, because
the warp moves nothing. Likely cause: the zombie stops at point-blank via nav and swings unwarped —
at a 60cm gap an 87cm knuckle passes clean through a 30cm capsule. FIX THE REGRESSION, THEN MEASURE
with the `[MeleeHit]` contact logs, THEN tune. (Seam-trace rule: three wrong diagnoses in one earlier
session came from reach arithmetic instead of instrumenting.)

## ★ TASK 2 (P1) — replace the hand-rolled stagger hold with a duration GameplayEffect

**There is a LIVE bug in what shipped at `753236d`.** `SetStaggeredFor`
(`AZ_PawnMoverInfectedCharacter.cpp`) guards its OWN clear with `IsStaggerReactionAbilityActive()`, but
`UAZ_GA_HitReact::EndAbility` zeroes the tag **unconditionally** (`AZ_GA_HitReact.cpp:246-249`) and
never checks `StaggerHoldEndTime`. Sequence: grab-shove holds stagger for section+1s → hero punches the
reeling Chalkie 0.5s in → HitReact ends ~1.9s later → tag forced to 0 while the shove's deadline is
still in the future; the pawn's pending timer only clears, never re-asserts. The stagger is cut short —
the counted-tag trap in the direction the header didn't cover.

**Shape (Fable + second reviewer):** hybrid ownership, NOT fixed-duration-only. A pure fixed duration
regresses under hit-stop/play-rate changes, interrupts, early beats and watchdog paths:
- **Shove**: apply a finite-duration GE, let it expire.
- **Hit reaction**: apply the SAME GE with a watchdog duration, keep its `FActiveGameplayEffectHandle`,
  and remove THAT handle in `EndAbility` — preserves "events drive, durations guard".
- **Death**: explicitly remove remaining stagger effects (give the GE an identifying asset tag so this
  cannot catch unrelated effects).
- On retrigger, establish the new contribution BEFORE retiring the old handle: a 1→0→1 tag transition
  fires GAS tag delegates and the BT/blackboard mirror synchronously.

**Base shape:**
- ONE `GE_Staggered`: duration = `SetByCaller` (`SetByCaller.StaggerDuration`), grants
  `State.Combat.Staggered`. One class, not per-tier — callers already compute the float
  (`Desc.ResolveStaggerHold()`, `SectionLength + PostShoveStaggerSeconds`).
- **Stacking: `None`** — independent active effects, tag up until the LAST expires = union of
  intervals = extend-only semantics for free. Explicitly NOT `AggregateByTarget` + refresh-duration:
  refresh is latest-wins and CAN shorten, the exact failure `SetStaggeredFor` existed to prevent.
- Deletes: `StaggerHoldTimer`, `StaggerHoldEndTime`, `IsStaggerReactionAbilityActive`, both
  `SetLooseGameplayTagCount` sites in HitReact — and the wipe bug with them.

**Consumers verified unaffected:** `IsStaggerReactionPlaying` is a bare `HasMatchingGameplayTag`
(source-agnostic); the AI controller's `bStaggered` mirror uses `RegisterGameplayTagEvent`, which
GE-granted tags fire identically.
**One hygiene line needed:** `GA_Death` (`CancelAbilities`) currently drops the tag instantly; a GE
would outlive death → add `RemoveActiveEffectsWithGrantedTags(Staggered)` there.
**Drift is why the hybrid above is mandatory, not optional:** today the tag tracks the ability's ACTUAL
lifetime (BeatEnd/blend/recover/watchdog); a fixed-duration GE tracks a PREDICTED hold and would drift
under hit-stop, play-rate changes, interrupts and watchdog paths. Hence: reaction keeps its handle and
removes it on end; only the shove is allowed to be duration-only.
**Replication is a strict improvement:** infected ASC is `Minimal`, hero `Mixed`; GE-granted tags
replicate to sim proxies through the minimal-replication tag container, `SetLooseGameplayTagCount`
replicates to no one. No Mover hazard — Staggered is never read on the sim path (only crouch is bridged
into the InputCmd). Verify the minimal-rep tag path under **Iris** when co-op lands.

## ★ TASK 3 (user-requested 2026-08-04) — the escape anims don't read face-to-face

**Symptom (user, in PIE):** during the grab ESCAPE the hero turns away instead of staying face to face
with the Chalkie. User's own framing: *"it should be like catch"* (the Catch section reads correctly),
*"this can be RM in this case, I think we can wait the end of synced anims"* — i.e. accept root-motion
driven alignment and do not hand control back until BOTH paired montages finish.

**★ DISPROVEN 2026-08-04 — `GrabHoldDistance = 0` IS WRONG. DO NOT RETRY IT.** Tested in PIE: at 0 the
hero and Chalkie stand **BACK TO BACK**; at the hand-tuned **92** the catch is, in the user's words,
"perfect". The old suspicion below was built on a comment that is itself wrong, and both that comment
(`AZ_GA_ChalkieGrab.cpp:157`) and the property doc have been corrected in code.
WHY 0 fails: the shared-origin argument only holds if the two actors share a ROTATION as well as a
position, and this feature deliberately opposes them ~180 degrees (the hero look-ats the grabber; the
shared-yaw variant was already tried 2026-08-01 and read as "same line, not face to face"). Each NAAT
clip bakes its body ~30cm IN FRONT of its actor origin (pelvises ~29.6cm apart; the mesh's -90 yaw maps
anim +Y to actor forward), so two OPPOSED actors at one origin push their bodies along opposite facings
— apart, backs together. ~2x that offset puts each body in front of the other, which is why 92 is
correct and is not a fudge factor. The escape's face-to-face problem, if any remains, is NOT this value.

**Second factor — the separation is entirely one-sided.** Measured: hero escape clips travel **0.0cm**,
Chalkie's travel **39.6cm**. If the pair starts misaligned, only one body corrects, so the error grows
across the section instead of cancelling.

**Facing mechanism (traced, looks correct — rule it out rather than assume):**
`GA_PlayerGrabbed:141` `Hero->SetGrabFacingTarget(Grabber)`, cleared at `:547` in EndAbility; the hero's
`ProduceInput` aims OrientationIntent at that target (`AZ_PawnMoverHeroCharacter.cpp:315-325`), and
`AZ_MoverAnimInstance.cpp:79` reads it too. Since the ability ends only when the outcome section ends,
the override SHOULD survive the escape — so if the hero still turns, the cause is the paired pose /
hold distance, not the facing override. Possible third factor: the Mover's rotation RATE lagging a
fast paired rotation, or the clip's own baked rotation fighting OrientationIntent.

**Plan:** (1) read the live `GrabHoldDistance`; test at 0 and compare; (2) if 0 fixes alignment, decide
whether 92 was tuning that should move into the clip instead; (3) consider driving the hero side with RM
too (its clips carry none today) so both bodies are animation-driven through the section rather than one
being posed and one transported; (4) hold control until both synced montages complete, per the user's
call. Check `VerifyPairedMontages` still passes and that `MontageSync_Follow` is actually mirroring
during the outcome section, not just the Wrestle loop.

## Bonus finding — the GAS cooldown surface is DEAD; delete, don't wire
`CooldownGameplayEffectClass` is assigned nowhere in `Source/AZ`; the only reference is a read in
`UAZ_GameplayAbility::GetCooldown`, itself UI plumbing for `FAZ_AbilityInfo::CooldownTag`. No C++
ability puts `Ability.Cooldown.*` in `ActivationBlockedTags`; the six tags exist only as declarations.
`BP_GA_Shoot` carries `ActivationBlockedTags = Ability.Cooldown.Shoot` — inert, since nothing grants it.
Fire rate is already owned by `UAZ_GA_Shoot::TimeBetweenShots`; `GA_Interact` rolled its own timer.
Removal list: the six tags, `GetCooldown`, `FAZ_AbilityInfo::CooldownTag`, the BP blocked-tags entry.
If a real cooldown ever appears (grenade/dash) the Task-2 GE pattern is the template — and
`AZ_InfectedAIController.h:168` already earmarks the 45s GRAB cooldown for that upgrade.

## Order
1. Task 1 gate fix — blocks all NPC combat observation.
2. BT clamp (`Min`, latched — NOT a naive per-tick re-clamp) — same session, same PIE pass, since the
   bug is masked today and goes live with step 1.
3. PIE MEASURE before any stand-off tuning. Expect "no change needed".
4. Task 3 escape face-to-face — start by reading the live `GrabHoldDistance` (comment says 0, BP says
   92); it is self-contained and needs an editor session anyway.
5. Task 2 GE refactor — behaviour-neutral in the good case, deletes the live wipe bug.
6. Cooldown deletion — hygiene, needs an editor session for the BP side.

## Method note (cost me twice this session)
Two of three premises I handed the reviewer carried stale numbers (stand-off 90 vs live 100; wrist-era
reach vs the header's own knuckle-era comment). Both would have sent the next session tuning a value
that was already correct. **The live source and committed comments are ahead of any summary — read the
file.**

Related: [[project_combat_arch_refactor]], [[project_motion_warping]], [[project_grab_grapple_design]],
[[feedback_seam_trace_before_pie]]
