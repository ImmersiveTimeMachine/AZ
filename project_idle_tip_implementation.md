---
name: project_idle_tip_implementation
description: AZ idle TIP — speed-independent accumulator design, working baseline. Single source of truth on HeroPawn (bIdleTurnInProgress), AnimInstance mirrors it. Caveats and tuning knobs documented.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# AZ Idle Turn-In-Place — Working Baseline

**Status (2026-04-21):** Working but tunable. User reaction: "almost good".
**Commit:** `b5c076e1` on main — "GASP-parity locomotion: trajectory predictor, state tracking, idle TIP".

## Design — speed-independent accumulator

**Why:** Earlier attempts (continuous OrientationIntent, threshold-on-current-delta, FutureFacingDelta-based trigger) all coupled the trigger to mouse speed — fast flicks worked, slow drags didn't. User explicitly rejected this.

**What works (current):**
1. `AAZ_HeroPawn` accumulates **absolute mouse yaw motion** since the last commit (`AccumulatedYawSinceCommit`).
2. When accumulator hits **60°**, `bIdleTurnInProgress = true` is set, accumulator resets, `LastIdleOrientationTarget` cached to current camera forward.
3. While `bIdleTurnInProgress`:
   - Cache target is continuously updated to current camera (player can adjust mid-turn).
   - `OrientationIntent` is emitted (Mover rotates capsule).
   - Releases when actor forward aligns with cache (`dot ≥ 0.998` ≈ 4°).
4. While `!bIdleTurnInProgress`: `OrientationIntent = ZeroVector` (body stays put — no stale-cache drift).
5. `UAZ_AnimInstance::ShouldTurnInPlace()` is a pure mirror: `return OwningHeroPawn->IsIdleTurnInProgress()`.

**Single source of truth:** HeroPawn owns the trigger logic. AnimInstance just reads. No duplicate accumulator, no chicken-egg loop.

## Tuning knobs

| Knob | Default | Notes |
|---|---|---|
| Commit threshold (HeroPawn) | **60°** accumulated mouse yaw | Higher = need bigger camera moves to commit; lower = TIP fires more often |
| Alignment release (HeroPawn) | **dot ≥ 0.998** (~4°) | Lower = wider tolerance, faster release; higher = wait until perfectly aligned |
| `CommonLegacyMovementSettings.TurningRate` | **80°/s** | Calibrated to match TurnLt90_Loop anim duration (~1.3s for ~90°). Increase for snappier rotation; decrease for slower/more weighty |
| TIP enter rule (SM) | unchanged from GASP — `Conduit → TransitionToIdle` on `NOT IsMoving AND MovementState==Idle` | The chooser then picks the turn anim |

## Other related fixes shipped in same commit

1. **OffsetRootBone enum mapping** — values were `Accumulate=0, Interpolate=1, …, Release=5` but our C++ returned 0/1/2 with comments saying "Interpolate/Accumulate/Release". Result: mesh was *Accumulate* during movement (offset accrued) and *LockOffsetAndConsumeAnimation* (LOCKED) when idle. Mesh drifted from capsule. Fixed: Moving→1 (Interpolate), Idle/InAir→5 (Release), Rotation→0 (Accumulate, GASP default).

2. **FutureFacingDelta mesh-space offset bias** — `MoverPoseSearchTrajectoryPredictor.cpp:130` reads facing from `VisualComp->GetComponentRotation()`. SK_SurvivalMan has -90° baked offset between mesh component and actor. Initializing `PrevYaw` from `ActorFacing.Yaw` then comparing to first sample (mesh-space) injected -90° as permanent bias. Fixed: init `PrevYaw` from first sample, not actor.

3. **AlignControllerWithActor** in `PossessedBy` (primary) + `BeginPlay` (fallback). Eliminates spawn-time controller/actor delta-90 → no false TIP on first frame.

4. **Lazy-create `MoverTrajectoryPredictor`** in `BeginPlay`. UCLASS `EditInlineNew` makes BP serialize the property as instanced subobject which can override C++ `CreateDefaultSubobject` default with null. Lazy-init bypasses the BP-overrides-C++ trap.

5. **Acceleration from input intent** (not d(velocity)/dt). For HeroPawn, pulls from Mover's `CommonLegacyMovementSettings.Acceleration` (default 4000 cm/s²) and rotates Mover's `CachedMoveInputIntent` by ControlRotation. Fixes "MovementState flips Idle while pressing W" bug — d(velocity)/dt is zero at terminal speed even when player is holding input.

## Known remaining tuning

- **TIP anim feel** — TurningRate=80 matches TurnLt90 visually, but if user wants snappier or more deliberate, adjust. 60-80 = matches anim; 120-180 = faster than anim (anim outlasts rotation).
- **60° commit threshold** — could be 45° for more responsive TIP, or 90° for "only big turns" feel.
- **Gradient mid-turn** — currently the cache updates to current camera every tick while in progress, so player can adjust direction mid-turn. If preferred to "lock" target at commit moment, change to update-once-on-commit-only.

## Architecture rule (don't violate)

Never compute the trigger from `FutureFacingDelta` (predictor-based) — that creates the chicken-egg loop where you need OrientationIntent set before the predictor will predict rotation, but you only set OrientationIntent when the predictor reports rotation. The accumulator is independent of body state and avoids this entirely.

## Files modified in baseline commit

- `Source/AZ/Public/Animation/AZ_AnimInstance.h` — state tracking 5-var fields, removed bHasMovementInput
- `Source/AZ/Private/Animation/AZ_AnimInstance.cpp` — trajectory generation, FutureFacingDelta, OffsetRootBone enum fix, ShouldTurnInPlace mirror
- `Source/AZ/Public/Character/AZ_HeroPawn.h` — bIdleTurnInProgress, accumulator fields, AlignControllerWithActor decl
- `Source/AZ/Private/Character/AZ_HeroPawn.cpp` — accumulator logic, AlignControllerWithActor impl, lazy MoverTrajectoryPredictor
- `Source/AZ/Public/Animation/AZ_ChooserUtils.h` + `.cpp` — SetCellAssetOnSub helper
- `Source/AZ/Public/Animation/AZ_PoseSearchUtils.h` + `.cpp` — added notify helpers (separate work)
