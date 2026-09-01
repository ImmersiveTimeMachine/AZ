// Copyright Artur. AZ project.

#include "Animation/AZ_MoverAnimInstance.h"

#include "Character/Cmc/AZ_CmcCharacterBase.h"          // [SPIKE: spike/cmc-backport] CMC (v3) backend
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"   // [v2 CrouchTrace] capsule half-height
#include "Kismet/GameplayStatics.h"        // [v2 CrouchTrace] camera manager
#include "PhysicsEngine/PhysicsAsset.h"    // [v2 CrouchTrace] component audit
#include "Camera/CameraComponent.h"          // [v2 CrouchTrace] camera rig
#include "GameFramework/SpringArmComponent.h" // [v2 CrouchTrace] camera rig   // CMC-branch velocity/accel/crouch reads

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_LocomotionStateMachine.h"
#include "AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "BlendStack/AnimNode_BlendStack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/AZ_ObstacleSensorComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMovementMode_RMAction.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "MoverDataModelTypes.h"   // FCharacterDefaultInputs
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryPredictor.h"

// ★ IK MASTER SWITCH (user decision 2026-08-31): fights will use a different technique (paired /
// contextual montages), so ALL procedural IK on the hero is OFF. The only IK in the Mover hero's
// pose path is the grab hand-IK — two TwoBoneIK + two ModifyBone nodes in the AdiativeCombatGrabbed
// anim layer, every one of them gated by GrabIKAlpha. Holding the alpha's TARGET at 0 here is
// therefore the single owner of "IK is off": the alpha decays to 0, the layer's nodes become
// pass-throughs, and the shake rotators are zeroed by the existing else-branch. No foot IK exists
// on this ABP (verified: 32 AnimGraph nodes, none IK-class; the layer holds only the grab nodes).
// File-scope constexpr rather than a UPROPERTY so it lands under Live Coding; promote to an
// EditDefaultsOnly toggle at the next editor-closed build if it ever needs to be data-driven.
static constexpr bool bGrabIKEnabled = false;

#if !UE_BUILD_SHIPPING
// Debug-HUD carry-over for values that are computed inside SetBlendStackAnimFromChooser but displayed a
// frame later by the HUD. Deliberately file-scope, NOT members: Live Coding cannot add a member (it changes
// the class layout), and this whole block is diagnostic. Single-hero assumption — with two Mover pawns
// alive the HUD would show whichever pushed last, which is acceptable for a debug readout and is the only
// cost of keeping this hot-reloadable.
static float GLastPickCost   = 0.f;
static bool  GLastPickUsedMM = false;
// What the last MM search was told was ALREADY PLAYING (null = it was told nothing).
static TWeakObjectPtr<const UObject> GLastContinuingAsset;
static float GLastContinuingTime = 0.f;
static int32 GLastContinuingConv = -1;   // 1 = BlendStackNode ref resolved to a blend stack, 0 = it did not
static int32 GLastContinuingLoop = -1;   // 1 = bLoop was true at the search (continuity is loop-only)

// SM phase at the moment of the last COMMITTED push, per instance. The phase-locked seam (R13) is valid
// only when the outgoing clip is one that ENDS ON THE LOOP'S FRAME 0 - starts, pivots, lands, i.e. clips
// pushed at TransitionToLocomotion. Stops (TransitionToIdle) end in the IDLE pose; a stop re-pressed near
// its end must resume the loop by SEARCH (or the frame-0 fallback), never by phase. Measured 2026-08-31
// 02:11: `Crouch_WalkFwdStop_LU_new -> Crouch_WalkFwd_new seam=lock rem=0.22` blended a pelvis-41 idle
// pose into a pelvis-65 mid-stride pose over 0.22 s. Keyed by `this` (LC-safe stand-in for a member;
// promote at the next editor-closed build). Not debug-only: the gate needs it in every build.
static TMap<const UAZ_MoverAnimInstance*, EAZ_StateMachineState> GLastPushSMStateByInstance;
// Committed-push counter per instance ([v2 CrouchTrace] prints it: a rising count while the clip name
// never changes = a silent same-asset re-push, invisible to [v2 Pick]/[v2 Snap]).
static TMap<const UAZ_MoverAnimInstance*, uint32> GPushCountByInstance;
#endif

FVector UAZ_MoverAnimInstance::ResolveGrabIKTarget(
	const USkeletalMeshComponent* OwnMesh, FName UpperArmBone, FName LowerArmBone, FName HandBone,
	const USkeletalMeshComponent* PartnerMesh, const FVector& Desired, float ReachScale,
	float* OutDeficit)
{
	if (OutDeficit)
	{
		*OutDeficit = 0.f;
	}
	if (!OwnMesh)
	{
		return Desired;
	}
	const FVector Shoulder = OwnMesh->GetSocketLocation(UpperArmBone);
	const FVector Elbow = OwnMesh->GetSocketLocation(LowerArmBone);
	const FVector Hand = OwnMesh->GetSocketLocation(HandBone);
	const float MaxReach = ((Elbow - Shoulder).Size() + (Hand - Elbow).Size()) * ReachScale;

	const FVector ToDesired = Desired - Shoulder;
	const float Dist = ToDesired.Size();
	if (Dist <= MaxReach || Dist < KINDA_SMALL_NUMBER || MaxReach < KINDA_SMALL_NUMBER)
	{
		return Desired;   // the authored grip wins whenever the arm can actually get there
	}
	if (OutDeficit)
	{
		*OutDeficit = Dist - MaxReach;
	}

	// Farthest the hand can go toward the grip, then pull that onto the partner's body surface. The
	// physics asset is capsule-per-bone, so "surface" is ~2-3cm off the visual mesh — good enough for a
	// clawing grip, and the reachable-socket path above still handles the precise case.
	const FVector Clamped = Shoulder + ToDesired * (MaxReach / Dist);
	if (PartnerMesh)
	{
		FClosestPointOnPhysicsAsset Closest;
		if (PartnerMesh->GetClosestPointOnPhysicsAsset(Clamped, Closest, /*bApproximate*/ true))
		{
			// Only take the surface point if the arm can reach IT — otherwise we would have traded an
			// air-grab pointing at the grip for an air-grab pointing at the chest.
			if ((Closest.ClosestWorldPosition - Shoulder).Size() <= MaxReach)
			{
				return Closest.ClosestWorldPosition;
			}
		}
	}
	return Clamped;   // no reachable surface: full extension toward the grip, but never a stretch
}

void UAZ_MoverAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// The locomotion phase machine (extracted from the old DeriveSMState). NewObject with `this` as outer so
	// the StateMachine UPROPERTY keeps it alive; tunables stay on this AnimInstance and are passed into Tick.
	StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);

	// RM bridge (anim side). Mover does NOT consume UAnimInstance::RootMotionMode directly, but
	// RootMotionFromEverything makes the engine extract the per-frame root delta from ALL playing
	// anims into the "RootMotionDelta" mesh attribute. FLayeredMove_RootMotionAttribute (queued on
	// the pawn's MoverComp in BeginPlay) consumes that attribute to move the capsule. Loops carry
	// ~zero authored RM, so this only drives stop/start/transition clips. Mirrors v1 UAZ_AnimInstance.
	// See project_root_motion_mode.
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;

	// Cache pawn + Mover refs — pawn class is fixed at spawn, no need to re-cast every tick.
	if (APawn* PawnOwner = TryGetPawnOwner())
	{
		Cached_Pawn = Cast<AAZ_PawnMoverHeroCharacter>(PawnOwner);
		if (Cached_Pawn)
		{
			Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
			Cached_CharacterMoverComponent = Cast<UCharacterMoverComponent>(Cached_MoverComponent);
		}
		else
		{
			// [SPIKE: spike/cmc-backport] CMC (v3) backend. RootMotionFromEverything (set above) is the
			// RIGHT mode here too — CMC consumes graph root motion natively in that mode, which is what
			// will drive the stop/start/transition clips in P1 (loops carry ~zero RM either way).
			Cached_CmcCharacter = Cast<AAZ_CmcCharacterBase>(PawnOwner);
		}
	}

	// Re-init can run on a re-used / Live-Coding-re-instanced object: clear every one-shot stash and
	// push-cache so the first real push can't inherit state from a clip that no longer exists.
	PendingBlendOut                   = 0.f;
	bPendingTransitionRMMove          = false;
	PendingTransitionRMMoveDurationMs = 0.f;
	TransitionSerial                  = 0;
	LastPushedTransitionSerial        = 0;
	LastPushedSMState                 = EAZ_StateMachineState::IdleLoop;
}

void UAZ_MoverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// [SPIKE: spike/cmc-backport] CMC (v3) backend takes its own compact path — one added branch, the
	// entire Mover body below stays byte-identical (zero risk to the v2 pawn the spike compares against).
	if (!Cached_Pawn && Cached_CmcCharacter)
	{
		UpdateAnimation_Cmc(DeltaSeconds);
		return;
	}

	if (!Cached_Pawn || !Cached_MoverComponent)
	{
		return;
	}

	// ============================== GRAB HAND-IK GATHER ==============================
	// Grabbed hold = base IDLE + hands pinned onto the grabber (two TwoBoneIK nodes near the AnimGraph
	// output bind to these). Cross-actor targets → world space, gathered here on the game thread.
	{
		float TargetAlpha = 0.f;
		// LET GO ON CONTACT. The grab keeps its facing target and State.Grabbed through the escape on
		// purpose (the hero rides its own shove animation while still "in" the grab), so those two alone
		// would hold the palms glued to a Chalkie that is being hurled 117cm away — the hands visibly
		// stretch after it. The shove's contact frame releases this; GrabIKBlendSpeed fades it out.
		// bGrabIKEnabled (file scope) is the master switch — see its comment at the top of this file.
		if (const AActor* Grabber = (!bGrabIKEnabled || Cached_Pawn->IsGrabIKReleased()) ? nullptr : Cached_Pawn->GetGrabFacingTarget())
		{
			const UAbilitySystemComponent* HeroASC = Cached_Pawn->GetAbilitySystemComponent();
			if (HeroASC && HeroASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
			{
				if (const USkeletalMeshComponent* GrabberMesh = Grabber->FindComponentByClass<USkeletalMeshComponent>())
				{
					TargetAlpha = 1.f;
					// Reach-clamped: the grip socket when the arm can get there, else the nearest point on
					// the grabber's body it CAN reach (see ResolveGrabIKTarget). Smoothed because the clamp
					// hands off between socket and surface as the wrestle pose oscillates; SNAPPED on the
					// grab's first frame or the hands would lerp in from wherever the last grab left them.
					const FVector NewR = ResolveGrabIKTarget(GetSkelMeshComponent(),
						TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
						GrabberMesh, GrabberMesh->GetSocketLocation(GrabIKGrabberBoneForHandR), GrabIKReachScale);
					const FVector NewL = ResolveGrabIKTarget(GetSkelMeshComponent(),
						TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
						GrabberMesh, GrabberMesh->GetSocketLocation(GrabIKGrabberBoneForHandL), GrabIKReachScale);
					const bool bSnap = GrabIKAlpha < 0.05f;
					GrabIKTarget_HandR = bSnap ? NewR : FMath::VInterpTo(GrabIKTarget_HandR, NewR, DeltaSeconds, GrabIKTargetInterpSpeed);
					GrabIKTarget_HandL = bSnap ? NewL : FMath::VInterpTo(GrabIKTarget_HandL, NewL, DeltaSeconds, GrabIKTargetInterpSpeed);
				}
			}
		}
		GrabIKAlpha = FMath::FInterpTo(GrabIKAlpha, TargetAlpha, DeltaSeconds, GrabIKBlendSpeed);

		// Body + head shake: perlin rotation noise, faded by the same alpha (the AnimGraph's Transform
		// Modify Bone nodes add these in bone space). Distinct offsets per axis and per signal = two
		// decorrelated organic trembles — slow body strain, fast head panic — never a synchronized wobble.
		if (GrabIKAlpha > KINDA_SMALL_NUMBER)
		{
			const UWorld* ShakeWorld = GetWorld();
			const float Now = ShakeWorld ? static_cast<float>(ShakeWorld->GetTimeSeconds()) : 0.f;
			const float TBody = Now * GrabBodyShakeFrequency;
			GrabBodyShakeRot = FRotator(
				FMath::PerlinNoise1D(TBody) * GrabBodyShakeAmplitudeDeg,
				FMath::PerlinNoise1D(TBody + 49.3f) * GrabBodyShakeAmplitudeDeg * 0.6f,
				FMath::PerlinNoise1D(TBody + 151.7f) * GrabBodyShakeAmplitudeDeg * 0.6f) * GrabIKAlpha;
			const float THead = Now * GrabHeadShakeFrequency;
			GrabHeadShakeRot = FRotator(
				FMath::PerlinNoise1D(THead + 293.1f) * GrabHeadShakeAmplitudeDeg,
				FMath::PerlinNoise1D(THead + 397.7f) * GrabHeadShakeAmplitudeDeg * 0.7f,
				FMath::PerlinNoise1D(THead + 509.3f) * GrabHeadShakeAmplitudeDeg * 0.5f) * GrabIKAlpha;
		}
		else
		{
			GrabBodyShakeRot = FRotator::ZeroRotator;
			GrabHeadShakeRot = FRotator::ZeroRotator;
		}
	}

	// ============================== ROOT-MOTION BRIDGE ==============================
	// How a chooser-picked clip gets to move the CAPSULE (not just the mesh).
	//
	// The model: LOOP clips (idle/walk/run) are velocity-driven — their root motion is
	// extracted but never consumed, the capsule follows Mover's own velocity/input.
	// TRANSITION clips (start / stop / turn, and the hybrid-jump RISE) are authored
	// root-motion — for those, and only those, the capsule must follow the clip exactly.
	//
	// Mover's mechanism for that is a layered move: while an FLayeredMove_RootMotionAttribute
	// is live, the simulation reads the mesh's root-motion attribute (written every frame
	// because this AnimInstance has RootMotionFromEverything) and OVERRIDES the capsule
	// motion with it. No move queued = attribute ignored = velocity-driven. That single
	// bit — "is the move live" — is the whole loop-vs-transition switch.
	//
	// The bridge has two halves, because Mover may only be written from the game thread:
	//   1. SetBlendStackAnimFromChooser (anim WORKER thread) pushes a transition clip and
	//      sets bPendingTransitionRMMove + the clip's remaining play length.
	//   2. THIS block (GAME thread, the next NativeUpdateAnimation) consumes the flag and
	//      queues the move with DurationMs = that remaining length, so it expires exactly
	//      when the clip ends. Early exits (transition abandoned, jump apex) are handled
	//      by the cancel/teardown blocks further down in this function.
	// See project_root_motion_mode (approach A′).
	if (bPendingTransitionRMMove)
	{
		// Consume the flag on EVERY machine — it gets set wherever the chooser runs (including
		// machines that must not queue, gated below); a stale flag would queue on a later frame.
		bPendingTransitionRMMove = false;

		// Queue only where this machine actually SIMULATES the capsule:
		//  - IsGameWorld(): BP-editor preview worlds (SCS / AnimBP viewport) tick this AnimInstance
		//    too, but their MoverComponent has no simulation behind it — queuing there trips the
		//    "null backend liaison" ensure in UMoverComponent::QueueLayeredMove.
		//  - Not ROLE_SimulatedProxy: a sim proxy's capsule is driven by the replicated transform;
		//    RM-driving it as well makes the two sources fight (jerky proxy motion). The proxy still
		//    plays the clip and extracts root motion in place, so the mesh animates correctly while
		//    replication carries the world movement.
		const UWorld* World = GetWorld();
		if (World && World->IsGameWorld() && Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			// REPLACE, don't stack: a direction reversal chains stop → turn-start with no
			// non-transition frame between them, so the teardown below never runs in the gap —
			// without this cancel, both clips' moves would be live and double-drive the capsule.
			// No-op in the normal case (no prior transition move active).
			Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);

			TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
			RMMove->DurationMs = PendingTransitionRMMoveDurationMs;
			Cached_MoverComponent->QueueLayeredMove(RMMove);
		}
	}

	const EAZ_StateMachineState PreviousSMState = ChooserContext.SMState;

	// ---- Trajectory (option A — PoseSearch FTransformTrajectory via the Mover predictor) ----
	// SINGLE source: feeds the AnimGraph PoseHistory node (MM) via the "Trajectory" property binding AND
	// the intent-based IsMoving below. Mirrors v1 Update_Trajectory. The predictor is Setup on the pawn in
	// BeginPlay. Future velocity is finite-differenced from the predicted samples (the sample struct carries
	// position/facing/time, not velocity), giving a leading indicator that catches start/stop on intent.
	if (UMoverTrajectoryPredictor* Predictor = Cached_Pawn->GetTrajectoryPredictor())
	{
		TScriptInterface<IPoseSearchTrajectoryPredictorInterface> PredictorInterface;
		PredictorInterface.SetObject(Predictor);
		PredictorInterface.SetInterface(Cast<IPoseSearchTrajectoryPredictorInterface>(Predictor));

		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
			PredictorInterface, DeltaSeconds, Trajectory, PredictionYawLast, Trajectory,
			/*HistoryInterval*/ 0.04f, /*HistoryCount*/ 10,
			/*PredictionInterval*/ 0.1f, /*PredictionCount*/ 10);

		const FVector PNow = Trajectory.GetSampleAtTime(0.f).Position;
		const FVector PFut = Trajectory.GetSampleAtTime(TrajectoryFutureLookahead).Position;
		PredictedFutureVelocity = (PFut - PNow) / FMath::Max(0.01f, TrajectoryFutureLookahead);
	}

	// ---- Mover-derived fields ----
	const FVector Velocity = Cached_MoverComponent->GetVelocity();
	ChooserContext.Speed2D = static_cast<float>(Velocity.Size2D());   // measured speed (gait thresholds, leaning)

	// Intent-based "is moving" — keyed on the player's ACTUAL movement INPUT, not predicted/measured
	// velocity. During an RM-driven stop (and the velocity-driven decel of the walk loop) the capsule is
	// still moving forward, so a velocity-based test keeps IsMoving true → the SM never leaves
	// LocomotionLoop / oscillates (the WalkFwdLoop "stuck loop" bug). Reading the last Mover input cmd makes
	// release→stop fire on intent (the frame W is released), with the RM stop clip then driving the decel.
	// (GASP: IsMoving = future velocity + accel; input intent is the cleanest accel/intent proxy and works
	// for start AND stop.) PredictedFutureVelocity is still used below for MovementDirection + MM trajectory.
	FVector MoveIntentWS = FVector::ZeroVector;
	{
		const FMoverInputCmdContext& LastInput = Cached_MoverComponent->GetLastInputCmd();
		if (const FCharacterDefaultInputs* CharIn = LastInput.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			MoveIntentWS = CharIn->GetMoveInput_WorldSpace();
		}
	}
	ChooserContext.bIsMoving = MoveIntentWS.SizeSquared2D() > FMath::Square(0.1f);   // small input deadzone

	// MovementMode from the active mode's registered name (set on the MoverComponent's MovementModes map).
	const FName ModeName = Cached_MoverComponent->GetMovementModeName();
	if (ModeName == TEXT("Walking"))
	{
		ChooserContext.MovementMode = EAZ_MovementMode::OnGround;
	}
	else if (ModeName == TEXT("Falling"))
	{
		ChooserContext.MovementMode = EAZ_MovementMode::InAir;
	}
	else if (ModeName == TEXT("RMAction"))
	{
		// RMAction is airborne; report InAir so the SM air phase + trajectory treat it like Falling.
		// Used by (a) the HYBRID JUMP rise (bUseHybridJump on the mover comp: jump enters RMAction, the Start
		// clip's RM lifts the capsule, the mode hands itself to Falling at the apex) and (b) the FUTURE
		// vault/mantle traversal (bHandOffToFallingAtApex=false there — plays to completion).
		ChooserContext.MovementMode = EAZ_MovementMode::InAir;
	}
	// Slide / Swim left as default until those modes exist.

	// ---- Hybrid jump: flag cache + apex-handoff RM teardown ----
	// bRMActionIsJumpRise: TYPED RMAction-kind read (audit pre-work) — the mode's own
	// bHandOffToFallingAtApex distinguishes a jump RISE (true: hands to Falling at apex, the SM must hold
	// the takeoff phase) from a self-contained traversal action (false: vault/mantle plays to completion
	// and must NOT be classified as a jump takeoff; it gets its own Traversal SM phase when built).
	bool bRMActionIsJumpRise = false;
	{
		const UAZ_PawnMoverComponent* AZMover = Cast<UAZ_PawnMoverComponent>(Cached_MoverComponent);
		bHybridJumpActive = AZMover && AZMover->bUseHybridJump;

		if (ModeName == TEXT("RMAction"))
		{
			if (const UAZ_PawnMovementMode_RMAction* RMMode = Cast<UAZ_PawnMovementMode_RMAction>(
					Cached_MoverComponent->FindMovementModeByName(TEXT("RMAction"))))
			{
				bRMActionIsJumpRise = RMMode->bHandOffToFallingAtApex;
			}
		}

		// At the apex, RMAction switches itself to Falling. The rise's OverrideAll root-motion move must die
		// WITH it: under Falling the engine merely downgrades it (SkipVerticalAnimRootMotion strips the Z) while
		// the HORIZONTAL would stay clip-driven and fight air control/gravity pathing. Cancel on ANY observed
		// RMAction-exit edge, not just →Falling: a low-ceiling abort or instant ground re-contact exits straight
		// to Walking, and the move (whose DurationMs is the FULL remaining clip incl. the fall tail) would keep
		// clip-driving the capsule on the ground until it expired (audit P1-15). Simulating machines only —
		// proxies never queued the move.
		if (Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy &&
			LastRawMoverModeName == TEXT("RMAction") && ModeName != TEXT("RMAction"))
		{
			Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
		}
		LastRawMoverModeName = ModeName;
	}

	// Stance — CharacterMoverComponent owns crouch state.
	ChooserContext.Stance = (Cached_CharacterMoverComponent && Cached_CharacterMoverComponent->IsCrouching())
		? EAZ_Stance::Crouching
		: EAZ_Stance::Standing;

	// Gait — read the authoritative INTENT from the Mover input cmd (the same FAZ_MoverCustomInputs.Gait
	// the walking mode uses to pick WalkSpeed/RunSpeed/SprintSpeed), NOT re-derived from measured speed.
	// Intent-based so a from-idle run start selects the Run chooser rows on frame 1 (speed is still ~0 then,
	// which would mis-read as Walk), and so a mid-locomotion walk->run flips rows immediately instead of
	// lagging until speed crosses a threshold. Mirrors how bIsMoving above reads the input cmd. The gait is
	// produced in ProduceInput from the player's Movement.* GAS tags. Falls back to Walk when absent (e.g.
	// sim proxies without bSyncInputsForSimProxy — same pending MP fix as bIsMoving).
	const FVector CurrentForward = Cached_Pawn->GetActorForwardVector();
	{
		const FMoverInputCmdContext& GaitInput = Cached_MoverComponent->GetLastInputCmd();
		if (const FAZ_MoverCustomInputs* Custom = GaitInput.InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
		{
			ChooserContext.Gait = Custom->Gait;
		}
		else
		{
			ChooserContext.Gait = EAZ_Gait::Walk;
		}
	}

	// MovementDirection — the directional clip selector. Cheap dot/sign decision for F/B/L/R.
	if (ChooserContext.bIsMoving)
	{
		// Reference frame:
		//   EXPLORE — velocity vs the BODY (orient-to-movement; the body faces where it goes).
		//   STRAFE  — the KEYBOARD INTENT vs the CAMERA. In strafe the body faces the camera, so "W = forward"
		//     is true the instant the key is pressed, regardless of where the body currently points. Classifying
		//     by velocity-vs-body instead would read a side/back clip on move-start (body still rotating to the
		//     camera, velocity lagging) and only correct once the body caught up — the "walks sideways then snaps
		//     forward" bug. Using intent-vs-camera makes the FIRST start clip correct, no align stall needed.
		//     Falls back to velocity for the STOP (input released → MoveIntentWS is zero) so the directional stop
		//     clip still matches the last travel direction.
		FVector RefForward, RefRight, Dir2D;
		if (ChooserContext.bStrafe)
		{
			const FRotator CamYaw(0.f, ChooserContext.AimingRotation.Yaw, 0.f);
			RefForward = CamYaw.Vector();
			RefRight   = FRotationMatrix(CamYaw).GetScaledAxis(EAxis::Y);
			Dir2D      = MoveIntentWS.IsNearlyZero()
				? PredictedFutureVelocity.GetSafeNormal2D()
				: MoveIntentWS.GetSafeNormal2D();
		}
		else
		{
			RefForward = CurrentForward;
			RefRight   = Cached_Pawn->GetActorRightVector();
			Dir2D      = PredictedFutureVelocity.GetSafeNormal2D();   // intent direction (future, not lagging current)
		}
		const float Forward = static_cast<float>(FVector::DotProduct(Dir2D, RefForward));
		const float Right   = static_cast<float>(FVector::DotProduct(Dir2D, RefRight));
		if (FMath::Abs(Forward) > FMath::Abs(Right))
		{
			ChooserContext.MovementDirection = Forward >= 0.f ? EAZ_MovementDirection::F : EAZ_MovementDirection::B;
		}
		else
		{
			// L/R reported without foot-lead specificity (LL / RR) until we wire the
			// foot-leading classifier — the GASP-parity 4-way split (LL/LR/RL/RR) needs
			// a foot-phase signal we don't have yet. Chooser rows for first-pass
			// locomotion can collapse LL+LR / RL+RR with the MultiEnum column.
			ChooserContext.MovementDirection = Right >= 0.f ? EAZ_MovementDirection::RR : EAZ_MovementDirection::LL;
		}
	}
	// When idle, leave MovementDirection as last computed — chooser rows for IdleLoop use Any.

	// Turn-start angle — signed yaw from current facing to the desired (world-space input) heading.
	// This is the selector for the 90/135/180 L/R turn-start clips: at the idle->moving edge the body
	// can face far from where the stick points, and the RM turn-start clip pivots it there. Recomputed
	// every moving frame here; DeriveSMState LATCHES it on the TransitionToLocomotion entry edge (the clip
	// rotates the capsule as it plays, collapsing the live angle — re-bucketing mid-turn would restart it).
	// +ve = turn right (UE yaw: +90 yaw rotates ForwardVector toward RightVector). VERIFY sign in PIE.
	if (ChooserContext.bIsMoving)
	{
		const float FacingYaw  = static_cast<float>(CurrentForward.Rotation().Yaw);
		const float DesiredYaw = static_cast<float>(MoveIntentWS.Rotation().Yaw);
		PendingStartAngleDeg   = FMath::FindDeltaAngleDegrees(FacingYaw, DesiredYaw);
	}

	// Planted-foot — read the contact_l curve from the playing clip (baked 0/1 step, threshold 0.5).
	// Same convention as v1 UAZ_AnimInstance. Drives the BoolColumn that selects L/R stop/start
	// variants; false when the current clip carries no contact curve (idle, break, jump).
	// ★ PLANTED FOOT — write ONLY while a contact curve is actually present. The old unguarded read
	// (`contact_l > 0.5`) silently became "false" on every clip WITHOUT contact curves: all jump
	// takeoffs and all land clips. So at touchdown — the one frame the CHT land rows read this — it was
	// always false, and every landing picked the _LU land regardless of which foot the jump left on.
	// Measured 2026-08-31: "WalkFwdLoop -> JumpWalkStart_RU | Lfoot=1" followed by
	// "JumpWalkStart_RU -> JumpWalk_LU_Land2Walk | Lfoot=0" on every RU jump — the wrong leg sequence.
	// The air latch below could not save it: it only holds the value while SMState is an AIR state, and
	// touchdown has already flipped the state to the land transition before this line runs.
	// Guarded, the last grounded value simply persists through clips that carry no curve (takeoff, air,
	// land), which is exactly the latch the CHT rows need — and it is the same guard the CMC anim
	// instance uses (Update_MovementDirection). Walk loops always have one foot planted; the run loop's
	// flight phase (both curves 0) also just holds the previous foot, which is correct.
	{
		const float ContactL = GetCurveValue(FName(TEXT("contact_l")));
		const float ContactR = GetCurveValue(FName(TEXT("contact_r")));
		if (ContactL > 0.5f || ContactR > 0.5f)
		{
			ChooserContext.bLeftFootDown = (ContactL >= ContactR);
		}
	}

	// ---- GAS tag snapshot from the pawn (routes through IGameplayTagAssetInterface → ASC). ----
	ChooserContext.OwnedTags.Reset();
	Cached_Pawn->GetOwnedGameplayTags(ChooserContext.OwnedTags);
	// Strafe / combat-ready — derived from the replicated Movement.Strafe tag (set on equip).
	// Gates the chooser's directional strafe rows. Replicated loose tag -> present on sim proxies too.
	ChooserContext.bStrafe = ChooserContext.OwnedTags.HasTag(FAZ_GameplayTags::Get().Movement_Strafe);
	// Upper-body fists-up combat stance — timed Combat.Ready tag (set on fist equip / refreshed on attack).
	// Drives the spine_02 layered fists-up overlay in the AnimGraph; independent of bStrafe.
	bCombatReady = ChooserContext.OwnedTags.HasTag(FAZ_GameplayTags::Get().Combat_Ready);
	// Ease the fists-up overlay weight here — Layered Blend Per Bone has no built-in alpha interp (raw BlendWeights).
	// Rises to 1 on enable, falls to 0 on disable, with separate in/out speeds. Bind CombatReadyAlpha -> BlendWeights[0].
	CombatReadyAlpha = FMath::FInterpTo(CombatReadyAlpha, bCombatReady ? 1.f : 0.f, DeltaSeconds,
		bCombatReady ? CombatReadyBlendInSpeed : CombatReadyBlendOutSpeed);

	// ---- Camera/facing — for rotation-aware chooser rows (TIP, AO chains) ----
	if (const AController* Controller = Cached_Pawn->GetController())
	{
		ChooserContext.AimingRotation = Controller->GetControlRotation();
	}
	// RotationOffset is signed delta from actor yaw to camera yaw (radians or degrees?
	// Project convention: degrees. Matches FAZ_MoverCustomInputs::RotationOffset units.)
	ChooserContext.RotationOffset = FRotator::NormalizeAxis(
		ChooserContext.AimingRotation.Yaw - Cached_Pawn->GetActorRotation().Yaw);

	// ---- Additive lean (port of AZ_AnimInstance::Update_AdditiveLean) — FORWARD ONLY ----
	// Lateral acceleration (velocity derivative, rotated into velocity-local space) -> a [-1..1] lean signal.
	// Gated to forward movement (both explore and strafe); falls smoothly to 0 otherwise. Consumed by the lean
	// BlendSpace through an Apply-Additive node in the ABP. (Lean is a continuous overlay, NOT an MM pick — the
	// lean clips are pose-tilts with no curving root motion, so MM can't select them; see project_strafe_system.)
	{
		const FVector VelAccel = (Velocity - PrevVelocity) / FMath::Max(0.0001f, DeltaSeconds);
		PrevVelocity = Velocity;
		const FRotator VelRot = Velocity.IsNearlyZero(1.0) ? FRotator::ZeroRotator : Velocity.Rotation();
		const FVector LocalAccel = VelRot.UnrotateVector(VelAccel);
		const float Divisor = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(200.f, 320.f), FVector2D(500.f, 800.f), ChooserContext.Speed2D));
		const float Lat = FMath::Clamp(static_cast<float>(LocalAccel.Y) / FMath::Max(1.f, Divisor), -1.f, 1.f);
		// Forward-only gate (both explore + strafe). bForwardLean drives BOTH the BS coordinate AND the layer
		// Alpha: the coordinate alone isn't enough — the additive node would still apply the walk-derived delta
		// at full strength over the IDLE base pose (the "takes a root anim / weird at idle" bug). Gating Alpha to
		// 0 lets the base pose pass through untouched; it ramps to 1 only while walking/running forward.
		// Only in the steady LocomotionLoop — NOT during any transition (start / stop / turn / pivot / air). Those
		// phases play an RM clip that already owns the body; the additive lean stacked on top of root motion reads
		// wrong (it tilts over the planted turn-start). Loops carry ~zero RM, so this == "no lean while RM plays".
		const bool bForwardLean = ChooserContext.bIsMoving
			&& ChooserContext.MovementDirection == EAZ_MovementDirection::F
			&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop;
		const FVector2D Target = bForwardLean ? FVector2D(Lat, 0.f) : FVector2D::ZeroVector;
		LeanAmount = FMath::Vector2DInterpTo(LeanAmount, Target, DeltaSeconds, 10.f);
		LeanAlpha  = FMath::FInterpTo(LeanAlpha, bForwardLean ? 1.f : 0.f, DeltaSeconds, 10.f);
	}

#if !UE_BUILD_SHIPPING
	if (bDebugTrajectory && GEngine)
	{
		// ★ On-screen HUD, ported from the CMC anim instance. Same information, same order, so the two
		// backends can be read side by side. The log lines ([v2 Pick]/[v2 Snap]/[v2 MMFallback]) answer
		// "what changed and when"; this answers "what is true RIGHT NOW", which is what you need while
		// actually driving the character.
		const uint64 KeyBase = reinterpret_cast<uint64>(this);

		auto EnumName = [](const TCHAR* EnumPath, int64 Value) -> FString
		{
			if (const UEnum* E = FindObject<UEnum>(nullptr, EnumPath))
			{
				return E->GetNameStringByValue(Value);
			}
			return FString::Printf(TEXT("%lld"), Value);
		};

		const UObject* CurAnim = BlendStackInputs.Anim;
		float CurLen = 0.f;
		if (const UAnimSequenceBase* CurSeq = Cast<UAnimSequenceBase>(CurAnim))
		{
			CurLen = CurSeq->GetPlayLength();
		}

		GEngine->AddOnScreenDebugMessage(KeyBase + 0, 0.f, FColor::Yellow,
			FString::Printf(TEXT("ANIM  %s"), *GetNameSafe(CurAnim)));
		GEngine->AddOnScreenDebugMessage(KeyBase + 1, 0.f, FColor::Orange,
			FString::Printf(TEXT("PICK  entry %.2f/%.2fs   cost %.1f   useMM %d   loop %d   blend %.2f"),
				static_cast<float>(BlendStackInputs.StartTime), CurLen, GLastPickCost,
				GLastPickUsedMM ? 1 : 0, BlendStackInputs.bLoop ? 1 : 0,
				static_cast<float>(BlendStackInputs.BlendTime)));
		GEngine->AddOnScreenDebugMessage(KeyBase + 2, 0.f, FColor::Green,
			FString::Printf(TEXT("state %s   gait %s   dir %s   stance %s"),
				*EnumName(TEXT("/Script/AZ.EAZ_StateMachineState"), static_cast<int64>(ChooserContext.SMState)),
				*EnumName(TEXT("/Script/AZ.EAZ_Gait"),              static_cast<int64>(ChooserContext.Gait)),
				*EnumName(TEXT("/Script/AZ.EAZ_MovementDirection"), static_cast<int64>(ChooserContext.MovementDirection)),
				*EnumName(TEXT("/Script/AZ.EAZ_Stance"),            static_cast<int64>(ChooserContext.Stance))));
		GEngine->AddOnScreenDebugMessage(KeyBase + 3, 0.f, FColor::Cyan,
			FString::Printf(TEXT("spd   %.0f   trj fut %.0f   samples %d   moving %d   justLanded %d"),
				ChooserContext.Speed2D, PredictedFutureVelocity.Size2D(), Trajectory.Samples.Num(),
				ChooserContext.bIsMoving ? 1 : 0, ChooserContext.bJustLanded ? 1 : 0));
		// Foot diagnostic: are the walk loop's contact curves reaching GetCurveValue at runtime?
		// If cL/cR stay 0.00 while walking, the BlendStack isn't propagating curves (→ bLeftFootDown
		// always False → always _LU). If they swing 0↔1, the signal works and we look elsewhere.
		GEngine->AddOnScreenDebugMessage(KeyBase + 4, 0.f, FColor::White,
			FString::Printf(TEXT("foot  contact_l %.2f  contact_r %.2f  ->  Lfoot %d    strafe %d   movingTrans %d"),
				GetCurveValue(FName(TEXT("contact_l"))), GetCurveValue(FName(TEXT("contact_r"))),
				ChooserContext.bLeftFootDown ? 1 : 0, ChooserContext.bStrafe ? 1 : 0,
				ChooserContext.bMovingTransition ? 1 : 0));
		GEngine->AddOnScreenDebugMessage(KeyBase + 5, 0.f, FColor::Magenta,
			FString::Printf(TEXT("lean  %+.2f / %+.2f   alpha %.2f   reaction %d"),
				LeanAmount.X, LeanAmount.Y, LeanAlpha, static_cast<int32>(ChooserContext.Reaction)));

		// A slot montage does NOT go through BlendStackInputs (that is the chooser/MM selection), so a
		// turn or attack playing on the slot was invisible in the ANIM line above — the clip on screen
		// and the clip named on screen were different things. Show it explicitly.
		{
			const UAnimMontage* ActiveMontage = GetCurrentActiveMontage();
			GEngine->AddOnScreenDebugMessage(KeyBase + 6, 0.f,
				ActiveMontage ? FColor::Emerald : FColor::Silver,
				ActiveMontage
					? FString::Printf(TEXT("MONTAGE %s   %.2f/%.2fs   (owns the pose)"),
						*GetNameSafe(ActiveMontage), Montage_GetPosition(ActiveMontage),
						ActiveMontage->GetPlayLength())
					: FString::Printf(TEXT("MONTAGE none   (anim above is the rendered clip)")));
		}

		// ★ [v2 CrouchTrace] — per-frame trace while CROUCHED and moving/transitioning, to localise a
		// "small shake / twitching" the user sees only in crouch (2026-08-31). Everything that can move
		// per frame is printed side by side so the oscillating one is visible in the log: capsule Z/yaw,
		// mesh world Z + relative Z (the stance modifier's re-based visual offset), capsule half-height,
		// speed, velocity-vs-facing yaw, the lean signal, and the BODY head bone vs the FACE component's
		// head bone (a Face that ticks before the body copies LAST frame's pose -> one-frame head lag,
		// most visible with the closer crouch camera). Bounded: only while crouched & moving.
		// The Face's tick prerequisites are printed once per instance (does it wait for the body?).
		// ★ [v2 CrouchEnd] — END-OF-FRAME sample of the same actor/mesh/bones. Every start-of-frame sample so
		// far is static; if the rendered transform differs (Mover/physics move it after the anim update and
		// something restores it before the next update), only an end-of-frame sample can see it.
		{
			static TMap<const UAZ_MoverAnimInstance*, FDelegateHandle> GEndFrameByInstance;
			if (!GEndFrameByInstance.Contains(this))
			{
				const TWeakObjectPtr<UAZ_MoverAnimInstance> Weak(this);
				GEndFrameByInstance.Add(this, FCoreDelegates::OnEndFrame.AddLambda([Weak]()
				{
					UAZ_MoverAnimInstance* Self = Weak.Get();
					if (!Self || !Self->GetWorld() || !Self->GetWorld()->IsGameWorld()) { return; }
					if (Self->ChooserContext.Stance != EAZ_Stance::Crouching) { return; }
					const USkeletalMeshComponent* Mesh = Self->GetOwningComponent();
					const AActor* Actor = Self->GetOwningActor();
					if (!Mesh || !Actor) { return; }
					const FVector HeadW   = Mesh->GetBoneLocation(FName(TEXT("head")));
					const FVector PelvisW = Mesh->GetBoneLocation(FName(TEXT("pelvis")));
					const FVector HandW   = Mesh->GetBoneLocation(FName(TEXT("hand_l")));
					// Mover: the SIMULATED state vs what the smoothing wrote into the mesh.
					float SimZ = -999.f; int32 SimFrame = -1; float SimMs = -1.f; float SimVelZ = 0.f;
					if (const UMoverComponent* MC = Self->Cached_MoverComponent.Get())
					{
						if (const FMoverDefaultSyncState* St = MC->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
						{
							SimZ = St->GetLocation_WorldSpace().Z; SimVelZ = St->GetVelocity_WorldSpace().Z;
						}
						SimFrame = MC->GetLastTimeStep().ServerFrame; SimMs = MC->GetLastTimeStep().BaseSimTimeMs;
					}
					UE_LOG(LogTemp, Display, TEXT("[v2 CrouchEnd] f=%llu simZ=%.3f simVelZ=%.2f simFrame=%d simMs=%.1f | actor=(%.2f,%.2f,%.2f) yaw=%.2f | mesh=(%.2f,%.2f,%.2f) relZ=%.3f yaw=%.2f | headW=(%.2f,%.2f,%.2f) pelvisW=(%.2f,%.2f,%.2f) handW=(%.2f,%.2f,%.2f)"),
						GFrameCounter, SimZ, SimVelZ, SimFrame, SimMs,
						Actor->GetActorLocation().X, Actor->GetActorLocation().Y, Actor->GetActorLocation().Z, Actor->GetActorRotation().Yaw,
						Mesh->GetComponentLocation().X, Mesh->GetComponentLocation().Y, Mesh->GetComponentLocation().Z, Mesh->GetRelativeLocation().Z, Mesh->GetComponentRotation().Yaw,
						HeadW.X, HeadW.Y, HeadW.Z, PelvisW.X, PelvisW.Y, PelvisW.Z, HandW.X, HandW.Y, HandW.Z);
				}));
			}
		}

		// ★ Per-frame crouch diagnostics ([v2 MeshMove] hook, [v2 CrouchEnd] end-of-frame sampler, [v2 CrouchTrace]).
		// They found the 2026-08-31 crouch bob (UpdateGrabMeshAnchor fighting Mover's visual-component base — see
		// feedback_mover_visual_component_two_writers) and are kept for the next one, but they cost ~3 log lines per
		// frame while crouched (p90 frame time 17 -> 38 ms), so they are OFF unless this switch is flipped.
		static constexpr bool bCrouchDiagnostics = false;
		if (bCrouchDiagnostics)
		{
		// ★ [v2 MeshMove] — WHO moves the mesh inside the frame? Measured 2026-08-31 03:25: the mesh's world Z is
		// 0.08 at the start of every frame and -4.9 +/- 0.6 at the end (394 direction flips / 2363 frames) while
		// the actor never moves. Hook the component's TransformUpdated and dump the call stack for the first
		// few moves while crouched.
		{
			static TMap<const UAZ_MoverAnimInstance*, TWeakObjectPtr<UAZ_MoverAnimInstance>> GMeshMoveHooked;   // weak: a reused address is a NEW instance
			static int32 GMeshMoveDumps = 0;
			const TWeakObjectPtr<UAZ_MoverAnimInstance>* Existing = GMeshMoveHooked.Find(this);
			if (USkeletalMeshComponent* HookMesh = GetOwningComponent(); HookMesh && (!Existing || !Existing->IsValid() || Existing->Get() != this))
			{
				GMeshMoveHooked.Add(this, TWeakObjectPtr<UAZ_MoverAnimInstance>(this));
				GMeshMoveDumps = 0;
				UE_LOG(LogTemp, Display, TEXT("[v2 MeshMove] hooked TransformUpdated on %s (bound=%d)"), *GetNameSafe(HookMesh), HookMesh->TransformUpdated.IsBound() ? 1 : 0);
				const TWeakObjectPtr<UAZ_MoverAnimInstance> Weak(this);
				HookMesh->TransformUpdated.AddLambda([Weak](USceneComponent* Comp, EUpdateTransformFlags Flags, ETeleportType Teleport)
				{
					UAZ_MoverAnimInstance* Self = Weak.Get();
					if (!Self || !Comp || Self->ChooserContext.Stance != EAZ_Stance::Crouching) { return; }
					if (GMeshMoveDumps >= 0) { return; }   // writer found (UpdateGrabMeshAnchor) — dumps disabled
					++GMeshMoveDumps;
					UE_LOG(LogTemp, Warning, TEXT("[v2 MeshMove] #%d f=%llu worldZ=%.3f relZ=%.3f flags=%d teleport=%d  <- call stack follows"),
						GMeshMoveDumps, GFrameCounter, Comp->GetComponentLocation().Z, Comp->GetRelativeLocation().Z,
						static_cast<int32>(Flags), static_cast<int32>(Teleport));
					FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
				});
			}
		}

		if (ChooserContext.Stance == EAZ_Stance::Crouching
			|| ChooserContext.SMState == EAZ_StateMachineState::TransitionStance)
		{
			static TMap<const UAZ_MoverAnimInstance*, TWeakObjectPtr<USkeletalMeshComponent>> GFaceByInstance;
			static TMap<const UAZ_MoverAnimInstance*, uint64> GAuditFrameByInstance;   // once per PIE (address reuse-safe)
			USkeletalMeshComponent* Body = GetOwningComponent();
			const AActor* Owner = GetOwningActor();
			TWeakObjectPtr<USkeletalMeshComponent>& FaceWeak = GFaceByInstance.FindOrAdd(this);
			if (!FaceWeak.IsValid() && Owner)
			{
				TArray<USkeletalMeshComponent*> Skels;
				Owner->GetComponents<USkeletalMeshComponent>(Skels);
				for (USkeletalMeshComponent* C : Skels)
				{
					if (C && C->GetFName() == FName(TEXT("Face")))
					{
						FaceWeak = C;
						break;
					}
				}
			}
			USkeletalMeshComponent* Face = FaceWeak.Get();
			const uint64 LastAudit = GAuditFrameByInstance.FindRef(this);
			if (Face && (LastAudit == 0 || GFrameCounter - LastAudit > 600))
			{
				GAuditFrameByInstance.FindOrAdd(this) = GFrameCounter;
				FString Prereqs;
				for (const FTickPrerequisite& P : Face->PrimaryComponentTick.GetPrerequisites())
				{
					Prereqs += GetNameSafe(P.PrerequisiteObject.Get()) + TEXT(",");
				}
				UE_LOG(LogTemp, Display, TEXT("[v2 CrouchTrace] Face setup: tickGroup=%d bodyTickGroup=%d prereqs=[%s] animClass=%s leader=%s attachParent=%s"),
					static_cast<int32>(Face->PrimaryComponentTick.TickGroup),
					Body ? static_cast<int32>(Body->PrimaryComponentTick.TickGroup) : -1,
					*Prereqs, *GetNameSafe(Face->GetAnimInstance() ? Face->GetAnimInstance()->GetClass() : nullptr),
					*GetNameSafe(Face->LeaderPoseComponent.Get()), *GetNameSafe(Face->GetAttachParent()));
				// Update-rate optimisation / tick-option audit for every skeletal component on the actor.
				if (Owner)
				{
					TArray<USkeletalMeshComponent*> AllSkels;
					Owner->GetComponents<USkeletalMeshComponent>(AllSkels);
					for (const USkeletalMeshComponent* C : AllSkels)
					{
						if (!C) { continue; }
						UE_LOG(LogTemp, Display, TEXT("[v2 CrouchTrace] comp %-14s URO=%d tickOpt=%d attachBound=%d renderStatic=%d leader=%s lods=%d | simPhys=%d blendPhys=%d physAsset=%s profile=%s | rel=(%.2f,%.2f,%.2f) relYaw=%.2f"),
							*C->GetName(), C->bEnableUpdateRateOptimizations ? 1 : 0,
							static_cast<int32>(C->VisibilityBasedAnimTickOption), C->bUseAttachParentBound ? 1 : 0,
							C->bRenderStatic ? 1 : 0, *GetNameSafe(C->LeaderPoseComponent.Get()), C->GetNumLODs(),
							C->IsSimulatingPhysics() ? 1 : 0, C->bBlendPhysics ? 1 : 0,
							*GetNameSafe(C->GetPhysicsAsset()), *C->GetCollisionProfileName().ToString(),
							C->GetRelativeLocation().X, C->GetRelativeLocation().Y, C->GetRelativeLocation().Z,
							C->GetRelativeRotation().Yaw);
					}
				}
			}
			const float ActorYaw = Cached_Pawn ? Cached_Pawn->GetActorRotation().Yaw : 0.f;
			const float VelYaw   = Velocity.IsNearlyZero(1.0) ? ActorYaw : Velocity.Rotation().Yaw;
			float HalfHeight = -1.f;
			if (Owner)
			{
				if (const UCapsuleComponent* Cap = Owner->FindComponentByClass<UCapsuleComponent>())
				{
					HalfHeight = Cap->GetScaledCapsuleHalfHeight();
				}
			}
			const FVector BodyHead = Body ? Body->GetBoneLocation(FName(TEXT("head"))) : FVector::ZeroVector;
			const FVector FaceHead = Face ? Face->GetBoneLocation(FName(TEXT("head"))) : FVector::ZeroVector;
			// Pelvis in ACTOR space (so walking does not swamp a per-frame wobble), camera in world space.
			FVector PelvisLocal = FVector::ZeroVector;
			if (Body && Cached_Pawn)
			{
				PelvisLocal = Cached_Pawn->GetActorTransform().InverseTransformPosition(Body->GetBoneLocation(FName(TEXT("pelvis"))));
			}
			FVector CamLoc = FVector::ZeroVector; FRotator CamRot = FRotator::ZeroRotator;
			if (const APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
			{
				CamLoc = PCM->GetCameraLocation(); CamRot = PCM->GetCameraRotation();
			}
			const FVector ActorLoc = Cached_Pawn ? Cached_Pawn->GetActorLocation() : FVector::ZeroVector;
			// LOD flicker check: predicted LOD of the body, the face and every other skeletal component on the
			// actor (garments follow the leader's LOD; a boundary straddled at the closer crouch camera would
			// alternate LODs every frame and read as the whole body twitching).
			FString Lods;
			if (Owner)
			{
				TArray<USkeletalMeshComponent*> AllSkels;
				Owner->GetComponents<USkeletalMeshComponent>(AllSkels);
				for (const USkeletalMeshComponent* C : AllSkels)
				{
					if (!C) { continue; }
					const FAnimUpdateRateParameters* URO = C->AnimUpdateRateParams;
					Lods += FString::Printf(TEXT("%s:%d%s%s "), *C->GetName().Left(6), C->GetPredictedLODLevel(),
						C->bDisableClothSimulation ? TEXT("") : TEXT("c"),
						(URO && (URO->ShouldSkipUpdate() || URO->ShouldSkipEvaluation())) ? TEXT("!skip") : TEXT(""));
				}
			}
			// Input/GAS/Mover crouch agreement per frame (user hint 2026-08-31: "check the key input, GA tags").
			const bool bGasCrouch   = ChooserContext.OwnedTags.HasTag(FAZ_GameplayTags::Get().Movement_Crouching);
			const bool bMoverCrouch = Cached_MoverComponent ? Cached_MoverComponent->IsCrouching() : false;
			// Camera rig state: TWO writers interpolate this rig every tick (UAZ_PawnCameraMovementComponent::
			// TickComponent and AAZ_PawnMoverHeroCharacter's own camera update) — if their crouch targets differ,
			// FOV/boom/socket get pulled toward two values on alternate ticks: the picture pulses, the body
			// "shakes" while every bone and transform is static (all measured 2026-08-31).
			float CamFOV = -1.f, BoomLen = -1.f; FVector Sock = FVector::ZeroVector;
			if (Owner)
			{
				if (const UCameraComponent* CamComp = Owner->FindComponentByClass<UCameraComponent>()) { CamFOV = CamComp->FieldOfView; }
				if (const USpringArmComponent* Boom = Owner->FindComponentByClass<USpringArmComponent>()) { BoomLen = Boom->TargetArmLength; Sock = Boom->SocketOffset; }
			}
			UE_LOG(LogTemp, Display, TEXT("[v2 CrouchTrace] f=%llu lods= %s| gasCrouch=%d moverCrouch=%d stance=%d | meshRel=(%.2f,%.2f) relYaw=%.2f worldYaw=%.2f | fov=%.3f boom=%.3f sock=(%.2f,%.2f,%.2f)"),
				GFrameCounter, *Lods, bGasCrouch ? 1 : 0, bMoverCrouch ? 1 : 0, static_cast<int32>(ChooserContext.Stance),
				Body ? Body->GetRelativeLocation().X : 0.f, Body ? Body->GetRelativeLocation().Y : 0.f,
				Body ? Body->GetRelativeRotation().Yaw : 0.f, Body ? Body->GetComponentRotation().Yaw : 0.f,
				CamFOV, BoomLen, Sock.X, Sock.Y, Sock.Z);
			UE_LOG(LogTemp, Display,
				TEXT("[v2 CrouchTrace] f=%llu dt=%.4f SM=%d push=%u anim=%s mont=%s | actor=(%.1f,%.1f,%.2f) yaw=%.1f | meshZ=%.2f relZ=%.2f hh=%.1f | spd=%.1f dYaw=%+.1f | pelvisL=(%.2f,%.2f,%.2f) headZ=%.2f faceDz=%+.2f | cam=(%.1f,%.1f,%.1f) pitch=%.2f yaw=%.2f camDist=%.1f"),
				GFrameCounter, DeltaSeconds, static_cast<int32>(ChooserContext.SMState),
				GPushCountByInstance.FindRef(this), *GetNameSafe(BlendStackInputs.Anim), *GetNameSafe(GetCurrentActiveMontage()),
				ActorLoc.X, ActorLoc.Y, ActorLoc.Z, ActorYaw,
				Body ? Body->GetComponentLocation().Z : 0.f, Body ? Body->GetRelativeLocation().Z : 0.f, HalfHeight,
				ChooserContext.Speed2D, FRotator::NormalizeAxis(VelYaw - ActorYaw),
				PelvisLocal.X, PelvisLocal.Y, PelvisLocal.Z, BodyHead.Z, FaceHead.Z - BodyHead.Z,
				CamLoc.X, CamLoc.Y, CamLoc.Z, CamRot.Pitch, CamRot.Yaw, FVector::Dist(CamLoc, ActorLoc));
		}
		}   // bCrouchDiagnostics
	}
#endif

	// ---- Obstacle reactions (brace / blocked) — from the pawn's forward-trace sensor. Read BEFORE the SM so a
	// reaction can HOLD the loop (otherwise turning / stick-flicker into the wall fires start/stop/turn
	// transitions that out-match the reaction row). Lazily (re)find the component; absent -> None. The Run2Wall /
	// Idle6 rows key on ChooserContext.Reaction (the Reaction enum column). See project_obstacle_reaction_system.
	if (!Cached_ObstacleSensor.IsValid() && Cached_Pawn)
	{
		Cached_ObstacleSensor = Cached_Pawn->FindComponentByClass<UAZ_ObstacleSensorComponent>();
	}
	// LATCH the reaction for the chooser: the sensor reports the flinch only for its brief trigger window, but the SM
	// holds the flinch clip until it's ~done (NotifyReactionClipPushed). Keep ChooserContext.Reaction set while the
	// sensor reports it OR the SM is still holding it — otherwise the chooser drops back to the walk/run loop mid-
	// flinch (the "plays only the trigger window then resumes loco" bug). Cleared when both are false.
	{
		const EAZ_ObstacleReaction SensorReaction = Cached_ObstacleSensor.IsValid()
			? Cached_ObstacleSensor->CurrentReaction
			: EAZ_ObstacleReaction::None;
		const UWorld* ReactWorld = GetWorld();
		const float   ReactNow   = ReactWorld ? ReactWorld->GetTimeSeconds() : 0.f;
		if (SensorReaction != EAZ_ObstacleReaction::None)
		{
			LatchedReaction = SensorReaction;   // fresh / ongoing sensor trigger
		}
		else if (!(LatchedReaction != EAZ_ObstacleReaction::None && StateMachine && StateMachine->IsReactionActive(ReactNow)))
		{
			LatchedReaction = EAZ_ObstacleReaction::None;   // sensor cleared AND the SM is no longer holding -> done
		}
		ChooserContext.Reaction = LatchedReaction;
	}

	// NOTE: "blocked" is no longer an anim concern. The pawn's movement-capability clamp zeroes the move intent
	// when you face into a wall, so bIsMoving (read from the clamped input cmd above) already goes false -> idle,
	// and a glancing hit slides -> matching loco. The sensor's Reaction now only carries cosmetic IMPACT flinches
	// (Brace / Stumble / HeadHit) for the chooser's Reaction column.

	// ---- Phase derivation (the "C++ SM") — now owned by UAZ_LocomotionStateMachine. We resolve all role /
	// mode / jump-edge awareness HERE (at the boundary) and hand it in; the SM stays a pure decision function,
	// and applies the latch lifetime-gating (StartDirection / bMovingTransition / bJustLanded) inside Tick. ----
	{
		const UWorld* SMWorld = GetWorld();
		FAZ_LocoSMInputs SMIn;
		SMIn.WorldNow             = SMWorld ? SMWorld->GetTimeSeconds() : 0.f;
		// Pure INTENT-driven (the clamped input cmd): a wall ahead already zeroed the intent upstream (capability
		// clamp), so no Reaction-gating is needed here — straight-in -> intent 0 -> SM stops -> idle; glancing ->
		// slid intent -> loco along the wall. Impact flinches ride the chooser's Reaction column, not bIsMoving.
		SMIn.bIsMoving            = ChooserContext.bIsMoving;
		// Full movement-mode enum (engine Falling/RMAction map to InAir above; Slide/Traversing become SM
		// cases when those modes land). Persistent replicated STATE, so the SM derives the whole air phase
		// identically on simulated proxies and the authority — there is no jump-press edge to read and no
		// proxy-only mirror branch (that was the old one-shot-edge RM jump, which proxies routinely missed).
		SMIn.MovementMode         = ChooserContext.MovementMode;
		// Vehicle/driver-pose pin — set by gameplay code on vehicle enter/exit; SM holds IdleLoop while true.
		SMIn.bSuppressLocomotion  = bSuppressLocomotion;
		// HYBRID JUMP: hold TransitionToInAir while the RM rise (RMAction) owns the capsule, so the
		// rising Start clip keeps driving until the apex handoff to Falling (then InAirLoop + air MM).
		// Typed: keyed off the mode's bHandOffToFallingAtApex, not the bare mode name — a vault/mantle
		// RMAction (plays to completion, never hands to Falling) must not be held as a jump takeoff.
		SMIn.bHoldTakeoffPhase    = bHybridJumpActive && bRMActionIsJumpRise;
		SMIn.PendingStartAngleDeg = PendingStartAngleDeg;
		SMIn.bStrafe              = ChooserContext.bStrafe;   // strafe: directional starts/stops, no body-turning
		SMIn.MovementDirection    = ChooserContext.MovementDirection;   // strafe forward move-start → cosmetic turn-start
		SMIn.IdleBreakMinTime     = IdleBreakMinTime;
		SMIn.IdleBreakMaxTime     = IdleBreakMaxTime;
		SMIn.Stance				  = ChooserContext.Stance;
		// IMPACT REACTION: HOLD LocomotionLoop while a one-shot impact reaction (Brace/Stumble/HeadHit) is active so
		// its CHT row — which lives on LocomotionLoop and wins because the loop rows defer via Reaction=None — plays
		// IN FULL, instead of being cut after ~2 frames when the capability clamp zeroes intent and the SM would
		// otherwise fall straight to the stop transition. The sensor is impact-only (one-shot), so this releases the
		// instant the reaction expires -> bIsMoving is already false (clamped) -> SM goes to idle. So the visible flow
		// is still "impact -> reaction clip -> idle"; LocomotionLoop is just the bucket the reaction clip lives in.
		SMIn.bObstacleReacting    = (ChooserContext.Reaction != EAZ_ObstacleReaction::None);

		// Defensive: StateMachine is created in NativeInitializeAnimation, but Live Coding re-instancing (and any
		// path that ticks an AnimInstance whose Init didn't run) can leave this Transient pointer null — calling
		// ->Tick() on null then faults inside ComputeNextState reading this->PreviousState. Lazily re-create it.
		if (!StateMachine)
		{
			StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);
		}
		const FAZ_LocoSMOutputs SMOut = StateMachine->Tick(SMIn);
		ChooserContext.SMState           = SMOut.State;
		ChooserContext.StartDirection    = SMOut.StartDirection;
		ChooserContext.bMovingTransition = SMOut.bMovingTransition;
		ChooserContext.bJustLanded       = SMOut.bJustLanded;

		// FromStance — the last SETTLED stance (pre-transition). With two stances the chooser's "from" is
		// implied by the Stance column; a third stance (prone) makes Stand2Prone vs Crouch2Prone undecidable
		// without it, so surface it now (audit scalability pre-work). During TransitionStance the SM's settled
		// stance still holds the OLD stance — exactly the "from" the chooser row wants.
		ChooserContext.FromStance = StateMachine->GetSettledStance();

		// Transition-entry token for the push lock in SetBlendStackAnimFromChooser (see header).
		if (ChooserContext.SMState != PreviousSMState)
		{
			++TransitionSerial;
		}
	}

	// ---- Foot/intent latch: hold the takeoff foot + move-intent stable through the air ----
	// bLeftFootDown is curve-driven (contact_l) and jump clips carry NO foot-contact curve, so it goes
	// false/oscillates airborne. While GROUNDED, remember the planted foot + move-intent; while AIRBORNE, hold
	// those values so the LAND rows pick the LU/RU land clip matching the takeoff foot chain at touchdown, and
	// bIsMoving distinguishes the standing land (JumpIdleLand) from Land2Walk/Land2Run. (Final 2-clip jump
	// design: the air pushes nothing — the Start clip's tail carries the fall — so the latch's only consumer
	// is land-row selection.)
	{
		const bool bAirState =
			ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
			ChooserContext.SMState == EAZ_StateMachineState::InAirLoop;
		if (!bAirState)
		{
			LastGroundedLeftFootDown = ChooserContext.bLeftFootDown;
			LastGroundedIsMoving     = ChooserContext.bIsMoving;
		}
		else
		{
			ChooserContext.bLeftFootDown = LastGroundedLeftFootDown;
			ChooserContext.bIsMoving     = LastGroundedIsMoving;
		}
	}

	// A′ RM-move teardown: whenever we leave a transition phase — whether it completed normally or was
	// abandoned early (e.g. input re-pressed mid-stop) — cancel the per-transition RM move so it can't keep
	// OverrideAll-driving the resumed loop until its DurationMs auto-expires. Our move is tagged
	// Mover.AnimRootMotion.MeshAttribute (child of Mover_AnimRootMotion); a non-exact cancel matches it.
	// No-op when no such move is active. (CancelFeaturesWithTag only touches layered moves/modifiers.)
	auto IsTransitionPhase = [](EAZ_StateMachineState S)
	{
		return S == EAZ_StateMachineState::TransitionToIdle || S == EAZ_StateMachineState::TransitionToLocomotion;
	};
	// Sim proxies never queued an RM move (gated above), so there's nothing to cancel — and they must not
	// touch their Mover (the capsule is replication-driven). Gate the teardown to the simulating machine too.
	if (Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy &&
		IsTransitionPhase(PreviousSMState) && !IsTransitionPhase(ChooserContext.SMState))
	{
		Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
	}

	// ---- Jump mode flow (HYBRID: RM rise → physics fall; toggleable) ----
	// bUseHybridJump=true (UAZ_PawnMoverComponent): jump press → Jump() queues RMAction → the takeoff Start
	// clip's root motion lifts the capsule through anticipation+rise (the RM bridge below queues the
	// OverrideAll move for TransitionToInAir) → RMAction switches to Falling at the clip apex → physics
	// descent (terrain-adaptive) → land on real floor contact. The teardown above kills the RM move on the
	// apex edge. bUseHybridJump=false: pure physics — gait-scaled impulse, Walking→Falling, no RM move
	// (the old float-then-drop fix path, kept for A/B).
}

void UAZ_MoverAnimInstance::UpdateAnimation_Cmc(float DeltaSeconds)
{
	// [SPIKE: spike/cmc-backport] P0-essentials drive for the CMC (v3) backend. Deliberately COMPACT and
	// SEPARATE from the Mover body: one owner per backend, no interleaved if/else at 30 touchpoints. What
	// this fills is exactly what the chooser/BlendStack layer consumes; what it skips (real phase machine,
	// RM transition handoff, obstacle sensor, lean, combat overlay, grab IK) is spike P1/P2 work.
	const UCharacterMovementComponent* Move = Cached_CmcCharacter->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	if (!bLoggedCmcBranch)
	{
		bLoggedCmcBranch = true;
		UE_LOG(LogTemp, Display,
			TEXT("[CmcAnim] %s: UAZ_MoverAnimInstance driving a CMC character — P0 essentials (loops + MM trajectory; transitions land in P1)"),
			*GetNameSafe(Cached_CmcCharacter));
	}

	// Trajectory — the production 5.8 for-Character generator (CMC-simulated prediction). Sampling counts
	// mirror the Mover path (10 history @ 0.04s, 15 prediction @ 0.1s) so the MM schema sees the same shape.
	FTransformTrajectory Generated;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(this, CmcTrajectoryData, DeltaSeconds,
		Trajectory, PredictionYawLast, Generated,
		/*HistorySamplingInterval*/ 0.04f, /*HistoryCount*/ 10,
		/*PredictionSamplingInterval*/ 0.1f, /*PredictionCount*/ 15);
	Trajectory = MoveTemp(Generated);

	FVector FutureVel = FVector::ZeroVector;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(Trajectory, 0.1f, 0.3f, FutureVel, false);
	PredictedFutureVelocity = FutureVel;

	// IsMoving = INTENT (live accel + predicted future velocity), never lagging measured speed — the
	// anim-debug rule; measured speed alone makes every stop arrive a beat late.
	ChooserContext.Speed2D = Move->Velocity.Size2D();
	ChooserContext.bIsMoving = Move->GetCurrentAcceleration().SizeSquared2D() > 1.f
		|| PredictedFutureVelocity.Size2D() > 40.f;

	ChooserContext.Gait          = Cached_CmcCharacter->GetCurrentGait();
	ChooserContext.Stance        = Cached_CmcCharacter->bIsCrouched ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	ChooserContext.FromStance    = ChooserContext.Stance;
	ChooserContext.MovementMode  = Move->IsFalling() ? EAZ_MovementMode::InAir : EAZ_MovementMode::OnGround;
	{
		// Same guard as the Mover path above: only write while a contact curve is present, so the
		// planted foot persists through curve-less clips (takeoff / air / land) for the CHT land rows.
		const float ContactL = GetCurveValue(FName(TEXT("contact_l")));
		const float ContactR = GetCurveValue(FName(TEXT("contact_r")));
		if (ContactL > 0.5f || ContactR > 0.5f)
		{
			ChooserContext.bLeftFootDown = (ContactL >= ContactR);
		}
	}
	ChooserContext.AimingRotation = Cached_CmcCharacter->GetBaseAimRotation();
	Cached_CmcCharacter->GetOwnedGameplayTags(ChooserContext.OwnedTags);

	// P0 phase pick: LOOPS ONLY (Idle / Locomotion / InAir), immediate edges. The real phase machine
	// (UAZ_LocomotionStateMachine — foot-aware stops, RM turn-starts, land transitions) wires up in P1;
	// keeping it out of P0 keeps this branch additive and the v2 path untouched. The chooser's loop rows
	// + MM databases give walking/running/idling today; transitions will briefly cross-fade instead of
	// playing their authored clips — a KNOWN, logged P0 limitation, not a bug to chase.
	EAZ_StateMachineState NewState;
	if (Move->IsFalling())
	{
		NewState = EAZ_StateMachineState::InAirLoop;
	}
	else
	{
		NewState = ChooserContext.bIsMoving ? EAZ_StateMachineState::LocomotionLoop : EAZ_StateMachineState::IdleLoop;
	}
	if (NewState != ChooserContext.SMState)
	{
		ChooserContext.SMState = NewState;
		++TransitionSerial;   // the push-gate keys same-state transitions off this serial
	}
}

void UAZ_MoverAnimInstance::SetBlendStackAnimFromChooser(
	bool bForceBlend,
	FAnimNodeReference BlendStackNode,
	FAZ_ChooserOutputs ChooserOut,
	UAnimationAsset* ChosenAnim,
	const TArray<UObject*>& Candidates)
{
	// ChooserContext.SMState (written by the StateMachine in NativeUpdateAnimation) is the single source
	// of truth for the SM phase — never assign it from a caller-supplied value here. The old `State`
	// parameter was removed for exactly that reason: a stale EventGraph wire (e.g. a literal IdleLoop on
	// the pin) could clobber the derived phase, making IdleBreak flash one tick then revert.
	ChooserOutputs = ChooserOut;

#if !UE_BUILD_SHIPPING
	// ★ [v2 Play] / [v2 Replay] — the RENDERED truth, read straight off the outer blend stack every tick.
	// [v2 Pick] and [v2 Snap] only see what this function PUSHES; a clip that gets re-pushed at the same
	// StartTime (so no snap), or that the blend stack restarts on its own, is invisible to both — and
	// "the leg plays from the start" survived three selection-side fixes whose logs all looked clean.
	// [v2 Replay] fires when the SAME asset's playhead moves backwards (loop wrap excluded); [v2 Play]
	// traces one line per tick while a transition phase is active so the land -> start -> loop window
	// can be read frame by frame. Bounded: transitions last ~1 s.
	{
		// Per-INSTANCE previous state. Two UAZ_MoverAnimInstance objects were alive in the same PIE
		// (measured 2026-08-31 01:21: an idle stream "AnimPro_Idle SM=0" interleaved with the hero's
		// jump stream every frame), so a single static compared consecutive calls from DIFFERENT
		// instances — the asset always differed, and a genuine same-clip restart on the hero could never
		// register as a [v2 Replay]. Keyed by `this` (weak, so a dead instance's entry is just ignored).
		struct FPlayPrev { TWeakObjectPtr<const UObject> Asset; float Time = 0.f; };
		static TMap<const UAZ_MoverAnimInstance*, FPlayPrev> GPrevPlayByInstance;
		FPlayPrev& Prev = GPrevPlayByInstance.FindOrAdd(this);
		TWeakObjectPtr<const UObject>& GPrevPlayAsset = Prev.Asset;
		float& GPrevPlayTime = Prev.Time;
		EAnimNodeReferenceConversionResult PlayConv = EAnimNodeReferenceConversionResult::Failed;
		const FBlendStackAnimNodeReference PlayRef = UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, PlayConv);
		if (PlayConv == EAnimNodeReferenceConversionResult::Succeeded)
		{
			if (const FAnimNode_BlendStack_Standalone* BS = PlayRef.GetAnimNodePtr<FAnimNode_BlendStack_Standalone>())
			{
				const UObject* PlayAsset = BS->GetAnimAsset();
				const float    PlayTime  = BS->GetAccumulatedTime();
				const bool bSameAsset = (PlayAsset != nullptr) && (PlayAsset == GPrevPlayAsset.Get());
				if (bSameAsset && (PlayTime < GPrevPlayTime - 0.05f) && !(BlendStackInputs.bLoop && GPrevPlayTime > 0.5f))
				{
					UE_LOG(LogTemp, Warning, TEXT("[v2 Replay] %s playhead went BACKWARDS %.3f -> %.3f | SM=%d justLanded=%d"),
						*GetNameSafe(PlayAsset), GPrevPlayTime, PlayTime,
						static_cast<int32>(ChooserContext.SMState), ChooserContext.bJustLanded ? 1 : 0);
				}
				const bool bTransitionPhase =
					ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle ||
					ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion ||
					ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir;
					// (crouch-idle playhead traced 2026-08-31 03:17: adv == dt every frame, rate 1.0, weight 1.0 — uniform)
				if (bTransitionPhase || !bSameAsset)
				{
					UE_LOG(LogTemp, Display, TEXT("[v2 Play] <%s/%s> %s @%.4f adv=%+.4f dt=%.4f rate=%.3f w=%.3f | SM=%d serial=%u/%u pushedAnim=%s pushedStart=%.2f justLanded=%d moving=%d"),
						*GetNameSafe(GetOwningActor()), *GetNameSafe(GetOwningComponent()),
						*GetNameSafe(PlayAsset), PlayTime, bSameAsset ? (PlayTime - GPrevPlayTime) : 0.f, GetDeltaSeconds(),
						[&]() { const FAnimNode_BlendStack* Full = PlayRef.GetAnimNodePtr<FAnimNode_BlendStack>(); return Full ? Full->WantedPlayRate : -1.f; }(),
						BS->GetCachedBlendWeight(),
						static_cast<int32>(ChooserContext.SMState),
						TransitionSerial, LastPushedTransitionSerial,
						*GetNameSafe(BlendStackInputs.Anim), static_cast<float>(BlendStackInputs.StartTime),
						ChooserContext.bJustLanded ? 1 : 0, ChooserContext.bIsMoving ? 1 : 0);
				}
				GPrevPlayAsset = PlayAsset;
				GPrevPlayTime  = PlayTime;
			}
		}
	}
#endif

	// Nothing to play: first-match mode gives ChosenAnim, return-all mode gives Candidates.
	// Bail only when both are empty.
	if (!ChosenAnim && Candidates.Num() == 0)
	{
		return;
	}

	// NOTE (2026-06-06, jump air RESOLVED): the whole in-air "replay/held" saga was NEVER a push/timing problem.
	// The ABP's Blend Stack node had a second FULL Blend Stack node inside its per-sample inner graph (instead of
	// a Blend Stack Input node): every sample spawned a fresh inner stack that played the bound Anim from its
	// literal AnimationTime=0, so the VISIBLE pose ignored every StartTime / MM SelectedTime written here, while
	// the outer node (the one this function drives) rendered to nobody. Fixed by swapping the inner node for
	// Blend Stack Input — StartTime and MM results now reach the screen. The earlier "mid-air pushes never
	// advance" theory is DISPROVEN; the InAirLoop chooser rows are viable again (direct-play from apex StartTime
	// or single-clip MM). See project_jump_system_status § TRUE ROOT CAUSE 2026-06-06.

	// Transition clip lock: once a start/stop clip is pushed, let it play to completion (DeriveSMState's
	// TransitionEndTime governs the fall-through to the target loop). Without this, the stop clip's own
	// contact_l curve would keep flipping bLeftFootDown, re-trigger the chooser, and restart the stop on
	// the other foot every few frames. The entry frame (SMState just changed from the loop) is NOT locked
	// — only subsequent frames where we're still in the same transition.
	const bool bInTransition =
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionStance;

	// Serial-keyed: each SMState CHANGE mints a new TransitionSerial (game thread); a committed push stamps
	// it. Equal serials = "this same transition entry already pushed" → locked. Comparing raw SMState here
	// let a bailed push's stale cache suppress a later same-state transition entirely (audit P0-3).
	if (bInTransition && TransitionSerial == LastPushedTransitionSerial && !bForceBlend)
	{
		return;
	}

	// Short-circuit: if the chooser's selection inputs haven't changed since the last
	// successful push, skip re-pushing to BlendStack. Without this, RandomizeColumn rows
	// (e.g. 3 idle-break variants) churn the stack every tick because the random pick
	// re-rolls per evaluation. Bypassed when:
	//   - bForceBlend is true (caller explicitly asked for a fresh blend)
	//   - bUseMM is true (motion matching intentionally re-picks each frame)
	const bool bSelectionChanged =
		ChooserContext.SMState           != LastPushedSMState ||
		ChooserContext.Stance            != LastPushedStance ||
		ChooserContext.Gait              != LastPushedGait ||
		ChooserContext.MovementDirection != LastPushedDir ||
		ChooserContext.bLeftFootDown     != LastPushedLeftFootDown ||
		ChooserContext.Reaction          != LastPushedReaction;   // impact flinch changes the row w/o SMState/Gait/Dir

	if (!bSelectionChanged && !bForceBlend && !ChooserOut.bUseMM && BlendStackInputs.Anim != nullptr)
	{
		return;
	}

	// NOTE: the LastPushed* cache is committed AFTER the push lands (next to the PendingBlendOut refresh
	// below) — every abort path between here and there must leave the cache untouched so the next frame
	// retries instead of wedging behind the transition lock (audit P0-2).

	bool bAssetLooping = false;

	// [v2 Pick]/[v2 Snap] diagnostics: capture what is playing BEFORE either publish path overwrites it.
	// Cheap (two locals), and no new members — the CMC side needed CurrentSelectedAnim/Time for this, but
	// here BlendStackInputs is the published state and is still untouched at this point.
	const UObject* PrevPickAnim = BlendStackInputs.Anim;
	const float    PrevPickTime = static_cast<float>(BlendStackInputs.StartTime);
	// Filled from FPoseSearchBlueprintResult::SearchCost on the MM branch; stays 0 on a direct-play row
	// (there is no search, so there is no cost — useMM=0 in the log says which case a line is).
	float PickCost = 0.f;
	// >= 0 when the LocomotionLoop entry below was PHASE-LOCKED to the outgoing transition clip instead of
	// taken from the search (see the seam block in the MM branch); -1 = MM owned the entry time.
	float SeamLockRemaining = -1.f;

	// Outgoing-wins blend-out: if the clip we're REPLACING authored a BlendOut (one-shot stash from its own
	// push), that clip owns this crossfade; otherwise the incoming clip's BlendTime governs (standard). The
	// stash is refreshed to THIS clip's BlendOut after the push commits, below.
	const float Crossfade = (PendingBlendOut > 0.f)
		? PendingBlendOut
		: static_cast<float>(ChooserOut.BlendTime);

	if (ChooserOut.bUseMM)
	{
		// MotionMatch over the chooser's candidate set — picks both the clip (when several are
		// supplied) and the best entry frame by current pose+trajectory cost. When the chooser
		// runs in first-match mode it hands us one anim via ChosenAnim and an empty Candidates
		// array, so fall back to searching just {ChosenAnim}. Requires a PoseHistory node tagged
		// "PoseHistory" in the AnimGraph.
		TArray<UObject*> AssetsToSearch = Candidates;
		if (AssetsToSearch.Num() == 0)
		{
			AssetsToSearch.Add(ChosenAnim);
		}

		// Strafe directional loop: search the full 8-way strafe DB so MM picks the directional clip (incl. the
		// 45/135 diagonals) by trajectory, instead of refining the entry frame within the single chooser-picked
		// clip. Keyed off CONTEXT, not the matched row — any bUseMM strafe loco row routes here. Gait-gated:
		// walk-speed set for Walk, run-speed set for Run/Sprint, so the clip speed matches the Mover's gait-
		// driven move speed (no foot-slide). Null DB → falls through to the direct single-clip MM below.
		if (ChooserContext.bStrafe && ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop)
		{
			// Crouch takes priority (gait-agnostic — one crouch speed); else gait-gated walk/run. (Strafe DBs
			// also carry the forward-lean clips, so a forward-curving strafe corner gets the lean too.)
			UPoseSearchDatabase* StrafeDB;
			if (ChooserContext.Stance == EAZ_Stance::Crouching)
				StrafeDB = StrafeCrouchDatabase.Get();
			else
				StrafeDB = (ChooserContext.Gait == EAZ_Gait::Walk) ? StrafeWalkDatabase.Get() : StrafeRunDatabase.Get();
			if (StrafeDB)
			{
				AssetsToSearch.Reset();
				AssetsToSearch.Add(StrafeDB);
			}
		}
		else if (ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop
			&& ChooserContext.Stance == EAZ_Stance::Crouching)
		{
			// ★ Orientation-mode CROUCH loop, keyed off CONTEXT like the strafe branch above. Two independent
			// failures made "Ctrl while walking" a no-op (measured 2026-08-31 03:31: 513 crouched frames rendering
			// AnimPro_WalkFwdLoop, no [v2 Pick], no [v2 MMFallback]): the chooser kept returning the STANDING loop
			// row after the stance flipped mid-walk, and even when it does return the crouch row its raw clip
			// (Crouch_WalkFwd_new) has no BranchIn, so the single-clip search is empty (R14). The crouch DB indexes
			// every crouch loop; MM picks the forward clip along the facing direction and the diagonals only when
			// the trajectory actually curves — the same behaviour the strafe branch already relies on.
			if (UPoseSearchDatabase* CrouchDB = StrafeCrouchDatabase.Get())
			{
				AssetsToSearch.Reset();
				AssetsToSearch.Add(CrouchDB);
			}
		}
		else if (ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop
			&& ChooserContext.Stance == EAZ_Stance::Standing)
		{
			// EXPLORE forward cornering lean: standing loco searches the per-gait loco DB (Fwd + LeanL/R) so MM
			// picks the lean variant when the trajectory curves. Gait-gated; crouch falls through to single-clip.
			UPoseSearchDatabase* LocoDB =
				(ChooserContext.Gait == EAZ_Gait::Walk) ? WalkLocoDatabase.Get() : RunLocoDatabase.Get();
			if (LocoDB)
			{
				AssetsToSearch.Reset();
				AssetsToSearch.Add(LocoDB);
			}
		}

		// FINAL JUMP DESIGN (2026-06-07): the air NEVER pushes — there are no InAirLoop chooser rows. The
		// jump is two clips following the walk start/stop pattern: the Start clip (RM rise drives the capsule
		// to the apex; its tail plays cosmetically under the physics fall) and the Land clip (bUseMM row —
		// the single-clip search below picks the impact ENTRY FRAME via the clip's BranchIn->PSD_v2_Jump
		// link). So this MM path serves exactly two callers: the gait-gated loco loops and the jump lands —
		// both single-clip entry-frame refinement, no continuing pose needed (loops self-stabilize, lands
		// push once on the touchdown edge). The old air-MM machinery (whole-DB search, single-clip air
		// search, continuing-pose tracking) is RETIRED — see project_jump_system_status.

		// v2 loop MM is GAIT-GATED (decision 2026-06-01): MM searches only the chooser-picked clip for the
		// CURRENT gait (WalkFwdLoop on the walk row, RunFwdLoop on the run row) and picks the best entry FRAME
		// within that one loop — it never crosses walk<->run. Rationale: gait is tag-driven (Movement.Running),
		// so a hard turn that momentarily cuts forward speed must NOT let trajectory-based cross-clip MM
		// downgrade a held Run to the walk cycle. Walk<->run changes now ride the gait tag flipping the chooser
		// row + the BlendStack cross-fade. (The old cross-clip walk+run search over LocomotionLoopDatabase is
		// intentionally disabled — re-enable here, GATED PER-GAIT, only if we want the speed-blended gear-change
		// back; the DB UPROPERTY + ABP CDO assignment are kept so the asset still cooks.)

		// ★ CONTINUITY. This call used to pass FPoseSearchContinuingProperties() — EMPTY — so every tick
		// was a fresh argmin with no idea what was already playing. Measured 2026-08-31 with [v2 Snap]:
		// the walk loop re-cut itself EVERY FRAME between its two half-cycle phases (0.67 <-> 0.93,
		// 0.07 <-> 0.67, 0.37 <-> 0.17 — always ~half a cycle apart, i.e. left stride vs right stride)
		// with all candidates within +-0.1 of each other, so the pick flipped on noise. Under
		// "slomo 0.1" that read as ~100ms wall between flips; in game time it is every 1-2 frames.
		// Without a PlayingAsset the engine has nothing to protect: the database's
		// ContinuingPoseCostBias, bDisableReselection and PoseJumpThresholdTime all key off the
		// continuing pose, so none of them could ever engage. This is the prerequisite for all three.
		// Loops ONLY (same rule the CMC spine proved): a one-shot that has finished must not get a
		// continuity bonus at LocomotionLoop, or a completed stop resurrects itself instead of handing
		// to the loop.
		// What is RENDERING right now and how far in — read ONCE from the OUTER blend stack node.
		// ★ The first version of this used UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset/Time/
		// Mirrored, which look deceptively right but take a reference to the INNER "Blend Stack Input" node
		// (FAnimNode_BlendStackInput — BlendStackAnimNodeLibrary.cpp:15-37) and return null/0 for anything
		// else. The ABP hands us the outer FAnimNode_BlendStack, so they silently returned nothing: measured
		// 2026-08-31 "cont=None" on 52/52 searches while the reference itself converted fine
		// ("nodeRefIsBlendStack=1"). The outer node exposes the same three facts via its
		// FAnimNode_AssetPlayerBase interface, reading AnimPlayers[0] — the NEWEST player
		// (AnimNode_BlendStack.cpp:871), i.e. the clip the incoming one will blend FROM.
		const UAnimationAsset* OutgoingAsset   = nullptr;
		float                  OutgoingTime    = 0.f;
		bool                   bOutgoingMirror = false;
		{
			EAnimNodeReferenceConversionResult Conv = EAnimNodeReferenceConversionResult::Failed;
			const FBlendStackAnimNodeReference BSRef = UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, Conv);
			if (Conv == EAnimNodeReferenceConversionResult::Succeeded)
			{
				if (const FAnimNode_BlendStack_Standalone* BS = BSRef.GetAnimNodePtr<FAnimNode_BlendStack_Standalone>())
				{
					OutgoingAsset   = BS->GetAnimAsset();
					OutgoingTime    = BS->GetAccumulatedTime();
					bOutgoingMirror = BS->GetMirror();
				}
			}
		}

		FPoseSearchContinuingProperties Continuing;
		if (BlendStackInputs.bLoop && BlendStackInputs.Anim && OutgoingAsset)
		{
			Continuing.PlayingAsset                = OutgoingAsset;
			Continuing.PlayingAssetAccumulatedTime = OutgoingTime;
			Continuing.bIsPlayingAssetMirrored     = bOutgoingMirror;
		}
#if !UE_BUILD_SHIPPING
		// Prove the continuity actually reached the search: a null PlayingAsset here means the node
		// reference did not resolve (or bLoop was false), and every downstream guard is inert again.
		GLastContinuingAsset = Continuing.PlayingAsset;
		GLastContinuingTime  = Continuing.PlayingAssetAccumulatedTime;
		// Which link in the chain broke: does the reference we were handed even point at a blend stack?
		{
			EAnimNodeReferenceConversionResult ConvResult = EAnimNodeReferenceConversionResult::Failed;
			UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, ConvResult);
			GLastContinuingConv = (ConvResult == EAnimNodeReferenceConversionResult::Succeeded) ? 1 : 0;
			GLastContinuingLoop = BlendStackInputs.bLoop ? 1 : 0;
		}
#endif

		FPoseSearchBlueprintResult MMResult;
		UPoseSearchLibrary::MotionMatch(
			this, AssetsToSearch, FName("PoseHistory"),
			Continuing, FPoseSearchFutureProperties(),
			MMResult);

		UAnimationAsset* MMAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
		double MMStartTime = MMResult.SelectedTime;
		PickCost = MMResult.SearchCost;

		// ★ EMPTY RESULT WHILE THE SAME LOOP IS ALREADY PLAYING = KEEP PLAYING. An empty search is what
		// the engine returns while a database's derived data is being rebuilt (every "PreCancelled because
		// of PSD_*" line in LogPoseSearch after a database save), and — by design — what a single-clip
		// pool returns once bDisableReselection forbids re-entering the continuing clip. The MM node
		// treats "nothing better" as "continue"; the fallback below instead re-pushed the loop at FRAME 0
		// every tick (measured 2026-08-31: 961 pushes of AnimPro_RunFwdLoop@0.00 in one run, cost=FLT_MAX).
		// Only fall back to frame 0 when there is genuinely nothing playing. If ANY loop is already
		// playing, keep it: during an index rebuild the fallback would otherwise push Candidates[0]
		// (the first clip of the row — e.g. the FORWARD strafe loop) at frame 0 whatever direction the
		// player is moving, which is exactly the "fight-mode locomotion is wrong" symptom on 8-way
		// strafe rows. A stale-but-correct loop beats a fresh wrong one; the search resumes normally
		// the moment the index is back.
		// ★ ...but ONLY when the playing loop is a CANDIDATE of the row that just searched. Measured
		// 2026-08-31 03:00: pressing crouch while walking switches the chooser to the crouch-walk row
		// (`Crouch_WalkFwd_new`, bUseMM, no BranchIn -> empty search, R14); the unconditional keep held the
		// STANDING walk loop for 513 crouched frames ("I don't go into crouch while walking"). A row change
		// with an empty result must still take the row's clip at frame 0 (the fallback below); only a
		// re-search of the same pool (index rebuild, single-clip reselection) may keep what is playing.
		if (!MMAnim && BlendStackInputs.bLoop && BlendStackInputs.Anim)
		{
			bool bPlayingLoopIsCandidate = AssetsToSearch.Contains(BlendStackInputs.Anim.Get());
			for (const UObject* Cand : AssetsToSearch)
			{
				if (bPlayingLoopIsCandidate) { break; }
				if (const UPoseSearchDatabase* Db = Cast<UPoseSearchDatabase>(Cand))
				{
					bPlayingLoopIsCandidate = Db->Contains(BlendStackInputs.Anim.Get());
				}
			}
			if (bPlayingLoopIsCandidate)
			{
				return;
			}
		}

		if (!MMAnim)
		{
			// MM returned nothing (e.g. LocomotionLoopDatabase not assigned yet, or an empty search result) —
			// fall back to the chooser's direct clip from frame 0 so the loop never FREEZES. Without this, a
			// bUseMM=True row with no resolvable search target would push no anim and lock the current pose.
			MMAnim = ChosenAnim ? ChosenAnim : (Candidates.Num() > 0 ? Cast<UAnimationAsset>(Candidates[0]) : nullptr);
			MMStartTime = 0.0;

#if !UE_BUILD_SHIPPING
			// ★ This fallback was SILENT, and it is a visible artifact: the clip starts at frame 0 regardless
			// of the body's current pose, so a landing pops instead of blending from the matched entry frame.
			// The usual cause is not an empty database but an unsearchable CLIP: UPoseSearchLibrary::MotionMatch
			// can only search a raw sequence THROUGH its BranchIn notify's Database, so a clip whose BranchIn
			// is missing (or names a database that does not index it) returns nothing every single time.
			// Measured 2026-08-31: the CMC foot-split work stripped BranchIn off AnimPro_JumpWalk_RU_Land2Walk
			// and AnimPro_JumpRun_RU_Land2Run, which CHT_v2 rows 34/36 still select with bUseMM=True.
			UE_LOG(LogTemp, Warning,
				TEXT("[v2 MMFallback] MotionMatch returned NOTHING for SM=%d (justLanded=%d) — falling back to "
				     "%s at frame 0. Check that clip's BranchIn notify names a database that indexes it."),
				static_cast<int32>(ChooserContext.SMState), ChooserContext.bJustLanded ? 1 : 0,
				*GetNameSafe(MMAnim));
#endif
			if (!MMAnim)
			{
				return;
			}
		}
		// MMCostLimit: when > 0, a real MM result must beat that cost or we keep the current anim (skipped on
		// the fallback path above, where there's no meaningful search cost).
		else if (ChooserOut.MMCostLimit > 0.0 && MMResult.SearchCost > ChooserOut.MMCostLimit)
		{
			return;
		}

		UPoseSearchLibrary::IsAnimationAssetLooping(MMAnim, bAssetLooping);

		// ★ PHASE-LOCKED SEAM: when a transition clip hands over to the loop, the OUTGOING CLIP owns the
		// loop's entry phase — not the search.
		// Content fact (measured 2026-08-31, pelvis-space pose distance over every loop frame): EVERY
		// non-jump start, pivot, strafe/backward/crouch start and every Land2Walk/Land2Run in this pack
		// ends within d~1-5 of its loop's FRAME 0, and at end-0.15s sits at loop phase 0.87-0.93 (walk) /
		// 0.50-0.67 (run), d~50-100 from frame 0. The SM hands a transition to LocomotionLoop
		// TransitionAlmostCompleteThreshold (0.15s) before the clip ends, and MM then picked frame 0
		// (entry=1.00/1.00, cost +9..+99) on 8/8 WalkFwdStart->WalkFwdLoop handoffs — the pose the start
		// will reach in 0.15s, not the one it is at. Under the crossfade that reads as the leg restarting
		// ("leg plays from the start after the idle jump" — it was there on every start).
		// Rule R2: a moment defined by content is not a cost contest. MM still decides WHICH loop (the
		// 8-way strafe rows need that); the entry time is (LoopLen - Remaining) so both blend streams
		// are pose-identical for the whole crossfade, and the crossfade is clamped to Remaining so the
		// blend ends exactly when the transition ends (a non-looping player past its end holds its last
		// frame — any longer blend would drift the two apart again).
		// Gate: outgoing is a non-looping sequence within (threshold + 0.10s) of its end. A stop that
		// was re-pressed mid-clip (Remaining large) keeps MM ownership; loop->loop continuity is untouched.
		float CrossfadeForPush = Crossfade;
		const bool bOutgoingIsLocoTransition =
			(GLastPushSMStateByInstance.FindRef(this) == EAZ_StateMachineState::TransitionToLocomotion);
		if (bAssetLooping && bOutgoingIsLocoTransition
			&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop && OutgoingAsset)
		{
			const UAnimSequenceBase* OutSeq  = Cast<UAnimSequenceBase>(OutgoingAsset);
			const UAnimSequenceBase* LoopSeq = Cast<UAnimSequenceBase>(MMAnim);
			bool bOutgoingLoops = true;
			if (OutSeq)
			{
				UPoseSearchLibrary::IsAnimationAssetLooping(OutSeq, bOutgoingLoops);
			}
			if (OutSeq && LoopSeq && !bOutgoingLoops)
			{
				const float RawRemaining = OutSeq->GetPlayLength() - OutgoingTime;
				const float LoopLen      = LoopSeq->GetPlayLength();
				if (RawRemaining <= TransitionAlmostCompleteThreshold + 0.10f && LoopLen > KINDA_SMALL_NUMBER)
				{
					const float Remaining = FMath::Max(0.f, RawRemaining);
					MMStartTime       = FMath::Fmod(LoopLen - Remaining, LoopLen);
					CrossfadeForPush  = FMath::Clamp(Remaining, 1.f / 30.f, Crossfade);
					SeamLockRemaining = Remaining;
				}
			}
		}

		BlendStackInputs.Anim         = MMAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = MMStartTime;
		BlendStackInputs.BlendTime    = CrossfadeForPush;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}
	else
	{
		// Non-MM path: take the chooser-supplied asset + timing/profile/tags directly.
		// Prefer the single Result pin; fall back to the first candidate if only the array is wired.
		UAnimationAsset* DirectAnim = ChosenAnim ? ChosenAnim : Cast<UAnimationAsset>(Candidates[0]);
		if (!DirectAnim)
		{
			return;
		}
		UPoseSearchLibrary::IsAnimationAssetLooping(DirectAnim, bAssetLooping);

		BlendStackInputs.Anim         = DirectAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = ChooserOut.StartTime;
		BlendStackInputs.BlendTime    = Crossfade;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}

	// ---- COMMITTED-PUSH bookkeeping: everything from here down runs only when an anim actually landed in
	// BlendStackInputs (every no-push path returned above). ----
	GLastPushSMStateByInstance.FindOrAdd(this) = ChooserContext.SMState;
	++GPushCountByInstance.FindOrAdd(this);

#if !UE_BUILD_SHIPPING
	// ★ [v2 Pick] / [v2 Snap] — the CMC instrumentation ported to the Mover spine. This is the pair that
	// diagnosed every jump bug on the CMC side, and the Mover path had nothing equivalent: its only
	// selection log is Verbose and its debug output is on-screen only, so a log capture could not show
	// which clip a landing chose or where it entered.
	//
	// [v2 Pick] fires only when the ASSET changes, so volume is a handful of lines per second.
	// entry=t/len is the field that matters most: "right clip name, wrong motion" is almost always a
	// wrong ENTRY TIME, and the name alone cannot show it.
	// bUseMM distinguishes a chooser direct-play row from an MM-searched row — on this spine that is the
	// difference between "the table decided" and "the search decided", and they fail differently.
	{
		// Carry these to the HUD, which runs a frame later on the game thread.
		GLastPickCost   = PickCost;
		GLastPickUsedMM = ChooserOut.bUseMM;

		const UObject* NewAnim = BlendStackInputs.Anim;
		const float    NewTime = static_cast<float>(BlendStackInputs.StartTime);
		float PickLen = 0.f;
		if (const UAnimSequenceBase* PickSeq = Cast<UAnimSequenceBase>(NewAnim))
		{
			PickLen = PickSeq->GetPlayLength();
		}

		if (NewAnim != PrevPickAnim)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[v2 Pick] %s -> %s | SM=%d useMM=%d cost=%+.2f | entry=%.2f/%.2f loop=%d seam=%s rem=%.2f blend=%.2f ")
				TEXT("| spd=%.0f moving=%d Lfoot=%d dir=%d gait=%d justLanded=%d"),
				*GetNameSafe(PrevPickAnim), *GetNameSafe(NewAnim),
				static_cast<int32>(ChooserContext.SMState), ChooserOut.bUseMM ? 1 : 0, PickCost,
				NewTime, PickLen, BlendStackInputs.bLoop ? 1 : 0,
				(SeamLockRemaining >= 0.f) ? TEXT("lock") : TEXT("mm"), SeamLockRemaining,
				static_cast<float>(BlendStackInputs.BlendTime),
				ChooserContext.Speed2D, ChooserContext.bIsMoving ? 1 : 0,
				ChooserContext.bLeftFootDown ? 1 : 0,
				static_cast<int32>(ChooserContext.MovementDirection),
				static_cast<int32>(ChooserContext.Gait),
				ChooserContext.bJustLanded ? 1 : 0);
		}
		// Same-asset TIME SNAPS are invisible to [v2 Pick] (it only fires on asset change). A re-push that
		// lands on a different pose inside the SAME clip restarts the blend and reads as a visible hitch —
		// this is exactly what "the land clip replays its first quarter" looked like on the CMC side.
		// Loop wrap is excluded: a looping clip legitimately jumps from near its end back to near zero.
		else if (NewAnim != nullptr)
		{
			const float Advance = NewTime - PrevPickTime;
			bool bSnap = (Advance < -0.05f) || (Advance > 0.25f);
			if (bSnap && BlendStackInputs.bLoop && PickLen > KINDA_SMALL_NUMBER
				&& PrevPickTime > 0.75f * PickLen && NewTime < 0.25f * PickLen)
			{
				bSnap = false;
			}
			if (bSnap)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[v2 Snap] %s in-clip time snap %.2f -> %.2f (len %.2f) | SM=%d useMM=%d cost=%+.2f justLanded=%d | cont=%s@%.2f"),
					*GetNameSafe(NewAnim), PrevPickTime, NewTime, PickLen,
					static_cast<int32>(ChooserContext.SMState), ChooserOut.bUseMM ? 1 : 0,
					PickCost, ChooserContext.bJustLanded ? 1 : 0,
					*GetNameSafe(GLastContinuingAsset.Get()), GLastContinuingTime);
				UE_LOG(LogTemp, Warning, TEXT("[v2 Snap]    ^ continuity chain: nodeRefIsBlendStack=%d loopAtSearch=%d"),
					GLastContinuingConv, GLastContinuingLoop);
			}
		}
	}
#endif

	// Per-push cache + transition serial stamp — gates re-pushes within the same logical context / the same
	// transition entry. Committed here, NOT before the push: an aborted push must leave them stale so the
	// next frame retries (audit P0-2/P0-3).
	LastPushedSMState          = ChooserContext.SMState;
	LastPushedStance           = ChooserContext.Stance;
	LastPushedGait             = ChooserContext.Gait;
	LastPushedDir              = ChooserContext.MovementDirection;
	LastPushedLeftFootDown     = ChooserContext.bLeftFootDown;
	LastPushedReaction         = ChooserContext.Reaction;
	LastPushedTransitionSerial = TransitionSerial;

	// Refresh the one-shot stash with THIS clip's blend-out, so the NEXT push (which replaces it) honors it.
	// Reached only on a committed push, so PendingBlendOut survives across the transition lock until the clip
	// is actually replaced.
	PendingBlendOut = static_cast<float>(ChooserOut.BlendOut);

	// Idle-break duration tracking — when an IdleBreak anim is pushed, record when DeriveSMState
	// should flip back to IdleLoop. Using world time + (anim length - threshold) lets the
	// BlendStack cross-fade begin before the break anim's last frame, hiding the cut.
	// Reads ChooserContext.SMState (not the parameter) so a misconfigured EventGraph pin
	// can't suppress break tracking.
	if (ChooserContext.SMState == EAZ_StateMachineState::IdleBreak)
	{
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			if (StateMachine)
			{
				// Remaining = Len - StartTime, same as the transition path below — a break row authored
				// with StartTime > 0 must not hold IdleBreak past the clip's actual end (audit P2-19).
				const float BreakRemaining = FMath::Max(0.05f,
					Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
				StateMachine->NotifyIdleBreakClipPushed(Now, BreakRemaining, IdleBreakAlmostCompleteThreshold);
			}
		}
	}

	// Impact-reaction (Brace/Stumble/HeadHit) tracking — the reaction clip plays in the HELD LocomotionLoop
	// (bObstacleReacting). Hand the SM the clip's real remaining length so it holds the loop until the flinch is
	// almost done, then releases to idle — the hold tracks whatever clip the CHT picked (no per-clip magic number;
	// this is why the sensor's per-reaction hold-time params were removed). Detected by an active Reaction on a
	// LocomotionLoop push (a normal loop push has Reaction == None).
	if (ChooserContext.Reaction != EAZ_ObstacleReaction::None
		&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop)
	{
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			const float Remaining = FMath::Max(0.05f,
				Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
			if (StateMachine)
			{
				StateMachine->NotifyReactionClipPushed(Now, Remaining, TransitionAlmostCompleteThreshold);
			}
		}
	}

	// Transition (start/stop) tracking + A′ RM-bridge handoff. Reached only on the transition's ENTRY
	// frame (subsequent frames hit the transition lock above and return early). (1) Schedule the
	// fall-through to the target loop, and (2) flag NativeUpdateAnimation (game thread) to queue the
	// per-transition RM move for the clip's REMAINING length (Len - StartTime — StartTime is non-zero
	// when MM picks an entry frame).
	if (bInTransition)
	{
#if !UE_BUILD_SHIPPING
		// DIAGNOSTIC: which transition phase did we enter, what did the chooser see, and what clip did it pick?
		UE_LOG(LogTemp, Verbose, TEXT("[v2 TRANS] entry: SM=%d ctx.bLeftFootDown=%d Dir=%d Gait=%d -> chosen=%s"),
			static_cast<int32>(ChooserContext.SMState), ChooserContext.bLeftFootDown ? 1 : 0,
			static_cast<int32>(ChooserContext.MovementDirection),
			static_cast<int32>(ChooserContext.Gait), *GetNameSafe(BlendStackInputs.Anim));
#endif
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			const float Remaining = FMath::Max(0.05f,
				Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
			if (StateMachine)
			{
				StateMachine->NotifyTransitionClipPushed(Now, Remaining, TransitionAlmostCompleteThreshold);
			}
			// RM bridge for ground start/stop/turn transitions — AND, under the HYBRID jump, for the takeoff
			// RISE. Hybrid (bHybridJumpActive): TransitionToInAir queues the OverrideAll RM move so the Start
			// clip's baked arc lifts the capsule through anticipation+rise; duration = the full remaining clip,
			// because the actual end is the APEX — RMAction switches itself to Falling there and the teardown in
			// NativeUpdateAnimation cancels this move on that observed edge. Landings stay physics-driven always.
			// Pure-physics mode (bHybridJumpActive=false): jumps queue nothing (impulse owns the capsule; an RM
			// move would fight gravity and re-introduce the float-then-drop divergence).
			const bool bHybridJumpRise =
				bHybridJumpActive &&
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir &&
				!ChooserContext.bJustLanded;
			
			const bool bPhysicsDrivenTransition =
				(ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
				ChooserContext.bJustLanded) && 
				!bHybridJumpRise;
			
			const bool bInPlaceStanceTransition =
				ChooserContext.SMState == EAZ_StateMachineState::TransitionStance;

			// RM-drive ONLY the strafe TURN-bucket starts (StartDirection L90/R90/L135/R135/L180/R180): the SM
			// bucketed those to the body->camera angle so the clip's baked turn already aims at the camera, and the
			// OverrideAll RM grounds the turn (planted feet) landing the body ~on camera = explore's "Root-motion
			// turn" start. The Fwd bucket has NO baked turn (small-angle forward AND every sideways/back start, which
			// force StartDirection=Fwd), so RM there would hold a stale facing the whole clip then snap after = the
			// old "double". Keep those cosmetic so the facing spring aligns from the first moving frame.
			const bool bStrafeNoTurnStart =
				ChooserContext.bStrafe
				&& ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion
				&& ChooserContext.StartDirection == EAZ_StartDirection::Fwd;

			if (!bPhysicsDrivenTransition && !bInPlaceStanceTransition && !bStrafeNoTurnStart)
			{
				PendingTransitionRMMoveDurationMs = Remaining * 1000.f;
				bPendingTransitionRMMove = true;
			}
		}
	}

	// Force a fresh blend if the caller requested it AND we're not playing the same loop
	// (re-entering the same loop should not re-blend — matches v1's pivot-failure carve-out).
	if (bForceBlend && !BlendStackInputs.bLoop)
	{
		EAnimNodeReferenceConversionResult ConvResult;
		const FBlendStackAnimNodeReference BSNode =
			UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, ConvResult);
		if (ConvResult == EAnimNodeReferenceConversionResult::Succeeded)
		{
			UBlendStackAnimNodeLibrary::ForceBlendNextUpdate(BSNode);
		}
	}
}
