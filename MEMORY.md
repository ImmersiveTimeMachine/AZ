# AZ Project Memory — "CHALK"

**★★★ LANGUAGE: Artur ALWAYS speaks Russian — answer in Russian; English-looking text = misrecognized Russian speech ([user_language_russian](user_language_russian.md)).**

UE 5.8 survival-horror **CHALK** (2024 Montreal, drug mutates addicts into "Chalkies" — body-horror, NOT zombies). Lore: [lore_chalk](project_lore_chalk.md). Quickref: [project_quickref](reference_project_quickref.md). Engine `C:\UnrealEngine\Engine\`, project `C:\UnrealEngine\Games\AZ`.

## Skills (lazy-loaded)
`az-workspace` (load first) · `agent-and-research-discipline` · `cpp-build-livecoding` · `bp-to-cpp-port` · `unrealclaude-mcp-tools` · `anim-debug-pitfalls` · `az-cpp-utility-tools` · `gasp-parity-reference` · `asset-modification-via-python` · `blender-level-blockout` · `substance-designer-offline-materials` · `unreal-prop-optimization`.

## ★★★ CURRENT DIRECTION — read first
- [Painter MCP](reference_painter_mcp.md) — ★★★ start Start_Painter_MCP_OCIO.cmd; sRGB, one bitmap channel per fill layer.
- [School floors draft](project_school_floor2_draft.md) — ★★★ v03 10-14; floors 1-3 in one .blend; Blender 5.1.
- [Window glass variants](project_window_glass_variants.md) — ★★★ Painter variants + Designer graph v13.
- [Blender AI Utilities 1.5.0](project_blender_ai_utilities_audit.md) — ★★★ native job engine; AI only writes data.
- [H2L one depth check](feedback_h2l_one_depth_check.md) — ★★★ USER DECISION: one depth check + one correction.
- [UE Remote Execution](reference_ue_remote_execution.md) — ★★★ `python Tools/ue_remote/ue_run.py script.py`.
- [UE→Blender tool](reference_ue_to_blender_tool.md) — ★★★ USER RULE: reuse Tools/ue_to_blender.
- [Peeling plaster Designer v08](project_peeling_plaster_designer_v08.md) — ★★★ offline .sbs; [engine rules](reference_substance_designer_engine_rules.md) · [varied edges](feedback_peeling_edges_varied.md).
- [Wall two-windows UV](project_wall_twowindows_semantic_uv.md) — ★★ UV v02; [socket exec](reference_blender_socket_exec.md).
- [Perf baseline](project_perf_baseline_2026-10-09.md) — ★★★ GPU-bound ~9-10 ms; stutters = GC/Niagara/PSO.
- [Leg IK knee twist](project_leg_ik_knee_twist_fix_2026-10-04.md) — ★★★ weight 0 + knee-plane pole.
- [Crouch flap](project_crouch_start_loop_flap_2026-10-04.md) — ★★ fixed; PIE pending.
- [M16 sprint turns](project_m16_sprint_turns_2026-10-05.md) — ★★ fixed; pivots baked.
- [Winchester on M16 set](project_winchester_on_m16_set_2026-10-05.md) — ★★★ W2 + own HOLD/lever/tube; PIE pending.
- [Protected weapon sets](project_protected_weapon_sets.md) — ★★★ M16/pistol/unarmed clips: never touch.
- [Consider the local model](feedback_consider_local_model.md) — ★★★ USER RULE: delegate mechanical steps (skill `local-model-delegation`).
- [Master skeleton](project_master_skeleton_2026-09-25.md) — ★★★ clips NEED retarget_source_asset=SKM_AZ_Master.
- [STG44 grip assessment](project_naturalgrip_stg44_assessment_2026-10-23.md) — ★★★ right hand = placement rake, criteria lack positive hold; R-1 awaits Artur.
- [Natural Grip plugin](project_natural_grip_plugin_2026-10-02.md) — ★★★ Plugins/AZNaturalGrip; left hand + crouched legs solved.
- [Natural grasp](project_natural_grasp_2026-09-29.md) — ★★★ re-grip + baked grasp.
- [Weapon arm solver](project_weapon_arm_solver_2026-09-28.md) — ★★★ order: Winchester → shotgun → M16/pistol → knife.
- [Winchester audit](project_winchester_integration_audit_2026-09-26.md) — ★★★ R3 awaiting go.
- [RifleMega retarget](project_riflemega_retarget_2026-09-25.md) — ★★★ 446 clips on the MetaHuman.
- [Local agent (LM Studio)](reference_local_agent_lmstudio.md) — .agents/agent_script.py.
- [Weapon models import](project_weapon_models_import_2026-09-25.md) — 7 guns.
- [Gamepad analog vs gait](project_gamepad_analog_gait_mismatch.md) — ✔ done.
- [Throwable RESUME](project_throwable_resume_2026-09-17.md) — ★★★ start here; [art](project_throwable_art_assets.md).
- [Traversal resume](project_traversal_2026-09-15.md) — ★★★ one executor; [climb](project_climb_spec.md) · [hurdle](project_hurdle_spec.md) · [mantle handoff](project_mantle_handoff_spec.md) · [mantle](project_mantle_traversal_2026-09-14.md).
- [Aim TIP](project_aim_tip_next_session_2026-09-12.md) — ★★★ shipped. [Pistol pattern for rifle](project_pistol_pattern_for_rifle_2026-09-12.md) — ★★.
- [Mover + MetaHuman](project_mover_metahuman_2026-08-31.md) — ★★★ verdict; CMC parked @210247b.
- [MM spine continuity](feedback_mover_spine_search_continuity.md) — ★★★ read before touching spine MM / loop DBs.

## CryEngine port (C:/CryEngine/AZ)
- [MCP review](project_cryengine_mcp_review.md) — bridge 2.2.2; snapshot .pyc trap.
- [GameSDK & anim packs](reference_cryengine_gamesdk_assets.md) — F: Launcher; FP = Char1P/Char3P; GameSDK host verdict.
- [Build recipe](reference_cryengine_build.md) — cmake commands; dash MSBuild switches; resource.h; IncrediBuild ib_build.py (helper DESKTOP-HNBF0TK, small gain on unity builds).
- [Test level L_000](reference_cryengine_test_level_l000.md) — ★★★ USER RULE: l_000 = agent test level; playtest recipe; Katimavik classroom had no collision 10-18.
- [Noesis UI](project_cryengine_noesis_ui.md) — ★★★ HUD/quests/inventory/menu/save done 10-17; TD-009 closed; skill `cryengine-noesis-ui`.
- [HMG rip-off](project_cryengine_hmg_ripoff.md) — ★★ az_hmg_rip_cut 0.52; az_fp_camera_align 1.
- [DX12 state + ТЗ](project_cryengine_dx12.md) — ★★ crash classes C1/C2; 4096 shadow clamp → keep sizes ≤4096; SVO ResScaleAir must = Base.
- [Episodes/scenes research](project_cryengine_episode_scene_research.md) — AZStory design.
- [Wwise 2019.1](project_cryengine_wwise_2019.md) — ★★★ ACCEPTED 10-18 (Wwise audio + smoke grenade + classroom room_small reverb on l_000); keyless az_custom.bnk tool; TD-011 latest-Wwise deferred (PS5 needs it; later).
- [DLSS](project_cryengine_dlss_research.md) — ★★ Streamline 2.14.16 manual hooking; DLAA, upscale, Reflex, frame generation + Noesis UI layer DONE 10-20; settings menu Graphics (game.cfg) + HDR10 output DONE; next commit DLSS+HDR hunks; traps in file.
- [DXR ray tracing](project_cryengine_raytracing.md) — ★★ analysis DONE 10-21 (docs/implementation/raytracing-analysis.md): RayQuery+DXC-at-build; DENY_SHADER_RESOURCE on VB/IB trap; sun first (no CE flashlight); NRD before RR; plan 0a..6.
- [Chalkie NPC port](project_cryengine_npc_port.md) — ★★★ stages 0-11 done (grab DRAFT 10-18); 12 partial; 12 placed in l_000, suite s1-s12; player-eye pass 10-18 (screen blood, death keep-away, kill glance; post params need bForceValue); Mannequin Stop() vs ForceFinish trap.
- [CE blend fold trap](reference_cryengine_quat_hemisphere_fold.md) — ★★★ joint bind ~180° off clips → body folds at 50/50 blends; check bind·clip dot.
- [Traversal port](project_cryengine_traversal_port.md) — ★★★ mantle/hurdle/climb in CE: real Jump verified 10-18 (hooks H1-H3 + admission, Codex ack); quick vault over thin walls; skill `cryengine-traversal`.
- [Ragdoll pipeline](project_cryengine_ragdoll.md) — ★★★ Chalkie chr ragdoll 10-18 (UE PhysicsAsset -> proxies + limits + joint frames); skill `cryengine-ragdoll`; rebuild chr = rerun ragdoll_build.
- [M16 port](project_cryengine_m16_port.md) — ★★★ AZ.M16 on the GameSDK rifle rig 10-19 (data only, l_000 pickup); skill `cryengine-weapon-port`; z by SIGHT LINE; CDF attachments absolute.
- [Doors, throws, RPG core](project_cryengine_doors_throws_rpg.md) — ★★★ night 10-19: K08 door, stone/bottle distraction, RPG phase 1; 10-21 one-cell throwables + native incendiary craft done; traps (python -, autosave slot B, az_inv_equip toggle, Rider MCP direct HTTP).
- [NetLab learning](project_netlab_learning.md) — ★★★ C:/Projects/NetLab; commented code for Artur.

## Workflow rules
- [mechanics_rpg_hooks](feedback_mechanics_rpg_hooks.md) — ★★★ USER RULE: CHALK has RPG elements; mechanics' tunables = base × one RPG layer (AZ Ability System), no hardcoding.
- [creativity_deep_understanding](feedback_creativity_deep_understanding.md) — ★★★ USER RULE: max creativity + deep understanding; judge through the player's eyes, add missing reactions (no "чурбан").
- [cryengine_agents_lock_planning](feedback_cryengine_agents_lock_planning.md) — ★★★ CryEngine AZ multi-agent: announce lock minutes/handoffs (PLAN/QUEUE/EXTEND/HANDOFF/TASK); never use another agent's editor/game.
- [keep_stated_goal_scope](feedback_keep_stated_goal_scope.md) — ★★★ plan around his goal; side problems → separate task.
- [cryengine_rider_debug_runs](feedback_cryengine_rider_debug_runs.md) — ★★★ CE: Rider debug runs; render bugs → tech debt.
- [cryengine_check_runs_for_asserts](feedback_cryengine_check_runs_for_asserts.md) — ★★★ USER RULE: never leave a run on an assert dialog/Rider pause; run_game_test.py catches them (read 'asserts:').
- [screenshots_only_when_needed](feedback_screenshots_only_when_needed.md) — ★★★ USER RULE: no screenshots as proof.
- [rider_debugger_ensure_resume](feedback_rider_debugger_ensure_resume.md) — ★★★ ensure pauses → F5.
- [offer_runtime_solution_first](feedback_offer_runtime_solution_first.md) — ★★★ "universal" → runtime markers+IK.
- [bake_corrections_into_animation](feedback_bake_corrections_into_animation.md) — ★★★ USER RULE: bake offline, no per-frame solvers.
- [deliver_result_not_analysis](feedback_deliver_result_not_analysis.md) — ★★★ результат, а не разбор.
- [open_the_file_i_must_edit](feedback_open_the_file_i_must_edit.md) — ★★★ сначала открой ассет/файл.
- [history_rewrite_deletes_files](feedback_history_rewrite_deletes_files.md) — ★★★ restore recipe.
- [parallel_editor_edits](feedback_parallel_editor_edits.md) — ★★★ re-check, anchor on content, own hunks only.
- [inform_before_proposing](feedback_inform_before_proposing.md) — ★★★ grep memory + AZ_*Utils first.
- [verify_never_presume](feedback_verify_never_presume.md) — ★★★ fresh read; verify changes landed.
- [stop_the_patch_loop](feedback_stop_the_patch_loop.md) — ★★★ two strikes → re-model.
- [aaa_design_first](feedback_aaa_design_first.md) — ★ design + failure axes first.
- [model_routing_policy](feedback_model_routing_policy.md) — ★★★ spec → Sonnet executes → I verify; Haiku banned.
- [no_hardcoded_asset_paths](feedback_no_hardcoded_asset_paths.md) — ★ no /Game/ paths in C++.
- [mover_mode_state_not_rollback_safe](feedback_mover_mode_state_not_rollback_safe.md) — ★★★ use FAZ_MoverCustomInputs.
- [posesearch_mm_mechanism_rules](feedback_posesearch_mm_mechanism_rules.md) — ★★★ MM rules R1–R17.
- [animpose_root_motion_double_count](feedback_animpose_root_motion_double_count.md) — ★★★ never add the root track.
- [ik_retargeter_exact_transfer](feedback_ik_retargeter_exact_transfer.md) — ★★★ aligned pose + FK-only.
- [parallel_build_header_edit_corruption](feedback_parallel_build_header_edit_corruption.md) — ★★ wipe objs.
- [livecoding_header_reinstances_animbp](feedback_livecoding_header_reinstances_animbp.md) — ★★ .cpp only.
- [slot_mask_weight_squared](feedback_slot_mask_weight_squared.md) — ★★ use 0/1 gate.
- [montage_blend_profile_cross_skeleton](feedback_montage_blend_profile_cross_skeleton.md) — ★★ CRASH trap.
- [held_input_retries_ability](feedback_held_input_retries_ability.md) — ★★ exclude per-tag.
- [motionwarping_warppoint_provider](feedback_motionwarping_warppoint_provider.md) — ★★ Mover warping traps.
- [python_gc_crash](feedback_python_gc_crash.md), [python_save_only_if_dirty](feedback_python_save_only_if_dirty.md) — ★★ Python/MCP hazards.
- [chooser_row_add_tag_columns](feedback_chooser_row_add_tag_columns.md), [chooser_column_reorder](feedback_chooser_column_reorder.md), [chooser_autoremap_fuzzy_unsafe](feedback_chooser_autoremap_fuzzy_unsafe.md) — chooser traps.
- [bp_cdo_write_needs_recompile](feedback_bp_cdo_write_needs_recompile.md) — ★★ compile_blueprint after CDO writes.
- [mover_visual_component_two_writers](feedback_mover_visual_component_two_writers.md), [offsetrootbone_mover_graph](feedback_offsetrootbone_mover_graph.md) — ★★ Mover mesh/ORB.
- [metahuman_modular_hero](feedback_metahuman_modular_hero.md), [leaderpose_lod_bone_reduction](feedback_leaderpose_lod_bone_reduction.md) — ★ MH traps.
- [planted_foot_curve_guard](feedback_planted_foot_curve_guard.md), [seam_trace_before_pie](feedback_seam_trace_before_pie.md), [measure_the_clip_first](feedback_measure_the_clip_first.md), [log_reading_traps](feedback_log_reading_traps.md) — ★ checks.
- [abp_internal_graph_blindspot](feedback_abp_internal_graph_blindspot.md), [blendstack_input_node_ref](feedback_blendstack_input_node_ref.md), [posesearch_branchin_db_sync](feedback_posesearch_branchin_db_sync.md) — graph blindspots.
- [console_variables_lifetime](feedback_console_variables_lifetime.md), [build_paging_file_parallelism](feedback_build_paging_file_parallelism.md), [cpp_executescript_harness](feedback_cpp_executescript_harness.md) — CVars; builds; harness.

## GASP reference (imported at /Game/GameAnimationSample/)
- [gasp58_update_audit](project_gasp58_update_audit.md) ★★ · [gasp_cmc_abp_spec](project_gasp_cmc_abp_spec.md) ★★ · [gasp_pawn_bp_full](gasp_pawn_bp_full.md) · [gasp_movement_modes](gasp_movement_modes.md) · [index_gasp_reference](index_gasp_reference.md)

## Project state
- [asset_roundtrip](project_asset_roundtrip.md) — UE/Blender separate-and-reimport recipe.
- [protagonist_face_design](project_protagonist_face_design.md) — teacher face 03 applied.
- [weapon_switch](project_weapon_switch_2026-09-11.md) — holster/draw built; check pending.
- [rifle_aim_upper_body_lock](project_rifle_aim_upper_body_lock_2026-09-10.md), [hero_camera_aim_cone](project_hero_camera_aim_cone_2026-09-09.md), [rifle_mh_native_migration](project_rifle_mh_native_migration_2026-09-09.md) — ★★ rifle/camera.
- [jump_system_status](project_jump_system_status.md), [physics_jump_plan](project_physics_jump_plan.md) — ★★ jumps.
- [hud_design](project_hud_design.md) — HUD phase 1.
- [psia_heavy_strike_plan](project_psia_heavy_strike_plan.md), [punch_contact_next_session](project_punch_contact_next_session.md), [combat_next_session_plans](project_combat_next_session_plans.md) — ★★★ combat state.
- [combat_system_plan](project_combat_system_plan.md), [combat_fist_build_plan](project_combat_fist_build_plan.md), [combat_arch_refactor](project_combat_arch_refactor.md), [grab_grapple_design](project_grab_grapple_design.md) — melee design.
- [motion_warping](project_motion_warping.md), [contextual_anim_mover_assessment](project_contextual_anim_mover_assessment.md) — warping; CAS deferred.
- [audit_2026-06-12](project_audit_2026-06-12.md) — ★ P0–P3 backlog.
- [architecture_rationale](project_architecture_rationale.md), [v2_architecture](project_v2_architecture.md), [v2_locomotion_progress](project_v2_locomotion_progress.md), [idle_tip_implementation](project_idle_tip_implementation.md), [locomotion_sm_refactor_plan](project_locomotion_sm_refactor_plan.md) — v2 locomotion.
- [input_stack_rt_mirror](project_input_stack_rt_mirror.md), [crouch_system](project_crouch_system.md), [traversal_system](project_traversal_system.md), [obstacle_reaction_system](project_obstacle_reaction_system.md), [movement_clearance_plan](project_movement_clearance_plan.md) — input/movement.
- Chalkie AI: [npc_foundation](project_npc_foundation.md), [zombie_ai_plan](project_zombie_ai_plan.md), [chalkie_territory_combat_plan](project_chalkie_territory_combat_plan.md), [chalkie_fight_rules](project_chalkie_fight_rules.md), [crowd_engagement_design](project_crowd_engagement_design.md).
- [index_archive_cmc_ports](index_archive_cmc_ports.md) — CMC/MM/GASP ledgers; ★★★ [locomotion_quality_standard](project_locomotion_quality_standard.md) = THE STANDARD.
- [multipawn_class_design](project_multipawn_class_design.md), [sp_first_coop_extensible](project_sp_first_coop_extensible.md) — multi-pawn; SP-first.
- [mover_5_7_to_5_8_diff](project_mover_5_7_to_5_8_diff.md), [ue58_migration](project_ue58_migration_2026-05-10.md), [iris_replication_5_8](project_iris_replication_5_8.md), [local_plugin_patches](project_local_plugin_patches.md), [root_motion_mode](project_root_motion_mode.md), [mover_root_motion](reference_mover_root_motion.md) — 5.8 migration.
- [gas_gameplay](project_gas_gameplay.md), [inventory-system](inventory-system.md), [weapon_swap_architecture](weapon_swap_architecture.md), [ik_setup](feedback_ik_setup.md) — GAS; inventory; weapons; IK.

## Catalogs
- [rider_mcp_new_tools](reference_rider_mcp_new_tools.md) · [noweapon_anim_catalog](reference_noweapon_anim_catalog.md) · [punch_reaction_content_inventory](reference_punch_reaction_content_inventory.md) · [ue5_python_posesearch](reference_ue5_python_posesearch.md)
- [mirror_anim_bake](reference_mirror_anim_bake.md) — ★★ MirrorDataTable + baked copy.
- [Launcher Add to Project](reference_launcher_add_to_project.md) — ★ temp project + copy incl. __ExternalActors__.
- [Prop optimization + VT](project_prop_optimization_vt.md) — imported-prop tool; VT project-wide.
