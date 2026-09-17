# Armed moving180 chooser correction

September16 local / September17,2026 UTC. User reported the new rifle sprint loop switching to unarmed animations for180° reversals, then reported the same problem with pistol run.

Logs confirmed `AZ_RTG_MH_Rifle_SprintLoop → AnimPro_RunFwdTurn180_*` with gait2 and `AS_Pistol_RunFwdLoop → AnimPro_RunFwdTurn180_*` with gait1. Rifle's existing moving-turn rows only accepted Run and explicitly rejected Movement.Sprinting. Pistol's Run/Sprint weapon turn rows required MovingTransition=False, so a moving reversal missed them.

## Saved change

Only `/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations` was modified, from403 to407 rows:

- New rows403–406 clone the four working rifle Run moving180 variants276–279, change Gait to Sprint and clear their NOT Movement.Sprinting filter. Existing rifle Run choices and the new rifle sprint loop are preserved.
- Pistol rows349/350 (Run) and391/392 (Sprint) now accept MovingTransition=Any, covering both their existing from-rest case and moving reversals.
- Shared unarmed Run pivot rows24–27 now exclude Weapon.Pistol; their existing Rifle exclusion is preserved.
- Shared unarmed Sprint pivot rows94–97 now exclude both Weapon.Rifle and Weapon.Pistol.
- Every pre-existing result asset, disabled state and other filter/output cell was preserved. No animation, skeleton, database, C++ source, speed or playback-rate changes.

The pistol assets are existing exact-MH `AS_Pistol_WalkFwdStart180_L_RM` and `_R_RM` under `/Game/AZ/Assets/Pistol/Runtime/`,1.0s and1.3333s, root motion enabled, nonlooping,1x, with approximately180° root rotation. Rifle uses the same exact-MH W2 relaxed/aimed turn-start clips already used successfully for rifle Run.

These are **turn-start fallbacks**, not newly authored momentum-preserving sprint pivots. They keep weapon posture correct but may decelerate before the reversal. No retiming or hidden blend trick was added; dedicated moving pivots would be separate content/polish work.

## Verification

Native chooser compile/save succeeded. Readback verified407 rows, all403 old results preserved, the exact intended filter edits, and unchanged enabled states. The saved chooser is clean. No C++ rebuild is required. No agent-started PIE or automated tests.

Author/verify: `C:/UnrealEngine/Games/AZ/Tools/weapon_moving_180_setup.py`.
Backup/baseline and final readbacks: `C:/UnrealEngine/Games/AZ/Saved/WeaponMoving180/`.
Initial log/asset audit: `C:/UnrealEngine/Games/AZ/Saved/RifleSprintTurns/`.

Use this latest verifier for the407-row table. The previous rifle-sprint-loop verifier intentionally describes its earlier403-row snapshot and should not be treated as a current full-table acceptance check or rerun in author mode.

User check: with rifle and pistol, reverse left and right180° while running and sprinting; confirm the weapon posture stays correct and the actor turns/returns to the proper loop. Review deceleration and blending separately from the corrected clip selection.
