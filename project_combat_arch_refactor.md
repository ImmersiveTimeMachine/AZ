---
name: project-combat-arch-refactor
description: "Combat stability refactor (2026-08-03) — DriveRootMotion generations, State.Combat.Staggered gate, Fable-reviewed plan C→A′→B→A with backlog"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-03T04:58:47.546Z
---

# Combat architecture refactor — plan + state (started 2026-08-03, branch `feature/NPC`)

User mandate: "all should work and be scalable, I need a working solution for all cases" after an evening
where reactions were unpredictable. Diagnosis (Fable-review-confirmed): (1) animation playback used as the
state machine (`IsStaggerReactionPlaying` = `Montage_IsPlaying`), (2) the RM bridge hand-rolled 7×, (3) four
ad-hoc "clip longer than the beat" knobs (`BiteSeconds`/`RootMotionSeconds`/`FlinchRootMotionSeconds`/`HoldSeconds`).

## Order (Fable-amended, user-approved): C(+release) → A′ → B → A. D = user's ABP work, parallel.

- **C DONE** — `UAZ_PawnMoverComponent::DriveRootMotion(Seconds) -> uint64 generation` +
  `ReleaseRootMotion(Gen)` (no-op if superseded). Rule: **starts AND stops go through the component;
  a raw `CancelFeaturesWithTag(Mover_AnimRootMotion)` kills WHOEVER'S move is live** — GA_MeleeAttack's
  EndAbility was correct only by ordering luck. 3 sites deliberately NOT converted (different patterns):
  AZ_HeroPawn permanent (-1), AZ_MoverAnimInstance per-frame remainder, FLayeredMove_AZ_GrabAnchor.
  ZombieMelee collapse: `RootMotionSeconds = BiteSeconds` before Super — knob deleted.
- **A′ DONE** — `State.Combat.Staggered` loose tag; pawn `SetStaggeredFor(Seconds)` uses
  `SetLooseGameplayTagCount(1/0)` (loose tags are COUNTED — Add/Remove under re-trigger leaks a
  permanent gate), member timer = last-writer-wins across flinch + step-back.
  `IsStaggerReactionPlaying()` = pure tag query; BT call site unchanged. Gate default ≈1.9s
  (beat 1.5 + 0.4 blend) — DELIBERATE pacing change from the accidental 3.1-4.0s.
- **B DONE** — `FAZ_CombatMontage {Montage, ActiveSeconds, RootMotionSeconds(0=beat), BlendOutTime}`,
  semantics ONLY in its Resolve* helpers (`Animation/AZ_CombatMontage.h`). Read via
  `UAZ_GA_MeleeAttack::FindAnimSetCombatMontage` (FStructProperty + exact StaticStruct check; false ⇒
  legacy fallback `HitReactMontage` + pawn `FlinchRootMotionSeconds`, so un-authored variants keep pre-B
  behaviour). BP variable "HitReact" added to BP_ChalkieAnimSet from Python — EdGraphPinType has NO
  settable Python properties; craft it via `pt.import_text('(PinCategory="struct",PinSubCategoryObject=
  "/Script/CoreUObject.ScriptStruct\'/Script/AZ.AZ_CombatMontage\'",...)')` then
  `BlueprintEditorLibrary.add_member_variable`. Authored values (measured peak + ~0.15 settle):
  | DA | clip | peak | ActiveSeconds |
  | A_Standard | KB_Chase_2 | 118cm @1.34s (hold→1.81) | 1.50 |
  | B_Runner   | KB_Chase_5 |  85cm @1.75s (hold→2.15) | 1.90 |
  | C_Rotter   | KB_Chase_2 | 118cm @1.34s | 1.50 |
  | D_Sprinter | KB_Chase_3 | 146cm @1.37s (hold→1.72) | 1.50 |
  (Chase_3 travels FURTHEST yet peaks earliest — per-clip data, not a shared knob.)
  `BiteSeconds`/`HoldSeconds` stay caller beats (descriptor = per-clip default + ceiling).
- **A DONE (code)** — `UAZ_GA_HitReact`: two GameplayEvent triggers (`Event.Combat.HitReact` from
  HandleDamaged, `Event.Combat.StepBack` from the horde shim; payload = causer / montage+caller-beat),
  InstancedPerActor+retrigger, ActivationOwnedTags=`Staggered`, ActivationBlockedTags=`Grabbing` (the
  grab armor — R2 lock/scream/melee-cancel stay in HandleDamaged and fire always). **Beat clock = the
  `Event.Combat.BeatEnd` notify authored ON the montage** (events drive); BeatCutTimer only when the
  notify doesn't own the beat; watchdog gate+0.5 (timers guard). OnBlendOut deliberately unbound so the
  gate spans beat+blend (ends on OnCompleted/Interrupted/Cancelled). Bite converted too (full
  consistency, user call): GA_MeleeAttack ends on BeatEnd + FindBeatEndNotifyTime + BeatWatchdog;
  **GA_ZombieMelee gutted — BiteSeconds/timer/GBiteGenerations DELETED**. A′ loose-tag plumbing deleted.
  ★ Event trigger is SYNCHRONOUS (`Event_Death` precedent) — used for payload/decoupling, not safety.
  REMAINING: data pass (BeatEnd notifies: KB_Chase_2@1.50, _3@1.50, _5@1.90 must EQUAL descriptor
  ActiveSeconds; Atk_L/R@2.2 bite), PIE matrix incl. rate-scale smoke test, strip [MeleeHit] diag,
  COMMIT (nothing committed since 9d8d9e4).

- **E DONE — socket-swept hit detection** (user: "we need well-known hit detection"). `UAZ_AT_MeleeSweep`
  rewritten: ticking prev→current sweep of the strike socket (hand_l/r, fist 12cm / claws 15cm) between
  `Event.Montage.Melee.WindowBegin/End` notifies; all filters in the task (self/once-per-swing/team/
  corpse-before-single-target-consume); state on the TASK instance (notify-state objects are shared —
  classic trap). **Contact timing is physics now — zero authored contact frames in the project.** The
  old single-frame `Melee.Hit` event, the actor-forward volume, and the cos(55°) cone are deleted
  (`Melee.Hit` tag kept registered for old data; `RemoveGameplayEventNotifies` strips markers).
  Windows: jabs 0.12-0.50, Move 0.15-0.55, Heavy 0.90-1.95, claws 0.85-1.45. `MeleeRange/Radius` now
  ONLY size FindWarpTarget's search. WarpApproachDistance(hero)=105 — fists must physically reach.
  Heavy-punch epilogue: contact measured at 1.64-1.69s via `AnimPoseExtensions` world-space API (the
  root-motion-derived 1.0 guess was mid-lunge, fist still cocked) — clip is a movement transition, not
  an attack; socket sweep makes its timing self-correct anyway.

## ★★ SESSION END 2026-08-03 — RESUME HERE: socket sweep NOT WORKING in PIE

User's last report: **"not working, the problem can be the BT also"** — melee damage is currently DEAD
(the old volume-sweep path was deleted; the new socket-sweep path is unverified). Exact symptom NOT
captured (hero, zombie, or both — establish that first). User suspects the BT may be involved.

**Debug plan (instrument, don't theorize — the standing lesson):** add ONE log per link and punch once:
1. `[MeleeHit]`-style line in OnMontageEvent for WindowBegin/WindowEnd arrival (did the events reach the
   ability at all?) — if absent: EventTags/delegate wiring or notify didn't fire.
2. Log in StartHitWindow (authority? SweepTask created? which socket?).
3. Log in TickTask first ticks: socket world position + sweep segment length (is the fist where we think?).
4. `OnSweepHit` already logs. Whiff vs no-detection distinguished by 3.
Candidate causes to check IN ORDER: WindowBegin events not in the montage task's EventTags container at
runtime (BP ability children? predicted client vs authority on hero); hero punch = predicted activation
→ StartHitWindow's HasAuthority guard vs standalone (should be authority in SP); zombie side: BT
break-off/cancel ending the ability before 0.85s window opens (user's BT hypothesis — check
[ChalkieDiag] + whether claws even reach WindowBegin); sweep radius 12cm vs actual fist-to-capsule gap
at WarpApproachDistance 105 (zombie BT still parks at 120!  claw arm may not reach — check zombie
whiffs separately; BT WarpApproachDistance default 120 unchanged).

**COMMIT REMINDER: nothing committed since `9d8d9e4` — two days of work (warping + full refactor +
socket sweep) in working tree only. Commit early next session, even as WIP.**

## Key mechanism facts (hard-won)

- OverrideAll freeze mechanism: a live RM move past its montage applies the **~zero root delta that
  RootMotionFromEverything keeps writing from in-place loco clips** — intermittent, depends on what else
  plays. (Not "the move proposes zero" — GenerateMove returns false when the attribute is absent.)
- Layered-move expiry default = `MaintainLastRootMotionVelocity`: the pawn inherits the clip's velocity
  at the cut (~100+ cm/s). Current knockback tuning has this residual BAKED IN — expose FinishVelocityMode
  at B or descriptor numbers won't mean what they say.
- Horde step-back falls back to `HitReactMontage` when no `GrabStepBackMontage` (AZ_HordeSubsystem ~320),
  so the two stagger sources can share a clip — another reason one tag covers both.

## Fable backlog (valid findings, deferred)

GBiteGenerations static map never erases (→ batch #9) · AoE scream O(N×M) perception churn before AoE
weapons exist · BeginCorpse re-derives duration as `RagdollDelay / 0.6f` (fold into B) · melee-mid-jump
unblocked (one ActivationBlockedTags away) · AZ_HeroPawn's permanent bridge dies if a BP ever swaps the
component class to UAZ_PawnMoverComponent (single punch's EndAbility would... now mitigated by
generation-release, but comment it).

Related: [[project-motion-warping]], [[project-chalkie-fight-rules]]
