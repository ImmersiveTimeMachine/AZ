---
name: project_grab_grapple_design
description: "★ NEXT-SESSION design (grounded 2026-07-22, 3-agent research): TLOU-style GRAB/grapple — a Chalkie catches the player, player is rooted and MASHES to escape in 5-10s. Full file:line reuse map + build list + open decisions. Read before implementing grab."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-25T03:11:56.096Z
---

# Grab / Grapple ("caught") — design, grounded 2026-07-22

## ★ NEXT SESSION (state saved 2026-07-25 end-of-session)
GRAB V1 IS PLAYING IN-GAME (catch→close-in clinch→hold→mash→escape/timeout all verified in PIE).
Resume points, in order:
1. **PENDING BUILD**: head-shake signal (`GrabHeadShakeRot` 16/s + tunables in AZ_MoverAnimInstance.h/cpp)
   is WRITTEN BUT NOT BUILT — user ended session before the build. Editor-closed CLI build first.
2. **ABP HAND-WIRING (user, in `AZ_ABP_MoverAnimInstance` — NOT "AZ_ABP_Mover")**: 4 nodes in series
   before Output Pose: TransformModifyBone(spine_02, AddToExisting, bind Rot→GrabBodyShakeRot,
   Alpha→GrabIKAlpha) → TransformModifyBone(head, bind Rot→GrabHeadShakeRot) → TwoBoneIK(hand_l,
   World Space, Effector→GrabIKTarget_HandL, Alpha→GrabIKAlpha) → TwoBoneIK(hand_r,
   Effector→GrabIKTarget_HandR). Status UNKNOWN whether user already wired the 2 IK nodes.
3. **"REACH" ANIM → HERO**: user found the proper grabbed/struggle anim — the `Reach_*` pack anims
   (earmarked "TLOU grab LATER" in the territory plan). USER retargets it to the hero skeleton
   (SKEL_SurvivalMan) themselves; then WE wrap it as a FullBody-slot montage (AnimMontageFactory
   `source_animation` recipe → fix slot via struct-copy-safe write-back) and put it in
   `BP_GA_PlayerGrabbed.StruggleMontages[0]` (CDO edit + BlueprintEditorLibrary.compile_blueprint +
   save — compile is MANDATORY or instances see stale defaults). Hero currently holds IDLE + hand-IK
   (pool deliberately emptied).
4. PIE the full package; tune: GrabHoldDistance(50)/PackStepBackSeconds(1.5) in BP_GA_ChalkieGrab,
   IK bones + shake amp/freq in ABP class defaults, camera Yaw/Pitch/RotationSpeed on hero pawn.
5. **COMMIT**: crowd v3 + per-crowd intensity + the ENTIRE grab feature are UNCOMMITTED on
   feature/NPC (last commit bfaa7bc = crowd v2). Large batch — commit early next session.
6. Backlog: real struggle-meter HUD widget (UProgressBar; debug on-screen bar in place); body-shake
   spike on E-press (match the camera jolt envelope); AZ_ABP_Chalkie fires "Divide by zero:
   Divide_DoubleDouble" every tick (pre-existing BP bug, log spam); task #12 bystander convergence;
   hero Event.Death listener still missing (lethal grab damage = silent).

User ask: "as soon as we lose some health OR randomly" a Chalkie CATCHES the player (plays
`Zombie_Atk_KnockBack_1..5`, the pack has 5), player CANNOT MOVE, must mash the mouse ~5-10s to
escape — like The Last of Us. Design only (implement next session). No grab code exists today (verified).

## Assets
- Grab anims: `Content\Zombie_01\Animations\InPlace\Zombie_Atk_KnockBack_1..5_IPC` (IPC = in-place, right for a
  stationary grab; Root variants also exist). These are AnimSequences → need montage wrapping (1 montage w/ 5
  random sections, matching the variant `HitReactMontage` pattern).
- AnimSet DA: pawn has an object prop `AnimSet` → `DA_ChalkieAnims_{A_Standard,B_Runner,C_Rotter,D_Sprinter}`;
  abilities read montages by reflection name via `FindAnimSetMontage` (`AZ_GA_MeleeAttack.cpp:210-224`). Add a
  `GrabMontage` field, fill per variant. `Reach_*` pack anims were earmarked "TLOU grab LATER" in territory plan.
- GAP: those are the CHALKIE's anims (Zombie skel). The PLAYER (hero/GASP) has NO grabbed/struggle anim → v1
  likely camera-lock + root only (see decisions).

## The spine (5 steps) — TRIGGER MODEL REFINED 2026-07-24 (user)
Full NPC cycle: investigate → detect → chase/catch → ENGAGE (Press/Active melee) → grab fires RANDOMLY
mid-engagement. NOT predictable, NOT often, NO telegraph (no wind-up entry — blend straight into the hold).
1. TRIGGER (v1 = random-only; NO health-threshold wiring): inside the EXISTING `UAZ_BTTask_ZombieAttack`
   ExecuteTask — when the Chalkie already won its attack slot and is about to punch, roll small chance
   (tunable, ~10%) + per-Chalkie cooldown (tunable, ~45s) + `RequestGrabToken` (horde subsystem, one grabber
   per prey) + player not `State.Grabbed` → activate `GA_ChalkieGrab` INSTEAD of the melee ability.
   **NO BT structure changes, NO new BB key, NO new BT node** — the grab is "a longer attack" to the BT;
   existing latent-until-ability-end + facing + StopMovement + `SetMeleeTaskActive`→Locked all reused as-is.
   LATER (not v1): advantage-based trigger (NPC has upper hand), health-drop trigger, stealth-catch from
   behind using the spare `AM_Zombie_Atk_Start_*` telegraphed entries.
2. CHALKIE GRABS: `GA_ChalkieGrab` plays `GrabLoopMontage` directly (self-loop via
   `Montage_SetNextSection(Default,Default)`), fires `Event.Grabbed` at the player. On resolve: Montage_Stop
   + play `GrabEndMontage` (player failed → damage chunk) or `GrabEscapeMontage` (player broke free → stagger).
3. PLAYER GRABBED: `Event.Grabbed` triggers `GA_PlayerGrabbed` (event-triggered ability, SAME mechanism as
   `Event.Death`→`GA_Death`, `AZ_GA_Death.cpp:26-40`). Applies `State.Grabbed` (ActivationOwnedTags), plays the
   held montage/pose on the player, shows the struggle meter.
4. ESCAPE: mash input → new looping mash AbilityTask counts presses; parallel 5-10s timer.
5. RESOLVE: meter full first → free player + stagger the Chalkie (knockback/recovery); timer first → heavy
   damage chunk (or Downed).

## Reuse map (exists → file:line) — why this is LOW RISK
- Chalkie root/facing/latent → `AZ_BTTask_ZombieAttack.cpp`: StopMovement `:118-121`; facing override each tick
  `TickTask:201-207` (unit DIRECTION, not location); latent + `OnAbilityEnded` `:135-166`; `SetMeleeTaskActive`
  true `:147-150`, cleared in `Cleanup():275`.
- Crowd "leave me alone" → `AZ_HordeSubsystem.cpp`: `IsMeleeTaskActive()`→`ERankClass::Locked` `:443`, reachable
  ONLY inside the `bWasActiveOnPrey` incumbent branch `:425` → so the grab MUST live inside the Press/Active
  fork (option a). Flicker guard `:408-421` also keys on the flag. Grab holds one of MaxAttackers Active slots.
- Make the PLAYER play a montage + freeze → NO task plays a montage on another actor; use
  `SendGameplayEventToActor` (`AZ_VitalsAttributeSet.cpp:96`) → `AbilityTriggers` GameplayEvent
  (`AZ_GA_Death.cpp:26-40`). Triggered ability runs on its OWN avatar → normal self-montage works.
- Apply a GE to the player's ASC → `ApplyGameplayEffectSpecToTarget(*Spec, TargetASC)` +
  `GetAbilitySystemComponentFromActor` (`AZ_GA_MeleeAttack.cpp:284,306`).
- Freeze player MOVEMENT → GAS-tag gate already in `ProduceInput` (`AZ_PawnMoverHeroCharacter.cpp:491-497`
  zeroes WorldMove on `Ability.State.MeleeAttacking`); add `State.Grabbed` there. Live player pawn =
  `AAZ_PawnMoverHeroCharacter` (v2), confirmed (`AZ_PlayerController.cpp:189,207`).
- Freeze player CAMERA → NO existing gate; add a `State.Grabbed` early-return in `OnLookTriggered`
  (`AZ_PawnMoverHeroCharacter.cpp:344-349`). (Do NOT remove the IMC or the mash IA stops firing.)
- MASH counter → reuse `UAZ_AT_WaitInputPressWithTags::Reset()` re-bind pattern (rebinds `InputPressed`
  replicated event WITHOUT ending) → loop: on each press `++Count`+broadcast, rebind, finish at threshold or
  timer. No press-counter task exists today. Input delivery: `ASC->AbilityInputTagPressed`
  (`AZ_AbilitySystemComponent.cpp:46-65`). Escape input = mash primary-attack (LMB) tag `Input.Action.*`.
- Player damage/health → ASC on `AAZ_PlayerState` + `UAZ_VitalsAttributeSet`; damage via `GE_Damage`→
  `IncomingDamage`→`Health` (`AZ_VitalsAttributeSet.cpp:44-55`). TRIGGER hook = attribute-change delegate
  `GetGameplayAttributeValueChangeDelegate(HealthAttribute)` (pattern at
  `AZ_PawnMoverInfectedCharacter.cpp:139-140`; caveat: no causer) OR add a hero cast in
  `PostGameplayEffectExecute` `:62-81` (causer-aware). NO threshold event exists → new logic.

## Build list (all editor-closed CLI C++ + assets) — SIMPLIFIED 2026-07-24: no BT work at all
- `GA_ChalkieGrab` (grabber) + `GA_PlayerGrabbed` (victim, event-triggered).
- Grab roll inside `UAZ_BTTask_ZombieAttack` (chance + cooldown UPROPERTYs on the node = hand-tunable
  values in the BT editor, no structure change). ~~UAZ_BTTask_ChalkieGrab / bGrabReady / hand-placed node~~
  DROPPED — grab reuses the existing attack task slot.
- Looping mash AbilityTask + small struggle-meter HUD widget (new `UProgressBar`; HUD add pattern
  `AZ_PlayerController.cpp:269-284`; no transient meter exists today).
- New tags `State.Grabbed` (player) + `State.Combat.Grabbing` (Chalkie, for anim + a rule-8 flinch-cancel
  carve-out so a co-op partner hitting the grabber doesn't glitch). `Character.Stunned`/`Character.Downed`
  exist but UNWIRED — adoptable for the Chalkie-stagger / player-fail outcomes.
- Grab arbitration in `UAZ_HordeSubsystem` (one grabber per prey; RequestGrabToken sibling of attack token).

## DECISIONS LOCKED (user 2026-07-22)
1. Player-side visuals v1 = **(b) ONE looping hero STRUGGLE POSE** while grabbed. Needs 1 hero (GASP skel)
   clip — placeholder OK (reuse an existing GASP flinch/hit-react loop until an authored struggle clip lands).
   This is the ONLY art asset the feature needs beyond wrapping the Chalkie KnockBack montage.
2. Fail outcome v1 = **(a) HEAVY DAMAGE CHUNK** (tunable), then release. NO Downed/revive system. If it drops
   the player to 0, death comes through the normal `Event.Death` path (which still has no hero listener — a
   separate backlog item).
3. Escape input CHANGED 2026-07-24: **mash "E" (Interact — `AZ_IA_Interact`)**, NOT LMB. E already routes
   through GAS (input tag → ASC → `GA_Interact` pickup ability). GA_PlayerGrabbed binds the same Interact
   input; `GA_Interact` gets `State.Grabbed` in ActivationBlockedTags so E mid-grab feeds ONLY the mash.
   Mash primitive = `UAZ_AT_WaitInputPressWithTags` (ported, unused until now — `Reset()` rebind loop is
   purpose-built for this). Escape rule v1: N presses (~8) before window (~7s); meter = presses/N.
4. Grab cooldown = GAS Cooldown GE on `GA_ChalkieGrab` granting `Cooldown.Grab` tag (new native tag) on the
   Chalkie ASC; the roll skips Chalkies holding it. Trigger v1 = random roll only, mid-engagement.
5. TEST rig (user 2026-07-24): `az.Grab.ForceNext` CVar — next attack opportunity becomes a grab through the
   REAL path (roll→token→ability, no bypass); + `az.Grab.Chance`/mash-count/window override CVars.
   SEAM TO VERIFY before PIE: `AbilityInputTagPressed` routing when target ability is ALREADY ACTIVE
   (`AZ_AbilitySystemComponent.cpp:46-65`) — this feeds WaitInputPress.

## IMPLEMENTATION ORDER — seam-trace before PIE ([[feedback_seam_trace_before_pie]])
1. Tags + assets — **PHASE 1 COMPLETE 2026-07-24**:
   - [DONE 2026-07-22, built] native tags `State.Combat.Grabbing`, `State.Grabbed`, `Event.Grabbed`
     (`AZ_GameplayTags.h/.cpp`). Editor-closed CLI build green.
   - [DONE 2026-07-24] DA pattern confirmed by T3D export: `BP_ChalkieAnimSet` (BP DataAsset at
     `/Game/AZ/AI/`, NOT native yet — native port queued in the batch task) has SINGLE `AnimMontage*` fields;
     `HitReactMontage` is the only per-variant one (A=KB_Chase_2, B=5, C=1, D=3). Zombie montages: skeleton
     `UE4_Mannequin_Skeleton`, slot `DefaultSlot`, blend 0.25/0.25.
   - [DONE 2026-07-24, CORRECTED by user] The pack is a full 4-STAGE GRAB FAMILY, 5 matched variants each
     (same N = same mocap family, poses chain): `Zombie_Stand_To_Atk_N` = CATCH entry (2.9-6.3s — long;
     Phase 2 may start mash timer during it / trim), `Zombie_Atk_Loop_N` = HOLD/bite loop (6.4-17.3s, covers
     the mash window in one play; GA self-loops for safety), `Zombie_Atk_End_N` = FAIL exit (attack lands,
     damage chunk, 3.3-5.6s), `Zombie_Atk_KnockBack_N` = ESCAPE exit (Chalkie shoved off/stagger, 5.5-7.5s).
     Created 20 montages `AM_Zombie_Atk_Start/Loop/End_1..5` + `AM_Zombie_KB_Atk_1..5` (all IPC sources,
     DefaultSlot) via scripted AnimMontageFactory (`source_animation` prop → no dialog). `BP_ChalkieAnimSet`
     final fields (interim single `GrabMontage` and `GrabStartMontage` both REMOVED — no telegraph, per user):
     `GrabLoopMontage`, `GrabEndMontage`, `GrabEscapeMontage` — filled family-N: A=1, B=2, C=3, D=4
     (**family 5 spare**; `AM_Zombie_Atk_Start_1..5` kept on disk unreferenced, for future stealth-catch).
     Saved + verified read-back after every mutation.
   - [DONE 2026-07-24] Hero struggle placeholder: `AM_Hero_Struggle` (4.1s, slot **FullBody** — the hero
     montage slot per AM_Fists_Punch_L, skeleton SKEL_SurvivalMan) wrapping
     `LM_RM_Idle_Hit_Strong_Left` (enable_root_motion=True on ALL hero hit-reacts → RM extracted+discarded
     when no consumer = clean in-place playback). UE-Python gotcha hit: `tracks[0].set_editor_property(...)`
     mutates a COPY — must reassign `tracks[0] = t` then set the whole array back.
2-5. **IMPLEMENTED + BUILT GREEN 2026-07-24** (two CLI builds). What exists:
   - `AZ_GA_ChalkieGrab.h/.cpp`: BB-target read → Event.Grabbed → verify State.Grabbed took →
     self-looped GrabLoopMontage (`Montage_SetNextSection`) → WaitGameplayEvent verdicts
     (Escaped→EscapeMontage; Timeout→35dmg SetByCaller chunk via UAZ_GE_Damage→EndMontage);
     15s safety = escaped; abnormal EndAbility sends Event.GrabRelease. ConfigureCDO adds
     State.Combat.Grabbing ActivationOwnedTags. Granted native at infected possess.
   - `AZ_GA_PlayerGrabbed.h/.cpp`: Event.Grabbed-triggered (ConfigureCDO: trigger + State.Grabbed
     ActivationOwnedTags), CancelAbilities on catch, task-per-press mash loop on
     `UAZ_AT_WaitInputPressWithTags` (8 presses / 7s defaults), on-screen debug meter, verdict events
     back to grabber, Event.GrabRelease listener. Granted native at hero possess with
     Input.Action.Interact DYNAMIC tag (E-press routes via AbilityInputTagPressed→InputPressed on
     ACTIVE spec — verified; ASC Pressed never activates, Held does and is blocked by the grab gate).
   - Universal lockout: `bActivatableWhileGrabbed` (default false) checked in
     `UAZ_GameplayAbility::CanActivateAbility` — State.Grabbed blocks ALL ability activation except
     opted-in. Movement zeroed in ProduceInput; camera frozen in OnLookTriggered.
   - BT roll in `UAZ_BTTask_ZombieAttack` after token grant: GrabChance 0.10 + GrabCooldownSeconds 45
     (stamped on controller `LastGrabEndTimeSeconds` at grab END) + prey-free check +
     `RequestGrabToken`/`ReleaseGrabToken` (one grabber per prey, `UAZ_HordeSubsystem`).
     `ChosenAbilityClass` threads through all cancel/end paths; grab runs use GrabTimeoutSeconds 30.
   - Flinch carve-out: `HandleDamaged` skips the stagger while State.Combat.Grabbing.
   - Fixed: `WaitInputPressWithTags` hardcoded "State.Interacting" RequestGameplayTag would
     ensure-spam per press (tags unregistered) → ErrorIfNotFound=false.
   - CAMERA FEEL (2026-07-24 batch 2): `CameraGrabbed` framing mode on hero pawn (boom 130, offset
     (0,60,15), FOV 85, interp 5; precedence Grabbed > Aiming > Strafe > Explore in
     UpdateCameraForMode) + `AZ_GrabCameraShakes.h/.cpp` (`UAZ_CameraShake_GrabRumble` infinite perlin
     bSingleInstance; `UAZ_CameraShake_GrabJolt` 0.3s wave per press) + curve-scaled intensity:
     GA samples `/Game/AZ/Camera/Curve_GrabShakeIntensity` (0→0.85, 0.5→1.1, 1→1.9; CSV-imported —
     UCurveFloat.FloatCurve is NOT Python-writable, use CSVImportFactory+ECSV_CURVE_FLOAT) at each
     press and restarts the rumble (bSingleInstance = retune). Needed `EngineCameras` module dep.
   - New tags: Event.GrabEscaped/GrabTimeout/GrabRelease. New CVars: az.Grab.ForceNext (self-clearing
     one-shot), az.Grab.Chance/CooldownSeconds/PressesToEscape/WindowSeconds.
6. PIE test recipe: `az.Grab.ForceNext 1` in console → get engaged → next attack = grab. Expect: camera
   pulls in + rumble, movement/camera locked, E-jolts fill meter, 8 presses→Chalkie KB stagger, or 7s→
   35 damage + End anim. Verify E-press routing via LogAZ Verbose "AbilityInputTagPressed".
   HUD widget (real UProgressBar) still TODO — debug meter in place.
7. FIRST PIE 2026-07-24: grab rolled twice but NOTHING visible — player side never activated, and the
   ability gates were silent. Diagnostic UE_LOGs added to every gate ([Grab] tag). ALSO FOUND+FIXED:
   `AZ_InputConfig` had NO Interact row at all (E never reached GAS input) — USER hand-added
   `AZ_IA_RT_Interact → Input.Action.Interact` (row 6; scripted add impossible: FAZ_InputAction props
   are EditDefaultsOnly = Python "cannot be edited on instances"; GameplayTag construction from Python
   also blocked — copy one from a CDO that holds it, e.g. BP_GA_Interact.InputTag). IMC confirmed: E key.
7b. ★ ROOT CAUSE of "shake but no grab" (2026-07-24, PIE log proof): **runtime CDO patches to
   ActivationOwnedTags NEVER reach the instances of BP tuning children** — the instance activated with
   an EMPTY tag container (`instOwnedTags=''`) while the TRIGGER patch kept working (triggers are read
   into the ASC's GameplayEventTriggeredAbilities map once at GiveAbility; tag containers are copied
   per-instance from serialized BP data). FIX (the standing pattern now): apply state tags EXPLICITLY —
   `AddLooseGameplayTag` after commit in ActivateAbility + paired `RemoveLooseGameplayTag` in EndAbility
   guarded by a bAppliedXTag member. Applied to State.Grabbed (GA_PlayerGrabbed) and
   State.Combat.Grabbing (GA_ChalkieGrab). Also added PACK HOLD-OFF (user feedback: others kept clawing
   mid-grab): attack task fails its start gate while the prey has State.Grabbed (grabber is latent
   inside the task, never re-enters).
7c. POLISH BATCH (2026-07-24 late, all built + editor-assigned): (a) UNBREAKABLE loops — replay-on-end
   on both grab montage tasks (only the 2 grab anims may show during a hold); (b) hero BODY faces
   grabber (ProduceInput OrientationIntent override while State.Grabbed, target via
   SetGrabFacingTarget); (c) CAMERA faces grabber (control-rotation RInterpTo in UpdateCameraForMode,
   speed = CameraGrabbed.InterpSpeed); (d) CLOSE-IN — FLayeredMove_LinearVelocity slides the grabber to
   GrabHoldDistance (110cm) over GrabCloseSeconds (0.15s) at the catch: ATTACHMENT INTENT WITHOUT
   ATTACH (live Mover pawns can't be attached — sim stamps transforms; rooted+mutual-facing holds the
   contact; nothing to detach); (e) StruggleMontages POOL + bRandomStruggleMontage/StruggleMontageIndex
   (0-based, default 0). Technique map for future paired combat: melee alignment = MOTION WARPING (the
   planned warp primitive, replaces BT soft-tracking); executions/mounts = movement-suspend+attach
   (spike required) or Contextual Anim Scenes; contact polish = hand IK.
8. BP-FIRST GRANT ARCHITECTURE (user request 2026-07-24: "duplicate all GA/GE in BP"): grant sites
   resolve a BP tuning child at the conventional path via LoadClass, fall back native. CDO patches
   (Configure*) take the RESOLVED UClass* (BP child CDOs do NOT inherit runtime native-CDO patches).
   Child-safety fixes: BT task resolves ChosenAbilityClass to the CONCRETE granted class (IsChildOf
   scan) before activate/cancel-by-class; flinch cancel in HandleDamaged scans IsChildOf(ZombieMelee).
   BP children: Enemy/Abilities/BP_GA_{ZombieMelee,ChalkieGrab,Death}, Hero/Abilities/BP_GA_PlayerGrabbed,
   Effects/BP_GE_Damage. Editor close→build→reopen loop driven headlessly (quit_editor via Python;
   NOTE: QUIT_EDITOR console cmd no-oped; python quit crashed on exit in Slate teardown
   (SAssetShortcut, open asset-editor tab) — harmless, saves happened first).

Related: [[project_chalkie_fight_rules]] (rule 8 flinch-cancel — needs grab carve-out),
[[project_crowd_engagement_design]] (Locked/Active fork), [[feedback_seam_trace_before_pie]],
[[project_combat_fist_build_plan]] (montage-vs-SM rail doctrine).
