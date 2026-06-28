---
name: project_zombie_ai_plan
description: "LIVE implementation plan (drafted 2026-06-27) for the Chalkie NPC's 'first-class zombie reaction' AI: NavMesh + UNavMoverComponent path-follow, AIPerception (sight/hearing) + team attitude, BehaviorTree/Blackboard brain, stimulus reactions + dormant->aggressive phases, GAS melee/death, replacing the temp straight-line homing. Read FIRST when building Chalkie AI/perception/NavMesh/BT. Pairs with [[project_npc_foundation]] (shipped pawn/anim foundation)."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# Chalkie AI — "first-class zombie reaction" plan (implement next)

Drafted 2026-06-27. Builds on the SHIPPED foundation ([[project_npc_foundation]]): `AAZ_PawnMoverInfectedCharacter` (Mover pawn + own ASC + AI intent surface + clearance clamp), `AAZ_InfectedAIController` (currently a TEMP straight-line homing in Tick, `GetPlayerPawn(0)` — SP-only scaffolding to be REPLACED), `UAZ_InfectedAnimInstance` + `AZ_ABP_Chalkie` (speed-matched blendspace, slide-free via axisToScaleAnimation). Goal: replace homing with real perception-driven, nav-pathed, reactive behavior. SP-first but co-op-safe per [[project_sp_first_coop_extensible]] (per-controller perception, NO `GetPlayerPawn(0)`, team attitude solver, all state on pawn/controller).

## Known carry-over item
- **"Moves too fast vs the anim playing"** — accepted consequence of the locked "faster gameplay + scale anim" choice (Mover Run=375 >> clip pace, axisToScaleAnimation speeds the clip ~1.37x). User said OK for now. RESOLVE during Phase 2/3 by choosing the AI gait per state (patrol=Walk, chase=a tuned fast gait) and/or dialing the Chalkie's gait speeds; this is a FEEL knob, not a bug.
- **Cleanup debt:** `BS_IdleWalkChase` was edited in gitignored pack content (Content/Zombie_01) → duplicate into `/Game/AZ/Blueprints/Animation/BS_AZ_Chalkie_Loco`, repoint `AZ_ABP_Chalkie`'s IdleWalkChase state (anim domain `set_blend_space`), so the tuning is version-controlled. Do early in Phase 0.

## VERIFIED nav fact (from [[project_npc_foundation]])
Engine ships `UNavMoverComponent` (`Mover/.../DefaultMovementSet/NavMoverComponent.h`); it implements `INavMovementInterface`; the AIController's PathFollowingComponent AUTO-DISCOVERS it via `FindComponentByInterface<INavMovementInterface>()`. PathFollowing calls `RequestDirectMove`/`RequestPathMove` → caches the move; the pawn's `ProduceInput` pulls it via `ConsumeNavMovementData(OutIntent, OutVelocity)` and feeds the Mover cmd. So: add the component, consume it in ProduceInput (fall back to cached AI intent when no nav move), drop a NavMeshBoundsVolume → standard BT `MoveTo`/`MoveToActor` path-follows. `AIModule` already a dep. Supports Detour Crowd avoidance for hordes.

## PHASE 0 — NavMesh + Nav<->Mover path-follow (foundation; gets pathing around obstacles)
1. **Level nav:** add `NavMeshBoundsVolume` over the playable area (RecastNavMesh-Default auto-spawns). Project Settings > Navigation Mesh: agent **Radius 25 / Height ~180** (matches capsule 25r/90hh), Max Step ~ Mover step, Walkable Slope ~38 (matches `WalkableAngle`). `P` in editor to visualize.
2. **Pawn:** `UNavMoverComponent` as a CreateDefaultSubobject on `AAZ_PawnMoverInfectedCharacter` (auto-discovered by PathFollowing). [new UPROPERTY -> editor-closed CLI build + restart]
3. **ProduceInput:** at top, try `NavMoverComponent->ConsumeNavMovementData(OutIntent, OutVelocity)` (or the 5.8 equivalent); if a nav move is pending, use it for `WorldMove` + facing; ELSE fall back to `CachedAIMoveIntentWorld`. KEEP the clearance clamp + facing rules.
4. **Smoke test:** temporarily have the controller `MoveToActor(player)` (PathFollowing) instead of homing → confirm it paths AROUND an obstacle. Then remove temp homing entirely.
5. **Open:** Detour Crowd (`UCrowdFollowingComponent` as the path-follow comp) for horde avoidance — DESIGN for it, enable in Phase 5.

## PHASE 1 — Perception (senses + team)
1. `UAIPerceptionComponent` on `AAZ_InfectedAIController` with **AISenseConfig_Sight** (e.g. SightRadius 1500, LoseSight 2000, PeripheralVisionAngle 90, DetectionByAffiliation = Enemies) + **AISenseConfig_Hearing** (range ~1500; for gunshots/impacts/noise).
2. **Team attitude:** pawn already `IGenericTeamAgentInterface` (TeamId=1). Register a team attitude solver (`FGenericTeamId::SetAttitudeSolver`, once, e.g. in GameMode/GI startup) so player team 0 = Hostile to team 1. Per-controller — co-op-safe.
3. `OnTargetPerceptionUpdated` -> write Blackboard: `TargetActor`, `LastKnownLocation`, `bCanSeeTarget`.
4. **Noise:** gunshots + the obstacle-sensor impacts ([[project_obstacle_reaction_system]]) call `UAISense_Hearing::ReportNoiseEvent` so Chalkies investigate/aggro on sound (AI-noise was already a locked decision there).

## PHASE 2 — BehaviorTree + Blackboard (the brain; replaces homing Tick)
- **Blackboard keys:** `TargetActor` (Object), `LastKnownLocation` (Vector), `HomeLocation` (Vector, for patrol/return), `bAlerted`/`bAggressive` (Bool), `AttackRange` (Float).
- **BT (root Selector, priority order):**
  1. **Attack** — `TargetActor` valid && distance <= AttackRange: face target -> `BTTask_MeleeAttack` (activates GAS) -> cooldown.
  2. **Chase** — `bCanSeeTarget`: SetGait(fast) -> `MoveTo(TargetActor, acceptance=AttackRange)`.
  3. **Investigate** — `LastKnownLocation` valid (lost sight): `MoveTo(LastKnownLocation)` -> look-around wait -> clear -> idle.
  4. **Idle/Patrol** — Phase-1 dormant = stay put (idle/twitch); Phase-2 = wander (find random nav point in radius -> MoveTo at Walk gait).
- **BTService_TargetSelection:** poll perception -> set/clear `TargetActor` + `LastKnownLocation`.
- **Gait selection** (resolves the "too fast" feel): a BTTask/Service calls pawn `SetGait()` per branch — patrol=Walk, chase=tuned fast gait.
- Controller: `RunBehaviorTree(BT)` on possess; DELETE the temp homing Tick.
- **Authoring note:** BT/BB assets have NO write-MCP (unreal-mcp `aimodule_toolset.behavior_tree` is READ-only; unrealclaude `unreal_ue` domains don't include BT). -> hand-build BT/BB in editor; C++ for BTTasks/Service/AIController/perception (new UCLASS -> CLI build + restart).
- **DECISION (default BT):** BehaviorTree+Blackboard for v1 (mature MoveTo + nav, well-trodden). StateTree (UE5.8 modern; unreal-mcp `state_tree_toolset` can inspect) is the alt if state count explodes — revisit later.

## PHASE 3 — Reactions & phases (the "first-class reaction")
- **Dormant <-> Aggressive** (lore: frozen statue Chalkie wakes): GAS tag `State.Infected.Dormant`/`.Aggressive` (or BB bool). Start dormant; WAKE on strong stimulus (sees player close / loud noise / takes damage) -> wake/scream RM montage (GAS) -> chase. **Sequence tip:** build the AGGRESSIVE loop first (perception->chase->attack), then layer dormant/wake on top.
- **Stimulus reactions:** see->alert turn+anim->chase; hear->investigate noise loc; hit->hit-react montage + aggro + face attacker; lose target->search last-known->give up->dormant/patrol.
- **Obstacle integration:** Chalkie collides with wall -> stumble + emits noise other Chalkies hear (sensor + Brace/Stumble already built — [[project_obstacle_reaction_system]]).

## PHASE 4 — Combat (attacks/death on the NPC ASC)
- `GA_MeleeAttack` (parameterized, from [[project_combat_fist_build_plan]]) granted on the NPC ASC server-side; `BTTask_MeleeAttack` activates it in range; RM lunge via `FLayeredMove_AnimRootMotion` from Zombie_01 attack Root clips ([[reference_mover_root_motion]]); MotionWarping cone-snap onto the player; damage via GAS GE.
- Health/attributes on the NPC ASC (grant in a startup step; `InitAbilitySystem` already binds actor info). Death ability -> death montage / ragdoll -> despawn.

## PHASE 5 — Polish / scale
- Do the `BS_IdleWalkChase` -> `/Game/AZ` duplicate+repoint if not done in Phase 0; tune gait speeds for feel.
- Hordes: `UCrowdFollowingComponent` + RVO; URO / `VisibilityBasedAnimTickOption` / LOD anim-tick throttle (the Option-B crowd levers — classic ABP enables these). Spawner/wave director.

## Build & tooling reminders
- New UCLASS/UPROPERTY/UFUNCTION (nav comp, perception, BTTasks, AIController members) -> **editor-closed CLI build + restart** (Live Coding can't add symbols). Body-only -> Live Coding (`unreal_execute_script console "LiveCoding.Compile"`).
- BT/BB built by hand in editor (no write-MCP). C++ offline measurement/probes via `unreal_execute_script cpp` (delete stale Generated/UnrealClaude/*.cpp between runs).
- SP-first/co-op-safe gates (no GetPlayerPawn(0); per-controller perception; team attitude solver) per [[project_sp_first_coop_extensible]].

## Open decisions to confirm at implementation
1. BT vs StateTree (default: BT first).
2. Start dormant-statues or all-aggressive (default: aggressive loop first, layer dormant after).
3. Crowd avoidance now or Phase 5 (default: Phase 5; design for it in Phase 0).
4. Patrol style for non-aggro (wander vs stationary sentry).
5. Perception ranges / aggro thresholds (tune in PIE).
