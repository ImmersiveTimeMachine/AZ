---
name: project_pistol_pattern_for_rifle_2026-09-12
description: "★★ 2026-09-12 user likes the PISTOL animation pattern better and wants the rifle on the same pattern. Pistol = ONE 8-way leg set for relaxed AND aim (aim only swaps the torso via lock + AO), RM starts/stops with foot phase + turn-starts, 3-phase generic jump (Start -> FallingLoop -> Land, aim=Any), loop TIP. Rifle has everything except discrete turns, but keeps TWO leg sets (relaxed/aim) and a SurvivalMan-sourced jump. Recommendation + mapping table in docs/design-briefs/pistol-pattern-for-rifle.md."
metadata:
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-12T04:19:41.012Z
---

# Pistol pattern -> rifle (analysis, 2026-09-12)

Brief with the full tables: `docs/design-briefs/pistol-pattern-for-rifle.md`.

**Pistol (94 rows in CHT_v2, `AS_Pistol_*` on metahuman_base_skel):** one 8-way walk/run/sprint set with
`aim = Any` (WalkFwd/Bwd + Strafe L/R 45/90/135, run twins, SprintLoop, crouch 8-way); idle differs
(`Idle_Relaxed` vs `Idle`); aiming = upper-body lock on `AS_Pistol_Idle` + `AO_Pistol_Standing/Crouching`
(9 additive poses) - the LEGS never change when aim toggles. RM starts (`WalkFwdStart_RM`, turn-starts
`Start90/180_L/R_RM`, directional B/LL/RR starts, `SprintStart_RM`), RM stops per direction `_LU/_RU`,
`Idle2Crouch/Crouch2Idle`, jump `Jump_Platformer_Start -> FallingLoop -> Jump_Platformer_Land` (one generic
clip per phase, aim=Any, bUseAirLoop), TIP loops `TurnL/R_90Loop`; discrete RM turns `AZ_Pistol_TurnL/R_91`
(90 deg/1.0 s) and `_181` (180 deg/1.3-1.4 s) exist unused. Fire `ShootOnce`, reload `Reload_2` x4.

**Rifle (919 MH-native clips):** TWO leg sets (relaxed `Walk_*/Jog_*/CrouchWalk_*`, aim `*_Aim_*`, both 8-way +
backpedal, `Run_*` sprint carry 5 dirs), RM starts incl. turn-starts (`Stand_Relaxed_To_Walk_R90/L90/R180_Fwd`
+ aim twins), RM stops `_LU/_RU` + aim twins, stance transitions x2, 195 jump clips per direction/gait as
`_Start_IPC / _Air_IPC / _End_IPC` (aim and relaxed), `AO_Rifle_Aim` (17 samples), TIP loops only (no
discrete), 4 reloads, full fire set. Live jump still uses SurvivalMan-sourced `PSD_P01_Land` /
`AS_P01_Jump_*_Land` composites + generated `Fall_v2` clips.

**Recommendation (for the user to confirm):** (1) ONE leg set = the rifle AIM loops for relaxed and aim,
torso from the lock (aim idle / `RelaxedUpperBodyPose` = relaxed idle) -> aim toggle swaps torso only, no
leg-clip pop; (2) starts/stops already match; (3) jump onto the MH-native `_Start_IPC -> _Air_IPC -> _End_IPC`
per direction, rows aim=Any, retire the SurvivalMan composites; (4) TIP: rifle loops lower-body-only while
aiming ([[project_aim_tip_next_session_2026-09-12]]), pistol RM turns as legs if too slow; (5) reload/fire
stay per-stance. One leg set = one speed set (rifle aim loops depict 123/347/93).

**AO node fact (user asked "we have AO_Rifle_Aim but not for the pistol?"):** the AnimGraph node is titled
`AO_Rifle_Aim` only because that is its DEFAULT asset; its Blend Space pin is BOUND to `WeaponAimOffset`,
which the anim instance sets from the profile per stance - so the pistol runs through the same node with
`AO_Pistol_Standing/Crouching`. Both AOs: AimOffsetBlendSpace, X/Y +-90, mesh-space rotation offsets over a
frame-0 base that matches the lock idle to 0.0 cm (rifle base `Stand_Aim_Point_Center` == `Stand_Aim_Idle_IPC`;
pistol `AS_Pistol_Look_CC_Additive` == `AS_Pistol_Idle`). Rifle has 17 samples (adds +-45), pistol 9. Nothing
here explains a pistol/rifle feel difference. **Rifle RELAXED set EXISTS** (user doubted it): 10-dir walk/jog/
crouch-walk loops, relaxed idles, 12 starts, 22 stops, low-carry pose (hands 15-36 cm lower than aim) - and it
is what plays when not aiming. Recommendation revised: KEEP both leg sets; take from the pistol only the shared
jump phases + torso-only aim swap.

Related: [[project_aim_tip_next_session_2026-09-12]], [[project_jump_system_status]],
[[project_rifle_mh_native_migration_2026-09-09]], [[project_rifle_aim_upper_body_lock_2026-09-10]].
