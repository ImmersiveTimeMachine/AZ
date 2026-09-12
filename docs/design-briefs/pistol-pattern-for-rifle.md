# The pistol pattern, and how the rifle set maps onto it

**Date:** 2026-09-12 · **Status:** analysis for the next session (user: "I like the pistol anims better, do we have
similar anims for the rifle so we could keep the same pattern?")

## How the pistol moves today (94 chooser rows, `CHT_v2`)

| phase | relaxed | aiming | notes |
|---|---|---|---|
| idle | `AS_Pistol_Idle_Relaxed` | `AS_Pistol_Idle` | crouch: `CrouchLoop` for both |
| walk / run / sprint loops | ONE set, rows are `aim = Any` | same set | 8-way: `WalkFwd/Bwd` + `Strafe L/R 45/90/135`, same for run, `SprintLoop`; crouch 8-way walk |
| starts | root-motion: `WalkFwdStart_RM`, turn-starts `Start90/180_L/R_RM`, directional `Bwd/StrafeL/R Start_RM`, `SprintStart_RM` | same | picked by `StartDirection` bucket |
| stops | root-motion, per direction, `_LU/_RU` foot | same | `WalkFwdStop`, `WalkBwdStop`, `StrafeLeft/RightStop`, `SprintStop`, crouch equivalents |
| stance | `Idle2Crouch`, `Crouch2Idle` | same | |
| jump | `Jump_Platformer_Start` -> `FallingLoop` (InAirLoop) -> `Jump_Platformer_Land` | same | one generic clip per phase, `aim = Any`, `bUseAirLoop = true` |
| turn in place | loops `TurnL/R_90Loop` | same loops | discrete RM turns exist (`AZ_Pistol_TurnL/R_91`, `_181`) but are unused |
| aim | upper-body lock on `AS_Pistol_Idle` + AO `AO_Pistol_Standing/Crouching` (9 additive poses) | | the legs never change when aim toggles |
| fire / reload | `ShootOnce`; `Reload_2` for all four slots | | |

**What makes it feel good:** the legs are one set. Toggling aim only swaps the torso (lock + AO); nothing in the
lower body switches clip, so no pops mid-walk. Starts and stops are root-motion with foot phase, so they plant.
The jump is three generic phases that work identically relaxed and aiming.

## What the rifle set has (919 clips, `Riffle_RTG_MH`, MetaHuman-native)

| pistol feature | rifle equivalent | gap |
|---|---|---|
| one 8-way walk/run set | TWO sets: relaxed `Walk_*/Jog_*/CrouchWalk_*` (8-way + backpedal) and aim `Walk_Aim_*/Jog_Aim_*/CrouchWalk_Aim_*` (8-way + backpedal); `Run_*` sprint carry (5 dirs) | more content, but aim toggle mid-walk switches leg clips (pop risk) |
| RM starts incl. turn-starts | `Stand_Relaxed_To_Walk_{dir}`, `Stand_Relaxed_To_Walk_R90/L90/R180_Fwd`, aim twins `Stand_Aim_To_Walk_Aim_*`; 41 transition rows per aim state | none |
| RM stops with foot phase | `Walk_*_to_Stand_Relaxed_LU/RU`, `Jog_*_to_Stand_*`, aim twins | none |
| stance transitions | `Stand_Relaxed_To_Crouch_v2`, `Crouch_To_Stand_Relaxed_v2`, aim twins | none |
| 3-phase jump | 195 clips: per direction and gait, `*_Jump_LU/RU_Start_IPC` / `_Air_IPC` / `_End_IPC`, aim AND relaxed | the live jump still uses SurvivalMan-sourced `PSD_P01_Land` / `AS_P01_Jump_*_Land` composites and generated `Fall_v2` clips; the MH-native `_Air_IPC` / `_End_IPC` phases are the same pattern as the pistol, per direction |
| aim additive | `AO_Rifle_Aim` (17 samples) | none |
| turn in place | loops only (`Stand/Crouch_Aim/Rlx_Turn_In_Place_L/R_Loop` + IPC) | no discrete 90/180 turns; borrow the pistol RM turns for the legs under the lock |
| reload / fire | 4 reload clips (relaxed/aim x stand/crouch), full fire set | richer than the pistol |

## Recommendation: give the rifle the pistol pattern

1. **One leg set.** Use the rifle AIM loops (`Walk_Aim_* / Jog_Aim_* / CrouchWalk_Aim_*`) as the only movement
   set, relaxed and aiming. The torso comes from the upper-body lock: aim idle when aiming (already built),
   relaxed idle when relaxed (the `RelaxedUpperBodyPose` path exists on the profile, unset today). Aim toggle
   then swaps torso only, exactly like the pistol. Keep the relaxed loops as an option, not the default.
2. **Starts / stops** already match (root motion, foot phase, turn-starts) - keep.
3. **Jump: same three phases, per direction.** Move the live jump onto the MH-native `_Start_IPC` ->
   `_Air_IPC` (loop) -> `_End_IPC` clips and retire the SurvivalMan composites and generated fall clips.
   Rows `aim = Any` like the pistol, torso from the lock.
4. **Turn in place:** rifle loops for the lower body only, aiming only (next-session start); pistol discrete
   RM turns as legs if the loops are too slow.
5. **Reload / fire** stay per-stance (richer than the pistol).

Speeds: the rifle aim loops depict walk 123 / jog 347 / crouch 93 cm/s (already applied as the rifle's gait
speeds); the pistol loops depict 159-206 walk / 340 run against 165 / 450. One leg set means one speed set.
