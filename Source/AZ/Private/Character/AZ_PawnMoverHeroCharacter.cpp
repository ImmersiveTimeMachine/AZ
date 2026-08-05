// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverHeroCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"   // reaction parity: hero runs the same stagger-class react
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/AZ_MovementDirectionCapabilityComponent.h"
#include "Character/AZ_ObstacleSensorComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Animation/AZ_LocomotionTypes.h"   // FAZ_MoverCustomInputs, EAZ_Gait
#include "Animation/AZ_MoverAnimInstance.h"  // IsPlayingImpactReaction (lock movement during the flinch)
#include "AZ_GameplayTags.h"                 // FAZ_GameplayTags::Get()
#include "Engine/CollisionProfile.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "NetworkPredictionComponent.h"
#include "Perception/AISense_Hearing.h"      // movement-noise reports (TLOU stealth)
#include "Player/AZ_PlayerState.h"

AAZ_PawnMoverHeroCharacter::AAZ_PawnMoverHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicatingMovement(false); // Mover handles movement replication

	// Body rotation is driven by the Mover mode (always-back-to-camera target via
	// ResolveRotationTarget() — implemented in Step 3). Controller rotation never
	// drives the actor directly; the camera orbits independently of the body.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// --- Capsule (collision root) ---
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(25.f, 90.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	// Pawns must NOT affect navigation: a nav-relevant capsule carves a hole in the navmesh under the pawn,
	// which breaks every AI path query that starts/ends at a pawn ("start point not on navmesh"). Agents use
	// avoidance (RVO/Detour), not carving.
	Capsule->SetCanEverAffectNavigation(false);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetGenerateOverlapEvents(false);
	// Pawns are never floors: Mover's step-up honors this (GroundMovementUtils::CanStepUpOnHitSurface),
	// so NPCs slide along the hero's capsule instead of mounting it. Blocking is unaffected.
	Capsule->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(Capsule);

	// --- Skeletal Mesh ---
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- Camera Boom + Camera (third-person; 1p ADS hybrid added later) ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Capsule);
	CameraBoom->TargetArmLength = 220.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->CameraLagMaxDistance = 50.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 10.f;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;
	CameraBoom->ProbeChannel = ECC_Camera;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Per-mode camera framing — interpolated toward in Tick by the active rotation mode (see UpdateCameraForMode).
	// Explore mirrors the boom set up above + the engine-default FOV (90), so explore looks unchanged.
	CameraExplore.BoomLength   = 220.f;
	CameraExplore.SocketOffset = FVector(0.f, 70.f, 0.f);
	CameraExplore.FOV          = 90.f;
	CameraExplore.InterpSpeed  = 8.f;
	// Strafe (combat-ready): only the SOCKET OFFSET differs from explore by default (shifted further over the
	// shoulder) — boom length + FOV left equal so the transition is purely the offset the request asked for.
	// Tune boom/FOV in the details panel if you want strafe to also pull in / zoom.
	CameraStrafe.BoomLength    = 220.f;
	CameraStrafe.SocketOffset  = FVector(0.f, 100.f, 10.f);
	CameraStrafe.FOV           = 90.f;
	CameraStrafe.InterpSpeed   = 8.f;
	// Aiming (ADS): close + narrow. Reachable once ProduceInput sets EAZ_RotationMode::Aiming.
	CameraAiming.BoomLength    = 120.f;
	CameraAiming.SocketOffset  = FVector(0.f, 55.f, 5.f);
	CameraAiming.FOV           = 55.f;
	CameraAiming.InterpSpeed   = 8.f;
	// Grabbed: tight over-the-shoulder on the struggle (TLOU hold framing). Normal FOV — the
	// closeness IS the drama; the grab camera shake supplies the violence. Slightly slower interp
	// so the pull-in reads as a deliberate move, not a snap.
	CameraGrabbed.BoomLength   = 130.f;
	CameraGrabbed.SocketOffset = FVector(0.f, 60.f, 15.f);
	CameraGrabbed.FOV          = 85.f;
	CameraGrabbed.InterpSpeed  = 5.f;
	// Grab OUTCOME: back off and widen so the shove is actually visible. Sized off the measured shove —
	// the Chalkie travels ~114cm backwards over 1.8s — plus the hero's own 2.3s escape animation, neither
	// of which fits in a 130cm over-the-shoulder. Faster interp than the hold: the pull-back should land
	// before the payoff, not glide through it.
	CameraGrabOutcome.BoomLength   = 320.f;
	CameraGrabOutcome.SocketOffset = FVector(0.f, 40.f, 25.f);
	CameraGrabOutcome.FOV          = 90.f;
	CameraGrabOutcome.InterpSpeed  = 7.f;

	// --- Mover ---
	MoverComponent = CreateDefaultSubobject<UAZ_PawnMoverComponent>(TEXT("MoverComponent"));
	MoverComponent->SetUpdatedComponent(Capsule);
	MoverComponent->SetHandleJump(true);    // Physics jump: the engine Walking mode applies the launch impulse
	                                        // and transitions Walking -> Falling (gravity + floor-contact land),
	                                        // so the arc adapts to terrain height. Re-enforced in BeginPlay.
	MoverComponent->SetHandleStanceChanges(true);   // Engine handles crouch stance: the base OnMoverPreSimulationTick
	                                                // queues/cancels the engine FStanceModifier from bWantsToCrouch.
	                                                // The component's input bridge feeds bWantsToCrouch from the
	                                                // Movement.Crouching GAS tag. Re-enforced in BeginPlay.
	MoverComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Required for Mover replication.
	NetworkPredictionComponent = CreateDefaultSubobject<UNetworkPredictionComponent>(TEXT("NetworkPredictionComponent"));

	// --- PoseSearch trajectory predictor (Mover-native) ---
	TrajectoryPredictor = CreateDefaultSubobject<UMoverTrajectoryPredictor>(TEXT("TrajectoryPredictor"));

	// --- "Where can I move" clearance query (no tick — ProduceInput calls it to clamp the move intent) ---
	MovementCapability = CreateDefaultSubobject<UAZ_MovementDirectionCapabilityComponent>(TEXT("MovementCapability"));

	// Motion warping. The Mover component discovers this by class in BeginPlay and adapts it; nothing else
	// needs wiring. Inert until an attack montage carries a warp window and the ability registers a target.
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void AAZ_PawnMoverHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Baseline for the grab mesh-lift: read the AUTHORED offset (a BP archetype may have moved the mesh off
	// the ctor's -92), so the lift is always measured from and restored to whatever this pawn actually uses.
	if (Mesh)
	{
		DefaultMeshRelativeZ = Mesh->GetRelativeLocation().Z;
	}

	// Physics-driven jump: the engine Walking mode (bHandleJump) consumes bIsJumpJustPressed (packed by the GA
	// jump via IAZ_JumpRequester), applies the launch impulse, and transitions Walking -> Falling; the engine
	// Falling mode does gravity / air-control and plants back into Walking on real floor contact — so the jump
	// adapts to terrain height instead of a baked flat-ground RM arc. Set here as well as the ctor so a BP
	// archetype that serialized bHandleJump=false can't override it. The SM air phase follows MovementMode
	// (InAir) in UAZ_LocomotionStateMachine; the anim is cosmetic. RMAction is kept for future vault/mantle.
	if (MoverComponent)
	{
		MoverComponent->SetHandleJump(true);
		MoverComponent->SetHandleStanceChanges(true);   // archetype-proof: a BP that serialized false can't override
	}

	// NOTE: RM bridge (capsule side) deliberately NOT queued here yet. FLayeredMove_RootMotionAttribute
	// uses MixMode=OverrideAll and contributes whenever the "RootMotionDelta" mesh attribute is present
	// — which RootMotionFromEverything makes true EVERY frame, including in-place idle/walk loops (≈zero
	// delta). OverrideAll then forces capsule velocity to ~zero, so velocity-driven locomotion can't even
	// start moving (W deadlocks in idle). It must only be active for transition clips that actually carry
	// RM. Re-add in the stops/starts iteration, gated by the Mover.SkipAnimRootMotion tag during loops
	// (the layered move early-outs on that tag — see RootMotionAttributeLayeredMove.cpp:137-141).
	// See project_root_motion_mode.

	// Wire the PoseSearch trajectory predictor to the Mover component. The v2 AnimInstance generates an
	// FTransformTrajectory each tick from it (PoseSearchGenerateTransformTrajectoryWithPredictor) — the
	// single source for both motion matching (PoseHistory node) and intent-based IsMoving.
	//
	// Lazy-create if the BP CDO nulled it out: UMoverTrajectoryPredictor is UCLASS(EditInlineNew), so a
	// Blueprint subclass CDO serializes this instanced-subobject property and clobbers the C++
	// CreateDefaultSubobject default with null (confirmed — AZ_BP_PawnMoverHeroCharacter's CDO has
	// TrajectoryPredictor=None). Without this guard GetTrajectoryPredictor() returns null at runtime and
	// the AnimInstance never builds the trajectory (samples=0). Mirrors v1 AAZ_HeroPawn::BeginPlay.
	if (!TrajectoryPredictor)
	{
		TrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this, TEXT("TrajectoryPredictor_Runtime"));
	}
	if (TrajectoryPredictor)
	{
		TrajectoryPredictor->Setup(GetMoverComponent());
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayerCameraManager->ViewPitchMax = 89.f;
		PC->PlayerCameraManager->ViewPitchMin = -89.f;
	}

	// Apply default team id once. Subclasses (AI) can override DefaultTeamId in their ctor
	// or call SetGenericTeamId at spawn for runtime faction switching.
	TeamId = FGenericTeamId(DefaultTeamId);
}

void AAZ_PawnMoverHeroCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraForMode(DeltaTime);
	UpdateGrabMeshAnchor(DeltaTime);
	TryMovementCancelAttack();
}

void AAZ_PawnMoverHeroCharacter::TryMovementCancelAttack()
{
	// Cheapest test first — most frames have no movement input at all, and this runs every Tick.
	if (CachedMoveInputIntent.SizeSquared() < FMath::Square(AttackCancelInputDeadzone))
	{
		return;
	}
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Combat_CancelWindow))
	{
		return;   // no attack running, or it is still in startup/strike — commitment holds
	}
	// By ASSET tag, so this reaches whichever hand is swinging without naming either ability class.
	FGameplayTagContainer MeleeIdentity;
	MeleeIdentity.AddTag(FAZ_GameplayTags::Get().Ability_Combat_Melee);
	ASC->CancelAbilities(&MeleeIdentity);
}

void AAZ_PawnMoverHeroCharacter::SetGrabMeshAnchor(const USkeletalMeshComponent* InAnchorMesh, FName InAnchorSocket,
	FName InOwnSocket, const FVector& InAnchorSpaceOffset)
{
	GrabAnchorMesh = InAnchorMesh;
	GrabAnchorSocket = InAnchorSocket;
	GrabOwnSocket = InOwnSocket;
	GrabAnchorSpaceOffset = InAnchorSpaceOffset;
}

void AAZ_PawnMoverHeroCharacter::ClearGrabMeshAnchor()
{
	GrabAnchorMesh = nullptr;   // UpdateGrabMeshAnchor eases the mesh back to DefaultMeshRelativeZ from here
}

void AAZ_PawnMoverHeroCharacter::UpdateGrabMeshAnchor(float DeltaTime)
{
	if (!Mesh)
	{
		return;
	}

	FVector Relative = Mesh->GetRelativeLocation();
	float TargetZ = DefaultMeshRelativeZ;

	if (GrabAnchorMesh.IsValid())
	{
		// Where the holding hand is vs. where our held socket currently is. The mesh transform moves the
		// socket rigidly with it, so "shift relative Z by the gap" puts the socket exactly on the hand —
		// re-measured every frame, which makes this a converging correction rather than a one-shot guess
		// (the socket keeps moving: it rides spine_05 through the whole struggle animation).
		const FTransform AnchorXf = GrabAnchorMesh->GetSocketTransform(GrabAnchorSocket, RTS_World);
		const FVector DesiredWorld = AnchorXf.TransformPosition(GrabAnchorSpaceOffset);
		const FVector CurrentWorld = Mesh->GetSocketTransform(GrabOwnSocket, RTS_World).GetLocation();

		// Capsule has no pitch/roll, so relative Z is world Z — no basis conversion needed.
		TargetZ = FMath::Clamp(Relative.Z + (DesiredWorld.Z - CurrentWorld.Z),
			DefaultMeshRelativeZ + GrabMeshLiftMin, DefaultMeshRelativeZ + GrabMeshLiftMax);
	}
	else if (FMath::IsNearlyEqual(Relative.Z, DefaultMeshRelativeZ, 0.05f))
	{
		return;   // settled and no anchor — the common case, costs nothing
	}

	Relative.Z = FMath::FInterpTo(Relative.Z, TargetZ, DeltaTime, GrabMeshLiftSpeed);
	Mesh->SetRelativeLocation(Relative);
}

void AAZ_PawnMoverHeroCharacter::UpdateCameraForMode(float DeltaTime)
{
	// Camera is a LOCAL-viewer concern — only the controlling client looks through this boom. Skip on the server
	// and on simulated proxies (their boom drives no viewport): saves the interp + the ASC query, and keeps the
	// framing a purely cosmetic, client-local thing (co-op-safe: each client frames its own pawn).
	if (!IsLocallyControlled() || !CameraBoom || !Camera)
	{
		return;
	}

	// (SetGrabFacingTarget / ComputeGrabCameraSweepAlpha are defined below this function.)

	// Resolve the framing by rotation mode from replicated GAS state, same precedence as ProduceInput's
	// RotationMode pick: Aiming (zoom) > Strafe (combat-ready) > Explore (default).
	const FAZ_CameraStanceConfig* Target = &CameraExplore;
	bool bGrabbedFraming = false;
	if (const UAbilitySystemComponent* AbilityComp = GetAbilitySystemComponent())
	{
		const FAZ_GameplayTags& GPTags = FAZ_GameplayTags::Get();
		if (AbilityComp->HasMatchingGameplayTag(GPTags.State_Grabbed))
		{
			// Caught: nothing outranks the grab framing. Which grab framing depends on the phase — the
			// hold is shot tight, the outcome is shot wide (the payoff moves ~114cm and does not fit in
			// a 130cm boom). Look-at and the sweep are unaffected; only boom/offset/FOV/interp swap.
			Target = bGrabOutcomeFraming ? &CameraGrabOutcome : &CameraGrabbed;
			bGrabbedFraming = true;
		}
		else if (AbilityComp->HasMatchingGameplayTag(GPTags.Ability_State_Aiming))
		{
			Target = &CameraAiming;
		}
		else if (AbilityComp->HasMatchingGameplayTag(GPTags.Movement_Strafe))
		{
			Target = &CameraStrafe;
		}
	}

	// Grabbed: the CAMERA also turns to frame the grabber (user rule 2026-07-24) — look input is
	// frozen (OnLookTriggered gate), so control rotation is ours to drive. Same interp knob as the
	// boom pull-in, so one editor value tunes the whole grabbed-camera move.
	if (bGrabbedFraming && GrabFacingTarget.IsValid())
	{
		if (AController* PawnController = GetController())
		{
			// Once the grabber has closed onto us (paired clips put both capsules at one transform) the
			// separation vector is degenerate and .Rotation() would snap the camera to garbage. Frame the
			// clinch along the grabber's facing in that case — the yaw/pitch offsets below still compose it.
			FVector ToGrabber = GrabFacingTarget->GetActorLocation() - GetActorLocation();
			if (ToGrabber.IsNearlyZero(1.f))
			{
				ToGrabber = GrabFacingTarget->GetActorForwardVector();
			}
			if (!ToGrabber.IsNearlyZero())
			{
				FRotator LookAt = ToGrabber.Rotation();
				// CINEMATIC SWEEP: arc the yaw across the hold instead of parking at one offset. The
				// easing is the point — a constant-rate orbit reads mechanical, while ease-in-out gives
				// the move weight (caught, then dragged around, then settling) for the price of one curve.
				// CINEMATIC SWEEP: the yaw travels between the two offsets, shaped entirely by
				// GrabbedCameraSweepCurve. An oscillating curve gives left/right lurching; packing its
				// keys tighter toward the end makes those lurches accelerate.
				LookAt.Yaw += FMath::Lerp(GrabbedCameraYawOffsetDeg, GrabbedCameraYawEndDeg, ComputeGrabCameraSweepAlpha());
				LookAt.Pitch += GrabbedCameraPitchOffsetDeg;  // tilt onto the struggle
				PawnController->SetControlRotation(
					FMath::RInterpTo(PawnController->GetControlRotation(), LookAt, DeltaTime, GrabbedCameraRotationSpeed));
			}
		}
	}
	else if (bRestoringGrabCamera)
	{
		// RELEASE: glide back to the view the player had before the catch, so the fight doesn't leave
		// them facing a wall. Eased from a FIXED start rotation (see GrabCameraRestoreFrom) and abandoned
		// the moment they touch the stick — see OnLookTriggered.
		AController* PawnController = GetController();
		const UWorld* CameraWorld = GetWorld();
		if (!PawnController || !CameraWorld)
		{
			bRestoringGrabCamera = false;
		}
		else
		{
			const float Alpha = (GrabbedCameraRestoreSeconds > KINDA_SMALL_NUMBER)
				? FMath::Clamp(static_cast<float>(CameraWorld->GetTimeSeconds() - GrabCameraRestoreStartTime)
					/ GrabbedCameraRestoreSeconds, 0.f, 1.f)
				: 1.f;
			// Ease OUT only: the return decelerates into place. Easing in as well would read as a second
			// deliberate camera move, when this should feel like control simply coming back.
			const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
			// Slerp, not FRotator lerp: component-wise interpolation takes the long way round whenever the
			// sweep crossed the +/-180 seam, which is exactly when a big grab arc ends.
			PawnController->SetControlRotation(
				FQuat::Slerp(GrabCameraRestoreFrom.Quaternion(), PreGrabControlRotation.Quaternion(), Eased).Rotator());
			if (Alpha >= 1.f)
			{
				bRestoringGrabCamera = false;
			}
		}
	}

	// Critically-damped-ish glide toward the mode's framing (the "transition"). Composes fine with the boom's
	// own camera lag — that smooths the camera following the boom; this moves the boom's target offset/length.
	const float Speed = Target->InterpSpeed;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Target->BoomLength, DeltaTime, Speed);
	CameraBoom->SocketOffset    = FMath::VInterpTo(CameraBoom->SocketOffset, Target->SocketOffset, DeltaTime, Speed);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Target->FOV, DeltaTime, Speed));
}

void AAZ_PawnMoverHeroCharacter::SetGrabFacingTarget(const AActor* Target)
{
	const bool bWasGrabbed = GrabFacingTarget.IsValid();
	GrabFacingTarget = Target;

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (Target && !bWasGrabbed)
	{
		// CAUGHT: remember the view the player chose, so we can hand it back afterwards, and start the arc.
		if (const AController* PawnController = GetController())
		{
			PreGrabControlRotation = PawnController->GetControlRotation();
		}
		GrabCameraSweepStartTime = Now;
		// A fresh grab outranks a restore still gliding from the previous one.
		bRestoringGrabCamera = false;
	}
	else if (!Target && bWasGrabbed)
	{
		// RELEASED: glide back from wherever the struggle left us. Stamping the start here (rather than
		// reading it per-frame) is what lets the restore follow an actual easing curve.
		if (const AController* PawnController = GetController())
		{
			GrabCameraRestoreFrom = PawnController->GetControlRotation();
			bRestoringGrabCamera = (GrabbedCameraRestoreSeconds > KINDA_SMALL_NUMBER);
		}
		GrabCameraRestoreStartTime = Now;
	}
}

float AAZ_PawnMoverHeroCharacter::ComputeGrabCameraSweepAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World || GrabbedCameraSweepSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	const float Raw = FMath::Clamp(
		static_cast<float>(World->GetTimeSeconds() - GrabCameraSweepStartTime) / GrabbedCameraSweepSeconds, 0.f, 1.f);

	// An authored curve wins: it can express a hitch or an overshoot that no single exponent can.
	// LoadSynchronous is cheap once resolved (it returns the cached pointer), and the soft ref keeps the
	// curve out of the pawn's hard reference set.
	if (!GrabbedCameraSweepCurve.IsNull())
	{
		if (const UCurveFloat* Curve = GrabbedCameraSweepCurve.LoadSynchronous())
		{
			return Curve->GetFloatValue(Raw);
		}
	}
	// Fallback: ease in AND out. Leaning into the move and settling out of it is what separates a
	// deliberate camera move from a turntable; the exponent decides how long it hangs before committing.
	return FMath::InterpEaseInOut(0.f, 1.f, Raw, GrabbedCameraSweepExponent);
}

// ========================================
// Possession + GAS init
// ========================================
//
// Two entry points because GAS ActorInfo must be initialized on both ends:
//   - Server: PossessedBy (also grants StartupAbilities — server is authority).
//   - Client: OnRep_PlayerState (granting is server-only; client just inits ActorInfo).
// Pawn-switch (entering a vehicle) re-fires PossessedBy on the new pawn — ActorInfo
// rebinds the AvatarActor to the new pawn while OwnerActor (PlayerState) stays the
// same, so player abilities continue to function but target the new avatar.

void AAZ_PawnMoverHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void AAZ_PawnMoverHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

void AAZ_PawnMoverHeroCharacter::InitAbilitySystem()
{
	AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	if (!PS)
	{
		// On clients, PlayerState replicates in a few frames after possess —
		// OnRep_PlayerState fires when it arrives and we retry.
		return;
	}

	UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	// Rebinds ActorInfo on every (re)possession. Safe to call repeatedly — GAS
	// resets actor refs and re-binds delegates. Required after pawn-switch so
	// the player ASC targets the new avatar.
	ASC->InitAbilityActorInfo(PS, this);

	// Server-only granting. GrantAbilitiesWithInputTag has an idempotency guard
	// (bStartupAbilitiesGiven) — re-possession or vehicle exit returns is a no-op.
	if (HasAuthority())
	{
		ASC->GrantAbilitiesWithInputTag(StartupAbilities);

		// Grant (idempotent by spec lookup): the grab-victim ability, EDITOR-ASSIGNED (user rule
		// 2026-07-24: no hardcoded asset paths in C++ — the hero pawn BP points GrabbedAbilityClass at
		// BP_GA_PlayerGrabbed; unset = native fallback; CDO patch goes to the ASSIGNED class).
		// Event-triggered by Event.Grabbed; the Interact dynamic tag routes E-presses to it WHILE
		// ACTIVE (the mash) — AbilityInputTagPressed only invokes InputPressed on active specs.
		UClass* GrabbedClass = *GrabbedAbilityClass ? *GrabbedAbilityClass : UAZ_GA_PlayerGrabbed::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(GrabbedClass))
		{
			UAZ_GA_PlayerGrabbed::ConfigureCDO(GrabbedClass);
			FGameplayAbilitySpec GrabbedSpec(GrabbedClass, 1, INDEX_NONE, this);
			GrabbedSpec.GetDynamicSpecSourceTags().AddTag(FAZ_GameplayTags::Get().Input_Action_Interact);
			ASC->GiveAbility(GrabbedSpec);
			UE_LOG(LogTemp, Display, TEXT("[Grab] %s granted to hero ASC"), *GrabbedClass->GetName());
		}

		// Reaction parity (same grant pattern): Event.Combat.HitReact now reaches the hero from the
		// vitals set, and this is the ability that answers it. ConfigureOnCDO goes to the ASSIGNED class
		// — a BP child's CDO does not inherit runtime patches made to the native one.
		UClass* HitReactClass = *HitReactAbilityClass ? *HitReactAbilityClass : UAZ_GA_HitReact::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(HitReactClass))
		{
			UAZ_GA_HitReact::ConfigureOnCDO(HitReactClass);
			ASC->GiveAbility(FGameplayAbilitySpec(HitReactClass, 1, INDEX_NONE, this));
			UE_LOG(LogTemp, Display, TEXT("[HitReact] %s granted to hero ASC"), *HitReactClass->GetName());
		}
	}
}

// ========================================
// Enhanced Input
// ========================================
//
// Pawn-side binding per the v2 multi-pawn design: each pawn class owns its input
// surface. The PC stays pawn-agnostic — on possess it queries GetDefaultMappingContext()
// and pushes it into the local player's EnhancedInput subsystem; on unpossess it pops.
//
// Handlers cache to member fields; the Mover-side ProduceInput_Implementation reads
// those fields and writes the deterministic InputCmd consumed by Mover modes. Caching
// keeps input on the game thread and avoids touching FCharacterDefaultInputs from
// callbacks (Mover wants a single producer point per sim tick).

void AAZ_PawnMoverHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	// BP-defaults sanity checks. Silent skip → "input doesn't work" with no error.
	// `ensureMsgf` warns loudly in dev (red log) without crashing; ship builds still run.
	ensureMsgf(DefaultMappingContext,
		TEXT("%s: DefaultMappingContext is null. Set it in the pawn BP defaults — without it, "
		     "the PC has no per-pawn IMC to push on possess and no IAs will trigger."),
		*GetName());
	ensureMsgf(MoveInputAction,
		TEXT("%s: MoveInputAction is null. Set it in the pawn BP defaults — movement won't work."),
		*GetName());
	ensureMsgf(LookInputAction,
		TEXT("%s: LookInputAction is null. Set it in the pawn BP defaults — look won't work."),
		*GetName());

	if (MoveInputAction)
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_PawnMoverHeroCharacter::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AAZ_PawnMoverHeroCharacter::OnMoveCompleted);
	}
	if (LookInputAction)
	{
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_PawnMoverHeroCharacter::OnLookTriggered);
	}
	// Jump is GAS-routed via UAZ_GA_PawnJump (InputTag Input.Action.Jump) — not
	// bound here. The PC's InputConfig DA maps the Jump IA to the input tag,
	// the GA fires, the GA calls IAZ_JumpRequester::SetJumpPressed on this pawn.
}

void AAZ_PawnMoverHeroCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	// IA_Move axis convention: X = Right/Left (A/D), Y = Forward/Back (W/S).
	// CachedMoveInputIntent uses pawn-local convention: X = Forward, Y = Right.
	// The Mover input producer rotates this by ControlRotation.Yaw to get world-space.
	const FVector2D Input2D = Value.Get<FVector2D>();
	CachedMoveInputIntent.X = FMath::Clamp(Input2D.Y, -1.f, 1.f);
	CachedMoveInputIntent.Y = FMath::Clamp(Input2D.X, -1.f, 1.f);
	CachedMoveInputIntent.Z = 0.f;
}

void AAZ_PawnMoverHeroCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void AAZ_PawnMoverHeroCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	// Grabbed = camera locked too (TLOU-style hold). The IMC stays pushed — removing it would also
	// kill the E-mash IA — so the freeze lives here, not in the input stack.
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
		{
			return;
		}
	}

	// The post-grab restore is a courtesy, not a cutscene: the first frame the player moves the camera,
	// it stops fighting them. Without this the glide keeps dragging the view back for its full duration
	// and reads as broken input rather than a considerate return.
	bRestoringGrabCamera = false;

	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X * LookRateYaw);
	AddControllerPitchInput(-LookVector.Y * LookRatePitch);
}

// ========================================
// IAZ_JumpRequester
// ========================================

void AAZ_PawnMoverHeroCharacter::SetJumpPressed(bool bPressed)
{
	// One-shot edge-detect: bIsJumpJustPressed = press-this-tick, consumed ONLY by
	// ProduceInput (both the normal path and the no-PC early-return consume it).
	// Release deliberately does NOT clear the edge — a press+release inside one game
	// frame (fast tap / low FPS) must still deliver its one-shot to the next sim tick;
	// clearing on release silently dropped those jumps (audit P2-18).
	if (bPressed && !bIsJumpPressed)
	{
		bIsJumpJustPressed = true;
	}
	bIsJumpPressed = bPressed;
}

// ========================================
// IAbilitySystemInterface
// ========================================

UAbilitySystemComponent* AAZ_PawnMoverHeroCharacter::GetAbilitySystemComponent() const
{
	// Player ASC lives on PlayerState (cross-pawn persistence — survives possession
	// changes / vehicle entry / respawn). Direct query each call; no cache. If this
	// becomes a measured hot path, cache in PossessedBy/OnRep_PlayerState.
	if (const AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

// ========================================
// IMoverInputProducerInterface
// ========================================

void AAZ_PawnMoverHeroCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// AI-audible movement noise: piggybacks on the per-frame producer (throttled + authority-gated inside).
	// Not part of the InputCmd — purely a world-side stimulus report.
	ReportMovementNoise();

	// Mover input producer. Single point per sim tick that converts cached game-thread
	// input state into the deterministic InputCmd Mover modes consume. NetworkPrediction
	// replays this on the server with the same inputs, so all input must flow through
	// here (never directly poke FCharacterDefaultInputs from a callback).
	//
	// Only FCharacterDefaultInputs is shipped — the v2 design routes all non-physics
	// intent (crouch, sprint, aim, weapon swap, etc.) through GAS tags on the player
	// ASC, which GAS predicts+replicates. Mover modes read those tags via OwnedTagsRef.
	// Gait / MovementDirection / RotationOffset are derived later in the pipeline
	// (AnimInstance / chooser / Mover mode's ResolveRotationTarget), not pre-computed
	// here — keeps the Mover InputCmd small and the producer free of derived state.
	FCharacterDefaultInputs& CharacterDefaultInputs =
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		// AI / unpossessed: zero the defaults AND the custom inputs, and consume the jump
		// one-shot — a press just before unpossession must not stay latched and fire on the
		// next possession, and a recycled cmd context must not carry stale gait/crouch
		// (audit P1 §ProduceInput no-PC path).
		CharacterDefaultInputs = FCharacterDefaultInputs();
		FAZ_MoverCustomInputs& NoPCCustom = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();
		NoPCCustom = FAZ_MoverCustomInputs();
		bIsJumpJustPressed = false;
		return;
	}

	const FRotator ControlRot = PC->GetControlRotation();

	// World-space move = camera-yaw-relative WASD. Pitch/roll discarded — character
	// moves on the ground plane regardless of where the camera tilts.
	const FRotator YawOnly(0.f, ControlRot.Yaw, 0.f);
	FVector WorldMove = FRotationMatrix(YawOnly).TransformVector(CachedMoveInputIntent);

	// Cache the RAW (pre-clamp) world intent — the obstacle sensor reads THIS (not the clamped cmd below) so a
	// straight-in wall hit still registers after the clamp zeroes the shipped input.
	CachedWorldMoveIntentRaw = WorldMove;

	// "Where can I move" — clamp the move INTENT to the free direction using Mover's own slide math (reuse, don't
	// reinvent): straight-in -> ~zero -> idle; angled -> tangent -> slide-with-matching-loco. Stays INTENT-driven
	// (predictive) — both Mover and the AnimInstance consume this single clamped cmd (no velocity read). Runs in
	// ProduceInput so NetworkPrediction replays it identically -> co-op-safe.
	if (MovementCapability)
	{
		WorldMove = MovementCapability->ConstrainIntent(WorldMove);
	}

	// LOCK movement when (a) an impact flinch is playing — it's a transition, don't slide/run through it; OR (b)
	// we're pushing directly into an obstacle the CLAMP couldn't catch. (b) makes a SLOW approach (no flinch) stop
	// the SAME as a fast one: the clamp already stops you at walls (closer), but its capsule CLEARS an overhead beam
	// — the sensor sees that, so we stop here too. Without this, slow == walk under; fast == stop (the flinch-lock
	// masked the clamp's overhead gap). SP-first: game-thread reads in the deterministic producer, same caveat as
	// the GAS-tag reads below — co-op would need these replicated.
	bool bLockMove = false;
	if (Mesh)
	{
		if (const UAZ_MoverAnimInstance* AnimInst = Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()))
		{
			bLockMove = AnimInst->IsPlayingImpactReaction();   // (a) flinch transition
		}
	}
	if (!bLockMove && MovementCapability && !MovementCapability->bBlockedThisQuery
		&& !CachedWorldMoveIntentRaw.IsNearlyZero())
	{
		// (b) clamp caught nothing this frame, but the sensor sees an obstacle dead ahead (e.g. an overhead beam
		// above the capsule). If we're pushing INTO it (within ~60deg of head-on) and it's within trigger range,
		// stop — so we don't walk under it on a slow approach. Walls the clamp DID catch (bBlockedThisQuery) are
		// already handled (closer stop) and skipped here.
		if (const UAZ_ObstacleSensorComponent* Sensor = FindComponentByClass<UAZ_ObstacleSensorComponent>())
		{
			FVector N = Sensor->ObstacleNormal;
			N.Z = 0.f;
			if (Sensor->bObstacleAhead && Sensor->ObstacleDistance <= Sensor->ImpactTriggerDistance
				&& N.Normalize()
				&& FVector::DotProduct(CachedWorldMoveIntentRaw.GetSafeNormal(), -N) > 0.5f)
			{
				bLockMove = true;
			}
		}
	}
	if (bLockMove)
	{
		WorldMove = FVector::ZeroVector;
	}

	// Where the camera is pointing this sim tick. Mover stores it on the
	// SyncState so modes / animation can read "look direction" deterministically
	// (don't query PlayerCameraManager from inside a mode — it isn't replayed).
	CharacterDefaultInputs.ControlRotation = ControlRot;
	
	// Read the movement-domain GAS state once, here on the game thread (the Mover sim / replay
	// can't query the ASC). Attack owns movement: drop locomotion intent mid-melee so the capsule
	// doesn't glide under the full-body punch. Strafe (combat-ready, set on equip of a strafe
	// profile): the body must face the camera/target, NOT the movement direction.
	bool bStrafe = false;
	bool bGrabbed = false;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())   // we implement IAbilitySystemInterface
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		bGrabbed = ASC->HasMatchingGameplayTag(AZTags.State_Grabbed);   // caught: fully rooted until the mash resolves
		if (ASC->HasMatchingGameplayTag(AZTags.Ability_State_MeleeAttacking) || bGrabbed)
			WorldMove = FVector::ZeroVector;
		bStrafe = ASC->HasMatchingGameplayTag(AZTags.Movement_Strafe);
	}

	// (Obstacle blocking is now handled upstream by the movement-capability clamp on WorldMove above — a wall ahead
	// zeroes / slides the intent, so the pawn idles or strafes-along the wall without a special "blocked -> strafe"
	// override here. The forward sensor is purely cosmetic impact flinches now.)

	// The move command itself. DirectionalIntent = "a unit-ish vector pointing
	// where I want to go" (vs. Velocity = "an exact velocity vector"). World-space
	// because bUsingMovementBase is false; if true, this would be base-relative.
	CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove);

	// Where the body should face. Mover modes use this as the rotation target unless
	// a mode overrides via ResolveRotationTarget().
	//   Explore (orient-to-movement): face where I move.
	//   Strafe (combat-ready): the body does NOT rotate at idle — it HOLDS its facing. When you start moving it
	//     smoothly rotates toward the camera (the walking-mode spring at StrafeFacingTime) AND moves at the same
	//     time. The align is LATCHED so a brief tap still completes the turn (no part-way freeze / multi-tap
	//     creep). NOTE: the strafe START transition is excluded from the RM move (see AZ_MoverAnimInstance) so the
	//     spring aligns from the FIRST moving frame instead of waiting out the start clip (that wait was the
	//     "double": start-clip motion, then realign). WASD = directional side-steps / back-pedal vs the camera.
	if (bStrafe)
	{
		constexpr float AlignExitDeg = 5.f;   // "aligned" once within this of the target → may hold again
		const bool bHasMoveInput = !CachedMoveInputIntent.IsNearlyZero();
		const float CamYaw = static_cast<float>(ControlRot.Yaw);
		if (bHasMoveInput)
		{
			bStrafeAligning = true;        // moving (even a tap) → align to + track the LIVE camera
			StrafeAlignTargetYaw = CamYaw; // keep the latch's frozen target current while the move is held
		}
		else if (bStrafeAligning)
		{
			// Input released mid-align: finish turning to the camera yaw CAPTURED at release (frozen), NOT the live
			// camera — so swinging the camera after you stop does not drag the idle body. Clear once we arrive.
			const float TargetBodyDelta = FMath::Abs(FRotator::NormalizeAxis(
				StrafeAlignTargetYaw - static_cast<float>(GetActorRotation().Yaw)));
			if (TargetBodyDelta <= AlignExitDeg) { bStrafeAligning = false; }
		}

		FVector OrientTarget;
		if (bHasMoveInput)
			OrientTarget = YawOnly.Vector();                                              // track the live camera while moving
		else if (bStrafeAligning)
			OrientTarget = FRotator(0.f, StrafeAlignTargetYaw, 0.f).Vector();             // finish to the FROZEN target
		else
			OrientTarget = FRotator(0.f, static_cast<float>(GetActorRotation().Yaw), 0.f).Vector();  // idle: HOLD facing
		CharacterDefaultInputs.OrientationIntent = OrientTarget;
	}
	else
	{
		bStrafeAligning = false;
		CharacterDefaultInputs.OrientationIntent = WorldMove;
	}

	// GRABBED: face the grabber — the struggle must read face-to-face, never back-bitten (user rule
	// 2026-07-24). Overrides BOTH modes' facing pick above; movement is already zeroed, so the walking
	// mode's rotation spring just turns the rooted body toward the target (same machinery the strafe
	// align-latch uses on an idle body).
	if (bGrabbed && GrabFacingTarget.IsValid())
	{
		// FACE the grabber. (Tried matching its yaw instead, on the theory that shared-origin paired
		// clips want both actors on one transform: wrong. Same yaw puts the two bodies side by side
		// along their shared right-axis — "in the same line" — instead of mirrored. The clips' baked
		// offsets separate the bodies; the ACTORS still have to oppose each other.)
		const FVector ToGrabber = (GrabFacingTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!ToGrabber.IsNearlyZero())
		{
			CharacterDefaultInputs.OrientationIntent = ToGrabber;
		}
	}

	// ---- Gait → Mover (v2: movement-domain GAS tags bridge to the Mover sim) ----
	// The walking mode's ResolveGait reads FAZ_MoverCustomInputs.Gait and maps it to
	// WalkSpeed/RunSpeed/SprintSpeed (+ accel, turn, facing). We translate the player's
	// MOVEMENT-domain tags into the gait HERE — on the game thread, where the ASC is
	// reachable — and ship the resolved enum in the deterministic InputCmd, because a Mover
	// mode runs in the prediction replay and cannot query the ASC. Tags are granted by
	// movement abilities (BP_GA_Run → Movement.Running via ActivationOwnedTags). We read
	// Movement.* (domain state), NOT Ability.State.* (which is ability-to-ability coordination).
	FAZ_MoverCustomInputs& CustomInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		if (HasMatchingGameplayTag(AZTags.Movement_Sprinting))
		{
			CustomInputs.Gait = EAZ_Gait::Sprint;
		}
		else if (HasMatchingGameplayTag(AZTags.Movement_Running))
		{
			CustomInputs.Gait = EAZ_Gait::Run;
		}
		else 
		{
			CustomInputs.Gait = EAZ_Gait::Walk;
		}
		
		CustomInputs.bWantsToCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
		// Strafe (combat-ready) → the walking mode tracks the camera TIGHTLY (aim-lock) instead of the
		// explore lag-then-snap. Reuses the existing (replicated/reconciled) RotationMode field; same
		// Movement.Strafe source as OrientationIntent above. GenerateWalkMove reads it.
		CustomInputs.RotationMode = bStrafe ? EAZ_RotationMode::Strafe : EAZ_RotationMode::OrientToMovement;
	}

	// Jump is EXPLORE-ONLY for now — suppress jump input while strafing (combat-ready). The one-shot 0->1
	// edge is still consumed at the bottom of this function, so a press during strafe can't latch and fire
	// the moment you leave strafe. (Strafe jump = a later decision: physics jump vs none.)
	const bool bJumpAllowed = !bStrafe;

	// "Held" jump flag — true the whole time the player is holding Space, false
	// on release. Mover's falling-mode air-control / coyote-time reads this each tick.
	CharacterDefaultInputs.bIsJumpPressed = bJumpAllowed && bIsJumpPressed;

	// One-shot edge — true ONLY on the sim tick where press transitioned 0→1. Used
	// by the walking mode to fire the initial jump impulse exactly once. Consumed
	// at the bottom of this function (we set bIsJumpJustPressed = false there).
	CharacterDefaultInputs.bIsJumpJustPressed = bJumpAllowed && bIsJumpJustPressed;

	// Optional movement-mode override. NAME_None = "let Mover pick the mode from
	// state" (walking, falling, swimming via the transitions registered on the
	// MoverComponent). Set this to a specific mode name to force a transition —
	// e.g. teleport into Flying mode, scripted slide, etc.
	CharacterDefaultInputs.SuggestedMovementMode = NAME_None;

	// false = inputs are world-space (the simple case). Set true + fill MovementBase
	// + MovementBaseBoneName when standing on a moving primitive (elevator, ship
	// deck) so input is inherited along with the platform's motion. Mover detects
	// the base via floor query; the conversion uses UBasedMovementUtils.
	CharacterDefaultInputs.bUsingMovementBase = false;

	// Consume one-shot.
	bIsJumpJustPressed = false;
}

// ========================================
// IGameplayTagAssetInterface — route through the ASC.
// Until Step 4 wires the ASC, all queries return empty/false (safe default).
// ========================================

void AAZ_PawnMoverHeroCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_PawnMoverHeroCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasMatchingGameplayTag(TagToCheck);
	}
	return false;
}

bool AAZ_PawnMoverHeroCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAllMatchingGameplayTags(TagContainer);
	}
	return false;
}

bool AAZ_PawnMoverHeroCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAnyMatchingGameplayTags(TagContainer);
	}
	return false;
}

// ========================================
// IGenericTeamAgentInterface
// ========================================

void AAZ_PawnMoverHeroCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId;
}

FGenericTeamId AAZ_PawnMoverHeroCharacter::GetGenericTeamId() const
{
	return TeamId;
}

// ========================================
// AI-audible movement noise (TLOU-style stealth)
// ========================================

void AAZ_PawnMoverHeroCharacter::ReportMovementNoise()
{
	// One "footstep" per interval; louder the faster; crouch is nearly silent. Chalkie Hearing (registered,
	// range-capped on BOTH ends: event MaxRange here vs listener HearingRange) turns these into Investigate
	// pulls — sprinting past a dormant Chalkie is now a mistake.
	// Server-authoritative: AI perception lives on the server; a client's mirrored input producer reporting
	// too would double-stimulate on a listen server and feed no one on a dedicated one.
	if (!HasAuthority())
	{
		return;
	}
	const double NowSeconds = FPlatformTime::Seconds();
	if (NowSeconds - LastMovementNoiseTimeSeconds < NoiseIntervalSeconds)
	{
		return;
	}
	LastMovementNoiseTimeSeconds = NowSeconds;

	const float Speed2D = MoverComponent ? MoverComponent->GetVelocity().Size2D() : 0.f;
	float Range = 0.f;
	if (Speed2D > 450.f)      { Range = SprintNoiseRange; }   // sprint gait (585)
	else if (Speed2D > 250.f) { Range = RunNoiseRange;    }   // run gait (375)
	else if (Speed2D > 80.f)  { Range = WalkNoiseRange;   }   // walk gait (165)

	if (Range <= 0.f)
	{
		return;   // standing still = silent
	}

	if (HasMatchingGameplayTag(FAZ_GameplayTags::Get().Movement_Crouching))
	{
		Range *= CrouchNoiseScale;
	}

	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.f, this, Range, FName(TEXT("Footstep")));
}
