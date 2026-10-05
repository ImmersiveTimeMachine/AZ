---
name: project_winchester_turn_starts_2026-10-05
description: Winchester walk/run 180 "legs turn first, upper body after" = borrowed rifle turn starts + MESH-SPACE upper-body lock; fixed by baked AZ_WIN_WalkStart* (GraftUpperBody) + AZ_OwnWeaponHold lock exemption.
metadata:
  type: project
---
**User report 2026-10-05:** holding the Winchester and just walking, a 180 turn looks unnatural. The lower body turns first and the upper body follows.

**Cause (verified in log, profile and ABP):**
- The Winchester has no turn starts of its own. CHT_v2 rows 495-502 and 527-530 (c8 = Weapon.Rifle.Winchester; walk / run / sprint x 90 / 180 x L / R) borrow AZ_RTG_MH_Rifle_WalkFwdStart{90,180}_{L,R}. These are RifleAnimsetPro clips.
- The DA's bUpperBodyFromAimPoseInTransitions is on, so the lock layer replaces spine_01 and above with AZ_MST_Rifle01_St_Idle00.
- The lock is a Layered blend per bone (GUID 4920316949C2319082FF6BB0C8CEC966) with bMeshSpaceRotationBlend = TRUE. The torso therefore faces the capsule while the clip's hips turn ahead of it.
- The same lock also covers the straight start / stops (489-494, fine: no rotation) and the crouch turn starts 531-536. Those are unarmed clips and very likely show the same split; not fixed yet.

**Fix (user rule: bake):**
- New clips `/Game/AZ/Assets/Winchester/Turns/AZ_WIN_WalkStart{90,180}_{L,R}`, built by GraftUpperBody:
  - body: the borrowed clip;
  - arms: Idle00 relative to the chest;
  - stance delta on spine_01..05: about 4-8 deg;
  - curves: AZ_Grip_L/R plus `AZ_OwnWeaponHold = 1`.
- All checks 0.000 cm. The 12 rows were rewired. Backup: AZ_Backups/2026-10-05_WinchesterTurnStarts_171045. Script: Tools/diagnostics/bake_winchester_turn_starts.py.
- `UAZ_MoverAnimInstance` (.cpp, Live Coding): a transition clip with the AZ_OwnWeaponHold curve (checked with HasCurveData on BlendStackInputs.Anim) is not "borrowed". The lock stays off and the release hold is cleared.
- GraftUpperBody now accepts an ArmDonor on ANOTHER skeleton, provided its arm and stance bones match the hero's reference pose (SK_AZ_Master == metahuman_base_skel, 0.0000 cm). The arms check is then done raw.
- TODO at the next closed-editor build: the AZUpperBodyGraft.h comment still says "ArmDonor on HeroMesh's skeleton". It was not edited because the editor was open (Live Coding plus a header is a risk).

**Status:** PIE pending.

**Open:**
- The crouch turn starts need the same treatment.
- At jog speed (~250) the Winchester reversal still uses a from-rest start clip (the M16 has a momentum pivot, see [[project_m16_sprint_turns_2026-10-05]]).
- Saving new assets under the Rider debugger hit the FAppTime ensure again; resumed with xdebug.

**Second step, same day: running reversal.**
- **User:** "running, the 180 is strange: turn, the body sways back, then snaps into the run".
- **Log:** a moving 180 at ~220 cm/s played AZ_WIN_WalkStart180, a from-REST clip, so the RM bridge killed the speed.
  Before it, a 0.1 s WalkFwdStop blip appears (human W→S key gap, MoveKeys log shows `released` and then `held=[S]`).
- **Fix:** baked `/Game/AZ/Assets/Winchester/Turns/AZ_WIN_RunPivot180_{L,R}_{LU,RU}`.
  - Body: AnimPro_RunFwdTurn180_*. Arms: AZ_MST_Rifle01_St_Run_F_IPC. Templates: copies of the M16 pivots.
  - Curves: AZ_Grip_L/R + AZ_OwnWeaponHold = 1. Stance delta: spine_01 ~17-23 deg.
- **Rows:** 501/502 (run) and 529/530 (sprint) narrowed to c11 = False. New rows 577-584: c11 = True; c4 True → *_RU, c4 False → *_LU. Chooser now has 585 rows.
- Backup: AZ_Backups/2026-10-05_WinchesterRunPivots_172437. Script: Tools/diagnostics/bake_winchester_run_pivots.py.
- Status: PIE pending.
- If the stop blip still reads, the only fix is a short reversal grace before stops, a code change in the SM. Not done.

**SUPERSEDED 2026-10-05 evening** by [[project_winchester_on_m16_set_2026-10-05]]: the Winchester no longer uses its own locomotion rows.
