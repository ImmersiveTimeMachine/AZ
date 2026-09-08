# Rifle P01 exploration and aiming — ready for user validation

Updated 2026-09-07. The user selected existing /Game/AZ/Assets/RTG/Riffle_P01 as the first reference. This animation/aiming integration is authored, compiled and saved. Runtime acceptance is pending the user's PIE run. Firing, reload and HUD finish remain later phases.

**Latest correction after user feedback:** the sole main chooser now has302rows, including regular rifle ground transitions, an explicit rifle turn-start fallback for moving180, and20derived two-phase rifle jump/land roles with Sprint fallbacks. Both new chooser audits pass; the10member landing DB indexed successfully. Sprint-back attachment and opt-in end-frame weapon pose diagnostics are written and their four translation units compile individually; the full editor-closed build is still pending (do not Live Code the module lifecycle/layout change). Current resume/acceptance: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-p01-pose-debug-sprint.md. Both separately fitted relaxed/aim sockets are retained for the new runtime measurements. Earlier155/276-row and shared-jump statements below describe preceding baselines.

## Implemented and saved

- The existing /Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations is the sole runtime chooser. Its original 103 rows, cells and enabled/disabled states are preserved, with 52 P01 idle/movement choices appended (155 total). Weapon.Rifle tag filters separate normal unarmed rows from rifle rows before Randomize. Existing ground transitions, reactions, hybrid jump and sprint remain shared where P01 lacks replacement content.
- The provisional /Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/CHT_P01_CharacterAnimations was removed after consolidation, zero referencers and a disk backup were verified. No per-profile chooser override remains in C++ or the profile class.
- Existing P01 assets are reused: 48 directional relaxed/aimed Walk/Jog/CrouchWalk loops and four standing/crouched idles have Loop enabled. All 48 moving loops retain Speed and have contact_l/contact_r. Six PSD_P01_* databases each contain eight BranchIn-owned entries with disable-reselection. No P01 retargeting was performed.
- 44 contact curves were measured on verified matching source timelines. Four export/seam discrepancies were resolved from actual target foot poses plus their own Speed curves and periodic derivatives. Core contacts were near the floor, opposed travel at 0.972–1.007 speed ratios and had matching cyclic endpoints. These are authored contact candidates; visual quality is still a runtime gate.
- DA_WeaponAnim_P01 under /Game/AZ/Blueprints/Animation/MotionMatching/RifleP01 is assigned on the existing M16 pickup's WeaponState fragment. Its five native database overrides and RelaxedUpperBodyPose are null, so the CHT-selected raw clip owns MM vocabulary. The profile's exact 48-loop allowlist enables per-sample Speed-based rate correction. Unarmed additive lean and the pack-specific phase lock remain disabled.
- Existing /Game/AZ/Blueprints/Animation/AO_Rifle_Aim supplies the 17 P01 offset poses. Both stance profile references use this shared offset, masked above spine_02 to preserve base lower-body pose, curves and root motion. This is not a dedicated crouched AO set; crouched grip/vertical alignment requires user review.
- Active /Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC has the compiled/saved aim mask before FullBody and the inner BlendStackInput play-rate connection. Existing Inertialization and BS Output cache edits were preserved. GetWeaponLoopPlayRate reads each actual inner sample/time and only adjusts owned moving loops.
- BP_AZ_GA_FirearmAim is the rifle's sole newly granted ability: Input.Action.Aim, required Weapon.Rifle, no duplicate Blueprint-owned aim/strafe tags. The native ability validates the active weapon, owns counted aiming/strafe contributions, cancels/blocks sprint, waits for release and cleans up on switch/drop/menu/interrupt. Legacy Shoot/Aim are not granted.
- AZ_IA_RT_Aim maps RMB in AZ_IMC_RT_PawnInputs and has one InputConfig entry. Aim starts on a fresh press only. Existing fist secondary-attack input is preserved through selected-profile grants. Native Mover camera/facing consumes aim state; fist idle-facing behavior remains unchanged.
- Pickup Blueprint and aim Blueprint compiled UpToDate through native tools and were saved afterward. Both placed M16 pickups in L_001 read back with the new profile. One initial inserted magazine manifest remains unchanged. L_001 was backed up and saved; the map remains excluded by the repository's existing ignore policy.

## Verification completed

Final full AZEditor Win64 Development build: Result: Succeeded, eight actions, 22.66 seconds. This clean build includes editable data-asset metadata, removal of profile chooser re-evaluation and the scalar-pin helper fix. It resolves the stale Live Coding profile type seen during intermediate authoring. No temporary generated C++ scripts remain.

All six database BuildIndex Succeeded lines are in Saved/Logs/AZ.log at 19:29:44 UTC. Final readback reports: chooser already_consolidated, 155 rows; activation audit_complete; graph verified_saved_graph; provisional chooser absent; no dirty content packages. Both placed rifles contain the profile and a fresh aim ability instance has the intended input/required tags.

Receipts under C:/UnrealEngine/Games/AZ/Saved/RifleAnimationContent/: p01-consolidate-prepare.json, p01-activate-prepare.json, p01-final-asset-readback.json, p01-contacts-prepare.json, p01-four-target-contacts-prepare.json and p01-four-own-speed-diagnostics.json. Graph GUIDs and wiring receipt: C:/UnrealEngine/Games/AZ/Saved/RifleP01Graph/graph-receipt.json.

Finalization backups (map and unused chooser): C:/UnrealEngine/Games/AZ/Saved/Backups/RifleP01Finalization/20260907T194018/. Earlier P01 asset backups are under Saved/RifleP01Backups and Saved/Backups/RifleP01Contacts*.

## User acceptance and continuation

Ask the user to run PIE: pick up a rifle with E, select with 1, move standing/crouched, hold/release RMB, open/close inventory with I, switch to fists with 0, and drop/re-pick the rifle. Verify aim cleanup, fresh-input behavior and fist attacks. Then inspect Saved/Logs/AZ.log for [Aim], [Equipment], [Inventory], [v2 Pick], [v2 Snap]/[v2 Replay] and PoseSearch failures. Do not claim runtime success until the user reports it or evidence establishes each case.

P01 has no complete ground starts/stops/pivots, stance-change or forward sprint replacements; those remain explicit shared-animation fallbacks. Weapon grip and shared crouched AO need visual tuning. Shooting and reload were not implemented in this phase.

AGENTS.md: no automated tests unless explicitly requested; ask before starting PIE or editor tests. Codex added/ran no automated tests and did not start PIE. The user ran an intermediate PIE before wiring was complete and saw stalled playback from an unfinished rate override; that override was disabled until wiring was complete, then verified connected and compiled. Do not mistake that intermediate run for final acceptance.

The user drives editor close/reopen per C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/feedback_editor_close_build_open_loop.md. No further restart is currently needed. Preserve unrelated dirty/untracked work, including the pre-existing test directory. Nothing has been committed.

Earlier RifleAnimsetPro candidate retargets and rig remain unassigned under /Game/AZ/Assets/RTG/RifleAnimsetPro and /Game/AZ/Blueprints/Animation/MotionMatching/Rifle/Rigs. They are outside this selected P01 integration; do not resume that retarget pipeline.
