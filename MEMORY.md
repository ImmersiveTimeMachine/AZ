# AZ Project Memory — "CHALK"

**★★★ LANGUAGE: Artur ALWAYS speaks Russian — answer in Russian; English-looking text = misrecognized Russian speech, never another language ([user_language_russian.md](user_language_russian.md)).**

UE 5.8 third-person survival-horror **CHALK**: 2024 Montreal outbreak, drug "Elysium/Chalk" mutates addicts into chalk-skinned infected ("Chalkies") — body-horror, NOT zombies. Lore: [project_lore_chalk.md](project_lore_chalk.md). Quickref: [reference_project_quickref.md](reference_project_quickref.md). Engine `C:\UnrealEngine\Engine\`, project `C:\UnrealEngine\Games\AZ`.

## Skills (lazy-loaded)
`az-workspace` (load first) · `agent-and-research-discipline` · `cpp-build-livecoding` · `bp-to-cpp-port` · `unrealclaude-mcp-tools` · `anim-debug-pitfalls` · `az-cpp-utility-tools` · `gasp-parity-reference` · `asset-modification-via-python`.

## ★★★ CURRENT DIRECTION — read first
- [Leg IK knee twist fix](project_leg_ik_knee_twist_fix_2026-10-04.md) — ★★★ cause = leg solve (forced twist + fixed-hinge pole); fix weight 0 + knee-plane pole; user: "much better"; identity-test rule.
- [Crouch start<->loop flap](project_crouch_start_loop_flap_2026-10-04.md) — ★★ fixed 2026-10-04: borrowed crouch turn starts, 5 PSDs → Master schema, SM pivot block; PIE pending.
- [M16 sprint turns](project_m16_sprint_turns_2026-10-05.md) — ★★ left hand reaching on sprint turns = shared unarmed rows 88-93 leaked to M16; fixed 2026-10-05 (c9 + rows 537-548); run/sprint pivots = BAKED AZ_M16_RunPivot180_* (GraftUpperBody), user PIE OK 2026-10-05.
- [Winchester on the M16 set](project_winchester_on_m16_set_2026-10-05.md) — ★★★ Winchester MOVES on W2 (LocomotionWeaponTag=Weapon.Rifle); own HOLD (AZ Weapon Hold node: RifleMega arms in chest space) + lever fire + tube reload from RifleMega; PIE pending. (Supersedes the turn-start/pivot bakes.)
- [Protected weapon sets](project_protected_weapon_sets.md) — ★★★ M16/pistol/unarmed clips are tuned: never touch; backup AZ_Backups/2026-09-27_pre-WGS.
- [Consider the local model](feedback_consider_local_model.md) — ★★★ USER RULE: delegate mechanical steps to the local LM Studio agent (skill `local-model-delegation`).
- [Master skeleton](project_master_skeleton_2026-09-25.md) — ★★★ SK_AZ_Master = MH copy + az_weapon_r/az_prop_*; 446 AZ_MST_* clips; Master clips NEED retarget_source_asset=SKM_AZ_Master.
- [Natural Grip C++ plugin](project_natural_grip_plugin_2026-10-02.md) — ★★★ Plugins/AZNaturalGrip (grip solver + BakeSwitchClip + GraftUpperBody); left hand on switch + crouched M16 legs SOLVED; docs design-briefs/left-hand-weapon-switch-h1-h2-checkpoint.md.
- [Natural grasp](project_natural_grasp_2026-09-29.md) — ★★★ crooked fingers = palm in stock + twisted retarget; hand re-grip + baked anatomical grasp.
- [Weapon arm solver](project_weapon_arm_solver_2026-09-28.md) — ★★★ Body Clearance = body push + right-arm SDF solver; user order: Winchester → shotgun → M16/pistol → knife.
- [Winchester integration audit](project_winchester_integration_audit_2026-09-26.md) — ★★★ R1/R2 done; R3 reload designed, awaiting go; plans docs/design-briefs/winchester-*.md.
- [RifleMega retarget](project_riflemega_retarget_2026-09-25.md) — ★★★ 446 pack clips on the MetaHuman (exact + IK post); rifle wiring next, then throwables.
- [Local agent (LM Studio)](reference_local_agent_lmstudio.md) — .agents/agent_script.py: local model + MCP + skills for light tasks; measured limits.
- [Weapon models import](project_weapon_models_import_2026-09-25.md) — 7 guns as parts in /Game/AZ/Assets/Weapons; ".py in @Description" trap.
- [Gamepad analog vs gait](project_gamepad_analog_gait_mismatch.md) — ✔ DONE 2026-09-23: stick magnitude→1 in ProduceInput (raw kept for attack-cancel); verified on pad.
- [Throwable RESUME](project_throwable_resume_2026-09-17.md) — ★★★ throwables start here: review response, slot splice, standing/crouch policy, my corrected claims.
- [Throwable art](project_throwable_art_assets.md) — chosen grenade icon + M67 mesh (real scale); no stone art.
- [Traversal resume](project_traversal_2026-09-15.md) — ★★★ mantle+hurdle+climb on one executor; warp-point contract, per-clip clearances, open list.
- [Climb spec](project_climb_spec.md) — ★★★ user work order for high-ledge climb.
- [Hurdle spec](project_hurdle_spec.md) — ★★★ user work order: geometry-first hurdle-vs-mantle, one data-driven executor.
- [Mantle handoff spec](project_mantle_handoff_spec.md) — ★★★ 13-point user order for walk/run mantle→locomotion; read before traversal.
- [Mantle traversal](project_mantle_traversal_2026-09-14.md) — ★★★ standing mantle shipped; walk/run = play START only, hand back to loop.
- [Aim TIP](project_aim_tip_next_session_2026-09-12.md) — ★★★ aim turn-in-place shipped (rifle/pistol loops, user-tuned yaw clamp); follow-ups.
- [Pistol pattern for rifle](project_pistol_pattern_for_rifle_2026-09-12.md) — ★★ user prefers the pistol pattern; rifle mapping + brief.
- [Mover + MetaHuman](project_mover_metahuman_2026-08-31.md) — ★★★ verdict: Mover + MetaHuman hero, IK off, CMC parked @210247b.
- [MM spine continuity](feedback_mover_spine_search_continuity.md) — ★★★ read before touching the Mover spine MotionMatch call or any loop DB.

- [NetLab learning project](project_netlab_learning.md) — ★★★ separate repo C:/Projects/NetLab: UE-style replication over WinSock; I give code IN CHAT piece by piece, Artur transfers it; on disk only the skeleton.

## Workflow rules
- [feedback_rider_debugger_ensure_resume.md](feedback_rider_debugger_ensure_resume.md) — ★★★ editor under Rider debugger: an ensure PAUSES it → xdebug resume (F5).
- [feedback_offer_runtime_solution_first.md](feedback_offer_runtime_solution_first.md) — ★★★ "universal" request → runtime markers+IK first; anticipate failure axes.
- [feedback_bake_corrections_into_animation.md](feedback_bake_corrections_into_animation.md) — ★★★ USER RULE: fix poses OFFLINE and bake a corrected clip per situation; no per-frame runtime solvers/pushes (jerked 2026-10-03).
- [feedback_deliver_result_not_analysis.md](feedback_deliver_result_not_analysis.md) — ★★★ результат, а не разбор: значения выбирать самому, без A/B/C и цифр.
- [feedback_open_the_file_i_must_edit.md](feedback_open_the_file_i_must_edit.md) — ★★★ просишь поменять руками — сначала открой ассет/файл.
- [feedback_history_rewrite_deletes_files.md](feedback_history_rewrite_deletes_files.md) — ★★★ stripping files from git history deletes them from disk (face mesh lost 2026-09-22); restore recipe.
- [feedback_parallel_editor_edits.md](feedback_parallel_editor_edits.md) — ★★★ other agents edit in parallel: re-check, anchor on content, stage only your hunks.
- [feedback_inform_before_proposing.md](feedback_inform_before_proposing.md) — ★★★ grep memory + AZ_*Utils before proposing; tuned values have reasons.
- [feedback_verify_never_presume.md](feedback_verify_never_presume.md) — ★★★ no claim without a fresh read; verify changes landed.
- [feedback_stop_the_patch_loop.md](feedback_stop_the_patch_loop.md) — ★★★ two strikes → re-model; falsifiable prediction, one variable per test.
- [feedback_aaa_design_first.md](feedback_aaa_design_first.md) — ★ design + failure axes before code; wait for go.
- [feedback_model_routing_policy.md](feedback_model_routing_policy.md) — ★★★ I'm the brain: precise step-by-step spec → Sonnet executes → I verify. Haiku banned.
- [feedback_no_hardcoded_asset_paths.md](feedback_no_hardcoded_asset_paths.md) — ★ no /Game/ paths in C++.
- [feedback_mover_mode_state_not_rollback_safe.md](feedback_mover_mode_state_not_rollback_safe.md) — ★★★ sim decisions ride FAZ_MoverCustomInputs, never mode members.
- [feedback_posesearch_mm_mechanism_rules.md](feedback_posesearch_mm_mechanism_rules.md) — ★★★ MM rules R1–R17.
- [feedback_animpose_root_motion_double_count.md](feedback_animpose_root_motion_double_count.md) — ★★★ AnimPose world space already has root motion; never add the root track.
- [feedback_ik_retargeter_exact_transfer.md](feedback_ik_retargeter_exact_transfer.md) — ★★★ exact retarget = aligned pose + FK-only; 5.8 API traps.
- [feedback_parallel_build_header_edit_corruption.md](feedback_parallel_build_header_edit_corruption.md) — ★★ header edited mid-build → startup crash; wipe objs, rebuild.
- [feedback_livecoding_header_reinstances_animbp.md](feedback_livecoding_header_reinstances_animbp.md) — ★★ LC patch touching a header silently breaks AnimBPs; .cpp only.
- [feedback_slot_mask_weight_squared.md](feedback_slot_mask_weight_squared.md) — ★★ montage-weight-gated LayeredBoneBlend squares the blend; use a 0/1 gate.
- [feedback_montage_blend_profile_cross_skeleton.md](feedback_montage_blend_profile_cross_skeleton.md) — ★★ CRASH: retargeted montages keep source-skeleton blend profiles.
- [feedback_held_input_retries_ability.md](feedback_held_input_retries_ability.md) — ★★ held key re-activates ended abilities; exclude per-tag.
- [feedback_motionwarping_warppoint_provider.md](feedback_motionwarping_warppoint_provider.md) — ★★ missing warp-point bone = identity; Mover warping traps.
- [feedback_python_gc_crash.md](feedback_python_gc_crash.md) — ★★ Python-from-MCP crash/deadlock hazards; RM authoring recipe.
- [feedback_python_save_only_if_dirty.md](feedback_python_save_only_if_dirty.md) — ★★ save_loaded_asset can silently skip; verify by mtime.
- [feedback_chooser_row_add_tag_columns.md](feedback_chooser_row_add_tag_columns.md) — ★★ short chooser rows silently filtered; CHT_v2 c0–c20 map.
- [feedback_chooser_column_reorder.md](feedback_chooser_column_reorder.md), [feedback_chooser_autoremap_fuzzy_unsafe.md](feedback_chooser_autoremap_fuzzy_unsafe.md) — chooser scripted-edit gotchas.
- [feedback_bp_cdo_write_needs_recompile.md](feedback_bp_cdo_write_needs_recompile.md) — ★★ scripted CDO writes need compile_blueprint.
- [feedback_mover_visual_component_two_writers.md](feedback_mover_visual_component_two_writers.md) — ★★ Mover owns mesh relative transform; sample at OnEndFrame.
- [feedback_offsetrootbone_mover_graph.md](feedback_offsetrootbone_mover_graph.md) — ★★ OffsetRootBone in the MHC ABP; verify a node is committed.
- [feedback_metahuman_modular_hero.md](feedback_metahuman_modular_hero.md) — ★ MH re-assembly wipes skeleton/sockets (Tools/metahuman_fixup.py).
- [feedback_leaderpose_lod_bone_reduction.md](feedback_leaderpose_lod_bone_reduction.md) — ★ garments blowing off = LOD bone reduction.
- [feedback_planted_foot_curve_guard.md](feedback_planted_foot_curve_guard.md) — ★ write bLeftFootDown only with a contact curve.
- [feedback_seam_trace_before_pie.md](feedback_seam_trace_before_pie.md) — ★ trace one loop with real numbers before PIE.
- [feedback_measure_the_clip_first.md](feedback_measure_the_clip_first.md) — ★ reversal metric flags lively idles; standalone-actor test.
- [feedback_log_reading_traps.md](feedback_log_reading_traps.md) — ★ Slomo fakes timings; T3D omits editor props; FLT_MAX = no index.
- [feedback_abp_internal_graph_blindspot.md](feedback_abp_internal_graph_blindspot.md), [feedback_blendstack_input_node_ref.md](feedback_blendstack_input_node_ref.md) — BlendStack graph blindspots.
- [feedback_posesearch_branchin_db_sync.md](feedback_posesearch_branchin_db_sync.md) — PSD membership via BranchIn only.
- [feedback_console_variables_lifetime.md](feedback_console_variables_lifetime.md), [feedback_build_paging_file_parallelism.md](feedback_build_paging_file_parallelism.md), [feedback_cpp_executescript_harness.md](feedback_cpp_executescript_harness.md) — CVars; build parallelism; cpp harness.

## GASP reference (imported at /Game/GameAnimationSample/)
- [project_gasp58_update_audit.md](project_gasp58_update_audit.md) ★★ · [project_gasp_cmc_abp_spec.md](project_gasp_cmc_abp_spec.md) ★★ · [gasp_pawn_bp_full.md](gasp_pawn_bp_full.md) (Get_Gait analog styles) · [gasp_movement_modes.md](gasp_movement_modes.md)
- Detailed GASP notes (ABP architecture/audit, data model, SM/TIP flow, choosers, notifies, settings, cameras, components, C++): [index_gasp_reference.md](index_gasp_reference.md)

## Project state
- [project_asset_roundtrip.md](project_asset_roundtrip.md) — UE/Blender separate-and-reimport recipe (skill in ~/.codex/skills/ue-blender-asset-roundtrip).
- [project_protagonist_face_design.md](project_protagonist_face_design.md) — teacher face 03 applied; originals preserved.
- [project_weapon_switch_2026-09-11.md](project_weapon_switch_2026-09-11.md) — rifle/pistol holster/draw + carry actors built; user check pending.
- [project_rifle_aim_upper_body_lock_2026-09-10.md](project_rifle_aim_upper_body_lock_2026-09-10.md) — ★★ aim upper-body lock; TRAP: rejected pin bindings fall back to the literal.
- [project_hero_camera_aim_cone_2026-09-09.md](project_hero_camera_aim_cone_2026-09-09.md) — ★★ per-mode camera offsets + 60° aim cone, in UpdateCameraForMode.
- [project_rifle_mh_native_migration_2026-09-09.md](project_rifle_mh_native_migration_2026-09-09.md) — ★★★ rifle set is MetaHuman-native (AZ_RTG_MH_*); migration traps.
- [project_jump_system_status.md](project_jump_system_status.md), [project_physics_jump_plan.md](project_physics_jump_plan.md) — ★★ rifle three-phase jump shipped; unarmed 2-clip jump.
- [project_hud_design.md](project_hud_design.md) — HUD phase 1 done; keep inventory layout; brief docs/design-briefs/hud-phase1-status.md.
- [project_psia_heavy_strike_plan.md](project_psia_heavy_strike_plan.md) — ★★★ PSIA strikes shipped; open items; PSIA edits PreCancel the DB index.
- [project_punch_contact_next_session.md](project_punch_contact_next_session.md) — ★★★ punch contact + grab catch committed; open: facing spring, from-behind catch.
- [project_combat_next_session_plans.md](project_combat_next_session_plans.md) — ★★ combat backlog P0–P2.
- [project_combat_system_plan.md](project_combat_system_plan.md), [project_combat_fist_build_plan.md](project_combat_fist_build_plan.md), [project_combat_arch_refactor.md](project_combat_arch_refactor.md) — melee design, fist build, refactor.
- [project_grab_grapple_design.md](project_grab_grapple_design.md) — ★ GRAB = PoseSearch Interaction; step 2 = C++ MotionMatchMulti driver.
- [project_motion_warping.md](project_motion_warping.md) — ★ warping on Mover. [project_contextual_anim_mover_assessment.md](project_contextual_anim_mover_assessment.md) — CAS deferred.
- [project_audit_2026-06-12.md](project_audit_2026-06-12.md) — ★ 5-agent audit, P0–P3 backlog.
- [project_architecture_rationale.md](project_architecture_rationale.md), [project_v2_architecture.md](project_v2_architecture.md) — why Mover; v2 doctrine.
- [project_v2_locomotion_progress.md](project_v2_locomotion_progress.md), [project_idle_tip_implementation.md](project_idle_tip_implementation.md), [project_locomotion_sm_refactor_plan.md](project_locomotion_sm_refactor_plan.md) — v2 locomotion, idle TIP, SM extraction.
- [project_input_stack_rt_mirror.md](project_input_stack_rt_mirror.md), [project_crouch_system.md](project_crouch_system.md) — RT input mirror; crouch.
- [project_traversal_system.md](project_traversal_system.md), [project_obstacle_reaction_system.md](project_obstacle_reaction_system.md), [project_movement_clearance_plan.md](project_movement_clearance_plan.md) — traversal, obstacles, clearance.
- Chalkie AI: [project_npc_foundation.md](project_npc_foundation.md), [project_zombie_ai_plan.md](project_zombie_ai_plan.md), [project_chalkie_territory_combat_plan.md](project_chalkie_territory_combat_plan.md), [project_chalkie_fight_rules.md](project_chalkie_fight_rules.md), [project_crowd_engagement_design.md](project_crowd_engagement_design.md).
- CMC spike (parked @210247b) + early MM + GASP port ledgers: [index_archive_cmc_ports.md](index_archive_cmc_ports.md); ★★★ [project_locomotion_quality_standard.md](project_locomotion_quality_standard.md) = THE STANDARD.
- [project_multipawn_class_design.md](project_multipawn_class_design.md), [project_sp_first_coop_extensible.md](project_sp_first_coop_extensible.md) — multi-pawn rules; SP-first scope.
- [project_mover_5_7_to_5_8_diff.md](project_mover_5_7_to_5_8_diff.md), [project_ue58_migration_2026-05-10.md](project_ue58_migration_2026-05-10.md), [project_iris_replication_5_8.md](project_iris_replication_5_8.md), [project_local_plugin_patches.md](project_local_plugin_patches.md), [project_root_motion_mode.md](project_root_motion_mode.md), [reference_mover_root_motion.md](reference_mover_root_motion.md) — Mover/5.8 migration.
- [project_gas_gameplay.md](project_gas_gameplay.md), [inventory-system.md](inventory-system.md), [weapon_swap_architecture.md](weapon_swap_architecture.md), [feedback_ik_setup.md](feedback_ik_setup.md) — GAS; inventory; weapons/sockets; hand IK.

## Catalogs
- [reference_rider_mcp_new_tools.md](reference_rider_mcp_new_tools.md) — Rider MCP tools.
- [reference_noweapon_anim_catalog.md](reference_noweapon_anim_catalog.md) — 191 NoWeapon anims.
- [reference_punch_reaction_content_inventory.md](reference_punch_reaction_content_inventory.md) — punch/react content.
- [reference_mirror_anim_bake.md](reference_mirror_anim_bake.md) — ★★ mirror = MirrorDataTable + baked copy in Unreal; API traps.
- [reference_ue5_python_posesearch.md](reference_ue5_python_posesearch.md) — Python PoseSearch API.
