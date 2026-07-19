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

## VERIFIED nav fact (re-verified vs live engine 2026-07-18)
Engine ships `UNavMoverComponent` (`Engine/Plugins/Experimental/Mover/Source/Mover/Public/DefaultMovementSet/NavMoverComponent.h`); it implements `INavMovementInterface` **and `IRVOAvoidanceInterface`** (RVO avoidance UPROPERTYs + its own `FNavAgentProperties` live ON the component — Phase 5 horde avoidance is largely a checkbox); the AIController's PathFollowingComponent AUTO-DISCOVERS it via `FindComponentByInterface<INavMovementInterface>()`. PathFollowing calls `RequestDirectMove`/`RequestPathMove` → caches the move; the pawn's `ProduceInput` pulls it via `ConsumeNavMovementData(FVector& OutMoveInputIntent, FVector& OutMoveInputVelocity)` (line 130) and feeds the Mover cmd. **Canonical reference impl: `MoverExamplesCharacter.cpp:196`** — `bRequestedNavMovement = NavMoverComponent->ConsumeNavMovementData(CachedMoveInputIntent, CachedMoveInputVelocity)` in its input producer; crib that pattern. So: add the component, consume it in ProduceInput (fall back to `CachedAIMoveIntentWorld` when no nav move — hook verified at `AZ_PawnMoverInfectedCharacter.h:150`/`:73`), drop a NavMeshBoundsVolume → standard BT `MoveTo`/`MoveToActor` path-follows. `AIModule` already a dep; **`NavigationSystem` is NOT — add to AZ.Build.cs in Phase 0** (needed for any custom C++ nav query, e.g. wander's GetRandomReachablePointInRadius).

## PHASE 0 — ✅ DONE 2026-07-18 (verified in PIE: Chalkie MoveToActor-chased across the map, around the test wall, UP the ramp onto the elevated platform, stopped at StopDistance facing the player)
Shipped: `UNavMoverComponent` on the pawn (agent 25r/180h) + nav-consume in ProduceInput (nav wins, collapses both request flavors to a unit dir — GAIT stays the speed authority; falls back to CachedAIMoveIntentWorld); controller homing DELETED → `bDebugNavChasePlayer` MoveToActor(player, StopDistance) re-issued only when path-following Idle (path observes the moving goal actor); `NavigationSystem` in Build.cs; NavMeshBoundsVolume_Main in L_001 (X±3200, Y-2200..9400, Z-100..900); `BS_AZ_Chalkie_Loco` duplicated to /Game/AZ + ABP repointed (via unreal-mcp ObjectTools full-struct write on AnimGraphNode_BlendSpacePlayer_4.Node). Temp test wall `SM_NavTestWall` (Engine cube 8x0.3x3 @ (-500,845,150) yaw 21.9) left in L_001 — useful for Phase 1-2 testing, delete when real level geo exists.
**HARD-WON LESSONS (all cost real debugging time):**
1. **Pawns must NOT carve the navmesh**: `Capsule->SetCanEverAffectNavigation(true)` (was in BOTH v2 pawn constructors "mirroring the hero") makes the pawn's capsule a nav OBSTACLE → hex hole in the navmesh under the pawn → its own path queries fail with `InitPathfinding start point not on navmesh` → MoveToActor fail/re-issue loop that LOOKS like slow turning-in-place (~3°/s ratchet) with zero translation. Fixed to `false` in AZ_PawnMoverInfectedCharacter + AZ_PawnMoverHeroCharacter (hero carved a hole under the chase GOAL too). v1 AZ_HeroPawn.cpp:56 still carves (deprecated, untouched).
2. **Agent config lives in `[/Script/NavigationSystem.NavigationSystemV1] +SupportedAgents=(...)`, NOT RecastNavMesh class defaults** — the auto-spawned RecastNavMesh takes the fallback FNavDataConfig (35r/144h) when SupportedAgents is empty; RecastNavMesh-section ini values only feed class defaults (slope yes, radius/height no). Also 5.8: flat `AgentMaxStepHeight` is DEPRECATED (moved into per-resolution `NavMeshResolutionParams`, default 35).
3. **Never set AgentRadius/Height directly on the RecastNavMesh instance** — engine warns "unsupported", and the resulting config mismatch vs the session's agent list makes the PIE-duplicated navmesh FAIL REGISTRATION (symptom: `LogCrowdFollowing: Unable to find RecastNavMesh instance` at PIE start + AI frozen). Ini SupportedAgents requires editor restart to apply; current session ran on 35/144 (fine — more conservative than the capsule).
4. `find_actors`/SceneTools resolve the EDITOR world during PIE; reach PIE actors by explicit refPath `/Game/AZ/Maps/UEDPIE_0_L_001.L_001:PersistentLevel.<name>`. unreal-mcp StartPIE/StopPIE/CaptureViewport + LogsToolset (SetVerbosity LogNavigation=Verbose) = a full self-serve PIE test harness.

## PHASE 0 (original plan — kept for reference)
1. **Level nav:** add `NavMeshBoundsVolume` over the playable area (RecastNavMesh-Default auto-spawns). Project Settings > Navigation Mesh: agent **Radius 25 / Height ~180** (matches capsule 25r/90hh), Max Step ~ Mover step, Walkable Slope ~38 (matches `WalkableAngle`). `P` in editor to visualize.
2. **Pawn:** `UNavMoverComponent` as a CreateDefaultSubobject on `AAZ_PawnMoverInfectedCharacter` (auto-discovered by PathFollowing). [new UPROPERTY -> editor-closed CLI build + restart]
3. **ProduceInput:** at top, try `NavMoverComponent->ConsumeNavMovementData(OutIntent, OutVelocity)` (or the 5.8 equivalent); if a nav move is pending, use it for `WorldMove` + facing; ELSE fall back to `CachedAIMoveIntentWorld`. KEEP the clearance clamp + facing rules.
4. **Smoke test:** temporarily have the controller `MoveToActor(player)` (PathFollowing) instead of homing → confirm it paths AROUND an obstacle. Then remove temp homing entirely.
5. **Open:** Detour Crowd (`UCrowdFollowingComponent` as the path-follow comp) for horde avoidance — DESIGN for it, enable in Phase 5.

## PHASE 1 — ⚙ CODE WRITTEN 2026-07-18, awaiting the end-of-session CLI build + restart (new UPROPERTYs — NOT Live-Codeable; the session's running binary still has the temp distance+LOS+crouch prototype, which works)
Implemented in `AZ_InfectedAIController.h/.cpp` (full rewrite):
- **ALL SIX senses** configured up front on the inherited PerceptionComponent: Sight (radius 800 / lose 1500 / half-angle 70° cone / MaxAge 5 — the only aggro source), Hearing (1500, awaits ReportNoiseEvent), Damage (aggro-on-hit, awaits GAS ReportDamageEvent), Touch (bump-wake), Prediction (BT look-ahead), Team (horde alerts via FAITeamStimulusEvent, TeamAlertRadius 2000 lives on the EVENT not the config). Unused senses are inert until something reports.
- **Target selection = Tick POLL of GetCurrentlyPerceivedActors(Sight)**, NOT events — KEY: the crouch filter must re-evaluate as DISTANCE changes (event-driven filtering has a gap: sight fires success ONCE at the perimeter; a rejected crouched target never re-fires as it creeps closer → would never be detected). Already-my-target ⇒ crouch exemption (crouching mid-chase doesn't help). Crouch read via IGameplayTagAssetInterface `Movement.Crouching` (hero publishes it, AZ_PawnMoverHeroCharacter.cpp:567).
- **Teams**: hero pawn already IGenericTeamAgentInterface team 0; Chalkie team 1; controller adopts pawn team on possess. **Engine DEFAULT attitude solver = `A != B ? Hostile : Friendly`** (AIInterfaces.cpp) → hero auto-hostile at the SENSE level, no custom solver needed; controller GetTeamAttitudeTowards override refines NoTeam→Neutral for brain-level filtering. Sight affiliation: enemies+neutrals ON (safety net), friendlies OFF (no horde self-spam).
- OnTargetPerceptionUpdated = last-known-location bookkeeping only (BT Investigate consumes later); hearing/damage promote targets via BT, not laser-lock.
- Tick brain (temp until BT): poll→freshest target→MoveToActor chase→lose on grace expiry. UPROPERTY tunables: SightRadius/LoseSightRadius/PeripheralVisionHalfAngleDegrees/CrouchDetectRange(250)/HearingRange/TeamAlertRadius/LoseTargetGraceSeconds(3)/StopDistance(150)/bTickBrainEnabled/bLogChaseProbe.
- GetPlayerPawn(0) GONE — perception+team driven, co-op-safe.
**PIE-verify after build**: cone (sneak BEHIND works now — 360° eyes are gone), crouch-creep to 250, LOS hide, escape, teams. Then Phase 2 BT.

## PHASE 1 (original plan — kept for reference)
1. `UAIPerceptionComponent` on `AAZ_InfectedAIController` with **AISenseConfig_Sight** (e.g. SightRadius 1500, LoseSight 2000, PeripheralVisionAngle 90, DetectionByAffiliation = Enemies) + **AISenseConfig_Hearing** (range ~1500; for gunshots/impacts/noise).
2. **Team attitude:** pawn already `IGenericTeamAgentInterface` (TeamId=1). Register a team attitude solver (`FGenericTeamId::SetAttitudeSolver`, once, e.g. in GameMode/GI startup) so player team 0 = Hostile to team 1. Per-controller — co-op-safe.
3. `OnTargetPerceptionUpdated` -> write Blackboard: `TargetActor`, `LastKnownLocation`, `bCanSeeTarget`.
4. **Noise:** gunshots + the obstacle-sensor impacts ([[project_obstacle_reaction_system]]) call `UAISense_Hearing::ReportNoiseEvent` so Chalkies investigate/aggro on sound (AI-noise was already a locked decision there).

## PHASE 2 — ⚙ C++ DONE + COMMITTED (`7a9d9e7`, 2026-07-18); assets + graph = NEXT SESSION's first job
Shipped C++: `UAZ_BTService_ChalkieTargetSelection` (perception→BB mirror @0.1s; instanced; arms LastKnownLocation ONLY on the fresh→lost EDGE so Investigate's ClearBBKey isn't re-overwritten), `UAZ_BTTask_ChalkieSetGait`, `UAZ_BTTask_ChalkieClearBBKey`; controller: `BehaviorTreeAsset` UPROPERTY → RunBehaviorTree on possess + seeds HomeLocation/AttackRange(=StopDistance); `UpdatePerception()` frame-guarded (controller Tick + BT service share one poll); Tick brain = fallback, auto-stands-down when BrainComponent runs; BB names in `AZ_ChalkieBBKeys` namespace (controller header).
**NEXT SESSION (editor open, new binary active):**
1. Create `/Game/AZ/AI/`: `BB_Chalkie` (keys EXACTLY: TargetActor Object:Actor, LastKnownLocation Vector, HomeLocation Vector, AttackRange Float, bAlerted Bool, bAggressive Bool), `BT_Chalkie` (BlackboardAsset=BB_Chalkie), `BP_AZ_InfectedAIController` (parent = native controller). Hand-create is 3 min and SAFE; the python script at scratchpad/make_ai_assets.py NEVER RAN (editor closed first) and is UNVALIDATED — if scripting, validate on a throwaway asset first.
2. BT graph (root Selector + service on root): [Chase: dec TargetActor IsSet (observer aborts lower) → SetGait Run → MoveTo TargetActor acc 150 → RotateToFaceBBEntry] [Investigate: dec LastKnownLocation IsSet → SetGait Run → MoveTo LastKnownLocation acc 100 → Wait 3 → ClearBBKey LastKnownLocation] [Home: SetGait Walk → MoveTo HomeLocation acc 100 → Wait 5].
3. BP_AZ_InfectedAIController: set BehaviorTreeAsset=BT_Chalkie; BP_AZ_Chalkie CDO: AIControllerClass → BP_AZ_InfectedAIController_C (re-place the level Chalkie if the instance serialized the old class — placed instances keep instanced-component/class state; same gotcha as the walking-mode swap).
4. Restart notes: SupportedAgents 25/180 now applies → navmesh may prompt/need rebuild (Build→Build Paths); PIE matrix: sight cone (sneak BEHIND), crouch-creep 250, LOS hide → INVESTIGATE last-known → give up → walk home; escape past 1500.

## PHASE 2 (original plan — kept for reference)
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

## PHASE 2 — ✅ LIVE 2026-07-18 late-night: BT VERIFIED IN PIE (chase 274cm/s → catch → re-engage; hand-built tree after scripted-tree serialization failure). Session lessons: (1) runtime-tree-only BT assets DO NOT survive save/load (Children stripped, "missing decorator node" warnings) — author BT graphs in the BT EDITOR, always; scripted BB creation (keys) DID work via cpp harness. (2) LC initializer quirk: first cpp-script compile of an editor session no-ops; identical-content resubmits dedupe — salt the content. (3) python execute_script ALSO requires @Description (silent job-level fail without it). (4) **NavMeshBoundsVolume transform DID NOT survive save/reload** (reverted to origin/scale-1 → "no green" mystery, all MoveTo fail, BT flickers Chase↔Home) — after ANY reload, VERIFY the volume bounds; consider hand re-scaling in Details (proper PostEditMove path) or accept rebuild-paths-on-open. (5) RotateToFaceBBEntry in a chase sequence = stuck-loop when the target orbits (TargetActor never CHANGES → no ValueChange abort) — REMOVED; the controller's bArrived stare already covers facing; Chase = SetGait→MoveTo→Wait(0.5) breather. (6) Force Success decorator on Investigate's MoveTo (unreachable last-known would loop forever). Grace raised 3→4.5s (BP override).

## TLOU CODE PACKAGE — ⚙ WRITTEN 2026-07-19 (new UPROPERTYs → compiles at next editor-closed CLI build):
1. **Alert beat**: calm→ALERTED (freeze + body-snap toward stimulus, `AlertDelaySeconds=0.6`) →aggressive; `InstantDetectRange=350` = no-beat zone. Lives in UpdatePerception; BT unchanged (TargetActor just publishes late).
2. **Glimpse investigation**: target lost DURING the alert window → LastKnownLocation (controller + BB) = the "huh? let me check" walk-over.
3. **Movement noise**: hero `ReportMovementNoise()` (called from ProduceInput, throttled `NoiseIntervalSeconds=0.4`): sprint(>450)=1200, run(>250)=800, walk(>80)=300, ×`CrouchNoiseScale=0.25` when crouched, still=silent. Controller handler: heard hostile noise while not chasing → BB LastKnownLocation → Investigate branch pulls to the SOUND. Sneak loop complete: crouch-walk≈silent, sprint=dinner bell.
4. **Config-push fix**: `ApplyPerceptionTuning()` in BeginPlay re-pushes SightRadius/LoseSightRadius/PeripheralVisionHalfAngleDegrees/HearingRange into the sense configs + RequestStimuliListenerUpdate → the BP floats are now live (constructor-only push ignored BP overrides!).
5. **Bug fix**: stale PerceivedTarget now cleared after grace → re-arms the alert beat AND restores crouch-sneak after a first chase (stale ptr kept the "already my target" crouch exemption forever).
6. bAlerted/bAggressive mirrored to BB each poll (Phase-3 anim/branch hooks ready).
BT edits done by hand in-editor (user): Chase = SetGait→MoveTo→Wait(0.5) (RotateToFaceBBEntry REMOVED — stuck-loop when target orbits; controller bArrived stare covers facing); Force Success on Investigate MoveTo; grace 3→4.5s on BP.

## ★ NEXT SESSION — NATURAL INVESTIGATE→HOME CYCLE (planned 2026-07-19 w/ user; proposals AGREED to build)
User's tuning (BP overrides, live via ApplyPerceptionTuning after `ef04280` build): **Sight 500 / Hearing 700 / LoseSight 800** (tight, intimate encounters; easier escape). Current investigate = beeline→statue-stand-3s→pivot→march home = robotic. The build list:
1. **`AZ_BTTask_FindPointNear`** (C++, the workhorse): params = Center BB key + Radius + Out BB key; `UNavigationSystemV1::GetRandomReachablePointInRadius` (NavigationSystem dep already in Build.cs). REUSED for both search points AND home idle-wander.
2. **`AZ_BTTask_ChalkieScanAround`** (C++): look-around via facing pulses — SetDesiredFacingWorld +120°, −120°, forward @ ~0.7s apart (body turns via Mover spring; reads as scanning; turn ANIMS wire later via the pack ABP's Turns SM).
3. **New BB keys** (hand-add, 30s): `SearchLocation` (Vector), `bInvestigateUrgent` (Bool).
4. **Investigate branch rework** (hand, BT editor): MoveTo(LastKnown) → ScanAround → **Loop×2** [stock UBTDecorator_Loop on inner Sequence: FindPointNear(LastKnown, 400, SearchLocation) → MoveTo(SearchLocation) → Wait(1) ] → Wait(1.5 "settle" beat) → ClearBBKey(LastKnown). = walks the area checking spots instead of statue-standing.
5. **Gait by stimulus confidence**: heard-only noise → investigate at WALK (wary/curious, TLOU); lost-after-chase → RUN (it KNOWS). Controller writes `bInvestigateUrgent` at ARM time (hearing handler = false unless escalated; service fresh→lost edge = true); upgrade `AZ_BTTask_ChalkieSetGait` with optional "from-urgency" mode reading the BB bool.
6. **Escalation memory**: controller counter — 2+ investigations within ~30s → bInvestigateUrgent=true + wider search radius (it's "onto you"). Decays.
7. **Home idle-wander** (patrol-lite): Home branch → Loop [FindPointNear(Home, 400, SearchLocation) → MoveTo → Wait(~8)] — no more eternal statue at the post. (Re-aggro/re-investigate still preempt via decorator aborts — already work.)
8. **ANIM PASS (user ask — sell the behavior with body language):**
   a. AUDIT Zombie_01 pack anims first (asset search: idle variants / scream / alert / look-around / turns — catalog what exists before designing).
   b. **Wire the Turns SM** — the pack ABP already HAS it (Left/Right 90/180 states + our EventGraph keeps AnimNotify_EndRotation/RotationOverrideClear handlers); feed its driver vars from UAZ_InfectedAnimInstance (rotation-delta detection) so ScanAround's facing pulses + giving-up pivots play REAL turn anims instead of skating. (skill anim-debug-pitfalls applies.)
   c. **Alert anim on the notice beat** — head-snap/scream at alerted→aggressive commit (bAlerted/bAggressive already mirrored to BB + readable on the AnimInstance via the controller); the pack's scream ~= the Phase-3 wake anim, pull it forward if it exists.
   d. **Look-around idle** at search points + idle VARIATIONS for home wander/dormant (anti-statue).
   e. Slot/montage vs SM state per the RAIL DOCTRINE ([[project_combat_fist_build_plan]]): scream/flinch = anim-led one-shots → montage slot; turns = SM.
9. **GAS INTEGRATION (user directive 2026-07-19: GAS FIRST — it's the RPG component of the game):**
   a. Infected phase = **ASC loose gameplay tags**, not just BB bools: add native tags `State.Infected.Dormant` / `State.Infected.Alerted` / `State.Infected.Aggressive` to AZ_GameplayTags; controller (server) publishes on phase transitions to the pawn's OWN ASC. BB bAlerted/bAggressive stay as BT-local mirrors of the ASC truth.
   b. **AnimInstance reads the phase from the pawn's ASC** (IGameplayTagAssetInterface — already on the pawn), NOT from the controller — this is what makes alert/scream anims work on CLIENT-side Chalkies in coop (AI controllers exist only on the server; the ASC replicates).
   c. RPG hooks (design now, build later): detection/noise ranges as ATTRIBUTE-modifiable (player Stealth attribute scales noise ranges via GE; Chalkie Rage attribute scales gait speeds/aggro ranges; per-variant Chalkie stats = AttributeSet rows). Matches v2 doctrine: GAS-as-bookkeeping, tags bridge GAS↔anim.
10. **COOP-SAFETY PASS (second priority, but keep the rails per [[project_sp_first_coop_extensible]]):**
   a. Movement noise: gate `ReportMovementNoise()` with `HasAuthority()` (stimulus must be server-authoritative; currently fires on the owning client too — flagged in code comment).
   b. Already coop-safe ✓: per-controller perception, team attitude targeting (no GetPlayerPawn(0)), server-side BT/controller, nearest-hostile selection (picks the closest of MULTIPLE players naturally).
   c. Bar: 2-player listen-server PIE "just works" — smoke it once the phase tags replicate (client Chalkie anims driven by ASC tags = the test).
11. PIE tune: search radius/count, settle beats, walk-vs-run thresholds, anim timings.
ORDER: C++ tasks first (one editor-closed build), BB keys + BT edits by hand in editor (NEVER scripted — serialization lesson), anim audit + wiring, PIE. Also FIRST: check NavMeshBoundsVolume transform after editor start (reload-revert gotcha) + Build Paths.

## TLOU-GRADE REALISM ROADMAP (user ask 2026-07-18: "realistic like The Last of Us")
Feel gaps vs TLOU, mapped to phases — the plan already carries most of it:
1. **Reaction beat** (TLOU enemies take ~0.5s to *notice* — head snap, vocal bark, THEN chase; instant-aggro reads robotic) → Phase 3 alert states: Dormant→Alerted(turn+bark, 0.3-0.6s)→Aggressive. bAlerted/bAggressive BB keys already exist.
2. **Active search** (TLOU searches NEARBY COVER/points after last-known, not stand-and-wait-3s) → Phase 3: replace Investigate's Wait with 2-3 EQS/random-reachable-point peeks around LastKnownLocation before giving up.
3. **Attack standoff** (close → attack, circle, reposition — not stop-and-stare) → Phase 4 GA_MeleeAttack replaces the Wait(0.5) breather; stare IS the placeholder.
4. **Vocal/anim telegraphs** (barks on spot/lose/search) → Phase 3 wake/scream + notify-driven audio.
5. **Noise investigation** (thrown-object distraction loop) → Hearing sense ALREADY registered; Phase 1-noise: report footstep/gunshot noise events → Investigate branch already consumes LastKnownLocation.
6. **Group dynamics** (one spots → alerts others, flanking) → Team sense registered; Phase 5.
Keep the BT SHALLOW (selector of 3-4 branches) — TLOU-like depth comes from perception states + anim telegraphs, not tree complexity.

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
