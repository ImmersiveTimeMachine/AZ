---
name: AZ architecture rationale — why Mover and why Chooser+SM+BlendStack
description: The two foundational architecture decisions for the AZ project (Mover over CMC, Chooser+SM+BlendStack over classical SM+BlendSpace+Montage), with concrete trade-offs, costs already paid, and signals that would warrant reconsidering
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
Two foundational decisions made early in the GASP port that compound through every later system. Both are right for AZ's target (TLoU-style third-person, hybrid rotation, multiple weapons, MM-driven locomotion, traversals). Both have real ongoing costs documented honestly here so the question doesn't keep reopening.

---

## Decision 1 — Chooser + SM + BlendStack (instead of classical SM + BlendSpace + Montage)

The chooser is a **data-driven narrowing function**: given the current AnimInstance context (gait, stance, direction, speed, foot-down, weapon, tags), return the N candidate clips that match. The SM still owns the *logical* state (IdleLoop, LocomotionLoop, TransitionToIdle…). The chooser+BlendStack just decides *which actual asset* fills the slot this frame; optional MotionMatching picks the best continuity from the candidate array.

**Why we picked it:**

| Axis | Classical (SM + BlendSpace + Montage) | Chooser + SM + BlendStack |
|---|---|---|
| Add 1 anim variant | New SM state OR new blendspace dimension | New chooser row |
| Foot-down stop variants (L/R × walk/run/sprint = 6) | 6 SM states + 6 transition rules | 6 chooser rows, 0 SM changes |
| TIP variations (8 directions × stand/crouch) | 16 states or one giant blendspace with sliding feet | 16 rows picked by enum column |
| Per-weapon variants | Whole separate SM tree per weapon | Swap chooser child by `WeaponAnimIndex` |
| Scaling cost | Quadratic (state explosion) | Linear (rows) |
| Debug "why this clip?" | Visual SM arrow you can step | Trace chooser column values; less obvious |
| CPU | ~free | Tiny chooser eval per frame |
| Maturity | Battle-tested since UE3 | New (UE 5.3+); less community knowledge |

**How to apply:**
- Default new locomotion variants to chooser rows, not SM states. SM should stay in the 9-state range (the GASP shape: IdleLoop, IdleBreak, LocomotionLoop, TransitionToIdle, TransitionToLocomotion, InAirLoop, TransitionToInAir, SlideLoop, TransitionToSlide).
- For one-shot animations (gameplay-driven attacks, reactions, pickups, dialog) keep using **Montages on slots** — chooser+BlendStack is for state-driven locomotion, not for ability-driven scripted moments. The two coexist (BlendStack pose + slot-blended Montage on top is the GASP pattern).
- A new chooser column is cheaper than a new chooser tree. Prefer adding columns to the existing tree before splitting into a new tree (split only when one branch grows into hundreds of rows).
- See `reference_cht_chooser_structure.md` for the live structure of `CHT_AZ_CharacterAnimations` (the only referenced CHT today; `CHT_NoWeapon_Locomotion` and `CHT_AZ_CharacterAnimationsOld` are orphans).

**When to revisit:**
- If total locomotion clip count stays under ~30 for the life of the project AND you don't add weapon variants AND you don't add MM, the classical approach would be simpler. We're targeting >100 clips, multiple weapons, and MM, so this signal will not fire.
- If chooser eval ever shows up in profiles >0.2ms / pawn, revisit (precompiled choosers / nested split).

---

## Decision 2 — Mover (instead of `UCharacterMovementComponent`)

`AAZ_HeroPawn` is a `Pawn` (not `Character`). Movement is a `UCharacterMoverComponent` plus per-mode classes (`UAZ_SmoothWalkingMode`, `UAZ_FallingMode`).

**Why we picked it:**

| Axis | CharacterMovementComponent | Mover |
|---|---|---|
| Movement modes | Enum + virtual overrides on a 4000-line class | Each mode is its own `UMovementMode` subclass |
| Adding a mode (climb, vault, prone) | Subclass CMC, override switch, fight existing logic | New mode class, register, done |
| Per-frame modifiers (RM, push, knockback) | Hand-merged inside CMC overrides | Composable `LayeredMove`s |
| Animation-driven movement (root motion) | CMC consumes `RootMotionMode` internally | First-class via `RootMotionDelta` mesh-attribute pipeline + `FLayeredMove_RootMotionAttribute` (see `project_root_motion_mode.md`) |
| MM trajectory | Bolt-on `UCharacterTrajectoryComponent` | First-class `UMoverTrajectoryPredictor` |
| Networking | Server-authoritative + client prediction baked in | NetworkPrediction-based, fixed-tick async |
| Async physics | Hard | Built in |
| Coupling to character class | Tight (`ACharacter` only) | Decoupled (any `APawn`) |
| Maturity | 10+ years, huge community | Experimental (UE 5.4+); APIs still move; integration gaps |
| Documentation | Plenty | Sparse |

**Concrete wins already realized in AZ:**
1. `UAZ_SmoothWalkingMode` is a 180-line file with its own UPROPERTY tunables instead of a `if (CurrentMovementMode == X)` branch inside a 4000-line override.
2. Layered-move composition: queueing one shared pointer (`FLayeredMove_RootMotionAttribute`) wired up the entire RM pipeline without touching any movement mode.
3. `UMoverTrajectoryPredictor` feeds the MM node natively without a separate sync layer.
4. Pawn isn't stuck inheriting `ACharacter`'s assumptions (capsule semantics, AnimInstance assumptions, default mesh socket).

**Costs already paid (documenting so we don't forget):**
- Root motion → capsule chain has THREE pieces and zero engine docs: AnimationWarping plugin + `RootMotionMode = RootMotionFromEverything` + `FLayeredMove_RootMotionAttribute` queued. Discovered the hard way after the slide returned. See `project_root_motion_mode.md`.
- Re-implemented the JustLanded sticky brake by hand (CMC has it free).
- Re-implemented MovementDirection bucketing in `Get_MovementDirectionAndOffset`.
- Re-implemented FacingSmoothingTime / TurningStrength tuning per gait.

**How to apply:**
- New movement behavior → new `UMovementMode` subclass, not a branch inside an existing mode. New modifier (knockback, ledge nudge) → new `LayeredMove`, not an override.
- Don't try to manually extract root motion inside `GenerateWalkMove_Implementation` — three variants tried (B v1/v2/v3), all had time/playrate or coordinate-space issues. Use the engine attribute path.
- When a CMC-trained engineer joins, give them this doc and `project_root_motion_mode.md` first — Mover's mental model is different enough that pattern-matching from CMC produces wrong fixes (e.g., assuming `RootMotionMode = RootMotionFromEverything` alone moves the capsule).

**When to revisit:**
- If we ship before UE 5.8 or if Mover stops being upstream-maintained in some future UE release, we'd have to lock to a specific UE version or migrate. Low probability; Epic is investing.
- If async physics / fixed-tick networking turn out unnecessary AND every new mode we add fits CMC's enum cleanly AND we drop MM, the cost-vs-benefit flips. Unlikely given the scope.

---

## How these two decisions interact

The Chooser+SM+BlendStack pipeline assumes per-frame trajectory + state mirrors are available. Mover's `UMoverTrajectoryPredictor` is what populates them. Switching off Mover would also break the chooser routing path because the trajectory data wouldn't be there in the same shape. So the two decisions reinforce each other — they're effectively a single "GASP-class architecture" choice.
