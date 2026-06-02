# AZ Project Memory — "CHALK"

UE 5.8 third-person survival-horror project. **Game title: CHALK.** Setting: 2024 Montreal (Old Saint-Laurent), modern-day outbreak driven by a government-distributed drug "Elysium" / street name "Chalk" that mutates addicts into rotting, chalk-skinned aggressive infected. Body-horror modern urban survival, NOT generic zombies. See [project_lore_chalk.md](project_lore_chalk.md) for the canonical lore + design implications.

Migrated from UE 5.7.4 on 2026-05-10. Active work: v2 character system (hero pawn — `AAZ_PawnMoverHeroCharacter`) on `feature/rootmotion`. Engine path: `C:\UnrealEngine\Engine\`. Project path: `C:\UnrealEngine\Games\AZ`.

## Skills (lazy-loaded — invoke when triggered)

- `az-workspace` — workspace folder map; load at start of any non-trivial task.
- `agent-and-research-discipline` — check memory first, teams of 3-8 agents, validate findings, no read-approval, full absolute paths.
- `cpp-build-livecoding` — CLI build, LiveCoding.Compile via MCP, read errors from UnrealBuildTool Log.txt.
- `bp-to-cpp-port` — 7-gate dual-reviewer checklist for any BP→C++ port; past-misses catalog.
- `unrealclaude-mcp-tools` — MCP tool decision tree, deferred-tool loading, search_nodes/inspect/get_nodes gotchas.
- `anim-debug-pitfalls` — mesh spin, A-pose, SM stuck, OffsetRootBone enum, TIP threshold, IsMoving formula.
- `az-cpp-utility-tools` — AZ_BlueprintNodeUtils / AZ_AnimGraphNodeUtils / AZ_AnimBlueprintUtils / AZ_ChooserUtils / AZ_PoseSearchUtils / AZ_SkeletonUtils.
- `gasp-parity-reference` — index of every gasp_*.md file by topic.
- `asset-modification-via-python` — Python recipes via `unreal_execute_script`; GC crash rule; PoseSearch/notify bridges.

## Project Overview
- **Title:** CHALK ([project_lore_chalk.md](project_lore_chalk.md)) — Montreal 2024 modern survival horror; rotting drug-addict infected ("Chalkies"), conspiracy-faction antagonist
- **Type:** Third-person survival/action-horror with inventory, equipment, combat
- **Inventory:** dual implementation — Old (UMG, deprecated) + New (CommonUI, active migration); Fragment/Manifest pattern (TInstancedStruct), composite description UI, FastArray replication — see [inventory-system.md](inventory-system.md)
- **Weapon system:** single-actor flow (carried + active states); Relaxed/Aim socket interp; GA_Shoot Single/Auto/Burst; FOV+boom+offset per stance — see [weapon_swap_architecture.md](weapon_swap_architecture.md), [feedback_ik_setup.md](feedback_ik_setup.md)
- **GAS:** custom ASC, 3+ AttributeSets (WeaponAttributeSet on PlayerState); GA_Jump/Crouch/Aim/Shoot/Interact; tags Ability.State.* / Ability.Cooldown.* / Weapon.Slot.* — see [project_gas_gameplay.md](project_gas_gameplay.md)
- **Animation system:** C++ AnimInstance drives bools (bIsJumping/Falling/Crouching/Aiming/Shooting/WantsAimPose); per-stance blend spaces; Rifle SM 4 loco + 3 jump states; Layered Blend per Bone (spine_02); AO_Rifle_Aim from camera trace; WeaponAnimSpeedMultiplier
- **Naming:** Old `AZ_Inv_*`, New `AZ_Inv_CommonUI_*`; widgets `WBP_`, components `BP_`, data assets `DA_`

## Key Source Directories
- `Source/AZ/Public/Animation/` — AZ_AnimInstance, AZ_LocomotionTypes, all AZ_*Utils
- `Source/AZ/Public/Character/` — AZ_HeroPawn (Mover-based, idle TIP accumulator)
- `Source/AZ/Public/Player/` — AZ_PlayerController (IMC, GAS InputConfig)
- `Source/AZ/Public/Equipment/` — Equipment actors, components, proxy mesh
- `Source/AZ/Public/Items/` — AZ_Item, AZ_PickupItem, ItemComponent
- `Source/AZ/Public/Inventory/` — Shared inventory data
- `Source/AZ/Public/InventoryOld/` — Old UMG inventory (deprecated)
- `Source/AZ/Public/InventoryUI/` — New CommonUI inventory (active)
- `Source/AZ/Public/Weapon/` — AZ_Weapon

## Workflow / Behavior Rules (full content surfaced via skills above)
- [feedback_console_variables_lifetime.md](feedback_console_variables_lifetime.md) — never `static TAutoConsoleVariable<>`; register in FAZModule, unregister by NAME — fixes both shutdown crash variants
- [feedback_build_paging_file_parallelism.md](feedback_build_paging_file_parallelism.md) — build fails "C3859 / paging file too small / C1076 heap limit" = host memory, NOT code; cap `MaxParallelActions=4` + `bAllowXGE=false` in BuildConfiguration.xml. Also: Live Coding patches don't survive editor restart.

- [feedback_check_memory_first.md](feedback_check_memory_first.md), [feedback_use_agent_teams.md](feedback_use_agent_teams.md), [feedback_validate_agent_findings.md](feedback_validate_agent_findings.md), [feedback_no_read_approval.md](feedback_no_read_approval.md), [feedback_file_paths.md](feedback_file_paths.md) — covered by skill `agent-and-research-discipline`
- [feedback_bp_to_cpp_port_review_checklist.md](feedback_bp_to_cpp_port_review_checklist.md), [feedback_deep_pin_inspection.md](feedback_deep_pin_inspection.md) — covered by skill `bp-to-cpp-port`
- [feedback_animgraph_node_reference_wiring.md](feedback_animgraph_node_reference_wiring.md), [feedback_animbp_post_event_vs_thread_safe.md](feedback_animbp_post_event_vs_thread_safe.md), [feedback_blendstack_input_node_ref.md](feedback_blendstack_input_node_ref.md) — covered by skill `anim-debug-pitfalls`
- [feedback_python_gc_crash.md](feedback_python_gc_crash.md), [feedback_retarget_root_motion.md](feedback_retarget_root_motion.md), [feedback_az_chooser_utils_python_api.md](feedback_az_chooser_utils_python_api.md) — covered by skills `cpp-build-livecoding` + `asset-modification-via-python`
- [feedback_sm_transition_access.md](feedback_sm_transition_access.md), [feedback_transition_rule_tools.md](feedback_transition_rule_tools.md) — covered by skills `unrealclaude-mcp-tools` + `az-cpp-utility-tools`
- [feedback_chooser_column_reorder.md](feedback_chooser_column_reorder.md) — chooser editor reorders columns on reload (OutputStruct trails inputs); re-inspect indices every session before SetCell-by-index; check set_cell return values

## GASP Reference (full per-file index in skill `gasp-parity-reference`)
- **Standing rule:** GASP imported into AZ at `/Game/Blueprints/` — query via `unrealclaude` MCP on port 3000. Ignore external `C:\UnrealEngine\Games\GameAnimationSample` and the deprecated `gassample` MCP.
- [gasp_animbp_architecture.md](gasp_animbp_architecture.md), [gasp_animbp_full_audit.md](gasp_animbp_full_audit.md) — ABP architecture + authoritative deep audit (every node + binding, BS inner non-defaults, all driver fns 1-line, SM states/transitions, OnStateEntry events)
- [gasp_pawn_bp_full.md](gasp_pawn_bp_full.md), [gasp_pawn_cpp_port_plan.md](gasp_pawn_cpp_port_plan.md) — pawn BP audit + port inventory
- [gasp_character_movement.md](gasp_character_movement.md), [gasp_movement_modes.md](gasp_movement_modes.md) — Mover pawn + per-mode BPs
- [gasp_data_model.md](gasp_data_model.md), [gasp_data_model_full.md](gasp_data_model_full.md) — enums/structs summary + field-by-field with parity check
- [gasp_sm_tip_flow.md](gasp_sm_tip_flow.md), [gasp_orientation_intent_tip.md](gasp_orientation_intent_tip.md) — SM topology + per-mode TIP rules
- [gasp_update_logic_flow.md](gasp_update_logic_flow.md) — IsMoving = Trj_FutureVelocity+Accel; 6-state-enum 5-field tracking
- [gasp_posesearch_choosers.md](gasp_posesearch_choosers.md), [reference_cht_chooser_structure.md](reference_cht_chooser_structure.md), [reference_gasp_anim_notifies.md](reference_gasp_anim_notifies.md) — PoseSearch/Choosers/notify catalog
- [gasp_project_settings.md](gasp_project_settings.md), [gasp_framework_cameras_rigs.md](gasp_framework_cameras_rigs.md), [gasp_actor_components_and_notifies.md](gasp_actor_components_and_notifies.md), [gasp_cpp_architecture.md](gasp_cpp_architecture.md) — settings, framework, components, C++ overview

## AZ Project State (sessions, plans, decisions)
- [project_architecture_rationale.md](project_architecture_rationale.md) — Why Mover (over CMC) and why Chooser+SM+BlendStack (over classical SM+BlendSpace+Montage); concrete trade-offs, costs paid, signals to revisit
- [project_idle_tip_implementation.md](project_idle_tip_implementation.md) — speed-independent accumulator, 60° commit, working baseline (commit `b5c076e1`)
- [project_input_stack_rt_mirror.md](project_input_stack_rt_mirror.md) — RT IMC + 21 IA mirror; `Get_Gait` inverted
- [project_session_2026-04-22_gasp_pawn_done.md](project_session_2026-04-22_gasp_pawn_done.md) — pawn port Phases 1-8 log
- [project_gasp_pawn_port_audit_2026-05-02.md](project_gasp_pawn_port_audit_2026-05-02.md) — re-audit driving next round of fixes
- [project_gasp_animbp_cpp_port_plan.md](project_gasp_animbp_cpp_port_plan.md), [project_gasp_abp_port_ledger.md](project_gasp_abp_port_ledger.md) — ABP port plan + 107-var/63-fn ledger
- [project_session_2026-04-24_gasp_animbp_cpp_done.md](project_session_2026-04-24_gasp_animbp_cpp_done.md) — ABP Phases 0-8 log
- [project_session_2026-04-26_abp_first_motion.md](project_session_2026-04-26_abp_first_motion.md) — ABP wiring, character moves
- [project_local_plugin_patches.md](project_local_plugin_patches.md) — re-apply after upstream UnrealClaude sync. v1.5.0 in place 2026-05-25. Patches: #1 get_node_pins bindings, #1d recursive CollectGraphs, #1c SkipScriptPermissionDialog CVar (module-lifecycle), #1b execute_script in SIMPLE_TOOL_NAMES, #4 six-site UE5.8 FSharedString/FIterator shim, #3 engine USmoothWalkingMode MinimalAPI
- [project_mm_implementation_plan.md](project_mm_implementation_plan.md), [project_motion_matching_plan.md](project_motion_matching_plan.md), [project_motion_matching_progress.md](project_motion_matching_progress.md) — MM plan/progress
- [project_root_motion_mode.md](project_root_motion_mode.md) — Mover RM = three-part chain (AnimationWarping plugin + RootMotionFromEverything on AnimInstance + FLayeredMove_RootMotionAttribute queued on MoverComp); engine path, no manual extraction
- [project_v2_architecture.md](project_v2_architecture.md) — v2 character system: GAS gameplay × Chooser+PoseSearch anim × Mover physics; tags bridge GAS↔chooser, trajectory bypasses GAS. Always-back-to-camera rotation, parameterized abilities (one GA per event type, EventData picks variant), GAS-as-bookkeeping (SM fires anim immediately), AI parity via Mover virtual `ResolveRotationTarget()`, hierarchical CHT_v2_* files, co-op-ready foundation (deterministic trajectory + GAS-mediated state), CVar-gated v1↔v2 promotion. **Engine: UE 5.8 (migrated 2026-05-10).** GASP-parity dropped.
- [project_v2_locomotion_progress.md](project_v2_locomotion_progress.md) — LIVE resume point for the v2 locomotion build: iteration ladder (Idle✅→Walk✅→intent-IsMoving✅→MM in progress→directions→gaits→starts→stops), option-A FTransformTrajectory decision, MM recipe, and the exact next action (CLI build pending uncompiled edits, then PoseHistory node + bUseMM). **Read first when resuming locomotion work.**
- [project_locomotion_sm_refactor_plan.md](project_locomotion_sm_refactor_plan.md) — plan to extract DeriveSMState into a dedicated `UAZ_LocomotionStateMachine` (pure C++ Tick, NOT an AnimGraph SM) before traversal states; 5 incremental steps + the ABP-reference check gate; ChooserContext bool-vs-tag audit (clean — no promotions); latent AI-jump gap. RM jump + MP proxy fixes done & committed `bc20e53`. Step 1 (dormant class) underway.
- [project_jump_system_status.md](project_jump_system_status.md) — v2 jump system: RMAction-driven (gravity/floor-snap-free), moving-jump MVP DONE (chooser `bIsMoving` column + walk/run `…_ALL` foot-aware rows), foot rebind fix. **Known issues driving next steps:** run `…_ALL` lands-to-stop → componentized MM `Land2Run`; uneven-terrain float-then-drop → descent must be gravity+floor-governed (land on real floor contact, not baked clip end). Both fixed by takeoff(RM)→gravity-fall→floor-contact MM-land.
- [project_physics_jump_plan.md](project_physics_jump_plan.md) — **DECIDED 2026-06-02, next-session plan:** pivot jumps RM→PHYSICS (launch impulse + engine Falling mode + cosmetic anim + MM land). Fixes height divergence / speed pop / run-lands-to-stop with less code (reuse Falling). `SetHandleJump(true)`, stop RM-driving jumps, SM air governed by MovementMode==Falling, `InAirLoop` activated, MM land DB. RMAction kept for vault/mantle. Self-contained step list + open decisions + PIE validation.
- [project_traversal_system.md](project_traversal_system.md) — **foundational vision (2026-06-02):** unified obstacle-aware movement. Jump/traversal input traces ahead → a Chooser (CHT) decides if the obstacle fits a traversal entry → **fits = RM action** (RMAction + AnimationWarping to traced edges); **no fit = physics jump**. The `trace → chooser-by-context → warped-RM-action | physics-fallback` pattern is GASP's Traversal architecture and the **base for all contextual game movement** (vault/mantle/hurdle/climb/slide now; cover/interactions later). Physics jump is the first leg to build.
- [project_multipawn_class_design.md](project_multipawn_class_design.md) — multi-pawn scaling rules (hero now; cars/motorbikes/helicopters confirmed long-term, 2026-05-11). Input split (PC = IMC stack + ability InputConfig; pawn = SetupPlayerInputComponent + cache + IMC accessor). Two-ASC model (player ASC on PlayerState cross-pawn; vehicle ASC per vehicle for subsystem damage). Driver pose runs on hero AnimInstance via `State.InVehicle.Driver.*` tags. Seat-as-component recommended. Target folder layout locked. Vehicle physics backend (pure Mover vs ChaosVehicle) deferred.
- [project_sp_first_coop_extensible.md](project_sp_first_coop_extensible.md) — scope rule (2026-05-11): CHALK ships single-player first, co-op is a *possible future* extension. Bar: "2-player listen-server PIE should just work without refactor." ALWAYS rules (no `GetPlayerController(0)`, state-location by scope on PS/GS/GI/LocalPlayer, all mutations through GAS/replicated events, Mover for all movement, per-player AI perception/HUD/camera, save splits world-vs-character). AVOID list. DEFER list (lobby/voice/late-join/dedicated server). Some features explicitly OK as SP-only (photo mode, save-anywhere). v2 character system already on the right rails — no extra co-op work needed.
- [project_mover_5_7_to_5_8_diff.md](project_mover_5_7_to_5_8_diff.md) — Mover plugin 5.7 → 5.8 impact analysis for AZ. 4 method signature breaks (all add `const FMoverSimContext&` param), ~30 min mechanical migration cost. Lists safe symbols + relevant additive 5.8 features (`GetGameplayTags` virtuals, `ForEachActiveMoveOfType`, rollback infrastructure). Use at migration trigger.
- [project_ue58_migration_2026-05-10.md](project_ue58_migration_2026-05-10.md) — record of the actual 5.7→5.8 migration. Surprises beyond the predicted Mover diff (FSharedString in JSON, FAnimNotifyEvent.Link, FScriptMapHelper::FIterator no-deref, OverrideMotionMatchingBlendSettings 2-arg). AnimationToolsBundle disabled pending 5.8 port. SmoothWalkingMode MinimalAPI engine patch needed (see project_local_plugin_patches.md §3).
- [project_iris_replication_5_8.md](project_iris_replication_5_8.md) — Iris in 5.8: enabled 2026-05-15, PIE-MP passed 2026-05-21. Mover via NetworkPrediction SetupIrisSupport; GAS tag-count routes through `FMinimalReplicationTagCountMap` Iris NetSerializer (transitional surface, not a legacy fallback) until 5.9+.

## Catalogs / External References
- [reference_noweapon_anim_catalog.md](reference_noweapon_anim_catalog.md) — 191 NoWeapon anims at `/Game/AZ/Assets/RTG/NoWeapons/RootMotions/` by group; ✅ stand turn 90/180 L/R; ⚠ no sprint stop, slide exit, backward run stops
- [reference_bp_node_tools.md](reference_bp_node_tools.md), [reference_animgraph_node_tools.md](reference_animgraph_node_tools.md) — full API tables for AZ_BlueprintNodeUtils + AZ_AnimGraphNodeUtils (procedural surface in skill `az-cpp-utility-tools`)
- [reference_ue5_python_posesearch.md](reference_ue5_python_posesearch.md), [reference_ue5_python_anim_notifies.md](reference_ue5_python_anim_notifies.md) — Python recipes (procedural surface in skill `asset-modification-via-python`)
- [reference_zaggoth_lh_ik_tutorial.md](reference_zaggoth_lh_ik_tutorial.md) — FABRIK bone-space LH IK tutorial summary
- [ue_ai_plugins_comparison.md](ue_ai_plugins_comparison.md) — unrealclaude vs SpecialAgent vs UnrealGenAISupport

## CommonUI Migration Patterns
- UUserWidget → UCommonUserWidget / UCommonButtonBase
- Custom delegates → FCommonButtonBaseClicked
- InventoryMenuBase → UCommonActivatableWidget
- Fragment Assimilate() target: UAZ_Inv_CommonUI_CompositeBaseWidget*
