---
name: AZ Mover root motion = three-part chain
description: For AnimSequence root motion to drive the Mover capsule (not just montages), three things must coexist — AnimationWarping plugin, RootMotionMode=RootMotionFromEverything on the AnimInstance, and FLayeredMove_RootMotionAttribute queued on the CharacterMoverComponent
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
The Mover plugin does **not** consume `UAnimInstance::RootMotionMode` directly. It reads a per-frame `"RootMotionDelta"` custom mesh attribute (`UE::Anim::FAttributeId{"RootMotionDelta", 0}`) populated by `IAnimRootMotionProvider`. So three independent pieces must all be in place — drop any one and the visible symptom is different but always wrong.

## The chain

| # | Piece | Where it lives | Symptom if missing |
|---|---|---|---|
| 1 | `AnimationWarping` plugin enabled | `AZ.uproject` | Provider never registered → attribute never written → capsule slides AND mesh stays put |
| 2 | `RootMotionMode = RootMotionFromEverything` on the AnimInstance | `UAZ_AnimInstance::NativeInitializeAnimation` (also redundantly in `AZ_ABP_Mover`/`AZ_ABP_HeroPawn` CDO) | Asset player won't zero the root bone in the pose → mesh visibly drifts ahead of capsule = **double translation** |
| 3 | `FLayeredMove_RootMotionAttribute` queued on the MoverComponent | `AAZ_HeroPawn::BeginPlay` — `CharacterMoverComponent->QueueLayeredMove(MakeShared<FLayeredMove_RootMotionAttribute>())`. `DurationMs = -1` in its ctor so queueing once is permanent | Capsule decel runs alone with no RM source → **slide / pull-back** |

Engine source ref: `Engine/Plugins/Experimental/Mover/Source/Mover/Private/DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.cpp` lines 41–79 — proves Mover reads from the mesh attribute, not from `AnimInstance::RootMotionMode`. `Engine/Source/Runtime/Engine/Public/Animation/AnimRootMotionProvider.h` defines the modular feature interface.

## Why this was confusing

**Why:** The earlier session believed `RootMotionMode = RootMotionFromEverything` alone fixed the slide. That was wrong — what was actually applying RM was the manual `AnimSequence::ExtractRootMotion` block (Variant B v3) still active inside `UAZ_SmoothWalkingMode::GenerateWalkMove_Implementation`. When that ~45-line block was removed during cleanup (commit 9f09bef on `feature/rootmotion`), nothing was left bridging anim RM to capsule, so the slide returned the next session.

**How to apply:**
- When porting any new pawn from a CMC-based character to Mover, the `FLayeredMove_RootMotionAttribute::QueueLayeredMove` line is mandatory. CMC consumes RM internally; Mover does not.
- Don't try to extract RM manually inside a movement mode's `GenerateWalkMove_Implementation` (Variant B v1/v2/v3 all had time/playrate-sync or coordinate-space issues). The attribute pipeline is the engine-supported path.
- Don't change `RootMotionMode` per-state. It's set once at init and stays on; the asset player decides per-frame whether to extract based on the asset (loops have zero authored RM anyway).
- If a future clip (TIP, traversal) has authored RM you don't want applied, fix it on the asset (untick "Force Root Lock" / strip RM track) — don't toggle the global mode.
- `StoppingDeceleration` in `UAZ_SmoothWalkingMode` (currently 6000) is still consulted for the brief window before the stop anim starts and for stops with no chooser-picked stop clip; safe to leave high.
- `AZ_ABP_Hero` and `AZ_ABP_MoverAnimInstance` CDO still show `RootMotionFromMontagesOnly` (engine default). Doesn't matter while the pawn mesh uses `AZ_ABP_Mover`. If they're ever assigned, either set their CDO or make them inherit from `UAZ_AnimInstance` (currently `AZ_ABP_MoverAnimInstance` derives from the empty `UAZ_MoverAnimInstance` stub).

## Design intent — the system is a HYBRID, not pure root motion (clarified 2026-05-26)

Recurring confusion (asked again 2026-05-26): "didn't we aim for a pure-root-motion setup?" **No.** The intended and implemented design is:
- **Locomotion loops (walk/run/sprint): velocity-driven** by `UAZ_PawnMovementMode_Walking` (sets `MaxSpeedOverride`/`Acceleration`/`Deceleration`/`TurningStrength` per gait, then `Super::GenerateWalkMove`). Loop clips are in-place (≈zero authored RM), so the always-on RM bridge extracts nothing → velocity dominates. This is verbatim GASP parity (`gasp_character_movement.md` BP_MovementMode_Walking; GASP's `DDCVar.AttributeBasedRootMotion.Enable = false` by default).
- **Transitions (stops/starts/pivots/turns/traversal): root-motion-driven** via the bridge above. These clips carry real authored RM; it drives the capsule for that window.

The `project_v2_architecture.md:15` parenthetical "root motion drives capsule" is a loose one-line summary of GASP's anim pipeline — it means the RM *capability*, applied where clips have RM, NOT "every locomotion frame is RM-driven." The **branch name `feature/rootmotion` = the work of building the RM bridge** (Mover doesn't consume `RootMotionMode` out of the box), not a goal of making loops RM-driven. Data flow for loops is **physics → anim** (capsule velocity leads; AnimInstance reads `GetVelocity()` and the anim follows), the inverse of pure-RM.

## ⚠ The OverrideAll trap — do NOT queue the RM layered move permanently alongside velocity-driven loops (found 2026-05-26)

`FLayeredMove_RootMotionAttribute` has **`MixMode = EMoveMixMode::OverrideAll`** (ctor, `RootMotionAttributeLayeredMove.cpp:47`) and contributes its move **whenever the `RootMotionDelta` mesh attribute is present** (`:79`/`:132`) — NOT only when there's meaningful RM. And `RootMotionMode = RootMotionFromEverything` makes the AnimInstance write that attribute **every frame for every clip**, including in-place idle/walk loops (≈zero delta).

Net effect: queuing it permanently (`DurationMs=-1`) **OverrideAll-zeroes the capsule velocity every frame during loops** → a velocity-driven walking mode can't even start moving (press W from idle → walking mode builds velocity → layered move overrides it to ~0 → speed never crosses IdleSpeedThreshold → stuck in idle → **deadlock**). Empirically confirmed in v2: "as soon as we add `QueueLayeredMove(MakeShared<FLayeredMove_RootMotionAttribute>())`, W doesn't move."

This **corrects the earlier "loops have zero authored RM anyway" framing** below — zero RM under OverrideAll is *worse* than no contribution: it actively overrides velocity to zero. The two systems (velocity-driven loops + RM-attribute layered move) are NOT freely composable.

**Correct pattern for coexistence (use when building stops/starts):**
- The layered move early-outs (`return false`, no override) when the sync state has the **`Mover.SkipAnimRootMotion`** tag (`RootMotionAttributeLayeredMove.cpp:137-141` — engine uses it for jump/fall air control).
- So: queue the layered move, but **set `Mover.SkipAnimRootMotion` during idle/locomotion loops** (velocity drives) and **clear it during transition clips** (stops/starts/pivots — RM drives). OR queue the layered move with finite duration only for the transition window.
- v1 `AAZ_HeroPawn::BeginPlay:189` queues it permanently and "works" because v1's anim setup differs (likely RM-carrying loops and/or tag gating); do NOT blind-copy that line into a velocity-driven v2 pawn without the tag gate.

`RootMotionFromEverything` on the AnimInstance is safe to keep on its own (no consumer = no capsule effect; it just locks the mesh root so an RM clip won't drift). The danger is only the OverrideAll consumer.

## Diagnostic recipe (next time the slide returns)

1. Confirm pawn mesh `anim_class` — should be `AZ_ABP_Mover_C` (Python: walk pawn CDO `mesh.anim_class`).
2. Confirm CDO `RootMotionMode` — should be `ROOT_MOTION_FROM_EVERYTHING (2)` on whichever ABP the mesh uses.
3. Confirm `QueueLayeredMove(MakeShared<FLayeredMove_RootMotionAttribute>())` is still present in `AAZ_HeroPawn::BeginPlay` — it's a single line, easy to lose during refactor.
4. Console: `mover.debug.LogRootMotionAttrSteps 1` will log per-tick LocalT/WST when the layered move is firing. Silence = layered move never queued or attribute never written.
5. Console: `mover.debug.DisableRootMotionAttributes 1` toggles the consumer for A/B confirmation.
