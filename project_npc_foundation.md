---
name: project_npc_foundation
description: "LIVE resume point for the Chalkie infected NPC build on branch feature/NPC. Holds the SHIPPED v2 AI pawn foundation (committed 496920c), the locked NPC ARCHITECTURE decision (Option B: Mover movement + classic ABP, NOT Motion Matching), the chosen mesh/anim packs + retarget plan, the verified Mover<->navigation finding (UNavMoverComponent), and the exact pending steps. Read FIRST to resume NPC/AI/Chalkie work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# Chalkie Infected NPC — foundation (resume point)

Branch **`feature/NPC`** (from `dev`, from `main`; `dev` + `feature/NPC` both pushed). First NPC = the **Chalkie infected** enemy ([[project_lore_chalk]]: rotting drug-addict, primal/aggressive, pure-threat; Phase-1 frozen-statue vs Phase-2 chase later). It rides the **v2 Mover MOVEMENT rails** (the [[project_v2_architecture]] AI-parity payoff) but does NOT share the hero's animation pipeline — see the architecture decision below. NOT on the deprecated v1 `AAZ_CharacterBase`/`AAZ_EnemyCharacter` (classic ACharacter+CMC).

## ★ NPC ARCHITECTURE DECISION — Option B "Hybrid" (locked 2026-06-26)
The real question is TWO axes, not one binary: **movement** (Mover vs CMC) and **animation** (Motion Matching+CHT vs classic ABP). The hero uses premium on both; NPCs don't have to.

**CHOSEN: B = Mover (movement) + classic ABP (animation). NOT Mover+CHT+MM (option A), NOT classic CMC (option C).**

- **Movement = Mover** (shared with hero). Keeps the committed `AAZ_PawnMoverInfectedCharacter`, the AI-parity `ProduceInput` intent surface, nav via `UNavMoverComponent`, and the clearance/traversal reuse — ONE movement codebase, lowest churn.
- **Animation = classic ABP** (state machine + blendspace, speed/state-driven). **NO Motion Matching, NO CHT/PoseSearch for NPCs.** MM runs a per-instance per-frame nearest-neighbour search (Epic's MM sample is a SINGLE-character showcase; it does not throttle/crowd). Classic ABP is a cheap SM eval AND unlocks the engine's crowd levers: URO (Update Rate Optimization), `VisibilityBasedAnimTickOption`, LOD tick-throttle, master-pose sharing. A shambling zombie gains ~nothing visually from MM.
- **DOCTRINE (reusable):** *movement system = shared (Mover); animation system = per-archetype.* Hero → MM; Chalkie/hordes → classic ABP.
- **Consequence:** last session's MM copy **`UAZ_InfectedAnimInstance` is SHELVED** — kept in code (harmless, no asset references it) but it does NOT drive the Chalkie. The pawn's `UMoverTrajectoryPredictor` member is now unused too (only MM needed it) — leave for now, cleanup later. Revisit/delete both when the classic path is proven.
- **ESCAPE HATCH → Option C (CMC + classic ABP):** only if enemy density becomes TRUE swarms (dozens on screen, L4D-style). CMC is the most proven/lightest crowd path (mature NavMesh + Detour/RVO avoidance, cheap simulated-proxy replication) but costs TWO movement systems + abandons the Mover pawn. Mover is still **Experimental** in 5.8 (fine for one hero, riskier for a whole enemy population at high N). The AI controller + intent design port to CMC later, so B is reversible. **Enemy-density target still TBD** (user hasn't answered sparse-dread vs swarm) → default stays B until profiling says otherwise.

## Mesh + anim packs (chosen 2026-06-26)
- **Mesh = `/Game/ZombiePackV1`** zombie bodies — e.g. `SK_ZombieAA` (default; also `SK_ZombieAB_a/b` variants; each has a OneMesh and a Parts/dismemberment variant). Skeleton: `/Game/ZombiePackV1/DemoContent/Character/Mesh/SNP_UE4_Mannequin_Skeleton` (UE4 mannequin, 67 bones, 2 morph targets ZombieOpenMouth/ZombieGrin).
- **Anims = `/Game/Zombie_01`** — rich classic clip set: idle / walk_F (many) / chase (run) / turn L/R 90+180 / attack / death / crawl / shamble / onfire / hyper; each in **Root** (`Animations/Root/`) and **In-Place** (`Animations/InPlace/`, `_IPC` suffix) variants. Ships its own `ABP_DemoPlayable` + `BP_ZombiePlayable`. Skeleton: `/Game/Zombie_01/Core/Character/Mesh/UE4_Mannequin_Skeleton` (DIFFERENT asset, also UE4 mannequin).
- **Two different UE4-mannequin skeleton assets** → the clips don't drop straight onto the body. **User chose RETARGETING** (not Compatible Skeletons) to bring Zombie_01 clips onto the ZombiePackV1 mesh's skeleton. (Both are UE4 mannequin so bone names should match — retarget is straightforward.)

## SHIPPED — commit 496920c (C++ only; CLI build Result: Succeeded; NO editor/BP yet)
Three new classes (hero code 100% untouched). All still VALID under Option B (only the anim choice changed):
- **`AAZ_PawnMoverInfectedCharacter`** (`Character/`) — STANDALONE `APawn` sibling of the hero (NOT a subclass). v2 Mover stack: `UAZ_PawnMoverComponent` (modes self-register in its `OnRegister` — no BP wiring), `UNetworkPredictionComponent`, `UMoverTrajectoryPredictor` (now unused under B), `UAZ_MovementDirectionCapabilityComponent` (clearance clamp, hero-agnostic, reused), and its OWN `UAZ_AbilitySystemComponent` (NPCs have no PlayerState; `SetReplicationMode(Minimal)`, InitAbilityActorInfo(this,this) in BeginPlay+PossessedBy). `DefaultTeamId=1`. NO camera / Enhanced Input. `PrimaryActorTick=false`. Implements IAbilitySystemInterface / IMoverInputProducerInterface / IGameplayTagAssetInterface / IGenericTeamAgentInterface. `AIControllerClass`+`AutoPossessAI=PlacedInWorldOrSpawned`.
  - **AI intent surface** (server-written by controller/BT, read by ProduceInput): `SetMoveIntentWorld` / `SetDesiredFacingWorld` / `SetGait` → cached fields. `ProduceInput` fills `FCharacterDefaultInputs` (DirectionalIntent move, `OrientationIntent`=facing, ControlRotation=facing, no jump) + `FAZ_MoverCustomInputs` (Gait, RotationMode=OrientToMovement, no crouch). **Verified fact:** walking mode facing honours `OrientationIntent` (its `ResolveRotationTarget` reads it; `RotationMode` only sets turn SPEED) — AI faces its target with zero mode changes; gait→speed via `Gait` (Walk165/Run375/Sprint585).
- **`AAZ_InfectedAIController`** (`AI/`) — possesses the pawn; **TEMP "home toward the player" drive** (`bDebugHomeToPlayer`, GetPlayerPawn(0), StopDistance=150) writing the intent surface + `SetControlRotation`. SCAFFOLDING to verify Mover+anim in PIE; replaced later by perception+BT+nav.
- **`UAZ_InfectedAnimInstance`** (`Animation/`) — INDEPENDENT verbatim COPY of the hero's MM `UAZ_MoverAnimInstance`. **SHELVED by the Option-B decision** (not used; Chalkie uses a classic ABP instead). Kept in code for now.

## Navigation WITH Mover — VERIFIED (UE 5.8 engine, 2026-06-25)
YES. Engine ships **`UNavMoverComponent`** (`Mover/.../DefaultMovementSet/NavMoverComponent.h`) + optional `NavWalkingMode`/`AsyncNavWalkingMode`. It implements `INavMovementInterface`; the AIController's PathFollowingComponent **auto-discovers** it via `FindComponentByInterface<INavMovementInterface>()` (`PathFollowingComponent.cpp:1514`). PathFollowing calls `RequestDirectMove`/`RequestPathMove` → it CACHES the move; the pawn's `ProduceInput` pulls it via `ConsumeNavMovementData(OutIntent,OutVelocity)` and feeds the Mover cmd. Supports Detour Crowd avoidance (good for hordes). So: add `UNavMoverComponent` to the pawn, consume in ProduceInput (fall back to cached AI intent when no nav move), drop a `NavMeshBoundsVolume`, then standard BT `MoveTo`/`MoveToActor` works. `AIModule` already a dep.

## PENDING — do next (under Option B)
1. **Retarget** the `/Game/Zombie_01` clips → `SNP_UE4_Mannequin_Skeleton` (the ZombiePackV1 mesh skeleton). At minimum the locomotion set (idle / a walk / a chase/run); turns/attacks/deaths later. (User is doing the retarget.)
2. **Build classic `AZ_ABP_Chalkie`** — a small speed/state-driven locomotion state machine (Idle ↔ Walk ↔ Chase by ground speed) reading the Mover pawn's velocity (`Pawn->GetVelocity()` works on a Mover pawn; verify). Pure-BP or a thin NEW C++ AnimInstance — NOT `UAZ_InfectedAnimInstance` (shelved). Optional fast smoke-test first: assign the pack's `ABP_DemoPlayable` to verify mesh+skeleton+movement end-to-end before building our own.
3. **`BP_AZ_Chalkie`** (child of `AAZ_PawnMoverInfectedCharacter`); Mesh → `SK_ZombieAA` + Anim Class → `AZ_ABP_Chalkie`. Place near player → PIE → it chases (temp homing) with zombie locomotion anim.
4. Replace homing with **AIPerception (sight/hearing) + BehaviorTree + NavMesh path-follow via `UNavMoverComponent`** + `NavMeshBoundsVolume`. Team attitude solver.
5. Attacks/death/crawl (parameterized GA, ties to [[project_combat_fist_build_plan]]); health/attributes on the NPC ASC; Phase-1 frozen vs Phase-2 aggressive states.

## Tooling note (2026-06-26)
The rich `unrealclaude` MCP surface is gone/minimal (status + 2 tools only). The new **`unreal-mcp` ModelContextProtocol toolsets** (added in cdf2a57) are the surface now — call via `mcp__unreal-mcp__call_tool` with `toolset_name` + short `tool_name`. Useful toolsets: `editor_toolset.toolsets.asset.AssetTools` (get_asset_tags/find_assets/duplicate/get_dependencies), `...skeletal_mesh.SkeletalMeshTools` (get_skeleton/get_bone_names), `...blueprint.BlueprintTools`, `...object.ObjectTools` (list/get/set_properties), `...scene.SceneTools` (place actors). `get_asset_tags` reads the registry WITHOUT loading (skeleton recon). The old AZ_*Utils Python libs / `execute_script` path is NOT currently exposed.

Build rule reminder: new UCLASS/UPROPERTY/UFUNCTION → editor-closed CLI build (`Build.bat AZEditor`) + restart; body-only → Live Coding. See [[project_v2_architecture]], [[project_multipawn_class_design]], [[project_sp_first_coop_extensible]] (homing's GetPlayerPawn(0) is SP-only scaffolding; perception is per-controller/co-op-safe).
