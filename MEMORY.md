# AZ Project Memory — "CHALK"

UE 5.8 third-person survival-horror, **title CHALK**: 2024 Montreal outbreak, drug "Elysium/Chalk" mutates addicts into chalk-skinned infected ("Chalkies") — body-horror, NOT generic zombies. Lore: [project_lore_chalk.md](project_lore_chalk.md). Systems/naming/dirs quickref: [reference_project_quickref.md](reference_project_quickref.md). Migrated 5.7.4→5.8 2026-05-10. Engine `C:\UnrealEngine\Engine\`, project `C:\UnrealEngine\Games\AZ`. Active: Chalkie NPC AI on `feature/NPC`.

## Skills (lazy-loaded — invoke when triggered)
- `az-workspace` — folder map; load at start of any non-trivial task.
- `agent-and-research-discipline` — memory first, 3-8 agent teams, validate findings, no read-approval, absolute paths.
- `cpp-build-livecoding` — CLI build, LiveCoding.Compile via MCP, errors from UnrealBuildTool Log.txt.
- `bp-to-cpp-port` — 7-gate dual-reviewer checklist; past-misses catalog.
- `unrealclaude-mcp-tools` — MCP decision tree, deferred-tool loading, per-tool gotchas.
- `anim-debug-pitfalls` — mesh spin, A-pose, SM stuck, OffsetRootBone enum, TIP threshold, IsMoving formula.
- `az-cpp-utility-tools` — the six AZ_*Utils C++ scripting libraries.
- `gasp-parity-reference` — index of gasp_*.md files.
- `asset-modification-via-python` — Python via unreal_execute_script; GC crash rule; notify bridges.

## Workflow rules (feedback files; detail inside each)
- [feedback_console_variables_lifetime.md](feedback_console_variables_lifetime.md) — never static TAutoConsoleVariable; register in FAZModule, unregister by NAME.
- [feedback_build_paging_file_parallelism.md](feedback_build_paging_file_parallelism.md) — C3859/C1076 = host memory: MaxParallelActions=4, bAllowXGE=false. LC patches die on restart.
- [feedback_cpp_executescript_harness.md](feedback_cpp_executescript_harness.md) — cpp harness: file-scope static initializer, ASCII @Description, preload assets, clean Generated/*.cpp after.
- [feedback_posesearch_branchin_db_sync.md](feedback_posesearch_branchin_db_sync.md) — PSD membership via BranchIn notify; AddBranchInNotify ALONE; clip-swap recipe inside.
- [feedback_chooser_column_reorder.md](feedback_chooser_column_reorder.md) — chooser scripted-edit gotchas (columns reorder, rows re-sort, verify set_cell).
- [feedback_chooser_autoremap_fuzzy_unsafe.md](feedback_chooser_autoremap_fuzzy_unsafe.md) — AutoRemap fuzzy tier unsafe for bulk swaps; explicit RemapChooserAssets map.
- [feedback_parallel_editor_edits.md](feedback_parallel_editor_edits.md) — user-in-editor races: check mtimes/dirty before bulk ops; open tabs re-save stale state.
- [feedback_seam_trace_before_pie.md](feedback_seam_trace_before_pie.md) — ★ before ANY PIE request: trace one full loop with real numbers across BT-values × C++-constants; dump actual node values (defaults sneak in). 5 seam bugs in one evening prove it.
- [feedback_editor_close_build_open_loop.md](feedback_editor_close_build_open_loop.md) — RETIRED: user drives editor close/open (auto-quit crashes on exit w/ open tabs); mechanics kept for reference.
- [feedback_metahuman_modular_hero.md](feedback_metahuman_modular_hero.md) — ★ MetaHuman hero: re-assembly WIPES compatible-skeleton+sockets (run Tools/metahuman_fixup.py); LeaderPoseComponent property ≠ setter (needs bForceUpdate); never give garments their own ABP.
- [feedback_leaderpose_lod_bone_reduction.md](feedback_leaderpose_lod_bone_reduction.md) — ★ garments blowing off the body = LOD BONE REDUCTION, not proportions; leader-pose followers read the leader's current LOD. "Perfect in Blender" proves nothing about UE. Blender export settings inside.
- [feedback_no_hardcoded_asset_paths.md](feedback_no_hardcoded_asset_paths.md) — ★ USER RULE: no /Game/ paths in C++; all content refs = editor-assigned UPROPERTYs w/ native fallbacks; grep Source for "/Game/" to verify.
- [feedback_aaa_design_first.md](feedback_aaa_design_first.md) — ★ USER RULE: AAA approach on EVERY task — propose design + anticipate failure axes BEFORE coding, wait for go; events drive timers guard; one owner per fact; measure & instrument.
- Covered by skills: check-memory-first, agent-teams, validate-findings, no-read-approval, file-paths, port checklist, deep-pin-inspection, animgraph wiring, post-event-vs-threadsafe, blendstack-input-ref, python-gc-crash, retarget-root-motion, chooser-utils API, SM transition access, transition-rule tools.

## GASP reference (standing rule UPDATED 2026-08-15: GASP **5.8** now imported at **/Game/GameAnimationSample/** — old /Game/Blueprints/… paths are redirectors; query via MCP; the source project C:\UnrealEngine\Games\GameAnimationSample is used ONLY for its Config/.uproject on disk, its Content is never referenced)
- [project_gasp58_update_audit.md](project_gasp58_update_audit.md) — ★★ GASP 5.8 audit: PoseSearch Interaction (multi-char MM) engine facts, CMC pawn+ABP reference values, CAS dropped, config checklist. Read before spike-P1/interaction/parity work.
- [project_gasp_cmc_abp_spec.md](project_gasp_cmc_abp_spec.md) — ★★ BUILD SPEC for the new CMC hero ABP: SandboxCharacter_CMC_ABP teardown (19-field contract struct, graph spine, ~45 BP functions, required clip curves) + the measured database-density blocker (AZ ~125 clips vs GASP 160 DBs). Read before authoring the ABP.
- [gasp_animbp_architecture.md](gasp_animbp_architecture.md), [gasp_animbp_full_audit.md](gasp_animbp_full_audit.md) — ABP architecture + authoritative audit.
- [gasp_pawn_bp_full.md](gasp_pawn_bp_full.md), [gasp_pawn_cpp_port_plan.md](gasp_pawn_cpp_port_plan.md) — pawn audit + port inventory.
- [gasp_character_movement.md](gasp_character_movement.md), [gasp_movement_modes.md](gasp_movement_modes.md) — Mover pawn + per-mode BPs.
- [gasp_data_model.md](gasp_data_model.md), [gasp_data_model_full.md](gasp_data_model_full.md) — enums/structs, field-by-field.
- [gasp_sm_tip_flow.md](gasp_sm_tip_flow.md), [gasp_orientation_intent_tip.md](gasp_orientation_intent_tip.md) — SM topology, TIP rules.
- [gasp_update_logic_flow.md](gasp_update_logic_flow.md) — IsMoving formula; state tracking.
- [gasp_posesearch_choosers.md](gasp_posesearch_choosers.md), [reference_cht_chooser_structure.md](reference_cht_chooser_structure.md), [reference_gasp_anim_notifies.md](reference_gasp_anim_notifies.md) — PoseSearch/chooser/notify catalogs.
- [gasp_project_settings.md](gasp_project_settings.md), [gasp_framework_cameras_rigs.md](gasp_framework_cameras_rigs.md), [gasp_actor_components_and_notifies.md](gasp_actor_components_and_notifies.md), [gasp_cpp_architecture.md](gasp_cpp_architecture.md) — settings/framework/components/C++.

## Project state (topic files hold ALL detail — descriptions inside)
- [project_audit_2026-06-12.md](project_audit_2026-06-12.md) — ★ 5-agent audit: pillars SOUND; P0-P3 bug backlog. Read before fix sprints.
- [project_architecture_rationale.md](project_architecture_rationale.md) — why Mover; why Chooser+SM+BlendStack.
- [project_v2_architecture.md](project_v2_architecture.md) — v2 character system doctrine (GAS × Chooser/PoseSearch × Mover).
- [project_v2_locomotion_progress.md](project_v2_locomotion_progress.md) — v2 locomotion resume point.
- [project_idle_tip_implementation.md](project_idle_tip_implementation.md) — idle turn-in-place baseline.
- [project_input_stack_rt_mirror.md](project_input_stack_rt_mirror.md) — RT IMC + 21 IA mirror.
- [project_crouch_system.md](project_crouch_system.md) — crouch chain + proxy-spike doctrine. Read for stance work.
- [project_locomotion_sm_refactor_plan.md](project_locomotion_sm_refactor_plan.md) — DeriveSMState → UAZ_LocomotionStateMachine plan.
- [project_jump_system_status.md](project_jump_system_status.md) — ★ hybrid 2-clip jump FINAL + BranchIn audit. Read first for jump.
- [project_physics_jump_plan.md](project_physics_jump_plan.md) — physics-jump pivot decision record.
- [project_combat_system_plan.md](project_combat_system_plan.md) — ★ melee/combat design (equip axes, tiers, GA_MeleeAttack).
- [project_combat_fist_build_plan.md](project_combat_fist_build_plan.md) — ★ LIVE fist-first build doc + RAIL DOCTRINE (montage vs SM). Read first for combat.
- [project_motion_warping.md](project_motion_warping.md) — ★ motion warping on Mover: the bDidAttrHaveRootMotion gate, the RootMotionMode bug, SkewWarp's clip-dependent branches, notify traps, ABP_Chalkie routing hazards. Read before any warp/montage-reaction work.
- [project_combat_next_session_plans.md](project_combat_next_session_plans.md) — ★★ START HERE for combat: 3 planned tasks — P0 travel-gate regression killed the Chalkie's warp lunge AND rotation tracking (+ masked BT clamp, which is `Min` not `Max`); P1 stagger → duration GE (fixes a live tag-wipe); P2 grab-escape face-to-face (GrabHoldDistance 92 vs the shared-origin doctrine's 0). Two-reviewer validated.
- [project_combat_arch_refactor.md](project_combat_arch_refactor.md) — ★ LIVE combat stability refactor: C→A′ done (DriveRootMotion generations, State.Combat.Staggered), B (FAZ_CombatMontage descriptor) next, then A (GA_HitReact). Fable backlog inside. Read before combat/reaction work.
- [project_traversal_system.md](project_traversal_system.md) — trace→chooser→RM-action|physics pattern (base for contextual movement).
- [project_obstacle_reaction_system.md](project_obstacle_reaction_system.md) — obstacle reactions (sensor, bands, Brace/Stumble). Read for impact work.
- [project_movement_clearance_plan.md](project_movement_clearance_plan.md) — ★ intent-pure clearance clamp plan. Read for blocking/movement.
- [project_npc_foundation.md](project_npc_foundation.md) — ★ Chalkie foundation: Mover pawn, Option B classic ABP, no retarget, NavMover.
- [project_zombie_ai_plan.md](project_zombie_ai_plan.md) — ★ LIVE Chalkie AI (thru `41355dc`): search cycle, horde L1, team fix, RM-lite curve-follow, loco SM, anim-set variants, BB pacing. Read first for zombie AI.
- [project_chalkie_territory_combat_plan.md](project_chalkie_territory_combat_plan.md) — ★ combat build log §F-H (S1-S3 done, lessons) + anim census; territory postponed. Read before NPC combat work.
- [project_chalkie_fight_rules.md](project_chalkie_fight_rules.md) — ★ THE 9-rule combat/engagement RULEBOOK, each rule = one code owner. Read before touching NPC combat/perception.
- [project_crowd_engagement_design.md](project_crowd_engagement_design.md) — ★ NEXT SESSION: ring-slot + rotation design (fixes NPC stacking; observers/rotating engagement). Crowd brain v2 live at `bfaa7bc`.
- [project_grab_grapple_design.md](project_grab_grapple_design.md) — ★ TLOU-style GRAB. **PIVOTED 2026-08-01 to NAAT shared-origin PAIRED montages + engine `MontageSync_Follow`** (read the pivot block at top first; v1 socket-anchor/IK notes below it are superseded). Read before any grab work.
- [project_contextual_anim_mover_assessment.md](project_contextual_anim_mover_assessment.md) — ★ CAS×Mover feasibility (2026-08-05): CMC coupling quarantined in one replaceable class; reuse asset+editor, thin AZ runtime; DEFERRED until first execution/finisher (task #15). Read before any contextual-anim/execution work.
- [project_cmc_backport_spike.md](project_cmc_backport_spike.md) — ★★ LIVE: CMC back-port spike plan (branch spike/cmc-backport, task #16). KEEP CHT+MM decision + v1 AAZ_HeroCharacter resurrection discovery + 5-phase plan. Read FIRST on the spike branch.
- [project_mm_state_selection_plan.md](project_mm_state_selection_plan.md) — ★★ PLAN: select DISCRETE loco events (starts/stops/pivots) by STATE via pool-narrowing, keep MM for loops; evidence that 4 bugs = 1 root cause; measured clip reference table. Read before further MM selection work.
- [project_locomotion_quality_standard.md](project_locomotion_quality_standard.md) — ★★★ THE STANDARD: ownership doctrine + falsifiable predictor rule, mismatch-resolution ladder, content-derived constants, [CmcRatio] acceptance gate, P0-P5 work items. Read FIRST for any locomotion/movement-feel work.
- [project_cmc_movement_feel_tuning.md](project_cmc_movement_feel_tuning.md) — ★★ CMC feel: the latched STOP CONTRACT + CURVE-DRIVEN braking (clip owns the deceleration), EvaluateCurveData-not-GetCurveValue rule, closed entry-frame diagnosis, loop/start/stop measured speeds, 2 open bugs (empty InAir gate union, stale stop-band latch). Read before touching BP_CMC_Hero movement values.
- [project_cmc_curve_driven_turns.md](project_cmc_curve_driven_turns.md) — ★★ PLAN: drive capsule ROTATION from authored MoveData_TurnRate curves; measured 3-class turn taxonomy (in-place 0cm / pivots 78-356cm / arcs), the 104→727 °/s spread, the orient-to-movement blocker. Read before turn/pivot/TIP work.
- [project_multipawn_class_design.md](project_multipawn_class_design.md) — multi-pawn scaling rules (input split, two-ASC, seats).
- [project_sp_first_coop_extensible.md](project_sp_first_coop_extensible.md) — SP-first scope rule + ALWAYS/AVOID/DEFER lists.
- [project_mover_5_7_to_5_8_diff.md](project_mover_5_7_to_5_8_diff.md) — Mover 5.7→5.8 API diff.
- [project_ue58_migration_2026-05-10.md](project_ue58_migration_2026-05-10.md) — migration record + surprises.
- [project_iris_replication_5_8.md](project_iris_replication_5_8.md) — Iris on; Mover+GAS routing.
- [project_local_plugin_patches.md](project_local_plugin_patches.md) — UnrealClaude local patches; re-apply after sync.
- [project_root_motion_mode.md](project_root_motion_mode.md) — Mover RM three-part chain.
- [project_mm_implementation_plan.md](project_mm_implementation_plan.md), [project_motion_matching_plan.md](project_motion_matching_plan.md), [project_motion_matching_progress.md](project_motion_matching_progress.md) — MM plan/progress.
- [project_gasp_pawn_port_audit_2026-05-02.md](project_gasp_pawn_port_audit_2026-05-02.md), [project_gasp_animbp_cpp_port_plan.md](project_gasp_animbp_cpp_port_plan.md), [project_gasp_abp_port_ledger.md](project_gasp_abp_port_ledger.md) — port audits/ledgers.
- [project_session_2026-04-22_gasp_pawn_done.md](project_session_2026-04-22_gasp_pawn_done.md), [project_session_2026-04-24_gasp_animbp_cpp_done.md](project_session_2026-04-24_gasp_animbp_cpp_done.md), [project_session_2026-04-26_abp_first_motion.md](project_session_2026-04-26_abp_first_motion.md) — session logs.
- [project_gas_gameplay.md](project_gas_gameplay.md) — GAS layout. [inventory-system.md](inventory-system.md) — inventory. [weapon_swap_architecture.md](weapon_swap_architecture.md), [feedback_ik_setup.md](feedback_ik_setup.md) — weapons/IK.

## Catalogs
- [Rider MCP new tools](reference_rider_mcp_new_tools.md) — lint-before-compile, screenshots, refactor suite, call hierarchy, debugger attach.
- [reference_noweapon_anim_catalog.md](reference_noweapon_anim_catalog.md) — 191 NoWeapon anims by group; gaps noted.
- [reference_project_quickref.md](reference_project_quickref.md) — systems summary, naming, source dirs (ex-MEMORY.md detail).
