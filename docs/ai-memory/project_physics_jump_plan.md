---
name: project_physics_jump_plan
description: "Next-session plan — pivot jumps from RM-driven (RMAction) to PHYSICS (launch + engine Falling mode + cosmetic anim + MM land). Fixes height divergence, speed pop, run-lands-to-stop with less code. RMAction kept for vault/mantle."
metadata: 
  node_type: memory
  type: project
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Physics Jump — implementation plan (decided 2026-06-02, for next session)

## Decision & why (read first)
Pivot jumps from RM-driven to **PHYSICS**: a launch impulse → engine **Falling mode** (gravity + air control + floor-contact landing) → **cosmetic** anim (start/loop/MM-land). RM no longer drives the capsule during a jump.

Forced by three problems pure-RM jumps can't solve (a baked clip assumes flat ground / one speed):
- **Height divergence** — jump from X, land where ground Y<X → capsule floats at X for the whole clip then drops (the user-found bug).
- **Speed pop** — `…_ALL` RM is baked at one speed; OverrideAll makes the capsule move at the clip's speed, not the player's.
- **Run `…_ALL` lands-to-stop** — the run one-shots are jump-to-stop; continuing hitches.

Physics fixes all three with LESS code (the engine's `UAZ_PawnMovementMode_Falling` already does gravity + floor + landing). "RM launches you, gravity flies you, the floor catches you, MM lands you." User agreed physics is better+simpler once the vertical must be physics anyway.

**`UAZ_PawnMovementMode_RMAction` is NOT wasted** — it moves to **vault/mantle/traversal** (contextual RM + AnimationWarping to *measured* geometry — no flat-ground assumption there). See [[project_jump_system_status]].

## Architecture (GASP model)
1. **Launch:** GA jump (gates) → apply a jump launch (vertical impulse for height; horizontal momentum carries from Walking). Likely `MoverComponent->SetHandleJump(true)` (engine applies the impulse + transitions to Falling) — currently `false`. Tune jump Z + gravity for CHALK weight.
2. **Air:** engine **`UAZ_PawnMovementMode_Falling`** (already exists; gravity, air control, terminal, landing→Walking on floor contact). Variable length.
3. **Anim (cosmetic, chooser/MM-selected, does NOT drive the capsule):** takeoff pose by `Gait × MovementDirection × bLeftFootDown`; air = fall loop; land = **MM** over land poses by predicted gait/foot/weight. Reuse the RTG_RM jump clips IN PLACE (RootMotionFromEverything extracts the root → mesh animates in place; just DON'T queue the layered move for jumps), or use the GASP `M_Neutral_Jump_*` cosmetic set at `/Game/Characters/UEFN_Mannequin/Animations/Jump/`.

## KEEP (reuse, minimal change)
- SM jump phases: `TransitionToInAir` (takeoff) → **`InAirLoop`** (dead enum value 4 — activate it; the fall) → land. Air phase governed by **MovementMode==Falling** (variable length), NOT `TransitionEndTime`/clip length — a real change from the current held-clip jump.
- Chooser jump rows + the `bIsMoving` column + foot/gait selection — output becomes cosmetic poses.
- MM-landing plan (`PSD_v2_JumpLandings` DB + `bUseMM=True` land row).
- **MP proxy mirror:** already maps Falling→InAir; just key the proxy air phase off MovementMode==Falling (like today's RMAction mirror in `UAZ_LocomotionStateMachine`). Extend the mirror from "RMAction" to the air mode.

## CHANGE (the work)
1. **Re-enable launch:** `SetHandleJump(true)` (ctor + BeginPlay, currently false in `AZ_PawnMoverHeroCharacter`). Remove the jump-edge → RMAction handoff. Tune jump height + gravity.
2. **Stop RM-driving the jump:** in `UAZ_MoverAnimInstance`, do NOT queue `FLayeredMove_RootMotionAttribute` for the air phase (gate the RM-move queue to NON-jump transitions — stops/starts/turns keep RM, jumps don't). Remove `QueueNextMode("RMAction")`/`("Walking")` jump handoff — let the engine jump→Falling transition own the mode.
3. **SM phases** (in `UAZ_LocomotionStateMachine::ComputeNextState`): `TransitionToInAir` on edge (brief takeoff) → `InAirLoop` while MovementMode==Falling → land on Falling→Walking (floor contact): moving → MM moving-land; standing → idle-land. Replace the "hold TransitionToInAir for clip length" with MovementMode-driven air.
4. **Chooser rows:** replace the RM `…_ALL` one-shot jump rows with cosmetic takeoff (TransitionToInAir × Gait × foot) + `InAirLoop` fall-loop + MM land row. (The MVP RM rows are repurposed, not wasted — they proved the plumbing.)
5. **MM landing:** build `PSD_v2_JumpLandings` (Land2Run/Walk/Stop, schema pose+trajectory); chooser land row `bUseMM=True`; land triggered on floor contact.

## Caveats / polish (later, not blocking)
- Pose-vs-arc drift (foot slide, apex mismatch) → **jump-apex / stride warping** (AnimationWarping, already a dependency). Base physics jump lands correctly without it.
- Feel: tune gravity + launch velocity for weight.

## Open decisions for next session
1. `SetHandleJump(true)` (engine impulse) vs custom launch velocity? → recommend engine.
2. **Unify** all jumps on physics (simplest), or keep the in-place RM jump (no terrain issue) + moving physics (dual path)? → recommend unify on physics.
3. Cosmetic anim source: RTG_RM clips in-place vs GASP `M_Neutral_Jump_*`? → either; RM-in-place reuses current assets.
4. Land via MM now, or chooser-direct `Land2Run` first then MM? → MM is the goal; could do direct first.

## Validation (PIE)
flat jump (matches today) · jump OFF a ledge (lands lower correctly — the divergence fix) · jump UP onto higher ground · run-jump continues (no stop) · MP sim-proxy jump (Falling-mode mirror). Baselines: `bc20e53` + the chooser MVP.

See [[project_jump_system_status]], [[project_locomotion_sm_refactor_plan]], [[project_root_motion_mode]], [[gasp_sm_tip_flow]].
