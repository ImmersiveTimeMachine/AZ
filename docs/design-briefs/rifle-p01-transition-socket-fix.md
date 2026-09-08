# P01 transitions and socket alignment — 2026-09-07

User observations: movement transitions selected no-weapon clips; rifle grip/socket alignment was visibly off. User explicitly requires separate aim and relaxed rifle sockets and treats left-hand IK as a last priority. No IK has been enabled.

## Library correction

The original Riffle_P01 folder contains119 clips but is not the whole already-retargeted Rifle_01 library. A fresh registry import-provenance search found474 compatible Rifle_01/W2 assets, including163 transitions. Existing /Game/AZ/NoWeapons/RootMotions/rm_W2_* assets are genuine Rifle_01_PRO_v27A imports on SKEL_SurvivalMan despite the NoWeapons folder name. Do not retarget or discard them based on that name.

All108 ordinary ground start/stop candidates plus4 stance transitions already exist. Raw samples confirmed actual root travel/yaw; ground clips had EnableRootMotion=false. No new animations or retargets were created.

## Applied transition changes

Tools/rifle_p01_transition_setup.py backed up the112 assets and the master chooser. It enabled RM on108 ground clips, kept4 stance clips cosmetic, and appended112 rifle selections plus9 unchanged Sprint fallback copies to the existing main CHT:276 total rows. Original155 results/cells and enabled states were preserved except the reviewed negative-rifle gates on45 replaced source rows. All filters now precede Randomize.

Role-equivalent starts preserve their original Any/False moving-transition filters. Dedicated moving Run180 pivots and explicit Sprint behavior remain shared. This patch does not claim full rifle pivot/Sprint coverage. Already-retargeted rm_W2_Run_* assets also exist; they were discovered but not integrated in this bounded correction.

All48 stop variants confirmed the native foot convention: RU begins with the left foot lower/slower, LU with the right foot lower/slower. No suffix assumption was used without sampled evidence. Transition RateScale stays1; loop play-rate logic does not alter transition clocks.

Receipts: Saved/RifleAnimationContent/p01-compatible-library.json, p01-transition-audit.json, p01-transition-prepare.json and p01-transition-readback.json. Final readback: already_prepared,276rows,no dirty content packages. The older155-row consolidation audit is superseded by the transition audit for subsequent whole-table validation.

## Applied socket changes

EquipmentComponent now reconciles the committed active weapon's attachment on Ability.State.Aiming changes: AimSocketName while aiming and RelaxedSocketName on release/cancel. Existing carry/fists/drop paths remain authoritative; stale aim notifications cannot redraw carried/detached weapons. The code uses immediate authored socket attachment, not an added tick/interpolation system. No new reflected fields/functions were added. The source patch was present in the built DLL before the current editor session; subsequent build reported up to date.

Tools/rifle_p01_socket_alignment.py sampled actual MHC-body P01 standing/crouched idle and forward movement poses (20samples per mode). It fitted the two existing mesh-owned sockets independently, preserving parent bones (relaxed=hand_r, aim=middle_01_r), unit scale, and the exact current right-grip anchor. Minimal rotation aimed the existing weapon left-grip markers toward hand_l; no weapon resizing or IK compensation was used.

Applied asset: /Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh. Relaxed sampled wrist/grip RMS error15.115→2.246cm, maximum2.914cm; aimed4.902→1.886cm, maximum2.420cm. Right-grip anchor drift is numerical zero. These are pose-space measurements, not runtime visual acceptance; remaining grip-span differences and finger placement need the user's review.

Receipts: Saved/RifleAnimationContent/p01-socket-assets-before.json, p01-socket-alignment-samples.json, p01-socket-fit-audit.json and p01-socket-fit-prepare.json. Socket package and all other sockets were backed up/preserved by the authoring script. Review fingerprint318dfaf8252d9d07d836d3d7be2e77035412c042d2cc937f09177339a20f353e was checked before application.

## Log finding and correction

User said to check logs after a stop-PIE request. Logs showed PIE had ended at20:38:04 UTC, while Rider ue_play still returned Play. Native UnrealEditorSubsystem.get_game_world() correctly returnedNone. Prefer native world existence + lifecycle log lines over that stale Rider status. Later v2Play messages continued after PIE ended and are not evidence of a gameplay stall; do not misdiagnose them as the active run.

The genuine repeating warning was COLLISION PROFILE [Custom] is not found. UAZ_MovementDirectionCapabilityComponent::SweepForWall passed the capsule's Custom profile name to SweepSingleByProfile. It now uses the capsule's actual object channel and collision response container via SweepSingleByChannel, preserving per-instance custom responses. The explicit fallback TraceChannel path is unchanged. Do not create a global Custom profile or overwrite capsule responses to suppress the warning.

Live Coding for that query fix: UBT Result:Succeeded; patch successful at20:46:22 UTC. No source layout/reflection change. A normal full build is needed before a later editor restart because Live Coding patches do not survive it. No temporary generated C++ files were introduced in this correction.

## Moving run180 fallback prepared for review

The user's subsequent run exposed the deliberately shared moving Run180 pivots: `AnimPro_RunFwdTurn180_L_RU` and `_L_LU` were selected at20:57:25/26 UTC, while adjacent ordinary jogging stops correctly selected `rm_W2_Jog_F_to_Stand_Relaxed_RU/LU`. The current ordinary Jog start/stop rows and all36 associated clips passed live readback (RM enabled, Loop false, RateScale1).

No authored P01 footed moving180 pivot family was found. The accepted bounded fallback uses existing `rm_W2_Stand_Relaxed_To_Jog_L180_Fwd/R180_Fwd` and aimed counterparts, explicitly as turn-starts: StartTime0 retains their full measured180-degree root yaw; FootAny avoids inventing LU/RU variants. Relaxed left/right lengths are1.5333/1.2000s and aimed left/right1.6333/1.1333s. They travel only1.7-4.1cm during the first150ms, compared with44-54cm for the existing footed moving pivots, so this introduces deliberate slower stop-and-turn anticipation rather than equivalent momentum-preserving motion. No special blend treatment hides that difference.

`Tools/rifle_p01_pivot_setup.py` provides the read-only audit and separately authorized prepare pass. It appends four rifle-only moving180 rows to the current master, preserving a coordinated jump pass if already appended, and gates original rows24-27 only for Rifle. Explicit GaitRun and a `NOT Movement.Sprinting` filter keep the user's shared sprint/back-carry policy. It refuses writes while a game world exists, backs up the master, preserves current row results/cells, and does not modify any animation sequence. Source/audit preparation is complete; actual application is recorded by its `p01-pivot-prepare.json` receipt when run by the root.

## Next acceptance

After the pivot pass is applied, the user should run fresh PIE: rifle normal/aimed walk/jog starts and stops, moving180 reversals, crouch/stand, relaxed/aim sockets, and return to fists/holster. Expect rifle transition picks named rm_W2_*, including the deliberately slower turn-start fallback, socket logs selecting RightHandRifleSocketAim/Relaxed, and no recurring Custom-profile warning. Sprint remains shared with the rifle carried on the back. Inspect new logs after the user runs; Codex has not started PIE or editor tests and no automated tests were added.

Changes remain uncommitted. Preserve unrelated work. Updated source and assets are already applied; do not repeat preparation blindly.
