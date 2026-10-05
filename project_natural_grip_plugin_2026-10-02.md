---
name: project-natural-grip-plugin-2026-10-02
description: "AZNaturalGrip C++ editor plugin (Tools > AZ Natural Grip): bit-exact port of the Python natgrip solver, part B polish, asset readers; how to build/test it while the editor is open"
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-03T22:21:18.280Z
---

**2026-10-02: the Python grasp solver (Tools/wgs/natgrip) is ported to C++ as `Plugins/AZNaturalGrip`** (user: "сделай
на C++ с интеграцией в Unreal"; speed numbers explicitly NOT wanted - "меня не интересует замер скорости").
Plan + status: docs/design-briefs/natural-grip-cpp-plugin.md.
- Core = engine-agnostic C++ in `Source/AZNaturalGrip/Private/Core` (NGMath/NGHand/NGField/NGSetup/NGGeometry/
  NGSupportHand/NGTriggerHand/NGOptim/NGJsonDiff). Parity with Python is EXACT (max diff 0) for both M16 hands, place_nat,
  geometry/fields. Traps found: CPython>=3.12 sum() of floats is Neumaier-compensated (ng::PyFloatSum, used in QNorm);
  `x ** 2` = CRT pow; Python `%` is floored; min/max keep the first of equals.
- Harness: `Plugins/AZNaturalGrip/Tools/ngtest/build.bat all` (run from PowerShell, cmd from Git Bash can hang) ->
  `Intermediate/all/ngtest.exe prims|support|trigger|geometry|fast|jsondiff`. Run it after ANY core change.
- Part B: pure local search from a coarse grid LOSES to the exhaustive grid on these hinge/reject scores; shipped B =
  grid best + pattern-search polish (per finger never worse); "Solve weapon hand" keeps the better of A/B. Eigen LM not
  used (scores are not least squares).
- Editor module: panel + UAZNaturalGripProfile data assets (plugin content /AZNaturalGrip/Profiles, script
  Tools/create_m16_profiles.py) + `unreal.AZNaturalGripLibrary.run_stage(profile, stage, threads)`; Input=Assets reads
  the hero hand / weapon mesh like the dump scripts (MeshDescription->DynamicMesh); stage `inputs` checks vs dumps.
- ★ Compile-check plugin .cpp files WITHOUT closing the editor: `Build.bat AZEditor Win64 Development -Project=...
  -SingleFile=<abs .cpp>` (UBT LiveCodingPassThrough, output to temp). Full DLL builds still need the editor closed.
- In-editor test PASSED 2026-10-03: profiles /AZNaturalGrip/Profiles/NGP_M16_Left|Right; parity exact for both hands with
  Input = Python files and Input = Assets; `inputs` = hand/mesh/fields identical to the dumps.
- S7 Apply DONE 2026-10-03 (NGApply.cpp; panel "Apply: dry run" / "Apply..." with confirm; Python
  AZNaturalGripLibrary.apply(profile, dry_run)): backup to AZ_Backups/<stamp>_<profile>/ + manifest, write, save, read-back
  check; refuses during PIE or when a target package is already dirty. ★ M16 LeftHandGrip lives on the SKELETON asset
  M16_Skeleton_Skeleton (not the mesh) - Apply saves the socket's owning package. Tested on copies in
  /Game/AZ/Tests/NaturalGripApply (Tools/apply_test_setup.py); real M16 profiles dry-run = 0 change (current assets ==
  L2/N2). Real M16 writes only with the user's go (protected).
- POWER grasp CODED 2026-10-03 (NGSupportHand: SupportSettings::SetPower, SupportPowerStage1 / SolveSupportFingersPower /
  SolveSupportPower; harness `ngtest power 54` -> Intermediate/power/power_final.json; parity untouched). Stage 2 must take
  the best per roll angle (stage 1 ignores fingers). Result P1: p [0 20 15 1 -1 0] theta -15, palm 0.01 seated, proximal
  gaps 0.01-0.28 (L2 0.11-0.56), thumb MCP 60/IP 66 opposite the finger pads 158 deg, twist -13; wrap still 104-149 (the
  fat M16 handguard). APPLIED to the real M16 2026-10-03 with the user's go (backup AZ_Backups/2026-10-03_150822_NGP_M16_Left
  = skeleton + AS_Grip_M16 + AZ_BP_Rifle before; previous result kept as Saved/NaturalGrip/NGP_M16_Left/
  support_final_L2_before_power.json). User PIE: thumb curled and standing on its TIP (same hook lesson as the index) ->
  power-thumb rule fixed (both phalanges resting G2<=0.12, pad palm side = IP flexion motion direction must FACE the
  handguard axis, MCP/IP > 30 penalised) -> thumb straight along the near side (cmc -20/14, mcp/ip ~1), oppose 128;
  re-applied (backup AZ_Backups/2026-10-03_151030_NGP_M16_Left). User PIE #2: thumb straight but LOW beside the palm =
  still "support from below". ★ Root cause: "opposition" measured on the SHORTEST arc (through the bottom). Fix =
  ENCLOSURE (force closure): arc fingers -> palm -> thumb >= 220-260 deg, + thumb ALONG the handguard pointing forward
  (+Y dot >= 0.75; without it the thumb stood up across the top). Result P4: thumb along the upper near side, enclosure
  242, pad gap 0.01; applied (backup AZ_Backups/2026-10-03_152158_NGP_M16_Left). ✔ USER PIE #3: "Сейчас, я думаю,
  идеально" - M16 left hand DONE.
- ★ LESSON (support/any grasp): a firm grip = FORCE CLOSURE - measure the arc the hand covers around the held part
  (fingers -> palm -> thumb), never the shortest-arc angle between thumb and fingers; thumb pad presses with its palm
  side, lies along the part pointing forward, no curled joints. Contact-only rules pick claws/hooks (index, thumb).
- 2026-10-03 later (built, NOT yet tested in the editor): ROLE system (profile "Role" = EAZGripHand: Trigger, Support,
  TriggerStraight, Handle (knife/melee, side), PistolCup (left over the right hand: right solve -> capsule obstacles in
  the field, grip axis markers, thumb along WeaponForward)); weapon types (Rifle, Shotgun, Pistol, Knife, TwoHandMelee)
  + panel "Create profiles" (weapon = Content Browser selection, template = picked profile); power stage in the panel
  (bPowerGrasp); hold sampled from clips (ClipFolder + WeaponSocketOnHero, medoid aim/relaxed + elbows); marker
  sockets NG_Trigger / NG_GripFront / NG_ThumbLimit / NG_Handguard / NG_HandleBack / NG_HandleFront; auto fine box;
  general axis in SupportSettings (SetAxis); place_nat grid centred on the profile's SearchCentre (M16 shift 0).
  Pistol facts: hero sockets RightHandPistolSocketAim/Relaxed, clips /Game/AZ/Assets/Pistol, mesh
  /Game/MilitaryWeapDark/Weapons/Pistols_B, AZ_BP_Pistol has NO GripPose yet. User wants step-by-step instructions per
  weapon to do it himself (M16 then pistol) AFTER I verify in the editor. Also to investigate: the left hand takes a
  fraction of a second to settle on the handguard when the rifle is taken (WeaponGripBlendSpeed 6/s + AZ_Grip_L ramps
  from 16 cm in the draw clips = IK pulls from far).
- ★ 2026-10-03 "left hand searches for its place" on the rifle draw - ROOT CAUSE was the GATE, not the curve: the draw
  plays as a RifleFire-slot montage while Selection.Weapon is still the OLD selection (CommitSelection runs at
  FinishSwitch = end of the draw clip), so the grip IK was OFF for the whole draw and eased in (6/s) after it with the
  hand at rest 8.5 cm / 34 deg off the solved grasp. Fix (AZ_MoverAnimInstance.cpp grip gather, .cpp only, Live Coding
  OK): while bDrawPresentation, grip the weapon physically on its Relaxed/Aim socket (pawn's attached actors). Second
  part: Tools/wgs/set_left_curve_m16.py APPROACH mode - rising edge also follows the distance to the IK target (0 at 30
  cm, 1 at 12 cm, capped at the run's peak, holster clips skipped, writes only clips whose curve == old rule). DRY: 28
  M16 clips (draws, swaps, reload ends, unjams, Fgt); stand draw full at 1.045 s (was 1.185), crouch 1.22 (was 1.56).
- ★ 2026-10-03 HOLSTER: left arm folded behind the back - the holster clip's AZ_Grip_L is 0 from 0.14 s, but the grip
  node's POSE curve stays 1: upper-body layers fade exponentially at the switch start and LayeredBoneBlend's default
  curve blend (Override) lets ANY weight > 0 replace the clip curve with their 1. Fix (AZ_MoverAnimInstance.cpp, LC):
  while Equipment->IsSwitchingWeapon(), WeaponGripAlpha = in-hands x ((1-w) + w*clip AZ_Grip_L at the montage position)
  read from the switch clip itself, no 6/s lag. Same trap may hit reloads (not reported) - check in PIE.
- ★ 2026-10-03 SWITCH REACH (user design: "the left hand goes toward where the gun will lie, the brain predicts it"),
  full build done (editor closed): UAZ_MoverAnimInstance::BuildWeaponReachPlan (per switch clip + weapon class, from
  the clip's bones via UAnimSequence::GetBoneTransform, body frame spine_04): DRAW = quintic from the clip hand at the
  profile's AttachTime (its own velocity, capped) to the weapon's LeftHandGrip where the gun stops (catch speed < 20
  cm/s after the swing), meet/hand-over to the live grip in the last 0.12 s, fingers close then; HOLSTER = from the
  handguard to the clip's end rest pose (0.45-0.8 s), release 0.12 s, clip takes back over 0.15 s after its hand
  rests. AZ Weapon Grip reads FAZ_WeaponSwitchReach (UpdateInternal) and ignores the pose AZ_Grip_L while it owns the
  hand. CVar az.Weapon.Reach 0/1/2 (2 = per-frame log); [Reach] log line per built plan. Measured before: stand draw
  hand went straight but to a point 8-10 cm / 34 deg off the grip; crouch draw wandered x4; holster detour x2-2.8.
- ★ DOCS (user asked: description and problem SEPARATE): docs/design-briefs/az-natural-grip-tool-reference.md (the tool)
  + docs/design-briefs/left-hand-weapon-switch-problem.md (the open problem, requirements R1-R6, next steps). Keep both
  current. First PIE of the reach (2026-10-03): user screenshot = crouched, LEFT HAND THROUGH THE LEG; [Reach] plans show
  Arrive detected far too late (stand draw 1.483, crouch draw 1.767 - chest-frame grip-point speed never settles; the
  gun is in front ~1.0 / ~1.25 s) and the crouch holster HOLDS the chest-frame end pose 0.8-1.533 s. Leg causes to
  separate (Reach 0 vs 1): (a) upper-body montage over the LIVE crouch legs, (b) chest-frame reach ignores legs.
  Proposed: Arrive from the clip hand / gun vs hand bone, holster handback at Arrive, left-arm body clearance vs live
  leg/torso capsules in the grip node.
- 2026-10-03 late: another coordinator (Codex) wrote docs/design-briefs/left-hand-weapon-switch-handoff.md (plan H1-H5;
  leg task OWNS AZ_MoverAnimInstance.h/.cpp/_Procedural.cpp - active edits 21:45, FVisualMotion*). I delivered
  left-hand-weapon-switch-h1-h2-checkpoint.md (no code changed; baseline backup AZ_Backups/2026-10-03_214910_hand_switch_baseline).
  H1: equipment snapshot (PhaseId-validated coordinator montage via new AAZ_Weapon getter), master WeaponGripAlpha never
  modulated by the left hand (fix C currently scales right hand + body clearance - bug), tau ownership mix of normal/switch
  left goals. H2 (measured 30 Hz, AnimPose via Rider ue_execute_python - unrealclaude MCP was down): solved grip 8.1 cm /
  33 deg from authored hold; draw contact = authored hand within 8 cm of its terminal hold in az_weapon_r frame: stand
  0.987, crouch 1.207 (old plan 1.483/1.733); holster clip hand NEVER follows the gun (release 0.11/0.06 s) -> holster =
  release over 0.12 s onto the clip hand, NO path/hold; crouch holster clip's own AZ_Grip_L = 1 from 1.1 s (content defect,
  protected). Offline leg check ~ -1.4 cm even for the raw clip -> H3 runtime clearance vs live legs still required.
  NEXT: user review -> H3 design -> H4 after the leg task's compiling checkpoint (header changes -> editor closed build).
- 2026-10-04 FULL-BODY CROUCH SWITCH WORKS (user OK): node AnimNode_AZWeaponSwitchFullBody (+AZEditor AnimGraphNode)
  inserted in AZ_ABP_MoverHero_MHC after the RifleFire LayeredBoneBlend (73E21D1A) -> before Save 'AdiativePoses'; weight
  FAZ_WeaponSwitchReach::FullBodyAlpha (crouched + still, 0.15 s, CVar az.Weapon.SwitchFullBody). NEXT SESSION (user):
  legs swap after a crouched holster/draw - M16 W2 switch clips (not RifleMega) end/start in the UE4 demo "MOB1" crouch (= DMO_MOB1_Crouch_Idle_V2_IPC, measured 2026-10-04) (RIGHT knee up) while rifle
  crouch idle W2_Crouch_Idle_IPC and unarmed AnimPro_CrouchLoop_new are LEFT knee up; at commit chooser blends
  W2_Crouch_Idle_IPC -> AnimPro_CrouchLoop_new 0.5 s. Leads: retarget DMO_MOB1_Crouch_Idle_V2_IPC (Manny) as the unarmed
  crouch idle, or prime the target base under the holster (like the draw presentation). Doc checkpoint s.5.8.
- 2026-10-04 recordings (Saved/NaturalGrip/SwitchRecordings, 10 files): jerks GONE (user), crouch leg still through.
  Stand clean (>=7 cm). Crouch draw -12.7 cm / holster -11.1 cm (forearm in thigh_l/calf_l) while the clip arm over its
  OWN legs is clean (+1.2..1.6 cm). Cause: crouch switch clips are FULL-BODY (pelvis 40->56 cm draw, ->32 holster, torso
  lean); live keeps crouch-idle pelvis 40 cm + legs, chest folds over the left knee. Arm-only bake infeasible 0.80-0.96 s
  (needs 12-18 cm wrist detour). Recommended (awaiting user): crouched+stationary switch -> legs/pelvis from the switch
  clip (SwitchLowerBodyAlpha + graph blend of the RifleFire slot full pose). Solver prototype Tools/wgs/switcharm/.
  TD-006 added (visible stance-transition blends; deferred by user).
- 2026-10-04 00:22 REDESIGN BUILT (H4 rejected in PIE: hand jerked - leg push alternated hand/elbow 1-10 cm per frame,
  never cleared; reach path 45 cm in 0.43 s ~2x clip speed; clip's OWN arm (IK 0) 9.6 cm inside live calf_l in crouch
  holster = layering, not IK). USER RULE: corrections are BAKED INTO THE ANIMATION offline (one corrected clip per
  situation), never solved per frame at runtime. Runtime now: clip moves the arm; left IK = master x [(1-W)+W x clip's own
  AZ_Grip_L] (exact montage), holster release latched; path/plan/leg-push REMOVED. Recorder az.Weapon.RecordSwitch 1 ->
  Saved/NaturalGrip/SwitchRecordings/*.json (grip node INPUT pose, 36 bones CS). NEXT: user ABP Ctrl+F7 + PIE + record
  crouch/stand draw+holster -> offline switch-arm solver in AZNaturalGrip (elbow swivel + small hand offset, whole-trajectory
  smooth, >=1 cm from legs) -> review images -> NEW clip copies + DA_WeaponAnim_P01 repoint (user go). Doc section 5.
- 2026-10-03 22:22 H4 BUILT (full build OK, 0 errors): equipment TryGetSwitchPresentation + AAZ_Weapon::
  GetEquipmentAnimationMontage(PhaseId); reach code moved to NEW AZ_MoverAnimInstance_WeaponReach.cpp (CVar az.Weapon.Reach
  0/1/2/3 lives there); master WeaponGripAlpha never modulated by the switch; FAZ_WeaponSwitchReach.Ownership (tau) replaces
  bOwnsLeftHand, node mixes normal/switch goals; draw Arrive = contact 8 cm (WeaponReachContactRadius); holster = release
  onto the clip hand (no path); plans rebuilt max once per phase (PhaseId guard). Bounded H3 in AnimNode_AZWeaponGrip:
  left hand+forearm vs PA bodies thigh_/calf_/foot_ (live pose, radius x0.8), only while tau>0. User must Ctrl+F7 the ABP,
  then PIE (H5). Leg task's code compiled in the same build.
- 2026-10-03 panel safety (LC): Create profiles for ANOTHER weapon clears WeaponBlueprint / GripPoseOverride /
  ClipFolder / WeaponSocketOnHero / OtherHand copied from the template; a profile with ClipFolder set REFUSES to solve
  when its role's marker sockets are missing (else the M16 numbers silently solved it). The 4 profiles today = NGP_M16_
  Left/Right (real) + 2 *_ApplyTest copies in /Game/AZ/Tests/NaturalGripApply (deletable).
- (old) Open: the panel has no "power" stage yet (power runs via `ngtest power`; wiring it into NGJobs needs a module build);
  test copies in /Game/AZ/Tests/NaturalGripApply can be deleted; nothing committed. Compare image Tools/wgs/natgrip/cmp_left_m16_power.png; was: awaiting go to Apply
  (copy power_final.json -> Saved/NaturalGrip/NGP_M16_Left/support_final.json, Apply with backup, user PIE). Panel has
  no "power" stage yet (NGJobs; needs a module build).
- (old) design notes: M16 left POWER grasp in C++. Runtime checked: no mismatch (node IKs hand_l
  onto the LeftHandGrip socket, which == L2; bakes GripPose fingers as-is) -> the cause is the objective. Design:
  SupportSettings gets power params (rest weights {2,1.5,1} = proximals seated, wrap cap ~230 instead of 160, power
  thumb = pad resting + OPPOSITION: around-axis angle vs mean finger-pad angle >= ~140, wider abd/fl grid); defaults
  keep Python parity (re-run ngtest support all). New NGSupportPower.cpp: placement = corr + rotation theta about the
  handguard axis (-60..60), palm seated (palm_worst ~0.1), bend limits, forearm TWIST limit (swing-twist about
  elbow->wrist, ~45 deg); stage1 -> stage2 (power scoring) -> final + polish -> render with Tools/wgs/natgrip/
  left_view3d.py (lsolve JSON schema) for the user BEFORE Apply. Scoring lines: NGSupportHand.cpp EvalFingerCfg (Terms,
  WrCap) and EvalThumbCfg.
- OLD NEXT: S7 asset writers (socket/grip pose/BP flags/curves, backups),
  then the M16 left-hand power grasp on this solver ([[project-natural-grasp-2026-09-29]]).

## 2026-10-04 crouched M16 switch leg swap: FIXED, PIE confirmed by user + log
New clip AZ_RTG_MH_MOB1_Crouch_Idle_IPC (holster end pose exactly + DMO_MOB1 idle motion as bone-space deltas; sample RAW keys, should_retarget=False, or translations get retargeted twice). Profile field CrouchingSwitchRestIdle (M16 only; Winchester clips already match the unarmed crouch). Anim instance SwitchRestIdle replaces the direct-play idle pick while crouched/unarmed/still/idle; set only while the switch clip covers the whole body. Full-body blend in 0.35 s (out 0.15). Doc s.5.10.

## 2026-10-04 later: pistol fixes, Winchester holster, TD-006 first pass (PIE pending)
- Full body only for the profile's OWN crouch clip (bCrouchClip): the pistol has no crouch switch clips and its standing clip stood the hero up.
- Hands-empty priming: during a switch, while bHolster == bSocketApplied, the anim instance's chooser inputs are unarmed (Weapon.None, profile null); fixed the pistol crouch hold flashing after the holster.
- Winchester holster = TakeUp reversed (AZ_MST_Rifle01_St_TakeUp_Reversed / AZ_MST_Rifle_Cr_TakeUp_Reversed, exact), attach 1.30.
- TD-006: AimAlpha / CombatReadyAlpha eased (smoothstep of a linear ramp, 4/speed s); AimIdleBlendTime 0.4 for idle<->aim-idle swaps in place; profile aim in 18 (rifles) / 16 (pistol), out 10; switch montage blend 0.20 / 0.25; CombatReady 12 / 9.
- Winchester: AttachTime was 0.00 draw / 1.30 holster (wrong: the gun bone is 76 cm off the back at frame 0); measured grab -> draw 0.50 St / 0.47 Cr, holster 0.83 / 0.87. Draws re-baked to START from our idle (AZ_MST_*_TakeUp_FromIdle, 0.35/0.30 s smoothstep from AnimPro_Idle / AnimPro_CrouchLoop_new as seen on the hero mesh); holsters = those reversed. RifleMega take/hide start from a hunched pack stance (torso +14/+43 deg).
- Winchester reach hunch = RifleMega stance inside the clip (recording proved the graph adds nothing); baked out: spine/neck/head bone-space delta to our idle (spine_01 absorbs the pelvis tilt), faded by the clip's own torso straightening. Recorder trap: the grip node only evaluates while the master grip alpha > 0, so the draw records from the hand-attach only.
- Winchester own data (2026-10-04): carry socket BackWinchesterSocket (spine_04, copy of BackRifleSocket, in hero_sockets.json; user to tune by eye; the RifleMega take-up has no resting gun pose, so it cannot be derived); Quick Select icon Winchester_PrimaryIcon (Blender render of Art/CHALK_Winchester_Rig/SK_Winchester.fbx, 512x256, UI group) replacing the AK12 Rifle_PrimaryIcon; FireSound SniperRifleB_Fire_Cue (was the M16 RifleB). The L_001 pickup instance has its OWN manifest copy (ItemCategory=Equippable override) -> edited too; level left dirty for the user to save. Pickup package still lists stale deps on BP_Pickup_PistolMagazine / T_Ammo9mm (not in any live component/graph).
- Winchester switch clips: baked by C++ UAZNaturalGripLibrary::BakeSwitchClip(FAZSwitchClipBake, bDryRun) (NGSwitchBake.cpp; Python: unreal.AZNaturalGripLibrary.bake_switch_clip(job, False); dry run first). Winchester 2026-10-04: standing margin 0.6, crouch margin 1.2; auto grab 0.533/0.567 s onto BackWinchesterSocket; attach draw 0.533/0.567, holster 0.800/0.767. Trap: package names have no extension - FPaths::ChangeExtension leaves them bare (backup files collided; fixed). PIE-confirmed by the user 2026-10-04 (attach times one frame late = normal); the Python prototype is DELETED (was untracked). Idle breaks are held during a switch (FAZ_LocoSMInputs::bHoldIdleBreak).
- (history, script deleted) Winchester switch clips were first produced by Tools/wgs/winchester_switch_bake.py (start-from-idle + posture + gun-out-of-face as ONE smooth curve; holster = reverse). Face capsule in AZ Weapon Body Clearance (PA head body is a 3 cm capsule). Bake rule learned: a per-frame minimal correction baked key by key still reads as a twitch - bake corrections as smoothed curves (dilate + blur).

