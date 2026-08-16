---
name: project_gasp58_update_audit
description: "★★ GASP 5.8 update audit (2026-08-15): content now at /Game/GameAnimationSample/; PoseSearch Interaction (multi-char MM) engine facts — backend-agnostic, warp = RM override, constraints for holds; CMC pawn+ABP reference values/formulas for spike P1; CAS unused by Epic and dropped from AZ plans; config adoption checklist. Read before ANY spike-P1, interaction, or GASP-parity work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-16T03:09:27.124Z
---

# GASP 5.8 update audit — 2026-08-15 (4-agent team, raw dumps at C:\UnrealEngine\Games\AZ\Saved\ClaudeAudit\{CMC,MOVER}\)

Supersedes stale claims in [[project_architecture_rationale]], [[project_cmc_backport_spike]], and the old
"GASP at /Game/Blueprints/" standing rule. Related: [[project_grab_grapple_design]] (PSI is the candidate
successor to the NAAT paired-montage machinery), [[project_contextual_anim_mover_assessment]] (CAS now dropped).

## Location + hygiene
- GASP 5.8 content imported at **`/Game/GameAnimationSample/`** (moved in-editor 2026-08-15 with reference
  fixup; 638 stale duplicates from a move glitch deleted after zero-outside-referencer verification).
- Old paths `/Game/Blueprints/`, `/Game/IsolatedExamples/`, `/Game/Characters/UEFN_Mannequin/` hold ObjectRedirectors
  (117 in UEFN_Mannequin); old `/Game/Characters/{Echo,Paragon,UE4_Mannequin,UE5_Mannequins}` and `/Game/MetaHumans/Common` are GONE (fully moved).
- AZ-owned neighbors untouched: /Game/Characters/{DEMO,Mannequins,Mannequin_UE4}, /Game/MetaHumans/{AZMH_Hero,MHC_Hero,…}, /Game/Input/{IMC_Default,IMC_MouseLook,Actions,Touch}.
- GASP source project Config/.uproject were NOT copied — they live only at C:\UnrealEngine\Games\GameAnimationSample\{Config,GameAnimationSample.uproject} (read on disk when needed).
- GASP 5.8 is pure-BP (empty C++ module).

## The three sandbox characters
- **SandboxCharacter_CMC**: plain `/Script/Engine.Character` + STOCK UCharacterMovementComponent — all feel comes from a per-frame BP pass. Secondary pawn (cycled via IA_NextPawn / DDCvar.PawnClass).
- **SandboxCharacter_Mover**: `/Script/Engine.Pawn` + CharacterMoverComponent + NavMover. **Default pawn of GM_Sandbox.** Epic's own editor UserAssetTags mark it "Experimental". Extra features vs CMC: slide, twin-stick targeting, richer trajectory analytics, StrideWarping, ragdoll SM layer.
- **SandboxCharacter_Mover_Ragdoll** (child): PhysicsControlComponent (powered ragdoll via PCA_SandboxCharacter) + **the ONLY implementation of the multi-char interaction driver** (Try_MultiCharacterInteraction, CHT_CharacterInteractionPSDs, IA_Takedown). Base CMC and base Mover pawns have STUBS only (Get_MMIResult empty).

## ★ PoseSearch Interaction (multi-character MM) — engine facts (5.8, PoseSearch plugin runtime)
- **Backend-agnostic: ZERO ACharacter/CMC casts in the interaction path** (only in the separate trajectory helper). Proven on a Mover Pawn in Epic's own sample. Tick fencing via generic FTickFunction prerequisites; comments name CMC and CharacterMoverComponent as equal prerequisites.
- No dedicated anim node: the ordinary `FAnimNode_MotionMatching` grew Experimental fields — `Availabilities[]`, `bKeepInteractionAlive`, `bEnableWarp`, `bWarpUsingRootBone`, warp ratios. BP library `UPoseSearchInteractionLibrary`: MotionMatchInteraction(_Pure), CalculateFullAlignedTransform(s), CalculateNoAlignedTransform, GetMontageContinuingProperties (montage-driven playback!), GetMotionMatchInteractionConstraint, MotionMatchMulti, UpdateConstraints/GetConstraint.
- Model: each character's **UAnimInstance** publishes `FPoseSearchInteractionAvailability {Database, Tag, RolesFilter, BroadPhaseRadius=500 (+10 hysteresis), bDisableCollisions, TickPriority}` per willing frame → subsystem buckets into spatial **islands** → ONE actor's ABP runs ALL the island's searches (brute-force over every role/DB/actor combo — keep islands/radius small for horde) → each context gets Role + shared UMultiAnimAsset + SelectedTime + WantedPlayRate. **One frame latency** to first result. Pairwise collision auto-disable via IgnoreActorWhenMoving. C++ delegates OnInteractionStarted/Continuing/Ended.
- **★ Tag mechanism**: availability with Tag but null Database resolves against others carrying Tag+Database — hero can publish Tag-only, Chalkie carries the DB ⇒ satisfies the no-hardcoded-content-refs rule by design.
- **Warp = root-motion override** (IAnimRootMotionProvider) when bEnableWarp on the MM node — native on CMC RootMotionFromEverything; on Mover it must pass through the RM-attribute bridge. OR manual: CalculateFullAlignedTransform → feed MotionWarping/SkewWarp (API explicitly supports montage-rail usage).
- **Sustained holds**: `UAnimNotifyState_PoseSearchConstraint` on clips → per-frame socket-to-socket transforms (FromSocket/Role→ToSocket/Role, weights, RampUp/CoolDown 0.2, DesiredReach), queried thread-safe in ABP → feeds TwoBone/FBIK (targets only, no solve). Looping multi-role assets legal; bKeepInteractionAlive + continuing-interaction cost biases hold it.
- Status: whole API `Experimental` ("might be removed without warning") inside the otherwise-production PoseSearch plugin. Modules for Build.cs: `PoseSearch` (+Chooser/BlendStack if touched). No new .uproject plugins needed (AZ already enables PoseSearch/Chooser/BlendStack/MotionWarping).

## Sample interaction data model (the takedown/shove/tackle demo)
- Schema **PSS_CharacterInteraction** (the only one used; PSS_Interaction_{Takedown,Tackle,Shove} are 0-referencer leftovers with richer tuning ideas — wedge filters, foot-phase channel): 2 roles Attacker/Victim, exactly 2 cross-role root-Position channels (A-root-relative-to-V + inverse), NO trajectory/pose channels ⇒ partner query needs only a PoseHistory node, not a full MM ABP.
- DB rows = **PSIA_* (UPoseSearchInteractionAsset : UMultiAnimAsset)**: Items = [{Anim=AM_*_A montage, Role=Attacker, warpT/R weights}, {Anim=AM_*_V, Role=Victim, Origin = yaw-180 + stand-off}]. Weights: attacker translation 0 (he closes the gap), victim translation 1 (never moved). **Shared-origin authoring, victim yaw-180 = literally AZ's NAAT doctrine.** Origins: takedown stand-off 137.64cm (AZ GrabHoldDistance = 92), shove 85.64, tackle attacker origin −469.5 (run-up baked in).
- Granularity: direction (B/F/L/R) × gait (stand/walk/run takedowns) × foot phase (tackle only) = 24 PSIAs, 5 PSDs (BruteForce, continuingPoseCostBias −0.01). Chooser CHT_CharacterInteractionPSDs picks the PSD by {InteractionType, Speed} (shove <200→Shove, ≥200→Tackle; takedown ≤150 stand /150–200 walk /≥200 run) and outputs Initiator/Target role arrays.
- **Playback = montage on `DefaultSlot`** (not through the MM node): pawn polls Get_MMIResult → resolves this role's montage from the PSIA → plays; ABP meanwhile UpdateConstraints/GetConstraint per hand → MMI_IK_Hand{L,R} Alpha+Target (game-thread in sample). Clip flags: bEnableRootMotion=true, bForceRootLock=true, RootLock=RefPose. New 5.8 notifies: BP_NotifyState_MMI_MotionWarping, BP_NotifyState_MMI_IK, BP_AnimNotify_TriggerRagdoll, BP_NotifyState_OverrideCapsuleCollision / _OverrideMovementMode, BP_AnimNotify_SimpleCameraShake.

## CAS (ContextualAnimation) in 5.8 — DROPPED from AZ plans
Unchanged from 5.7: warp path still opens with Cast<ACharacter> (ContextualAnimSceneActorComponent.cpp:208) + requires UMotionWarpingComponent; CMC couplings at :564/:633/:697/:1324/:1391. Plugin moved DEEPER into Experimental (Plugins/Experimental/Animation/). **Epic's own 5.8 sample uses CAS nowhere** — takedown-shaped problems are solved with PSI + SmartObject/StateTree + chooser-proxy + MotionWarping. AZ decision candidate (pending user go): drop CAS entirely; PSI replaces it for grab-v2/executions; task #15 repoints to PSI.

## CMC pawn reference (for spike P1/P2) — SandboxCharacter_CMC
- **PreCMCTick architecture**: AC_PreCMCTick component + `AddTickPrerequisiteComponent(CMC after it)` + `AddTickPrerequisiteActor(Mesh after actor)`; every frame BEFORE CMC ticks: UpdateRotation_PreCMC + UpdateMovement_PreCMC write derived CMC params.
- Formulas: Braking = has-input ? 500 : **2000**; MaxAccel = walk/run 800, sprint MapRangeClamped(SpeedXY, 300→700, 800→300); GroundFriction = 5, sprint MapRangeClamped(SpeedXY, 0→500, 5→3); RotationRate yaw = **−1 (instant) grounded / 200 falling**; strafe/aim ⇒ bUseControllerDesiredRotation, else bOrientRotationToMovement.
- Directional speeds (per-gait FVector fwd/strafe/back + Curve_StrafeSpeedMap on |CalculateDirection|): walk 200/180/150, run 500/350/300, sprint 700 flat, crouch 225/200/180. (*_Demo sets 165/375/600 ≈ AZ's current scalars — ours are the old demo numbers.)
- CMC detail values: capsule 30/86 mesh −88; JumpZ 500, AirControl 0.25, bUseFlatBaseForFloorChecks=true, MinAnalogWalkSpeed=150, BrakingFrictionFactor=0, PerchRadiusThreshold=20, CrouchedHalfHeight=60, bCanWalkOffLedgesWhenCrouching=true, NavMovementProperties.bUseAccelerationForPaths=true (infected AI!).
- Jump = **traversal-first**: IA_Jump → TryTraversalAction; fallback native Jump() only if traversal+montage selection failed; gated by !IsSlotActive(DefaultSlot). Crouch native. Sprint gate: |Δ(actor rot, input dir)| < 50° when strafing.
- Replication: S_PlayerInputState replicated via unreliable Server RPC per input edge; sim proxies detect land/jump via OnCharacterMovementUpdated edge (UpdatedMovementSimulated). JustLanded latch 0.3s retriggerable + LandVelocity, surfaced to ABP.
- Camera: new **GameplayCameras** (GameplayCameraComponent + CameraAsset_SandboxCharacter + CameraDirector, CVar DDCVar.NewGameplayCameraSystem.Enable) with SpringArm legacy fallback. GameplayCameras plugin is EnabledByDefault in 5.8.
- Seams: BPI_SandboxCharacter_Pawn {Get_PropertiesForAnimation/Camera/Traversal/Ragdoll, Get_MMIResult, Set_CharacterInputState, Add/RemoveTarget, Set_IsPlayingRootMotion, Set_PhysicsProfile}; ONE struct S_CharacterPropertiesForAnimation per frame to the ABP.

## CMC ABP reference (for spike P1) — SandboxCharacter_CMC_ABP
- **Dual path** by LocomotionSetup (CVar DDCVar.LocomotionSetupCMC; CDO default 0 = MM node): path 0 = MM node + CHT_PoseSearchDatabases (LOD → Dense/Sparse/ExtremeSparse nested choosers; inputs MovementMode/Stance/MovementState/Gait + JustLanded_Light/Heavy + TimeToLand + JustTraversed); path 1 "Experimental SM" = **controller SM (pose discarded) + one shared tagged BlendStack + CHT_CMCCharacterAnimations + MotionMatch-over-candidates against PoseHistory tag — EXACTLY AZ v2's doctrine, incl. same-named SetBlendStackAnimFromChooser.** AZ architecture = convergent with Epic's current direction.
- Trajectory: `PoseSearchGenerateTransformTrajectory` for-Character with **(HistorySamplingInterval=−1, HistoryCount=30, PredictionInterval=0.1, PredictionCount=15)** (AZ CMC currently 0.04/10/0.1/15); per-state FPoseSearchTrajectoryData (Idle maxControllerYawRate=100, Moving=0); then **`HandleTrajectoryWorldCollisions`** (gravity, floorOffset 0.01, obstacleHeight 150) — biggest AZ gap. Taps: past −0.3..−0.2, current 0..0.2, future **0.4..0.5** (AZ taps 0.1–0.3).
- **IsMoving on CMC path = |current velocity| > 0.1 (measured!)** vs Mover path = trajectory velocity @0.9–1.0s (intent). AZ's intent formula mirrors the MOVER side — not wrong, but GASP CMC stop-feel assumes velocity-based IsMoving + Stops-tagged FootPlacement plant-setting swaps (unplantRadius 40/replant 0.75/stiffness 250).
- 7-state controller SM (all bAlwaysResetOnEntry, state-entry anim-node functions push BlendStack): IdleLoop / TransToIdle (entry) / LocoLoop / TransToLoco (OnUpdate does RInterpTo rotation-lag) / TransToInAir / InAirLoop / IdleBreak. Alias-based transitions; key rules: IsMoving both ways; stance/gait/direction change re-transitions; TIP re-enter (ShouldTurnInPlace + 0.75s replay guard if already TurnInPlace-tagged); pivot re-enter (tag-conditioned 0.5s lock); 90° re-steer inside first 0.5s of Start; notify-driven ToLoop (custom-blend transition) + Re-Transition via BP_NotifyState_EarlyTransition; land conduit → idle/loco by IsMoving; IsAnimationAlmostComplete = !looping && TimeRemaining ≤ 0.75.
- MM node config (if AZ uses MM nodes): blendTime 0.5 HermiteCubic + blend profile FastFeet+Root_Weight, poseReselectHistory 0.3, playrate 0.85–1.15, notifyRecency 0.2, maxBlendInTimeToOverrideAnimation 0.03, bResetOnBecomingRelevant, maxActiveBlends 4; interrupt policy = InterruptOnDatabaseChange on mode/state/(gait&&moving) change.
- Sample-graph post-process (per BlendStack/MM entry): LocalToComponent → [OrientationWarping α=curve `Enable_Warping`] → Steering procedural (TargetOrientation=Get_DesiredFacing) → Steering TIP (α=tag/curve) → ComponentToLocal. NO StrideWarping on CMC (Mover has it). Dynamic play-rate = lerp(1, clamp(Speed2D/MoveData_Speed, MinDynamicPlayRate, MaxDynamicPlayRate), Enable_Warping) — needs authored curves MoveData_Speed/Min/Max on clips.
- Tail: …→ DefaultSlot (ONLY slot) → OffsetRootBone (Interpolate/Accumulate, halflife 0.2, maxErr 30) → RemapCurves (contact_l/r → (1−x)*100) → FootPlacement (+Stops settings swap by tag) → LegIK → **PoseSearchHistoryCollector (Tag="PoseHistory", poseCount 2, bones foot_l/r thigh_l/r spine_05 pelvis, curve Phase, trajectory pin ← Trajectory var)** = last node before Root.
- Logic curves/tags: Enable_Warping, Enable_TurnInPlaceSteering, MoveData_Speed, Min/MaxDynamicPlayRate, Phase, contact_l/r, MovingTraversal, Disable_AO; DB tags TurnInPlace/Pivot/Start/Stop(s).

## Config adoption checklist (from GASP Config on disk — apply per-feature, NOT wholesale)
- **Collision CONFLICT**: GASP GameTraceChannel1/2/3 = Traversable/Mouse/Obstacle; AZ GTC1–5 = Ability/Projectile/AbilityOverlapProjectile/Pickup/Interactable (DefaultEngine.ini:215–219). Adopting GASP traversal assets ⇒ add channels at slots 6–8 + remap copied assets (their serialized enums point at GASP numbering). Deferred until traversal port.
- **PSI**: add `[/Script/PoseSearch.PoseSearchSettings] AvailabilitiesBufferSize=230` when enabling interactions.
- Ragdoll char: DDCVar.CharacterPhysics.DrawDebug=0 + .DefaultProfile=1; physics bSubstepping=True MaxSubstepDeltaTime=0.016667 MaxSubsteps=16. PhysicsControl plugin arrives transitively via AnimatorKit (EnabledByDefault 5.8) — no uproject edit.
- Tags when needed: Foley.Event.*, MotionMatching.*, SmartObject.ObjectType.*, StateTree.SmartObject.* (+4 redirects). Mobile: DefaultTouchInterface → /Game/GameAnimationSample/Input/TIS_MobileControls.
- AZ already mirrors nearly all GASP DDCvars (incl. LocomotionSetupCMC/Mover, NewGameplayCameraSystem). Diffs: GASP FootPlacementMode=1 (AZ 0), AnalogInputStyle=1 (AZ 0). CVars AZ lacks: net.SubObjects.DefaultUseSubObjectReplicationList=1, fx.Niagara.ForceLastTickGroup=1.
- Plugins NOT needed: AnimationLayering, MovieSceneAnimMixer, HairStrands (cosmetic/cinematic only).

## Corrections to older memory (stale-claim ledger)
1. GASP location: now /Game/GameAnimationSample/ (old /Game/Blueprints/… = redirectors; cross-links exist old→new e.g. PSD_Traversal).
2. "CMC trajectory = bolt-on component" → FALSE in 5.8: Epic's CMC char uses NO component, pure library path (validates AZ's UpdateAnimation_Cmc route).
3. "Chooser routing breaks off-Mover" → FALSE: same CHT+MM stack both backends (CHT_PoseSearchDatabases vs _Mover).
4. "CAS/ecosystem assumes ACharacter ⇒ CMC needed for interactions" → superseded: PSI is backend-agnostic and replaces CAS.
5. Mover "frozen/abandoned" framing weakened: 5.8 gave Mover flagship investment (default pawn, ragdoll+MMI demo, NetworkPrediction ini) — but uplugin still Experimental and Epic's own asset tags call the Mover char Experimental. The CMC-migration case now rests on: experimental-forever risk + AZ's accumulated Mover-bridge pain + RM-native consumption (PSI warp = RM override) — NOT on CAS.
