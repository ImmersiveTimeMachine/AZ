# AZ Project Memory — "CHALK"

**★★★ LANGUAGE: Artur ALWAYS speaks Russian — answer in Russian; English-looking text = misrecognized Russian speech, never another language ([user_language_russian](user_language_russian.md)).**

UE 5.8 survival-horror **CHALK** (2024 Montreal, drug mutates addicts into "Chalkies" — body-horror, NOT zombies). Lore: [lore_chalk](project_lore_chalk.md). Quickref: [project_quickref](reference_project_quickref.md). Engine `C:\UnrealEngine\Engine\`, project `C:\UnrealEngine\Games\AZ`.

## Skills (lazy-loaded)
`az-workspace` (load first) · `agent-and-research-discipline` · `cpp-build-livecoding` · `bp-to-cpp-port` · `unrealclaude-mcp-tools` · `anim-debug-pitfalls` · `az-cpp-utility-tools` · `gasp-parity-reference` · `asset-modification-via-python` · `blender-level-blockout` · `substance-designer-offline-materials` · `unreal-prop-optimization`.

## ★★★ CURRENT DIRECTION — read first
- [Painter MCP](reference_painter_mcp.md) — ★★★ start Tools/painter_mcp/Start_Painter_MCP_OCIO.cmd (OCIO!); sRGB, ONE bitmap channel per fill layer; refs = style, not copy.
- [School floors draft](project_school_floor2_draft.md) — ★★★ v03 2026-10-14 (corridor 3.60, class 3.50, slab 3.70); floors 1-3 geometry draft in one .blend; rebuild with Blender 5.1.
- [Window glass variants](project_window_glass_variants.md) — ★★★ 2026-10-13 Painter variant groups + Designer graph `Art/WindowGlass_Designer` v13 (seed = new window).
- [Blender AI Utilities 1.5.0](project_blender_ai_utilities_audit.md) — ★★★ 2026-10-15 native job engine; AI only writes data; bake fix, assemble for Unreal, rebuild, fit metric.
- [H2L one depth check](feedback_h2l_one_depth_check.md) — ★★★ USER DECISION: one native depth check + one agent correction; rules general for any object.
- [UE Remote Execution](reference_ue_remote_execution.md) — ★★★ Python in the open editor: `python Tools/ue_remote/ue_run.py script.py`; Rider = fallback.
- [UE→Blender tool](reference_ue_to_blender_tool.md) — ★★★ USER RULE: reuse Tools/ue_to_blender (level actors → .blend with materials).
- [Peeling plaster Designer v08](project_peeling_plaster_designer_v08.md) — ★★★ offline .sbs generator (Tools/substance); needs user review. Rules: [Designer engine rules](reference_substance_designer_engine_rules.md) · taste: [varied edges](feedback_peeling_edges_varied.md).
- [Wall two-windows semantic UV](project_wall_twowindows_semantic_uv.md) — ★★ UV v02, 3 texture sets; UE not reimported. [socket exec](reference_blender_socket_exec.md)
- [Perf baseline](project_perf_baseline_2026-10-09.md) — ★★★ GPU-bound ~9-10 ms; Mover+NP 0.43 ms → no rewrite; stutters = GC/Niagara/PSO; Tools/perf/az_perf_capture.py.
- [Leg IK knee twist fix](project_leg_ik_knee_twist_fix_2026-10-04.md) — ★★★ fix: weight 0 + knee-plane pole; identity-test rule.
- [Crouch start<->loop flap](project_crouch_start_loop_flap_2026-10-04.md) — ★★ fixed 2026-10-04; PIE pending.
- [M16 sprint turns](project_m16_sprint_turns_2026-10-05.md) — ★★ unarmed rows 88-93 leaked to M16; fixed (c9 + rows 537-548); pivots BAKED AZ_M16_RunPivot180_*; PIE OK.
- [Winchester on the M16 set](project_winchester_on_m16_set_2026-10-05.md) — ★★★ moves on W2 (Weapon.Rifle tag); own HOLD node + lever fire + tube reload; PIE pending; supersedes turn bakes.
- [Protected weapon sets](project_protected_weapon_sets.md) — ★★★ M16/pistol/unarmed clips are tuned: never touch; backup AZ_Backups/2026-09-27_pre-WGS.
- [Consider the local model](feedback_consider_local_model.md) — ★★★ USER RULE: delegate mechanical steps to the local LM Studio agent (skill `local-model-delegation`).
- [Master skeleton](project_master_skeleton_2026-09-25.md) — ★★★ SK_AZ_Master; Master clips NEED retarget_source_asset=SKM_AZ_Master.
- [Natural Grip C++ plugin](project_natural_grip_plugin_2026-10-02.md) — ★★★ Plugins/AZNaturalGrip (grip solver, BakeSwitchClip, GraftUpperBody); switch left hand + crouched M16 legs SOLVED.
- [Natural grasp](project_natural_grasp_2026-09-29.md) — ★★★ hand re-grip + baked anatomical grasp.
- [Weapon arm solver](project_weapon_arm_solver_2026-09-28.md) — ★★★ user order: Winchester → shotgun → M16/pistol → knife.
- [Winchester integration audit](project_winchester_integration_audit_2026-09-26.md) — ★★★ R1/R2 done; R3 awaiting go.
- [RifleMega retarget](project_riflemega_retarget_2026-09-25.md) — ★★★ 446 clips on the MetaHuman.
- [Local agent (LM Studio)](reference_local_agent_lmstudio.md) — .agents/agent_script.py; measured limits.
- [Weapon models import](project_weapon_models_import_2026-09-25.md) — 7 guns in /Game/AZ/Assets/Weapons.
- [Gamepad analog vs gait](project_gamepad_analog_gait_mismatch.md) — ✔ DONE 2026-09-23.
- [Throwable RESUME](project_throwable_resume_2026-09-17.md) — ★★★ throwables start here.
- [Throwable art](project_throwable_art_assets.md) — grenade icon + M67 mesh.
- [Traversal resume](project_traversal_2026-09-15.md) — ★★★ mantle+hurdle+climb on one executor.
- [Climb spec](project_climb_spec.md) — ★★★ user work order for high-ledge climb.
- [Hurdle spec](project_hurdle_spec.md) — ★★★ user work order.
- [Mantle handoff spec](project_mantle_handoff_spec.md) — ★★★ 13-point user order; read before traversal.
- [Mantle traversal](project_mantle_traversal_2026-09-14.md) — ★★★ standing mantle shipped.
- [Aim TIP](project_aim_tip_next_session_2026-09-12.md) — ★★★ aim TIP shipped; follow-ups.
- [Pistol pattern for rifle](project_pistol_pattern_for_rifle_2026-09-12.md) — ★★ user prefers the pistol pattern; rifle mapping + brief.
- [Mover + MetaHuman](project_mover_metahuman_2026-08-31.md) — ★★★ verdict: Mover + MetaHuman hero, IK off, CMC parked @210247b.
- [MM spine continuity](feedback_mover_spine_search_continuity.md) — ★★★ read before touching the Mover spine MotionMatch call or any loop DB.

- [NetLab learning project](project_netlab_learning.md) — ★★★ C:/Projects/NetLab: UE-style replication over WinSock; I write commented code, Artur studies.

## Workflow rules
- [rider_debugger_ensure_resume](feedback_rider_debugger_ensure_resume.md) — ★★★ editor under Rider debugger: an ensure PAUSES it → xdebug resume (F5).
- [offer_runtime_solution_first](feedback_offer_runtime_solution_first.md) — ★★★ "universal" request → runtime markers+IK first.
- [bake_corrections_into_animation](feedback_bake_corrections_into_animation.md) — ★★★ USER RULE: fix poses OFFLINE, bake a clip per situation; no per-frame runtime solvers.
- [deliver_result_not_analysis](feedback_deliver_result_not_analysis.md) — ★★★ результат, а не разбор: значения выбирать самому, без A/B/C и цифр.
- [open_the_file_i_must_edit](feedback_open_the_file_i_must_edit.md) — ★★★ просишь поменять руками — сначала открой ассет/файл.
- [history_rewrite_deletes_files](feedback_history_rewrite_deletes_files.md) — ★★★ stripping files from git history deletes them from disk; restore recipe.
- [parallel_editor_edits](feedback_parallel_editor_edits.md) — ★★★ other agents edit in parallel: re-check, anchor on content, stage only your hunks.
- [inform_before_proposing](feedback_inform_before_proposing.md) — ★★★ grep memory + AZ_*Utils before proposing; tuned values have reasons.
- [verify_never_presume](feedback_verify_never_presume.md) — ★★★ no claim without a fresh read; verify changes landed.
- [stop_the_patch_loop](feedback_stop_the_patch_loop.md) — ★★★ two strikes → re-model; falsifiable prediction, one variable per test.
- [aaa_design_first](feedback_aaa_design_first.md) — ★ design + failure axes before code; wait for go.
- [model_routing_policy](feedback_model_routing_policy.md) — ★★★ I'm the brain: precise step-by-step spec → Sonnet executes → I verify. Haiku banned.
- [no_hardcoded_asset_paths](feedback_no_hardcoded_asset_paths.md) — ★ no /Game/ paths in C++.
- [mover_mode_state_not_rollback_safe](feedback_mover_mode_state_not_rollback_safe.md) — ★★★ sim decisions ride FAZ_MoverCustomInputs, never mode members.
- [posesearch_mm_mechanism_rules](feedback_posesearch_mm_mechanism_rules.md) — ★★★ MM rules R1–R17.
- [animpose_root_motion_double_count](feedback_animpose_root_motion_double_count.md) — ★★★ AnimPose world space already has root motion; never add the root track.
- [ik_retargeter_exact_transfer](feedback_ik_retargeter_exact_transfer.md) — ★★★ exact retarget = aligned pose + FK-only; 5.8 API traps.
- [parallel_build_header_edit_corruption](feedback_parallel_build_header_edit_corruption.md) — ★★ header edited mid-build → wipe objs.
- [livecoding_header_reinstances_animbp](feedback_livecoding_header_reinstances_animbp.md) — ★★ LC header patch breaks AnimBPs; .cpp only.
- [slot_mask_weight_squared](feedback_slot_mask_weight_squared.md) — ★★ weight-gated LayeredBoneBlend squares; use 0/1 gate.
- [montage_blend_profile_cross_skeleton](feedback_montage_blend_profile_cross_skeleton.md) — ★★ CRASH: retargeted montages keep source blend profiles.
- [held_input_retries_ability](feedback_held_input_retries_ability.md) — ★★ held key re-activates ended abilities; exclude per-tag.
- [motionwarping_warppoint_provider](feedback_motionwarping_warppoint_provider.md) — ★★ Mover warping traps.
- [python_gc_crash](feedback_python_gc_crash.md) — ★★ Python-from-MCP crash/deadlock hazards; RM authoring recipe.
- [python_save_only_if_dirty](feedback_python_save_only_if_dirty.md) — ★★ save_loaded_asset can silently skip; verify by mtime.
- [chooser_row_add_tag_columns](feedback_chooser_row_add_tag_columns.md) — ★★ short chooser rows silently filtered; CHT_v2 c0–c20 map.
- [chooser_column_reorder](feedback_chooser_column_reorder.md), [chooser_autoremap_fuzzy_unsafe](feedback_chooser_autoremap_fuzzy_unsafe.md) — chooser scripted-edit gotchas.
- [bp_cdo_write_needs_recompile](feedback_bp_cdo_write_needs_recompile.md) — ★★ scripted CDO writes need compile_blueprint.
- [mover_visual_component_two_writers](feedback_mover_visual_component_two_writers.md) — ★★ Mover owns mesh relative transform; sample at OnEndFrame.
- [offsetrootbone_mover_graph](feedback_offsetrootbone_mover_graph.md) — ★★ OffsetRootBone in the MHC ABP; verify a node is committed.
- [metahuman_modular_hero](feedback_metahuman_modular_hero.md) — ★ MH re-assembly wipes skeleton/sockets (Tools/metahuman_fixup.py).
- [leaderpose_lod_bone_reduction](feedback_leaderpose_lod_bone_reduction.md) — ★ garments blowing off = LOD bone reduction.
- [planted_foot_curve_guard](feedback_planted_foot_curve_guard.md) — ★ write bLeftFootDown only with a contact curve.
- [seam_trace_before_pie](feedback_seam_trace_before_pie.md) — ★ trace one loop with real numbers before PIE.
- [measure_the_clip_first](feedback_measure_the_clip_first.md) — ★ measure clips first.
- [log_reading_traps](feedback_log_reading_traps.md) — ★ log-reading traps.
- [abp_internal_graph_blindspot](feedback_abp_internal_graph_blindspot.md), [blendstack_input_node_ref](feedback_blendstack_input_node_ref.md) — BlendStack graph blindspots.
- [posesearch_branchin_db_sync](feedback_posesearch_branchin_db_sync.md) — PSD membership via BranchIn only.
- [console_variables_lifetime](feedback_console_variables_lifetime.md), [build_paging_file_parallelism](feedback_build_paging_file_parallelism.md), [cpp_executescript_harness](feedback_cpp_executescript_harness.md) — CVars; build parallelism; cpp harness.

## GASP reference (imported at /Game/GameAnimationSample/)
- [gasp58_update_audit](project_gasp58_update_audit.md) ★★ · [gasp_cmc_abp_spec](project_gasp_cmc_abp_spec.md) ★★ · [gasp_pawn_bp_full](gasp_pawn_bp_full.md) · [gasp_movement_modes](gasp_movement_modes.md)
- Detailed GASP notes (ABP architecture/audit, data model, SM/TIP flow, choosers, notifies, settings, cameras, components, C++): [index_gasp_reference](index_gasp_reference.md)

## Project state
- [asset_roundtrip](project_asset_roundtrip.md) — UE/Blender separate-and-reimport recipe (skill in ~/.codex/skills/ue-blender-asset-roundtrip).
- [protagonist_face_design](project_protagonist_face_design.md) — teacher face 03 applied; originals preserved.
- [weapon_switch_2026-09-11](project_weapon_switch_2026-09-11.md) — rifle/pistol holster/draw + carry actors built; user check pending.
- [rifle_aim_upper_body_lock_2026-09-10](project_rifle_aim_upper_body_lock_2026-09-10.md) — ★★ rejected pin bindings fall back to the literal.
- [hero_camera_aim_cone_2026-09-09](project_hero_camera_aim_cone_2026-09-09.md) — ★★ per-mode camera offsets + 60° aim cone, in UpdateCameraForMode.
- [rifle_mh_native_migration_2026-09-09](project_rifle_mh_native_migration_2026-09-09.md) — ★★★ rifle set is MetaHuman-native (AZ_RTG_MH_*); migration traps.
- [jump_system_status](project_jump_system_status.md), [physics_jump_plan](project_physics_jump_plan.md) — ★★ rifle three-phase jump shipped; unarmed 2-clip jump.
- [hud_design](project_hud_design.md) — HUD phase 1 done; keep inventory layout.
- [psia_heavy_strike_plan](project_psia_heavy_strike_plan.md) — ★★★ PSIA strikes shipped; open items.
- [punch_contact_next_session](project_punch_contact_next_session.md) — ★★★ punch contact + grab catch committed; open: facing spring, from-behind catch.
- [combat_next_session_plans](project_combat_next_session_plans.md) — ★★ combat backlog P0–P2.
- [combat_system_plan](project_combat_system_plan.md), [combat_fist_build_plan](project_combat_fist_build_plan.md), [combat_arch_refactor](project_combat_arch_refactor.md) — melee design, fist build, refactor.
- [grab_grapple_design](project_grab_grapple_design.md) — ★ GRAB = PoseSearch Interaction; step 2 = C++ MotionMatchMulti driver.
- [motion_warping](project_motion_warping.md) — ★ warping on Mover. [contextual_anim_mover_assessment](project_contextual_anim_mover_assessment.md) — CAS deferred.
- [audit_2026-06-12](project_audit_2026-06-12.md) — ★ 5-agent audit, P0–P3 backlog.
- [architecture_rationale](project_architecture_rationale.md), [v2_architecture](project_v2_architecture.md) — why Mover; v2 doctrine.
- [v2_locomotion_progress](project_v2_locomotion_progress.md), [idle_tip_implementation](project_idle_tip_implementation.md), [locomotion_sm_refactor_plan](project_locomotion_sm_refactor_plan.md) — v2 locomotion, idle TIP, SM extraction.
- [input_stack_rt_mirror](project_input_stack_rt_mirror.md), [crouch_system](project_crouch_system.md) — RT input mirror; crouch.
- [traversal_system](project_traversal_system.md), [obstacle_reaction_system](project_obstacle_reaction_system.md), [movement_clearance_plan](project_movement_clearance_plan.md) — traversal, obstacles, clearance.
- Chalkie AI: [npc_foundation](project_npc_foundation.md), [zombie_ai_plan](project_zombie_ai_plan.md), [chalkie_territory_combat_plan](project_chalkie_territory_combat_plan.md), [chalkie_fight_rules](project_chalkie_fight_rules.md), [crowd_engagement_design](project_crowd_engagement_design.md).
- CMC spike (parked @210247b) + early MM + GASP port ledgers: [index_archive_cmc_ports](index_archive_cmc_ports.md); ★★★ [locomotion_quality_standard](project_locomotion_quality_standard.md) = THE STANDARD.
- [multipawn_class_design](project_multipawn_class_design.md), [sp_first_coop_extensible](project_sp_first_coop_extensible.md) — multi-pawn rules; SP-first scope.
- [mover_5_7_to_5_8_diff](project_mover_5_7_to_5_8_diff.md), [ue58_migration_2026-05-10](project_ue58_migration_2026-05-10.md), [iris_replication_5_8](project_iris_replication_5_8.md), [local_plugin_patches](project_local_plugin_patches.md), [root_motion_mode](project_root_motion_mode.md), [mover_root_motion](reference_mover_root_motion.md) — Mover/5.8 migration.
- [gas_gameplay](project_gas_gameplay.md), [inventory-system](inventory-system.md), [weapon_swap_architecture](weapon_swap_architecture.md), [ik_setup](feedback_ik_setup.md) — GAS; inventory; weapons/sockets; hand IK.

## Catalogs
- [rider_mcp_new_tools](reference_rider_mcp_new_tools.md) — Rider MCP tools.
- [noweapon_anim_catalog](reference_noweapon_anim_catalog.md) — 191 NoWeapon anims.
- [punch_reaction_content_inventory](reference_punch_reaction_content_inventory.md) — punch/react content.
- [mirror_anim_bake](reference_mirror_anim_bake.md) — ★★ mirror = MirrorDataTable + baked copy in Unreal; API traps.
- [ue5_python_posesearch](reference_ue5_python_posesearch.md) — Python PoseSearch API.
- [Launcher Add to Project](reference_launcher_add_to_project.md) — ★ source build: temp project + copy incl. __ExternalActors__.
- [Prop optimization + VT](project_prop_optimization_vt.md) — 2026-10-14 imported-prop tool; VT project-wide; cloakroom + window bay fixed.
