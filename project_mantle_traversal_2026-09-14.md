---
name: project_mantle_traversal_2026-09-14
description: "★★★ RESUME HERE for traversal. Standing mantle SHIPPED and user-approved 2026-09-14. Walk/run built but REJECTED in PIE (plays the whole clip -> overshoots past the ledge, and the camera moves oddly). USER DIRECTION: for walk/run play only the START of the clip then hand back to the locomotion loop, the same start->loop pattern used everywhere else. Holds the measured clip data, what is wired, and what is still unbuilt."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T05:04:23.099Z
---

# Contextual mantle (GASP traversal on the MetaHuman Mover hero) — 2026-09-14

First delivery of [[project_traversal_system]]'s trace → select → warped-RM-action pattern. Level `L_001`
carries 13 authored `LevelBlock_Traversable` actors (seven at exactly 100 cm — a deliberate test course).

## Status
- **STANDING MANTLE: WORKS.** User-confirmed "it works" after the collision-exemption fix below.
- **WALK / RUN: built, content + code complete, REJECTED on first PIE.** Two faults reported:
  1. the **whole retargeted clip plays**, so the character climbs and then keeps travelling — the walk/run
     clips are authored to carry on past the lip (root ends +228…+739 cm vs stand's 0);
  2. **the camera moves oddly** during the action (cause not yet investigated).

## ★ USER DIRECTION for next session (2026-09-14)
★★★ **The full 13-point work order is [[project_mantle_handoff_spec]] — read that first; it supersedes the
sketch below and corrects several of the assistant's proposed answers** (notably: root-at-ledge-height is
NOT a sufficient cut point, the handoff must be event-driven AND pose/foot-phase matched against the
OUTGOING mantle pose, and successful handoff must be separated from cancellation in the ability).

> "first implementation was good maybe we just need to use only start than move back to loop like we do
> this all other cases on walk and run"

So: for walk and run, **do not play the montage to completion**. Play only the ENTRY/contact portion and
then hand back to the normal locomotion loop — mirroring the project's existing start→loop transition
pattern (the same shape as WalkFwdStart → loop, and the jump's takeoff → land handoff). The standing clips
settle on their own and should keep playing in full; only the moving families need the cut.
Practical hooks: each copy still carries GASP's `BP_NotifyState_TraversalBlendOut`, and the measured hand-
contact times give a natural cut point. Also re-check the camera during the action once the cut is in —
it may simply be following the overshoot.

## What is built
| Piece | Where |
|---|---|
| Detection + candidate | `UAZ_TraversalComponent` (`Source/AZ/.../Character/`) — no tick, queried once per Jump press; reads the authored `Ledge_1..4` **USplineComponents** directly (no BP interop), provider boundary kept for generic geometry later |
| Execution | `UAZ_GA_Mantle` — warp target → `Traversing` mode → FullBody montage @1.0 → generation-scoped `DriveRootMotion`; one idempotent cleanup path + watchdog |
| Mover mode | a SECOND `UAZ_PawnMovementMode_RMAction` instance registered as **`Traversing`** with `bHandOffToFallingAtApex=false` (`AZ_PawnMoverComponent::OnRegister`). The jump's own RMAction defaults untouched. `AZ_MoverAnimInstance.cpp` already discriminated the two by that flag |
| Anim spine | `Traversing` mapped to `EAZ_MovementMode::Traversing`; SM branch **before** the air phase returns `IdleLoop` and clears the latches (stops a Land2Walk being manufactured at the top); two broad `CancelFeaturesWithTag(Mover_AnimRootMotion)` sites guarded |
| Input | arbitration in `UAZ_GA_PawnJump::ActivateAbility` **above** `SetJumpPressed` |
| Tag | `State.Traversing` |
| Content | 12 gameplay-owned montages in `/Game/AZ/Blueprints/Animation/MHC/Traversal/AM_AZ_Mantle_1_0_{Stand,Walk,Run}_{Relaxed,Neutral}_{L,R}` |

Wired on `AZ_BP_PawnMoverHero_MHC` (the pawn `BP_AZ_GameMode_MHC` spawns in `L_001`): 12-entry `Clips`
array, 3 `ApproachBands`, `AZ_GA_Mantle` in `StartupAbilities`. **Scripted CDO writes need a manual
Compile+Save in the BP editor** — never compile that BP from Python.

## ★ The bug that cost the first PIE run — and the rule it produced
`UAZ_PawnMovementMode_RMAction::SimulationTick` **deliberately does NOT slide on a blocking hit**
(`AZ_PawnMovementMode_RMAction.cpp:87-95`, "the capsule simply stops against the surface; remaining root
motion is absorbed"). Correct for a jump-in-place; fatal for a mantle, whose first proposed move is
diagonally INTO the face it is climbing. Symptom: **animation perfect, capsule never rises**, with
`LogMover` "RootMotionAttr … has excessive speed" escalating (2662 → 4676) as warping chased a target the
pinned capsule never approached. That warning is diagnostic ONLY (inside `#if !UE_BUILD_SHIPPING`, clamps
nothing) and also fires at spawn every run — a red herring on its own.

**Fix (GASP does the same):** scope a collision exemption to the ONE validated primitive for the duration —
`Capsule->IgnoreComponentWhenMoving(Target, true)` — and **restore it BEFORE anything queues Walking**,
because the surface the capsule must then stand on is the component it was passing through. Never disable
the whole capsule.

## Measured clip data (do not re-derive; and do NOT trust filenames)
- **Warp anchor differs per approach.** Stand `(0,-50,100)`, **walk `(0,0,100)`, run `(0,-60,100)`** — the
  component-space `attach` transform, constant within a family. Baked as `Static` warp points on our copies
  (the MetaHuman skeleton has no `attach` bone; see [[feedback_motionwarping_warppoint_provider]] for why a
  missing bone is *worse* than a broken window).
- **Authored approach distance**: stand 51–67 cm from the lip, walk 200–266, run 318–341. Drove the bands
  (110 / 290 / 430) and reaches (120 / 300 / 440).
- **★ The foot suffix means OPPOSITE things in the two families.** On `stand_F_Rfoot` the RIGHT foot is
  planted; on `walk_F_Lfoot` / `run_F_Lfoot` the LEFT foot LIFTS first so the RIGHT is planted. Our copies
  are renamed by the MEASURED planted foot so all twelve are uniform, and selection reads the live
  `UAZ_MoverAnimInstance::IsLeftFootDown()` rather than any name.
- **Two GASP montages point at the wrong sequence** (stand Relaxed Lfoot/Rfoot, and walk Relaxed Rfoot) —
  an upstream authoring bug, with the correctly-named sequences existing but orphaned. Routed around by
  duplicating whichever montage already had the right window family and repointing the sequence; no GASP
  asset was edited.
- Root motion retargeted faithfully: source vs retargeted root differ < 1 cm at every sample.

## Still unbuilt (deliberately, per the "real classes, minimal path" scope the user chose)
1. **Armed entry (§4b)** — user chose auto-holster → mantle → redraw the same weapon relaxed, via a new
   temporary-carry phase in the equipment transaction. Today a mantle with a rifle in hand plays the
   two-handed clip with the gun attached.
2. **Reaction coordination (§4a)** — partial. A committed flinch refuses the press (user's choice), and the
   SM branch clears `ReactionEndTime` while traversing. Not done: clearing stale reaction latches at entry.
   Matters more at walk/run speed, where bumping the ledge first can pre-empt the mantle.
3. The locomotion handoff above — now the top priority.
4. Nothing committed yet. Source + the 12 montages are uncommitted work.

Plan of record: `docs/design-briefs/mantle-relaxed-neutral-plan.md` (its §0 level audit and §1 asset table
are accurate; its "stand first" scope call and its claim that the warp windows are merely "missing a
reference" are superseded by the measurements above).
Related: [[project_traversal_system]], [[feedback_motionwarping_warppoint_provider]],
[[project_obstacle_reaction_system]], [[project_jump_system_status]].
