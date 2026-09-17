# Rifle sprint loop replacement

**Follow-up:** armed moving180 selection is now corrected in the407-row chooser. See `C:/UnrealEngine/Games/AZ/docs/design-briefs/weapon-moving-180-status.md`; the403-row snapshot and transition limitations below describe the original loop-only pass.

September16 local / September17,2026 UTC. User requested replacing shared SprintFwdLoop1 with their new `AZ_RTG_MH_Rifle_SprintLoop` while holding a rifle. Implemented, saved and built; user gameplay review remains.

## Result

- New clip: `/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_Rifle_SprintLoop`, exact MetaHuman skeleton, length0.633333s, playback1x. Raw and evaluated root samples confirm it is in-place. Its root-motion-disabled policy is preserved; Mover supplies movement. Looping is now enabled.
- Main chooser `/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations` has403 rows. Shared row84 now excludes Weapon.Rifle. New row402 clones its other conditions/output settings, requires Weapon.Rifle and selects the new clip. All402 previous results and other cells/disabled states are preserved except the intended shared-row exclusion.
- New single-member BranchIn database: `/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/PSD_Rifle_MH_Sprint`, using the existing locomotion schema and disable-reselection policy. Native index succeeded at01:16:11.032 UTC, key `ca0ea7ebc790d6ac771ea49bae536ae02226077d`.
- Selected rifles now stay in hand during sprint. Equipment remains the attachment owner; carry refresh and socket reconciliation use the selected hand socket. Pose-debug expected-socket logic matches this. This supersedes the old shared-animation/back-carry policy.

Sprint starts/stops/turns/jumps were not replaced. Shared unarmed sprint sequence/database hashes and pistol-specific selections remain unchanged. No playback-rate, character-speed, AnimBP or level changes were made for this task.

## Contact handling

The supplied clip had no contact curves or BranchIn. The existing procedural-foot sampling, including in-place ground-stroke inference, could not corroborate reliable contact intervals. Both added contact curves use the established zero/unplanted fallback: terrain adjustment remains available without guessed foot locks. See `Saved/RifleSprintReplacement/contact-measurement.json`. Foot-lock polish and moving entry/exit appearance remain user visual checks.

## Validation and files

- Author/verify script: `C:/UnrealEngine/Games/AZ/Tools/rifle_sprint_loop_setup.py`.
- Package backups, baseline, measurement and saved readback: `C:/UnrealEngine/Games/AZ/Saved/RifleSprintReplacement/`.
- Native changes: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp`, its header comment, and `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Weapon/AZ_WeaponPoseDebug.cpp`.
- The combined build encountered two C4458 shadowing errors in concurrent throwable input additions to `AZ_PlayerController.cpp`. Renamed only those locals to `ThrowableQuickBar` and `ThrowInputTags`, preserving behavior.
- Live Coding build succeeded in57.76s; AZ.dll patch applied at2026-09-17 01:19:29.839 UTC in PID37076. During concurrent work the editor closed and a **normal build succeeded in52.48s**, explicitly compiling EquipmentComponent, WeaponPoseDebug and PlayerController and linking the AZ DLL at01:22:19 UTC. The reopened editor PID41588 loads that full DLL. New DB indexing and saved chooser/sequence/database readback passed; restarted readback is recorded separately.
- Other dirty map/MetaHuman/placeholder packages observed during the session were left untouched. No agent-started PIE or automated tests.

User acceptance: equip rifle, hold the existing sprint control and check the new loop/weapon-in-hand pose; release into normal rifle locomotion, then compare unarmed/pistol sprint. Keep playback1x. Native changes are already included in the normal DLL; no extra build is needed for this replacement. Current receipts do not claim validation of the concurrently developed throwable feature.
