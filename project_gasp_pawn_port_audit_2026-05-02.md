---
name: project_gasp_pawn_port_audit_2026-05-02
description: Phase 0 re-audit of the GASP SandboxCharacter_Mover → AAZ_HeroPawn port. Per-item parity table comparing live GASP BP (inspected via MCP) against current AZ source on branch feature/rootmotion. Drives the next round of phased fixes to bring AZ to "exact GASP" behavior.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP SandboxCharacter_Mover → AAZ_HeroPawn — Phase 0 Re-audit (2026-05-02)

**Goal:** Have *exact* GASP logic in `AAZ_HeroPawn`, same approach used for the ABP port. Earlier port (2026-04-22) finished Phases 1–8; this audit checks live divergence and drift.

**Sources of truth:**
- GASP BP: `/Game/Blueprints/SandboxCharacter_Mover` (inspected via MCP `inspect` + per-function `get_nodes` — 18 vars, 23 funcs, 5 events).
- AZ source: `Source/AZ/Public/Character/AZ_HeroPawn.h` + `Source/AZ/Private/Character/AZ_HeroPawn.cpp` (read live).

**Status legend:** ✅ parity · 🟡 partial / divergent · ❌ missing · ➕ AZ extra (no GASP analogue) · 🚫 deliberately rejected (record reason).

---

## 1. Variables (18 GASP)

| # | GASP variable | Type | AZ status | AZ name | Notes |
|---|---|---|---|---|---|
| 1 | MoverDefaultInputs_PreSim | FCharacterDefaultInputs | ✅ | same | `UPROPERTY(BlueprintReadOnly)` |
| 2 | MoverDefaultInputs_PostSim | FCharacterDefaultInputs | ✅ | same | |
| 3 | MoverCustomInputs_PreSim | S_MoverCustomInputs | ✅ | `FAZ_MoverCustomInputs` | inherits `FMoverDataStructBase` |
| 4 | MoverCustomInputs_PostSim | S_MoverCustomInputs | ✅ | `FAZ_MoverCustomInputs` | |
| 5 | MovementModeMap | TMap\<FName, E_MovementMode\> | ✅ | `MovementModeMap` (`TMap<FName,EAZ_MovementMode>`) | populated in ctor |
| 6 | FloorNormal | FVector | 🟡 | `MoverStateProxy.GroundNormal` | not a top-level pawn UPROPERTY; AZ wrote it onto a sub-struct fed to anim each Tick. GASP code reads it via `Self.FloorNormal`. |
| 7 | FloorLocation | FVector | 🟡 | `MoverStateProxy.GroundLocation` | same shape divergence as above |
| 8 | PlayerInputState | S_PlayerInputState | ✅ | `FAZ_PlayerInputState` | |
| 9 | Jump_JustPressed | bool | 🟡 | `bIsJumpJustPressed` (private) | not BP-readable, not a UPROPERTY; functionally equivalent |
| 10 | TwinStickMode | bool | ❌ | — | GASP uses for alt control scheme |
| 11 | TwinStickAimRotation | FRotator | ❌ | — | |
| 12 | LastControlRotation | FRotator | 🟡 | `LastControlRotationYaw` (double) | yaw-only, not full rotator |
| 13 | ControlRotationRate | double | ✅ | same | |
| 14 | TargetableActors | TArray\<AActor*\> | ❌ | — | populated by external Target Dummy actors in GASP demo level |
| 15 | TargetedActor | AActor* | ❌ | — | |
| 16 | SlidingAudioComponent | UAudioComponent* | ❌ | — | created in OnMovementModeChanged when entering Slide |
| 17 | SpeedHistory | TArray\<float\> | 🚫 | — | debug only |
| 18 | DebugAngle | double | 🚫 | — | debug only |
| ➕ | (AZ adds) `MoverStateProxy` (FAZ_MoverStateProxy) | — | ➕ | thread-safe snapshot for AnimInstance — no GASP analogue (GASP reads Mover directly) |
| ➕ | `bIdleTurnInProgress` | bool | ➕ | AZ-only TIP state |
| ➕ | `LastIdleOrientationTarget` / `AccumulatedYawSinceCommit` / `LastObservedControllerYaw` / `bAccumYawInitialized` | — | ➕ | AZ idle-TIP accumulator |

---

## 2. Functions (23 GASP)

| # | GASP function | Signature | AZ status | AZ symbol | Notes / divergence |
|---|---|---|---|---|---|
| 1 | UserConstructionScript | () | ✅ | `AAZ_HeroPawn(FObjectInitializer&)` ctor | C++ ctor instead of BP UCS (parity for `MovementModeMap` defaults + component creation) |
| 2 | SetupInput | () | 🟡 | `SetupPlayerInputComponent(...)` | AZ binds Move/Look/Jump/Sprint/Walk/Strafe; **missing Look_Gamepad, Mouse Wheel (camera-style cycle), D-pad (camera/pawn cycle)** |
| 3 | SetupCamera | () | 🟡 | done in ctor | AZ ctor sets ArmLength=220, SocketOffset(0,70,0), lag etc. — no `SetupCamera` analogue at runtime |
| 4 | Get_CurrentMovementMode | () → E_MovementMode | ✅ | same | uses `MovementModeMap.Find` via Mover's mode-name |
| 5 | Get_MoveInput | () → FVector | 🟡 | same | AZ player-only branch; **missing AI NavMovement branch** (deferred until AI uses pawn) |
| 6 | Get_AimingRotation | () → FRotator | 🟡 | same | AZ has only Tier-3 (ControlRotation). **Missing Tier-1 (TargetedActor look-at) and Tier-2 (TwinStickAimRotation)**. |
| 7 | Get_RotationMode | () → E_RotationMode | 🟡 | same | AZ has only Tier-3 (PlayerInputState bools). **Missing Tier-1 (TargetedActor → Aim/Strafe) and Tier-2 (TwinStickMode + IA_TwinStick_AimDirection deflection)**. |
| 8 | Get_OrientationIntent | () → FVector | 🟡 | same | per-mode matrix matches GASP. AZ adds idle-TIP accumulator hook (now mode-gated to Strafe/Aim). **Drift to verify:** the OnGround+idle Aim/Strafe branch in AZ returns `bIdleTurnInProgress ? LastIdleOrientationTarget : LastOrient` (with AZ snap-to-90); GASP returns raw `AimingFwd` after a `|delta|≥60°` cache update done elsewhere. |
| 9 | Get_Gait | () → E_Gait | 🟡 🚫 | same | **Deliberate AZ inversion:** default→Walk, Sprint→Sprint (GASP default→Run, Walk explicit). Strafe-mode dot test retained. **Missing GASP's `DDCvar.AnalogInputStyle` branch** (analog stick deflection > 0.8 → Run else Walk). **Missing GASP's `DDCvar.StrafeStyle`** which switches the strafe-sprint dot threshold between 0.5 and -0.1 (AZ hard-codes 0.5). |
| 10 | Get_Speed | () → double | ✅ | same | XY-magnitude of Mover velocity |
| 11 | DebugDraws | () | 🟡 | same (text only) | AZ prints one screen line. **GASP draws full debug overlay** (50+ nodes — speed history, direction angle wedge, orientation arrows, etc.) — not ported |
| 12 | Get_MovementDirectionThresholds | () → S_MovementDirectionThresholds | ✅ | same | AZ defaults FL/FR=55, BL/BR=125 vs GASP typical 70/110 — values are a tuning choice, structure parity ✅ |
| 13 | Get_MovementDirectionFromThresholds | (Thresholds, Direction) → E_MovementDirection | ✅ | renamed `Get_MovementDirectionFromAngle` | AZ uses 6-value enum (F/B/LL/LR/RL/RR) with foot-phase hysteresis from PostSim |
| 14 | Get_MovementDirectionAndOffset | () → (Dir, Offset) | ✅ | same | parity, returns both |
| 15 | GetTraversalCheckInputs | () → S_TraversalCheckInputs | 🟡 | same | AZ returns mostly defaults (TraceForwardDirection only) — GASP populates more fields per-pawn |
| 16 | Update_SlidingAudio | () | ❌ | — | sets `Speed` audio param from Velocity.Length() — needs SlidingAudioComponent + AC_FoleyEvents |
| 17 | CacheInputsFromMover | () | ✅ | same | exact parity |
| 18 | Update_TwinStickMode | () | ❌ | — | reads `DDCvar.ControlStyle == 1`, sets ControlRotation from IA_TwinStick_AimDirection atan2, writes TwinStickMode + TwinStickAimRotation |
| 19 | Update_TargetedActor | () | ❌ | — | `FindNearestActor(ActorLoc, TargetableActors)` → TargetedActor, draws debug cone above target |
| 20 | OnMovementModeChanged | (Prev, New: FName) | 🟡 | `HandleMovementModeChanged` (UFUNCTION) | **Massive GASP body — 53+ nodes:** (a) maps Prev/New names through MovementModeMap into `PreviousMovementMode`/`NewMovementMode` enum vars; (b) on enter Slide → `AC_FoleyEvents.PlayFoleyEvent("Foley.Event.Slide.Loop")` saves audio comp; (c) on leave Slide → fade out; (d) on enter Walking from Falling → Land foley with volume mapped from impact Z velocity; (e) on enter Falling with bIsJumpJustPressed → Jump foley with volume from speed; (f) on leave Walking while crouching → UnCrouch + clear bWantsToCrouch in PlayerInputState. **AZ stores names only, no audio, no auto-uncrouch.** |
| 21 | OnPreSimulateTick | (TimeStep, InputCmd) | ✅ | `OnProduceInput(DeltaMs, InputCmdResult)` | AZ packs Default+Custom PreSim via getters, applies base-relative transform, copies to InputCmdResult — parity. AZ adds idle-TIP accumulator + legacy GAS-tag handshake (FAZ_MoverInputCmd) — both are AZ-extras, planned removal in Phase 9 |
| 22 | Update_ControlRotationRate | () | ✅ | takes `DeltaSeconds` | parity (GASP gets DeltaTime via tick path; AZ takes it explicit) |
| 23 | Update_FloorValues | () | 🟡 | inlined into `Tick()` | logic matches (TryGetFloorCheckHitResult → ImpactNormal/ImpactPoint, fallback (0,0,1) + mesh world location) but written into MoverStateProxy instead of standalone FloorNormal/FloorLocation vars |

---

## 3. Events (5 GASP)

| GASP event | AZ status | Notes |
|---|---|---|
| BeginPlay | ✅ | binds `OnMovementModeChanged`, `AddTickPrerequisiteComponent(CharacterMoverComponent)`, plus AZ extras (camera pitch clamp, AlignControllerWithActor fallback, MoverTrajectoryPredictor lazy-init) |
| Possessed (PossessedBy) | ✅ | AZ has `PossessedBy(NewController)` override with AlignControllerWithActor + GAS init from PlayerState |
| Possessed_ClientReplicated From Server (custom event) | ❌ | client-side setup hook — not ported |
| Tick | 🟡 | AZ calls `Update_ControlRotationRate`, `CacheInputsFromMover`, `DebugDraws`, MoverStateProxy refresh. **Missing: `Update_FloorValues` (inlined ✅), `Update_TwinStickMode`, `Update_TargetedActor`, `Update_SlidingAudio`** |
| Input events: IA_Jump, IA_Crouch, IA_Sprint, IA_Walk, IA_Strafe, IA_Aim, IA_Move, IA_Look, IA_Look_Gamepad + Mouse Wheel + D-pad | 🟡 | AZ binds Move/Look/Jump/Sprint/Walk/Strafe via Enhanced Input. **IA_Aim, IA_Crouch deliberately on GAS** (kept). **Missing: IA_Look_Gamepad, Mouse Wheel (camera style cycling), D-pad (camera/pawn class cycling)** — last two are GASP demo features, low priority |

---

## 4. Interfaces

| GASP interface | AZ status | Notes |
|---|---|---|
| BPI_SandboxCharacter_Pawn (Get_PropertiesForAnimation/Camera/Traversal, Set_CharacterInputState) | ✅ | `IAZ_SandboxCharacterPawn` — all 4 methods implemented |
| IPoseSearchTrajectoryPredictorInterface | ✅ | on `MoverTrajectoryPredictor` (engine-provided) |
| IMoverInputProducerInterface (`ProduceInput_Implementation`) | ✅ | parity |
| (AZ extras) IAbilitySystemInterface, IAZ_CombatInterface | ➕ | GAS integration — no GASP analogue |

---

## 5. Summary — what to fix to reach "exact GASP"

### Real gaps (would change behavior)
1. **Targeting tier in `Get_RotationMode` / `Get_AimingRotation`** — adds TargetedActor look-at and TwinStickMode branches. Needs (10), (11), (14), (15), (18), (19) variables/functions ported first.
2. **`OnMovementModeChanged` body** — name→enum resolution into proper enum UPROPERTYs (`PreviousMovementMode` / `NewMovementMode`); Foley audio events on Land / Jump / Slide enter+exit; auto-UnCrouch + clear `bWantsToCrouch` when leaving Walking while crouched. Requires `AC_FoleyEvents` component + Foley gameplay tags or equivalent.
3. **`Update_FloorValues` as standalone vars** — promote `FloorNormal` / `FloorLocation` to top-level UPROPERTYs (BP-readable) instead of nested in MoverStateProxy. (Logic already matches.)
4. **`Get_Gait` GASP DDCvar branches** — `DDCvar.AnalogInputStyle == 1` branch (analog deflection-controlled walk/run) and `DDCvar.StrafeStyle`-driven sprint threshold (0.5 vs -0.1). Currently AZ hard-codes 0.5 and skips the analog branch.
5. **`Get_OrientationIntent` OnGround+idle (Aim/Strafe) divergence** — AZ returns `LastIdleOrientationTarget` (snap-to-90); GASP returns raw `AimingFwd` after a 60° threshold cache. Need to decide: keep AZ snap (we have only 90/180 turn anims), or restore GASP raw and accept whatever the chooser picks.
6. **`Get_MoveInput` AI NavMovement branch** — for AI-controlled pawns. Skip until needed.
7. **`SetupInput` extras** — IA_Look_Gamepad bind. Mouse Wheel + D-pad demo features can stay skipped.
8. **`DebugDraws` overlay** — 50+ nodes of debug visualization. Lower priority.
9. **`Update_SlidingAudio`** — depends on (16) component.
10. **`Update_TwinStickMode` + `Update_TargetedActor`** — alt control scheme + targeting demo. Required for (1) but can land together as one phase.

### Deliberate AZ divergences (to keep)
- ~~**`Get_Gait` baseline inversion** (default→Walk vs GASP default→Run)~~ — **REVERTED 2026-05-02 (Phase 5)** to full GASP parity (default→Run via DDCvar.AnalogInputStyle branch). User opted into "exact GASP except Debug".
- **`bIdleTurnInProgress` / accumulator** — narrowed to **Aiming-only** in Phase 6. Get_OrientationIntent now uses GASP's raw threshold-cache pattern; the accumulator survives only as an anim-side "TIP in progress" signal consumed by `AZ_AnimInstance::ShouldTurnInPlace()`. Snap-to-90, reversal-abort, and `LastIdleOrientationTarget` are all removed.
- **`MoverStateProxy`** — AZ thread-safe snapshot for AnimInstance worker thread, no GASP analogue (GASP reads Mover directly from BP — single-threaded).

### Removable AZ extras (Phase 9 cleanup)
- **Legacy GAS-tag handshake `FAZ_MoverInputCmd`** in `OnProduceInput` (lines 712–720) — superseded by `PlayerInputState` + Phase-9 GAS reintegration.

---

## Proposed phased fix plan (post-Phase 0)

| Phase | Scope | Touches |
|---|---|---|
| 1 | Variable parity: add `TwinStickMode`, `TwinStickAimRotation`, `TargetedActor`, `TargetableActors`, `SlidingAudioComponent`, `PreviousMovementMode`/`NewMovementMode` enums, promote `FloorNormal`/`FloorLocation` to UPROPERTY | .h |
| 2 | Targeting tiers in `Get_RotationMode` + `Get_AimingRotation` | .cpp |
| 3 | `Update_TwinStickMode` + `Update_TargetedActor` standalone, wire into Tick | .cpp |
| 4 | `OnMovementModeChanged` full body — name→enum resolution, Foley calls (or stub if AC_FoleyEvents not present yet), auto-UnCrouch + clear bWantsToCrouch | .cpp |
| 5 | `Get_Gait` DDCvar branches | .cpp |
| 6 | `Get_OrientationIntent` OnGround+idle revert/decision | .cpp |
| 7 | `SetupInput` Look_Gamepad bind | .cpp |
| 8 | `Update_SlidingAudio` (only if AC_FoleyEvents lands) | .cpp |
| 9 | Remove legacy `FAZ_MoverInputCmd` block | .cpp |

Each phase ends with a dual-reviewer-gate check (BP parity reviewer + memory/source reviewer), Live Coding compile, in-engine smoke test before moving on — same workflow as the ABP port.
