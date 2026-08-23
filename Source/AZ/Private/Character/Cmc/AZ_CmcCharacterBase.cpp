// Copyright Artur. AZ project.

#include "Character/Cmc/AZ_CmcCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

AAZ_CmcCharacterBase::AAZ_CmcCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// --- Capsule / mesh conventions carried over from the v2 pawns so the same skeletal meshes align
	// and floor queries behave identically (capsule 25/90, mesh at -92 with the -90 yaw the whole anim
	// stack assumes). BP children assign the actual mesh + ABP assets (no /Game/ paths in C++). ---
	GetCapsuleComponent()->InitCapsuleSize(25.f, 90.f);
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	// A nav-relevant capsule carves a hole in the navmesh under the pawn — the infected's own path
	// queries then start "off-navmesh" inside its own carve (found the hard way in the v2 Phase-0 smoke).
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// Pawns are never floors: a chaser slides along the hero's capsule instead of climbing on top.
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Body rotation policy: orient-to-movement (GASP explore semantics). Controller rotation never
	// drives the body directly; children tune RotationRate.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Native crouch is the stance owner (the v2 generation hand-built this as a Mover mode).
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// Native motion warping — warp windows on montages deform root motion with no adapter glue.
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// --- CMC feel constants shared by every archetype (GASP 5.8 SandboxCharacter_CMC reference values).
	// Per-frame DERIVED parameters (braking/accel/friction/rotation tapers) are a hero concern and live
	// on AAZ_CmcHeroCharacter — applying GASP's hero tuning to the infected would silently change the
	// Chalkie's chase feel and its BT rotation tracking, which nothing in this spike asked for. ---
	// Jump feel (JumpZVelocity / GravityScale / AirControl) is deliberately NOT set here — the hero owns
	// its own tuned jump arc and the infected never jumps. One owner per fact.
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CMC->MinAnalogWalkSpeed              = 150.f;
	CMC->BrakingFrictionFactor           = 0.f;     // braking uses BrakingDeceleration alone, not friction
	CMC->PerchRadiusThreshold            = 20.f;    // lets the capsule perch on ledges instead of sliding off
	CMC->bUseFlatBaseForFloorChecks      = true;    // flat base = no capsule-edge slip on step corners
	CMC->SetCrouchedHalfHeight(60.f);
	CMC->NavAgentProps.bCanCrouch        = true;
	CMC->bCanWalkOffLedgesWhenCrouching  = true;
	// Moved onto NavMovementProperties in 5.5 (the flat member is deprecated and the struct is protected).
	if (FNavMovementProperties* NavProps = CMC->GetNavMovementProperties())
	{
		NavProps->bUseAccelerationForPaths = true;   // AI paths accelerate instead of snapping to speed
	}
}

void AAZ_CmcCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Resolved here (not the ctor) so a BP-child override of the EditDefaultsOnly values is honored.
	TeamId = FGenericTeamId(DefaultTeamId);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	SetGait(CurrentGait);

	WireModularMeshFollowers();
}

void AAZ_CmcCharacterBase::WireModularMeshFollowers()
{
	USkeletalMeshComponent* LeaderMesh = GetMesh();
	if (!LeaderMesh)
	{
		return;
	}

	TArray<USceneComponent*> Descendants;
	LeaderMesh->GetChildrenComponents(/*bIncludeAllDescendants*/ true, Descendants);

	int32 WiredCount = 0;
	int32 ExcludedCount = 0;
	for (USceneComponent* Child : Descendants)
	{
		USkeletalMeshComponent* Follower = Cast<USkeletalMeshComponent>(Child);
		if (!Follower || Follower == LeaderMesh || !Follower->GetSkeletalMeshAsset())
		{
			continue;   // grooms and empty slots are not followers
		}

		if (ModularFollowerExclusions.Contains(Follower->GetFName()))
		{
			++ExcludedCount;   // the MetaHuman face: keeps ABP_Face + RigLogic
			continue;
		}

		// A garment must not evaluate its own graph. If one carries an AnimClass, clear it: an ABP per
		// garment means one motion-matching search per garment, each free to pick a different pose.
		if (Follower->GetAnimClass())
		{
			Follower->SetAnimInstanceClass(nullptr);
		}

		// bForceUpdate — the BP template may already serialize this exact leader, and the setter
		// early-outs on a match, which would make this a silent no-op. See the header note.
		Follower->SetLeaderPoseComponent(LeaderMesh, /*bForceUpdate*/ true);
		++WiredCount;
	}

	// Instrumented from day one: "wired 6" vs "wired 0" is the whole diagnosis if garments go stiff.
	UE_LOG(LogTemp, Display, TEXT("[CmcMesh] %s wired %d modular follower mesh(es) to %s (%d excluded)"),
		*GetName(), WiredCount, *LeaderMesh->GetName(), ExcludedCount);
}

void AAZ_CmcCharacterBase::Landed(const FHitResult& Hit)
{
	// Captured BEFORE Super: ProcessLanded -> SetPostLandedPhysics discards the fall velocity, so this is
	// the only moment the impact speed is observable.
	LandVelocity = GetCharacterMovement()->Velocity;

	Super::Landed(Hit);

	bJustLanded = true;
	// Same handle = retriggerable: landing again inside the window restarts it instead of stacking timers.
	GetWorldTimerManager().SetTimer(JustLandedTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bJustLanded = false;
	}), JustLandedDuration, /*bLoop*/ false);
}

FRotator AAZ_CmcCharacterBase::GetAimRotation() const
{
	// Locally controlled pawns have the authoritative, full-rate control rotation; everyone else can only
	// see the replicated (and quantised) BaseAimRotation.
	return IsLocallyControlled() ? GetControlRotation() : GetBaseAimRotation();
}

EAZ_Gait AAZ_CmcCharacterBase::BandForSpeed(float Speed2D) const
{
	// Midpoints, so a band is only claimed once the body is nearer that gait than the one above it.
	if (Speed2D > (RunSpeed + SprintSpeed) * 0.5f)  { return EAZ_Gait::Sprint; }
	if (Speed2D > (WalkSpeed + RunSpeed) * 0.5f)    { return EAZ_Gait::Run; }
	return EAZ_Gait::Walk;
}

void AAZ_CmcCharacterBase::UpdateSelectionGait()
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		SelectionGait = CurrentGait;
		return;
	}

	const float Speed2D = Move->Velocity.Size2D();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero();

	// LATCH ON THE STOP EDGE. A stop clip decelerates LINEARLY, so the capsule must too — but a band
	// recomputed from the CURRENT speed decays as the character slows, which made one stop from sprint
	// step 615 -> 375 -> 190 and travel 493cm over 2.15s against a clip depicting 167cm over 0.95s
	// (measured 2026-08-23). It also re-classified the POOL mid-stop, so a single stop played a run stop
	// for 770ms and then a walk stop for 1237ms — two clips for one stop. Capturing the band once, at the
	// instant the stop begins, fixes both: constant deceleration AND one pool for the whole stop.
	// Diagnostic: the ACTUAL stop, measured. Separates the two readings of "the stop looks wrong" that
	// 1 Hz sampling cannot — a clip cut before its plant (pose problem) vs the capsule travelling past
	// what the clip depicts (movement problem). Reference: the stop clips depict 68 cm / 0.92 s (walk)
	// and 167 cm / 0.95 s (run). Function-local statics, not members: a member changes the UCLASS layout
	// and Live Coding cannot patch that. One player character per world, so sharing is harmless.
	static FVector StopStartLocation = FVector::ZeroVector;
	static float StopElapsed = 0.f;
	static float StopEntrySpeed = 0.f;
	static bool bStopMeasureActive = false;

	if (!bHasInput)
	{
		if (!bStopBandLatched)
		{
			bStopBandLatched = true;
			LatchedStopBand = BandForSpeed(Speed2D);

			StopStartLocation = GetActorLocation();
			StopEntrySpeed = Speed2D;
			StopElapsed = 0.f;
			bStopMeasureActive = (Speed2D > 50.f);
		}

		if (bStopMeasureActive)
		{
			StopElapsed += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
			if (Speed2D <= 1.f)
			{
				bStopMeasureActive = false;
				const float Travelled = FVector::Dist2D(GetActorLocation(), StopStartLocation);
				UE_LOG(LogTemp, Display,
					TEXT("[CmcStop] entry=%.0f cm/s -> stopped in %.2fs over %.0f cm  (band=%d, braking=%.0f)"),
					StopEntrySpeed, StopElapsed, Travelled, static_cast<int32>(LatchedStopBand),
					Move->BrakingDecelerationWalking);
			}
		}
	}
	else
	{
		bStopBandLatched = false;
		bStopMeasureActive = false;
	}

	// While stopping the latched band wins outright. Otherwise take the HIGHER of commanded and
	// speed-implied, which leaves acceleration untouched (a commanded sprint opens sprint pools at once)
	// and only holds the wider pools open while the body is still fast.
	if (bStopBandLatched)
	{
		SelectionGait = LatchedStopBand;
	}
	else
	{
		const EAZ_Gait SpeedBand = BandForSpeed(Speed2D);
		SelectionGait = (static_cast<uint8>(SpeedBand) > static_cast<uint8>(CurrentGait)) ? SpeedBand : CurrentGait;
	}
}

void AAZ_CmcCharacterBase::FillAnimContract(FAZ_CmcAnimContract& Out) const
{
	// --- Facts available with or without a movement component ---
	Out.ActorTransform = GetActorTransform();
	Out.AimingRotation = GetAimRotation();
	Out.Gait = CurrentGait;
	Out.SelectionGait = SelectionGait;
	Out.WalkSpeed = WalkSpeed;
	Out.RunSpeed = RunSpeed;
	Out.SprintSpeed = SprintSpeed;
	Out.Stance = bIsCrouched ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	Out.bJustLanded = bJustLanded;
	Out.LandVelocity = LandVelocity;
	GetOwnedGameplayTags(Out.OwnedTags);

	// Base default; the hero overrides this from its own tags. Kept here rather than left uninitialised
	// so an NPC using this contract gets the correct answer without implementing anything.
	Out.RotationMode = EAZ_RotationMode::OrientToMovement;

	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	Out.Velocity = Move->Velocity;
	Out.InputAcceleration = Move->GetCurrentAcceleration();
	Out.CurrentMaxAcceleration = Move->GetMaxAcceleration();
	Out.CurrentMaxDeceleration = Move->GetMaxBrakingDeceleration();
	Out.CurrentMaxSpeed = Move->GetMaxSpeed();
	Out.MovementMode = Move->IsFalling() ? EAZ_MovementMode::InAir : EAZ_MovementMode::OnGround;

	// Floor: OffsetRootBone projects its simulated root onto this plane every frame, and foot placement
	// needs it too. CurrentFloor is only meaningful while walking — in the air the last hit is stale, so
	// we publish the neutral answer instead of a floor that is no longer under us.
	if (Move->CurrentFloor.IsWalkableFloor())
	{
		Out.GroundLocation = Move->CurrentFloor.HitResult.ImpactPoint;
		Out.GroundNormal = Move->CurrentFloor.HitResult.ImpactNormal;
	}
	else
	{
		Out.GroundLocation = GetActorLocation();
		Out.GroundNormal = FVector::UpVector;
	}

	// Movement contributed by whatever we are standing on. Must be separable from our own motion or
	// standing still on a moving platform reads as running. Zero in AZ today — nothing moves underfoot
	// yet — but publishing it now costs one call and means the trajectory layer never has to ask.
	//
	// 5.8 API: GetMovementBase() and the UPrimitiveComponent overload of GetMovementBaseVelocity were
	// both deprecated this version ("your project will no longer compile" next release) in favour of
	// FMovementBaseInterfaceData, which lets a base be any UObject rather than only a primitive
	// component. BasedMovement.BoneName is unaffected.
	if (const UWorld* World = GetWorld())
	{
		Out.BasedMovementDelta = MovementBaseUtility::GetMovementBaseVelocity(
			GetMovementBaseInterfaceData(), BasedMovement.BoneName) * World->GetDeltaSeconds();
	}

	// Where the BODY wants to face, which is not where the capsule already is: on the ground we rotate
	// the capsule instantly (GASP semantics) and let the graph carry the visible turn, so the capsule's
	// own rotation says nothing about intent.
	if (Out.RotationMode == EAZ_RotationMode::OrientToMovement && !Out.InputAcceleration.IsNearlyZero())
	{
		Out.OrientationIntent = Out.InputAcceleration.Rotation();
	}
	else
	{
		Out.OrientationIntent = FRotator(0.f, Out.AimingRotation.Yaw, 0.f);
	}
}

void AAZ_CmcCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_CmcCharacterBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TagToCheck);
}

bool AAZ_CmcCharacterBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAllMatchingGameplayTags(TagContainer);
}

bool AAZ_CmcCharacterBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAnyMatchingGameplayTags(TagContainer);
}

void AAZ_CmcCharacterBase::SetJumpPressed(bool bPressed)
{
	// GAS gated the request (UAZ_GA_PawnJump); CMC executes. This is the "legacy CMC pawn" leg the
	// IAZ_JumpRequester contract always documented.
	if (bPressed)
	{
		Jump();
	}
	else
	{
		StopJumping();
	}
}

void AAZ_CmcCharacterBase::SetGait(EAZ_Gait NewGait)
{
	CurrentGait = NewGait;
	float Speed = RunSpeed;
	switch (NewGait)
	{
	case EAZ_Gait::Walk:   Speed = WalkSpeed;   break;
	case EAZ_Gait::Run:    Speed = RunSpeed;    break;
	case EAZ_Gait::Sprint: Speed = SprintSpeed; break;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}
