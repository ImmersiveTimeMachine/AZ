---
name: project_rifle_aim_upper_body_lock_2026-09-10
description: "★★ 2026-09-10 rifle aim UPPER-BODY LOCK authored in AZ_ABP_MoverHero_MHC: LayeredBoneBlend {spine_01, depth 4}, mesh-space rotation, weight = WIRED Get AimAlpha node (a scripted binding on BlendWeights_0 was silently rejected -> literal 1.0 -> always-on aim pose), blend pose = TwoWayBlend(Stand_Aim_Idle / Crouch_Aim_Idle_v2) with Alpha bound to NEW C++ float AimStanceAlpha (enum bindings are rejected too); inserted BEFORE AdditiveLeans. Fixes the fire-while-moving torso pop (strafe loops' chest 30-52 deg off the aim idle). Rejected-binding trap + corrected graph topology."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-10T03:31:18.435Z
---

# Rifle aim upper-body lock (2026-09-10)

Brief: `docs/design-briefs/rifle-aim-upper-body-lock.md`. Built node-only with `AZ_AnimGraphNodeUtils`
(add_anim_graph_node / set_anim_node_property / set_pin_binding / connect_pose_link); the user compiles + saves.

**The bug it fixes.** Fire plays as a dynamic montage on slot `RifleFire` (`AAZ_Weapon::PlaySlotAnimationAsDynamicMontage`,
profile `FireAnimationSlot`), masked at spine_02 in the MAIN AnimGraph (`LayeredBoneBlend_1`, weight literal 1). The
fire clip == aim idle + recoil (chest <=2.5 deg off), but the masked slot's BASE is the moving loop's torso, which is
30-52 deg off the aim idle on STRAFE loops (forward only 3-19). Every shot snapped the torso -> "another anim". Not the
`AZ_AM_Rifle_Fire` montage (slot `UpperBody`): that asset is off the live path.

**Rules learned (keep):**
- A LayeredBoneBlend whose blend pose is a static idle MUST use mesh-space rotation when the base's pelvis heading
  differs (strafe loops: 42-70 deg off); local-space would turn the torso with the hips.
- Hard `spine_02` depth-1 masks put the entire twist in one joint; ramp `{spine_01, BlendDepth 4}` (.25/.5/.75/1).
- Insert torso overrides BEFORE additive-lean layers, not after.
- **★ AnimGraph pin BINDINGS that the compiler silently rejects (the pin then uses its LITERAL — a weight literal of
  1.0 means "always on"):** (1) an enum property to a float/int pin — `PromoteByteToFloat` exists but a UENUM class
  is an `FEnumProperty`, not a byte → "Cannot copy property (EAZ_Stance -> float)"; (2) `SetPinBinding` on an
  array-element pin like `BlendWeights_0` binds the whole array → "Cannot copy property (float -> TArray)". Always
  grep the compile log for "Cannot copy property" after scripted bindings. Wire a `Get <Var>` K2 node into the pin
  instead (`AZ_BlueprintNodeUtils.add_variable_get_node(bp, "AnimGraph", var)` + `connect_nodes`) — the pattern the
  existing aim-offset layer uses. Enum → pose selection needs C++: an eased float (`AimStanceAlpha`) bound
  float→float. `BlendListByEnum` spawned via Epic MCP `create_node("Animation|Blends|BlendPoses(EAZ_Stance)")` comes
  with the enum bound but only the default pose pin; exposing entries is editor-C++ only.
- `set_anim_node_property` looks under `Node` by default; TwoWayBlend's struct is `BlendNode` -> pass `BlendNode.X`.
- Rider `ue_export_blueprint_nodes` returns "" for AnimGraph graphs. Epic MCP `ObjectTools.get_properties` reads `Node`
  on anim nodes (not the enum node's `BoundEnum`); pin bindings are readable on the node's
  `AnimGraphNodeBinding_Base_0` sub-object (`PropertyBindings`).

**★ TRAP — AnimBP compile errors that are NOT the graph:** after a header edit adding UPROPERTYs to the anim
instance, Live Coding (auto-fired at the edit minute: `Binaries/Win64/UnrealEditor-AZ.patch_N.exe` mtimes are
the evidence, the AZ.log shows nothing) hot-patched the class; the ABP then failed with "Use cached pose
'RifleFireBase' does not have an associated Save Cached Pose node" + "Failed to find index for a saved pose
node" although the graph was structurally perfect on both bridges. Editor-closed full build + relaunch → the same
saved graph loaded BS_UP_TO_DATE. Diagnose order next time: (1) `ls -la Binaries/Win64 | grep patch` mtimes vs
the first failing compile, (2) per-node `error_msg` via Python (readable; `error_type`/`enabled_state` are not),
(3) only then the graph. Also: editor-panel compiles do NOT log `[Compiler]` lines to AZ.log (only PIE/load
compiles do) — the per-node `error_msg` is the reliable readback.

**Editor-Python hygiene (2026-09-10):** `time.sleep()` inside a RiderLink Python script runs ON the game thread —
a 0.4 s sleep showed up as a `dt=0.4000` hitch in `[v2 Play]`. Verify a save by re-reading the file mtime, never
by sleeping. `EditorAssetLibrary.save_asset` on a DataAsset can raise a benign non-fatal ensure
(`IsInGameThread()` in `AppTime.cpp:38`, thumbnail render on a task thread) — not a failure, the save lands.
Transient/Replicated-only UPROPERTYs (no Edit/Blueprint specifier) are invisible to Python `get_editor_property`;
prove their presence via the DLL's reflection strings (`FName` literals are ASCII in the binary).

**AIM TURN-IN-PLACE (2026-09-11).** ADS body tracks the camera continuously; a 60 deg free-look cone was tried
and REJECTED by the user as lag (and is not AAA ADS behaviour — that's hipfire). The real fixes were: aiming no
longer inherits the strafe facing ramp (flat `AimFacingTime` 0.07), aiming never buckets a move-start into a
90/135/180 RM turn clip (`FAZ_LocoSMInputs::bIsAiming` -> forced Fwd; the bucket quantised e.g. 60 deg up to a
90 deg clip whose RM over-rotated then unwound = "rotates too much and goes back"), and at REST the turn is now
ANIMATED: SM `IdleTurnLeft/Right` (enter 35 deg, exit 6) + chooser rows 304-307 + walking-mode pacing
`FacingSmoothingTime = |delta| / AimTurnInPlaceRateDegPerSec` (67 = the clip's 45 deg / 0.67 s). The four aim TIP
clips were set `loop=True` (turn length is the ANGLE, not the clip). Reused the VESTIGIAL IdleTurn enum values +
dead chooser rows 77/78 (2026-06 attempt removed because it used ROOT MOTION, which snaps back under Mover
re-sim) — this version is spring-driven + cosmetic clip, the strafe move-start split. ★ Thresholds are duplicated
in the SM (constants) and the mode (UPROPERTYs) and MUST agree. ★ FIRST CUT BUG (2026-09-11 evening): the SM entry
gate was placed inside `ComputeNextState`'s MOVING branch (`if (In.bIsMoving) { switch(Previous) ... }`), so its
`!bIsMoving` test could never pass — the mode paced the turn (peak 66 deg/s, verified) but the SM never left
IdleLoop (zero SM=10/11), so no clip played and the feet slid. `ComputeNextState` is TWO switches: moving
(inside the if) and not-moving (after it) — at-rest states go in the second. Also: a new case inserted between
a `[[fallthrough]]` and `default:` hijacks that fallthrough; insert ABOVE the falling-through case.

**★★ THE RIFLE'S ACTUAL PROBLEM WAS GROUND SPEED, NOT FACING (2026-09-11).** User's discriminator: "pistol is
perfect, rifle no" — same code, so it was data. Each clip's own `speed` curve vs the live gaits: pistol walk
loops depict 159-206 against a 165 gait (matches, looks right at play rate 1); RIFLE walk depicts median 123
vs 165 (+34%), jog 347 vs 450 (+30%), crouch walk 93 vs 200 (+115%). A permanent 30-47% skate in all 8
directions, aiming or not — and it was masking judgement on every facing/camera change made earlier that day.
Raising play rate to close it (rifle's LoopPlayRateMin/Max are pinned 1.0/1.0; pistol 0.5/2.5) would need
1.3-2.2x = visible fast-motion; USER RULE: **play rate stays 1, bring the CAPSULE speed down to the animation**.
Fix: per-weapon `WalkSpeedOverride`/`Run`/`Sprint`/`CrouchSpeedOverride` on the profile (0 = no opinion), applied
by `AAZ_PawnMoverHeroCharacter::UpdateWeaponGaitSpeeds` on equipment CHANGE (game thread, baseline captured once
so unequip restores designer tuning; logs `[WeaponSpeed]`). Rifle: walk 120 / run 345 / crouch 90.
**★ DIAGNOSTIC LESSON: measure the clip's depicted speed against the gait BEFORE tuning any facing/camera
feel — a constant skate makes every other change unjudgeable.** Also: read the LIVE mode instance
(`MoverComponent.movement_modes["Walking"]`), not the CDO — the user's tuning lives on the instance and the CDO
misled me for several rounds (enter=180/rate=1 there had silently disabled the turn-in-place pacing).

**AIM TIP, second PIE (2026-09-11 17:45) — two findings.** (1) The chooser rows used the NON-IPC turn loops
(`..._Turn_In_Place_L_Loop`, pelvis yaw swing -45/+45 baked in, rm=False): with the capsule ALSO rotating via the
spring the body turned twice as fast and POPPED back 45 deg at every 0.67 s loop seam — the user's "what happens
during the turn". Rule: a spring/capsule-driven TIP must use the **_IPC** twins (swing 0, verified all four);
the capsule owns rotation, the clip owns only the feet. (2) 67 deg/s (the clip's authored rate) is far too slow
for ADS — a 150 deg aim-entry turn took 2.2 s and the user released aim before it finished every time. Fix:
decouple speed from foot slide — `GetWeaponLoopPlayRate` plays the TIP clips at |body yaw rate| /
`Profile->AimTurnInPlaceClipRateDegPerSec` (67) in `IdleTurnLeft/Right` (BodyYawRateDegPerSec sampled on the
game thread, smoothed 20/s), so `AimTurnInPlaceRateDegPerSec` is a pure feel knob (pawn BP now 180). Verify
in the log: `[v2 Play] ... rate=2.x` on the TIP clip. Other agent added pistol TIP rows 397-400 AFTER mine
(304-307); if 304-307 inherited "any" tag cells from rows 77/78 the pistol may pick RIFLE turn clips — unverified.

**AIM TIP FINAL (2026-09-11 evening, verified):** decision state in the PAWN's ProduceInput, transported as
`FAZ_MoverCustomInputs::AimTurnYawRateLimit`; the mode applies a yaw-rate CLAMP after `Super::GenerateWalkMove`
(fast flat spring + clamp, not T=angle/rate pacing); an aim turn FINISHES after release (aim branch runs while
`bAiming || bAimTurningInPlace`, anim side ORs `IsAimTurningInPlace()` into bIsAiming); TIP clips = the _IPC
loops at play rate |body yaw rate|/67; 0.2 s idle dwell before entry. Measured: 0.62-0.79 s turns ending while
aiming at -0.4 deg, body at the 240 clamp, clip 3.6x. Full table in the brief. Lesson: the "sway at the end"
was (a) T=angle/rate ramping slowly so the user released aim mid-turn and (b) the explore idle hold receiving
170 deg/s of residual spring velocity on release -> overshoot + swing back. Not rollback (PIE was Standalone).

**AIM TIP VERDICT (2026-09-11 22:20): OFF BY DEFAULT.** After the whole chain worked on paper (0.7 s turns,
no overshoot), the user identified the visible defect as the STEPPING LOOP itself: hips rocking during the fast
turn (rifle loop 3 cm lateral sway/step, PISTOL loop 16 cm) - no loop clip authored at 67-90 deg/s reads well
under a 240 deg/s body, and the user's rule is play rate 1x (another agent pinned GetWeaponLoopPlayRate to 1.0
at 22:13). The user's reference for "right" = the PISTOL before any TIP: body tracks the camera with the flat
0.05 s spring, aim idle, feet slide, no stepping. So: `UAZ_PawnMovementMode_Walking::bAimTurnInPlaceEnabled`
(default false) gates the pawn latch AND the SM entry (thresholds + switch now travel in FAZ_LocoSMInputs from
the walking mode, no more duplicated constants). Everything that made the rifle feel right stays: flat
AimFacingTime, no move-start bucketing while aiming, per-weapon gait speeds, IPC rows, release tail. Re-enable
only with purpose-made fast aim-turn clips (90 deg in ~0.35 s with the rotation baked in).

**WHAT THE USER ACTUALLY SAW (2026-09-11 22:52, from their NVIDIA recording, read as frame sheets via
imageio-ffmpeg + PIL in the scratchpad):** on AIM ENTRY with the character facing the camera (171 deg off):
(1) the body spins 137 deg in the FIRST tick (2328 deg/s, the 0.05 s spring) = a one-frame motion-blur smear,
and (2) the aim camera dolly 220->30 cm at InterpSpeed 8 (~0.6 s) passes THROUGH the hood/head/rifle and the
character is out of frame for ~0.3 s before the sight rises into the ADS framing. Neither is the turn-in-place.
Fix (1): `UAZ_PawnMovementMode_Walking::AimMaxYawRateDegPerSec` (540, 0 = off) sent as the InputCmd yaw-rate
limit whenever aiming and not in a TIP -> 180 deg in ~0.35 s, a turn the eye can follow. Fix (2): raise
`CameraAiming.InterpSpeed` on the pawn from 8 to ~18 (user's field). ★ LESSON: when the user says "check the
log" and the log says fine, ASK FOR / READ THE RECORDING - two of the four visible defects today (loop-clip hip
rock, camera dolly-through) were invisible to every telemetry line I had.

**THE UPPER-BODY SWAY, finally (2026-09-11 23:25, user: "only the upper body, lower body position is ok"):**
the AIM OFFSET. `TargetAimYaw = Clamp(RotationOffset, +-MaxAimYaw=90)`: on an aim entry 162 deg off the torso
snaps to the +90 clamp, then unwinds to 0 during the last 0.17 s of the body turn while the legs are already done
-> upper body swings, lower body fine. With the old slow paced turns the twist hung for a second, with the 0.05 s
pistol snap it was over in 0.15 s (why the pistol "worked"). Fix: range fade on the AO yaw target
(`AimOffsetYawFadeStartDeg` 30 / `AimOffsetYawFadeEndDeg` 60 on the profile): the AO only ever carries the small
camera-body residual; a large residual means the body is turning and the torso stays neutral. Rule for any
aim-offset: NEVER let it represent an angle the body is about to turn through.

**FINAL AIM-ENTRY SHAPE (2026-09-11 23:32, measured):** `AimMaxYawRateDegPerSec` 540 (body: 152 deg in
0.3 s, flat 540 from tick one, soft tail, no overshoot) + `CameraAiming.InterpSpeed` 18 (boom 220->36 in
0.14 s, no dolly-through) + rifle profile `AimBlendInSpeed` 20 (weapon up with the camera) + AO yaw range fade
30..60 (torso stays square while the body turns). Turn-in-place OFF (`bAimTurnInPlaceEnabled`). Pistol shares
every piece. Left for the user's eye: the torso during the turn (not logged).

**Corrected graph facts** (earlier notes were wrong): `PoseSearchHistoryCollector_0` is near the END (after
OffsetRootBone_1), not after BlendStack; ModifyBone_0/_1 are live no-ops and `LinkedAnimLayer_1` (AdiativeCombat) is
the orphan; no TwoBoneIK in the main graph; `OffsetRootBone_1` is live with Accumulate rotation (contradicts
[[feedback_offsetrootbone_mover_graph]], unresolved). Crouch fire pop (~25 deg, standing fire clips on crouched legs) is fixed
THE RELOAD WAY, not with additive clips: per-stance clip fields on the profile (`CrouchingSingleFireAnimation`,
`CrouchingAutomaticFireAnimation`) selected in `AAZ_Weapon::Multicast_BeginFirearmAnimation` from
`Mover->IsCrouching()`, fallback standing (awaiting editor-closed build + DA assignment of
`Crouch_Fire_Single_IPC` / `Crouch_Fire_Continuous`). **★ USER RULE reinforced (2026-09-10, user objected):
before proposing ANY mechanism, grep the project for an existing implementation of the same need — reload
already had the per-stance selection pattern and I proposed an engine trick instead.**

See [[project_hero_camera_aim_cone_2026-09-09]] (per-key camera offsets become mostly redundant),
[[project_rifle_mh_native_migration_2026-09-09]].
