---
name: project_cmc_jump_build_order
description: "★★ APPROVED BUILD ORDER for CMC jump (2026-08-27): physics capsule + Start-carries-the-air + per-gait land DBs, the stop-anim pattern. All 24 AnimPro_Jump* clips mapped to a role, measured apexes, gate rows, notify plan, acceptance metrics."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-28T03:40:41.406Z
---

Approved by the user 2026-08-27 ("go"). Branch `spike/cmc-backport`. Nothing written yet at time of
approval — this is the order of work.

# What is already done (do not rebuild)

- **GAS → capsule is complete.** `AAZ_CmcCharacterBase` implements `IAZ_JumpRequester`
  (`SetJumpPressed` → native `Jump()`/`StopJumping()`, `CanRequestJump` → `CanJump()`).
  `AZ_GA_PawnJump` is already pawn-agnostic (`Avatar->Implements<UAZ_JumpRequester>()`, no concrete cast).
  `Move->JumpZVelocity = 420.f`. The anim contract already publishes
  `MovementMode = IsFalling() ? InAir : OnGround`.
  ⚠ CONFIRM the CMC hero grants `GA_PawnJump` (interface) and not the legacy `GA_Jump` (concrete cast).
- **The SM already implements the 2-phase shape.** `AZ_LocomotionStateMachine.cpp:117-123` returns
  `TransitionToInAir` for the WHOLE airborne duration; `InAirLoop` is a reserved enum value, never
  produced. Touchdown (`:129-143`) sets `bJustLanded` and routes to `TransitionToIdle` (standing land)
  or `TransitionToLocomotion` (moving land, by gait). **The land-outcome axis is therefore free.**

# Two defects that MUST be fixed first

**P0-a — the stop contract has no ground guard.** `AZ_CmcCharacterBase::UpdateSelectionGait` computes
`bHasInput` from acceleration + `IsAnimDrivingMovement()` with **no `IsMovingOnGround()` check**. Jump with
no stick and the stop latches *in the air*: `bStopActive=true`, `SelectionGait=LatchedStopBand`, and the
hero's curve braking then writes velocity mid-flight. Same failure class as the two bugs fixed in
[project_cmc_input_gap_doctrine]. **Land this before any jump testing or every result is contaminated.**

**P0-b — the empty-gate fallback lies.** Measured live 5× in one PIE:
`[CmcAnim] DatabaseGates union is EMPTY for mode=1 … fell back to the GROUNDED rows (5 db)` — which is why
a jump currently picks `AnimPro_Idle2Crouch_new` from `PSD_AZ_StanceTransitions` at
`SM=TransitionToInAir`. Change the fallback from silently substituting grounded DBs to a loud,
non-substituting failure.

# Content — all 24 `AnimPro_Jump*` clips have a role

Measured 2026-08-27 (root-Z via `AnimationLibrary.get_bone_pose_for_time(seq,"root",t,True)`):

| Set | # | Measured | Role |
|---|---|---|---|
| Starts | 5 | 3.53-4.00 s, peak +58…+98 cm, **end −5.3 k…−8.3 k cm** | takeoff + rise + **open-ended fall tail = the air** |
| `FallingLoop` | 1 | 3.33 s, rootZ flat **0** (in-place) | the **drop** case only (no takeoff clip exists) |
| `_Land` | 5 | 1.03-1.27 s, rootZ 0 | land → settle to stop |
| `_Land2X` | 5 | 0.90 s, rootZ 0 | land → keep moving (shorter, as expected) |
| `LandHard` | 1 | 1.57 s | heavy landing, gate on impact speed |
| `_ALL` | 6 | 1.50-2.27 s, peak +60…+99, **end 0.0** | CLOSED arc — same-height jump. **RM montage / traversal hop, NOT the physics jump** |
| `FallingLoop_RootMotion` | 1 | ends −6507 | unusable via BlendStack (`RootMotionFromMontagesOnly` drops it) |

Skeleton verified `/Game/SurvivalMan/Meshes/SKEL_SurvivalMan` for the ABP, an in-use CMC clip
(`AnimPro_WalkFwdLoop`), and `AnimPro_Jump_place_ALL` — **no retarget step needed.**

★ **`AnimPro_*` has NO per-gait InAir clips** (only the generic `FallingLoop`); `RTG_RM_*` has 5. That is
deliberate and fine: a physics fall IS gait-agnostic, and Mover's 5 per-gait InAir dups are exactly what
caused its walk→idle air collapse. Do not author per-gait air content.

# Databases — mirror the stop-anim pattern

Per-gait DBs, gate rows narrow by gait + SM state, MM picks foot + entry frame within.
```
PSD_AZ_Stand_{Walk,Run,Idle}_JumpStarts   ← gate: MovementMode=InAir, Gait=X
PSD_AZ_Stand_{Walk,Run,Idle}_JumpLands    ← gate: bJustLanded, Gait=X   (both _Land and _Land2X;
                                              the SM state separates the two families for free)
PSD_AZ_Stand_InAir                        ← gate: InAir with no start (the drop)
```
★ **Keep every jump DB OUT of `PSN_AZ_CMC`.** That set has 25 members and computes shared deviation
statistics; adding to it shifts EVERY MM cost in the project and would regress the locomotion tuning
landed 2026-08-27. Land selection is single-pool anyway, so cross-DB cost comparability is irrelevant.

# Notifies

| Role | ExcludeFromDatabase | BranchIn | BlockTransition |
|---|---|---|---|
| Starts | `[0, apex+0.05]` — masks the rise | `[apex+0.05, end]` | `[apex+0.35, end]` |
| Lands | — | `[0, end]` (MM picks the impact frame) | — |
| FallingLoop | — | `[0, end]` | — |

★ **RE-MEASURE the apex at fine resolution before authoring.** Coarse (0.17 s) sampling gave idle 0.44,
walk 0.33, run_LU 0.50, run_RU 0.33; the recorded Mover values (120 Hz) are idle 0.467, walk 0.367,
run_LU 0.433, run_RU 0.333. **An exclude window that stops short of the true apex leaves rising frames
searchable and the character visibly bobs UP again mid-air** — a documented bug from the Mover build.

BranchIn OWNS database membership (`PreSaveRoot` → `SynchronizeWithExternalDependencies`). Never also
explicitly add a clip that has a BranchIn — that yields DOUBLE entries on the next DB save.

# ★ MEASURED CONSTANTS (2026-08-27, 120 Hz root-Z, editor-live) — use these, do not re-guess

| Clip | launch frame | **apex** | apex Z | Exclude window |
|---|---|---|---|---|
| `AnimPro_JumpIdleStart`      | 0.175 | **0.467** | 100.1 cm | `[0, 0.517]` |
| `AnimPro_JumpWalkStart_LU`   | 0.150 | **0.367** |  60.0 cm | `[0, 0.417]` |
| `AnimPro_JumpWalkStart_RU`   | 0.150 | **0.367** |  60.0 cm | `[0, 0.417]` |
| `AnimPro_JumpRunStart_LU`    | 0.217 | **0.433** |  70.1 cm | `[0, 0.483]` |
| `AnimPro_JumpRunStart_RU`    | 0.117 | **0.333** |  70.1 cm | `[0, 0.383]` |

These match the recorded Mover/`RTG_RM_` values EXACTLY — same source animations, different retarget
output — so [project_jump_system_status]'s notify work transfers directly.
⚠ A coarse (0.17 s) pass had put idle at 0.44 and run_LU at 0.50. Idle 0.44 would have been **0.027 s
short of the true apex**, leaving rising frames searchable = the documented mid-air re-rise bug. Always
sample at 120 Hz.

## Gait-scaled JumpZVelocity — content-derived

`BP_CMC_Hero` CDO: **`GravityScale = 1.5`** (not 1.0), world gravity not overridden → **effective
g = 1470 cm/s²**. Current flat `JumpZVelocity = 420` → apex **60.0 cm**, i.e. it matches the WALK clip
exactly and nothing else.

| Gait | Clip apex | `JumpZVelocity` for `v = sqrt(2·g·h)` |
|---|---|---|
| Idle | 100.1 cm | **542.5** (currently 40 cm short — body plays a big jump, capsule does a small one) |
| Walk |  60.0 cm | 420 (already correct) |
| Run  |  70.1 cm | **454** |

★ This is why RM-rise is not needed: deriving `JumpZVelocity` per gait from the measured apex makes the
physics arc match the authored arc **by construction**, with none of the RM-in-air two-owners hazard.
Set it where the gait is set (one owner), alongside `MaxWalkSpeed`.

# Takeoff sync

The impulse is instant; Start clips carry 0.117-0.217 s of anticipation crouch (the launch column above),
so the capsule rises while the body still squats. Fix as Mover did: direct-play the takeoff from the
measured **launch frame**, not from 0. Anticipation is intentionally skipped — restoring it would mean
delaying the impulse, which is a gameplay-feel decision, not an animation one.

# Order of work

1. P0-a ground guard + P0-b loud fallback. Build, PIE, confirm no stop latches in air.
2. Re-measure apexes + launch frames at fine resolution. Record them here.
3. Create the 7 DBs; author notifies; DBs stay out of `PSN_AZ_CMC`.
4. Add gate rows for InAir + land, per gait.
5. PIE the four cases: standing jump · walk jump · run jump · **walk off a ledge** (the drop).

# Acceptance metrics

| Invariant | Where |
|---|---|
| no `DatabaseGates union is EMPTY` warnings at all | `[CmcAnim]` |
| every airborne pick comes from a Jump/InAir DB — never a Stance/Stop/Loop DB | `[CmcPick]` |
| no stop latches while `IsFalling()` | `[CmcStop]` |
| land clip matches gait AND outcome (`_Land` when stopping, `_Land2X` when input held) | `[CmcPick]` |
| **no mid-air re-rise** (the exclude-window bug) | visual + root-Z |
| the drop case plays `FallingLoop`, not a ground clip | `[CmcPick]` |

# Out of scope (deliberately)

- **RM rise.** Physics only for now. On CMC, montage RM while `Falling` means montage and gravity both
  integrate the same velocity — the "two owners writing one velocity" pattern removed on 2026-08-27.
  Mover needed a dedicated `RMAction` mode for it and it was flagged not network-prediction-safe.
- **The `_ALL` closed-arc clips / traversal hop.** Valid only when the landing height equals the takeoff
  height; belongs to the trace→chooser→RM-action traversal pattern, not jump. See
  [project_traversal_system].
- **Per-gait air content.** See the content note above.

See [project_cmc_input_gap_doctrine], [project_jump_system_status] (the Mover build's full saga —
read before touching notifies), [project_traversal_system], [feedback_verify_never_presume].
