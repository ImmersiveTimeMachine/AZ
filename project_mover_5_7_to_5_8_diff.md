---
name: AZ project Mover plugin 5.7 → 5.8 migration impact analysis
description: Point-in-time diff analysis (2026-05-09) of UE Mover plugin between 5.7 and 5.8 branches, focused on the AZ project's actual API surface area. Identifies the 4 method signature breaks we'll need to fix on migration, lists the symbols we use that are unchanged, and catalogs new 5.8 features relevant to the v2 architecture. Use this when planning the 5.8 GA migration so the analysis doesn't have to be redone.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
**Snapshot:** Compared `origin/5.7` against `origin/5.8` (commit `adb43e19eca2`) on Epic's UnrealEngine repo at `C:\UnrealEngine`. AZ project codebase grepped at `C:\UnrealEngine\Games\AZ\Source\` (commits `b347573` + `071227a`).

**Bottom line:** **4 method signature breaks**, all the same trivial fix (add `const FMoverSimContext&` parameter). **Migration cost: ~30 minutes** mechanical edits + rebuild + smoke test. Nothing structural changes.

**Why:**

5.7 → 5.8 promotes Mover from experimental to beta with API "mostly frozen" per Epic's commitment. The 4 breaks affecting AZ are all the same pattern — Epic added an `FMoverSimContext` parameter so simulation functions get systems context (rollback, scheduling) without needing globals. Additive, not restructuring.

**How to apply:**

**Breaking changes — must fix during migration:**

| # | Symbol we override | File:Line in AZ | Change | Fix |
|---|---|---|---|---|
| 1 | `UAZ_SmoothWalkingMode::GenerateWalkMove_Implementation` | `Source/AZ/Public/Character/AZ_SmoothWalkingMode.h:34`, `.cpp:63` | Added `const FMoverSimContext& SimContext` between `DeltaSeconds` and `DesiredVelocity` | Add param to override signature; pass through if calling super |
| 2 | `UAZ_SmoothWalkingMode::OnRegistered` | `AZ_SmoothWalkingMode.h:38` | Added `const FMoverSimContext& SimContext` param | Add param |
| 3 | `UAZ_SmoothWalkingMode::OnUnregistered` | `AZ_SmoothWalkingMode.h:39` | Added `const FMoverSimContext& SimContext` param | Add param |
| 4 | `UAZ_FallingMode::GenerateMove_Implementation` | `AZ_FallingMode.h:22-23`, `.cpp:9-10` | Added `const FMoverSimContext& SimContext` as **first** param | Prepend param |

5.8 signature reference for migration:

```cpp
// SmoothWalkingMode.h (5.8)
virtual void GenerateWalkMove_Implementation(
    FMoverTickStartData& StartState, float DeltaSeconds,
    const FMoverSimContext& SimContext,                    // NEW
    const FVector& DesiredVelocity, const FQuat& DesiredFacing,
    const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees,
    FVector& InOutVelocity) override;

// MovementMode.h base virtuals (5.8)
virtual void OnRegistered(const FName ModeName, const FMoverSimContext& SimContext);
virtual void OnUnregistered(const FMoverSimContext& SimContext);

// FallingMode.h (5.8)
virtual void GenerateMove_Implementation(
    const FMoverSimContext& SimContext,                    // NEW (first param)
    const FMoverTickStartData& StartState,
    const FMoverTimeStep& TimeStep,
    FProposedMove& OutProposedMove) const override;
```

**Definitively safe — no change needed (verified by grep + diff):**

- `UCharacterMoverComponent` instantiation (`AZ_HeroPawn.cpp:69`) — same
- `QueueLayeredMove(TSharedPtr<FLayeredMoveBase>)` (`AZ_HeroPawn.cpp:186`) — same signature
- `FLayeredMove_RootMotionAttribute` ctor — only cosmetic dtor change (`{}` → `= default`)
- `FCharacterDefaultInputs` / `FMoverDefaultSyncState` — only cosmetic dtor change
- `IMoverInputProducerInterface::ProduceInput(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)` — same signature, AZ_HeroPawn override unchanged
- `OnMovementModeChanged` delegate — same signature
- `GetLastInputCmd()` — same return type
- `GetMoverComponent<T>()` template — constraint corrected (was inverted in 5.7), our usage `GetMoverComponent<UMoverComponent>()` passes
- `Within = MoverComponent` UCLASS specifier removed — we don't depend on it; modes can now be more flexibly placed
- We don't override `Activate` / `Deactivate` / `Activate_External` / `Deactivate_External` / `GenerateMove` (the UBaseMovementMode virtuals that changed signatures most dramatically) — confirmed by grep

**Additive 5.8 features RELEVANT to the v2 architecture (no break, just opportunities):**

1. **`UBaseMovementMode::GetGameplayTags(FGameplayTagContainer&)`** virtual — modes can advertise their active tags. Bridge for v2's "Mover mode reads tags" pattern, now bidirectional (modes can also publish).
2. **`FLayeredMoveBase::GetGameplayTags()`** virtual — layered moves can publish tags (e.g., a `FLayeredMove_TurnInPlace` could publish `State.Movement.TurningInPlace` automatically).
3. **`FLayeredMoveGroup::ForEachActiveMoveOfType<T>(Func)`** template — clean iteration over active layered moves; makes "is X kind of move active?" trivial.
4. **`FMoverSimContext`** — new "everything you need from simulation" struct. Carries rollback blackboard, scheduling, simulation handle. Relevant for v2 co-op-ready foundation.
5. **Rollback infrastructure** — entire new `RollbackBlackboard` + `RollbackBlackboardLibrary` + `RollbackCircularBuffer` system. Networking client-side prediction now production-grade.
6. **`MoverGameplayTagLog`** — new logging category for Mover-tag interactions. v2 debugging asset.
7. **`ChaosMover`** — entire new sibling plugin (`Engine/Plugins/Experimental/ChaosMover/`) for physics-driven movement. Optional alternative if we ever need physics-based locomotion.

**Other 5.8 changes worth knowing:**

- `SimpleFlyingMode` — new mode (we don't use, but available)
- Multiplayer SM desync fix (state machines were non-deterministic across server/client) — directly relevant if/when co-op work begins
- ~30% network bandwidth reduction vs CMC; ~0.09ms per character tick (vs CMC's ~0.15ms)

**Total scope of 5.7 → 5.8 Mover plugin diff:** 125 files changed, +4568/-1411 lines. Of that, only the 4 method signatures listed above touch our code.

**AZ Mover surface area inventory (~17 symbols, 5 files):**

| File | Mover symbols touched |
|---|---|
| `AZ_HeroPawn.h/cpp` | `UCharacterMoverComponent`, `IMoverInputProducerInterface`, `FMoverInputCmdContext`, `FCharacterDefaultInputs`, `FLayeredMove_RootMotionAttribute`, `OnMovementModeChanged`, `QueueLayeredMove`, `GetLastInputCmd`, `ProduceInput_Implementation` |
| `AZ_SmoothWalkingMode.h/cpp` | `USmoothWalkingMode`, `UMoverComponent`, `FMoverTickStartData`, `FCharacterDefaultInputs`, `GenerateWalkMove_Implementation`, `OnRegistered`, `OnUnregistered`, `GetMoverComponent<>` |
| `AZ_FallingMode.h/cpp` | `UFallingMode`, `FMoverTickStartData`, `FMoverTimeStep`, `FProposedMove`, `FMoverDefaultSyncState`, `FCharacterDefaultInputs`, `GenerateMove_Implementation` |
| `AZ_AnimInstance.h/cpp` | `UMoverComponent`, `UCharacterMoverComponent` (read access only) |

**Migration trigger and process:**

- **Trigger:** UE 5.8 ships GA (estimated August 2026, may slip to Sep-Oct per Epic's history)
- **Pre-migration:** verify this analysis is still accurate against the GA branch (re-diff `origin/5.7..origin/5.8`)
- **Migration steps:**
  1. Update engine install at `C:\UE57` to 5.8 (or new path)
  2. Mechanically apply the 4 signature fixes (above table)
  3. Rebuild via cpp-build-livecoding skill
  4. Smoke test: PIE walk + jump + idle TIP + foot stop + RM bridge
  5. (Optional) adopt 5.8 additive features as v2 work continues
- **Worktree note:** v1 promotion-fallback CVar (`AZ.Chooser.UseV2`) means we can validate 5.8 + v2 together on a single branch
