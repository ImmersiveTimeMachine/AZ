---
name: project_npc_foundation
description: "LIVE resume point (2026-06-25) for the Chalkie infected NPC build on branch feature/NPC. Holds the v2 AI pawn foundation (committed 496920c, pushed): the standalone Mover NPC pawn, its AI controller, and its independent-copy AnimInstance; the locked architecture decisions; the verified Mover<->navigation finding (UNavMoverComponent); and the exact pending editor/BP setup + next steps. Read FIRST to resume NPC/AI/Chalkie work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# Chalkie Infected NPC — foundation (resume point, 2026-06-25)

Branch **`feature/NPC`** (from `dev`, from `main`; `dev` + `feature/NPC` both pushed). First NPC = the **Chalkie infected** enemy ([[project_lore_chalk]]: rotting drug-addict, primal/aggressive, pure-threat; Phase-1 frozen-statue vs Phase-2 chase later). Built as a **v2 Mover pawn** so it shares the player's locomotion/chooser/PoseSearch/anim — the [[project_v2_architecture]] AI-parity payoff. NOT on the deprecated v1 `AAZ_CharacterBase`/`AAZ_EnemyCharacter` (classic ACharacter+CMC).

## SHIPPED — commit 496920c (C++ only; CLI build Result: Succeeded; NO editor/BP yet)
Three new classes (hero code 100% untouched — user reverted an earlier shared-AnimInstance attempt):
- **`AAZ_PawnMoverInfectedCharacter`** (`Character/`) — STANDALONE `APawn` sibling of the hero (NOT a subclass). v2 Mover stack: `UAZ_PawnMoverComponent` (modes self-register in its `OnRegister` — no BP wiring), `UNetworkPredictionComponent`, `UMoverTrajectoryPredictor`, `UAZ_MovementDirectionCapabilityComponent` (clearance clamp, hero-agnostic, reused), and its OWN `UAZ_AbilitySystemComponent` (NPCs have no PlayerState; `SetReplicationMode(Minimal)`, InitAbilityActorInfo(this,this) in BeginPlay+PossessedBy). `DefaultTeamId=1` (enemy of player 0). NO camera / Enhanced Input. `PrimaryActorTick=false` (controller + Mover tick instead). Implements IAbilitySystemInterface / IMoverInputProducerInterface / IGameplayTagAssetInterface / IGenericTeamAgentInterface. `AIControllerClass`+`AutoPossessAI=PlacedInWorldOrSpawned`.
  - **AI intent surface** (server-written by controller/BT, read by ProduceInput): `SetMoveIntentWorld` / `SetDesiredFacingWorld` / `SetGait` → cached fields. `ProduceInput` fills `FCharacterDefaultInputs` (DirectionalIntent move, `OrientationIntent`=facing, ControlRotation=facing, no jump) + `FAZ_MoverCustomInputs` (Gait, RotationMode=OrientToMovement, no crouch). No PC/camera read. **Verified fact:** the walking mode's facing honours `OrientationIntent` (the mode's `ResolveRotationTarget` reads it; `RotationMode` only sets turn SPEED) — so AI faces its target with zero mode changes; gait→speed via `FAZ_MoverCustomInputs.Gait` (Walk165/Run375/Sprint585).
- **`AAZ_InfectedAIController`** (`AI/`) — possesses the pawn; **TEMP "chase the player" homing drive** (`bDebugHomeToPlayer`, GetPlayerPawn(0), StopDistance=150) writing the intent surface + `SetControlRotation`. SCAFFOLDING to verify Mover+anim in PIE; to be replaced (see Next).
- **`UAZ_InfectedAnimInstance`** (`Animation/`) — **INDEPENDENT verbatim COPY** of the hero's `UAZ_MoverAnimInstance` (cp + sed rename: class + `Cast<AAZ_PawnMoverInfectedCharacter>` + includes + generated.h). Decision below. Link-safe (no file-scope free funcs/statics in the original — only a local lambda).

## Locked decisions (user's calls this session)
- **Standalone sibling pawn**, not a hero subclass (keeps the Chalkie lean: no camera/input; hero untouched).
- **Separate, INDEPENDENT NPC AnimInstance** (full copy of the player's MM pipeline), NOT a shared/decoupled or subclassed one. User rationale: "create separate, then we'll see IF and HOW it could be merged." So the player↔infected AnimInstance MERGE (extract a common `...Base`) is an explicit FUTURE step, deferred until both are concrete. Do not pre-abstract.
- Earlier shared-AnimInstance work (made `UAZ_MoverAnimInstance` pawn-agnostic + a subclass) was REVERTED to cdf2a57 at user request before this build.

## Navigation WITH Mover — VERIFIED (UE 5.8 engine, 2026-06-25)
YES, nav works. Engine ships **`UNavMoverComponent`** (`Mover/.../DefaultMovementSet/NavMoverComponent.h`) + optional `NavWalkingMode`/`AsyncNavWalkingMode`. Bridge: it implements `INavMovementInterface`; the AIController's PathFollowingComponent **auto-discovers** it via `FindComponentByInterface<INavMovementInterface>()` (`PathFollowingComponent.cpp:1514`) — just adding the component wires it. PathFollowing calls `RequestDirectMove`/`RequestPathMove` → it CACHES the move; the pawn's `ProduceInput` pulls it via `ConsumeNavMovementData(OutIntent,OutVelocity)` and feeds the Mover cmd. Supports Detour Crowd avoidance (good for hordes). So: add `UNavMoverComponent` to the pawn, consume in ProduceInput (fall back to the cached AI intent when no nav move), drop a `NavMeshBoundsVolume`, then standard BT `MoveTo`/`MoveToActor` works. `AIModule` already a dep; no `NavigationSystem` module add needed for MoveToActor.

## PENDING — do next (tomorrow)
1. **Editor/BP setup** (the homing demo is testable without a navmesh):
   - Duplicate hero `AZ_ABP_MoverAnimInstance` → `AZ_ABP_Infected`; **Class Settings → Parent Class → `UAZ_InfectedAnimInstance`** (must NOT use the hero ABP — it casts to the hero pawn). Duplicate keeps the chooser + PSD_v2_* DB assignments.
   - Create `BP_AZ_Chalkie` (child of `AAZ_PawnMoverInfectedCharacter`); Mesh → hero's skeletal mesh (same skeleton so existing clips/chooser work; infected mesh later) + Anim Class → `AZ_ABP_Infected`.
   - Place in level near player, PIE → it should run at the player with full locomotion anim. (ABP reparent = do manually; rest can be MCP-scripted.)
2. Replace the homing drive with **AIPerception (sight/hearing) + BehaviorTree + NavMesh path-follow via `UNavMoverComponent`** (see nav section). Add `NavMeshBoundsVolume`.
3. Team attitude solver (so perception sees player as hostile); health/attributes on the NPC ASC; melee/attack (parameterized GA, ties to [[project_combat_fist_build_plan]]); Phase-1 frozen vs Phase-2 aggressive states.
4. THEN revisit the player↔infected AnimInstance merge (extract common base) once both concrete.

Build rule reminder: new UCLASS/UPROPERTY/UFUNCTION → editor-closed CLI build (`Build.bat AZEditor`) + restart; body-only → Live Coding. See [[project_v2_architecture]], [[project_multipawn_class_design]], [[project_sp_first_coop_extensible]] (homing's GetPlayerPawn(0) is SP-only scaffolding; perception is per-controller/co-op-safe).
