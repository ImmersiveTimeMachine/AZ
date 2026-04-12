#include "Character/AZ_PawnCameraMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AZ_GameplayTags.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/AZ_HeroPawn.h"

UAZ_PawnCameraMovementComponent::UAZ_PawnCameraMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// --- Default Stance ---
	DefaultStance.Camera.BoomLength = 220.f;
	DefaultStance.Camera.SocketOffset = FVector(0.f, 70.f, 0.f);
	DefaultStance.Camera.FOV = 80.f;
	DefaultStance.Camera.InterpSpeed = 5.f;
	DefaultStance.MoveOffsets.Forward = FVector2D(0.f, 3.f);
	DefaultStance.MoveOffsets.Backward = FVector2D(0.f, -2.f);
	DefaultStance.Movement.MaxSpeed = 195.f;
	DefaultStance.Movement.Acceleration = 450.f;
	DefaultStance.Movement.Deceleration = 150.f;
	DefaultStance.Movement.GroundFriction = 5.f;
	DefaultStance.Movement.BrakingFrictionFactor = 0.8f;

	// --- Aim Stance ---
	AimStance.Camera.BoomLength = 120.f;
	AimStance.Camera.SocketOffset = FVector(0.f, 55.f, 0.f);
	AimStance.Camera.FOV = 50.f;
	AimStance.Camera.InterpSpeed = 5.f;
	AimStance.MoveOffsets.Forward = FVector2D(0.f, 2.f);
	AimStance.MoveOffsets.Backward = FVector2D(0.f, -1.f);
	AimStance.MoveOffsets.Left = FVector2D(-100.f, 0.f);
	AimStance.MoveOffsets.Right = FVector2D(100.f, 0.f);
	AimStance.Movement.MaxSpeed = 150.f;
	AimStance.Movement.Acceleration = 400.f;
	AimStance.Movement.Deceleration = 200.f;
	AimStance.Movement.GroundFriction = 5.f;
	AimStance.Movement.BrakingFrictionFactor = 0.8f;

	// --- Crouch Stance ---
	CrouchStance.Camera.BoomLength = 150.f;
	CrouchStance.Camera.SocketOffset = FVector(0.f, 50.f, -40.f);
	CrouchStance.Camera.FOV = 80.f;
	CrouchStance.Camera.InterpSpeed = 2.f;
	CrouchStance.MoveOffsets.Backward = FVector2D(0.f, -1.f);
	CrouchStance.MoveOffsets.Right = FVector2D(300.f, 0.f);
	CrouchStance.Movement.MaxSpeed = 90.f;
	CrouchStance.Movement.Acceleration = 300.f;
	CrouchStance.Movement.Deceleration = 150.f;
	CrouchStance.Movement.GroundFriction = 5.f;
	CrouchStance.Movement.BrakingFrictionFactor = 0.8f;

	// --- Crouch+Aim Stance ---
	CrouchAimStance.Camera.BoomLength = 120.f;
	CrouchAimStance.Camera.SocketOffset = FVector(0.f, 55.f, -30.f);
	CrouchAimStance.Camera.FOV = 50.f;
	CrouchAimStance.Camera.InterpSpeed = 5.f;
	CrouchAimStance.MoveOffsets.Left = FVector2D(-100.f, 0.f);
	CrouchAimStance.MoveOffsets.Right = FVector2D(100.f, 0.f);
	CrouchAimStance.Movement.MaxSpeed = 70.f;
	CrouchAimStance.Movement.Acceleration = 250.f;
	CrouchAimStance.Movement.Deceleration = 200.f;
	CrouchAimStance.Movement.GroundFriction = 5.f;
	CrouchAimStance.Movement.BrakingFrictionFactor = 0.8f;

	ActiveStance = DefaultStance;
}

void UAZ_PawnCameraMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Find camera components
	CameraBoom = Owner->FindComponentByClass<USpringArmComponent>();
	Camera = Owner->FindComponentByClass<UCameraComponent>();

	// Find ASC via interface
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}

	ActiveStance = DefaultStance;
}

void UAZ_PawnCameraMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraBoom || !Camera)
	{
		return;
	}

	// --- Read GAS state tags ---
	bool bIsAiming = false;
	bool bIsCrouching = false;
	bIsSprinting = false;

	if (ASC.IsValid())
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		bIsAiming = ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming);
		bIsCrouching = ASC->HasMatchingGameplayTag(Tags.Movement_Crouching);
		bIsSprinting = ASC->HasMatchingGameplayTag(Tags.Ability_State_Sprinting);
	}

	// --- Resolve target stance ---
	const FAZ_StanceDefinition& TargetStance = ResolveStance(bIsAiming, bIsCrouching);
	const float InterpSpeed = TargetStance.Camera.InterpSpeed;

	// --- Compute per-direction move offset ---
	FVector2D TargetMoveOffset = FVector2D::ZeroVector;
	if (const AAZ_HeroPawn* Pawn = Cast<AAZ_HeroPawn>(GetOwner()))
	{
		// Get local move input from the pawn's cached intent
		const FVector& MoveIntent = Pawn->GetCachedMoveInputIntent();
		TargetMoveOffset = ComputeMoveOffset(TargetStance.MoveOffsets, MoveIntent);
	}
	CurrentMoveOffset = FMath::Vector2DInterpTo(CurrentMoveOffset, TargetMoveOffset, DeltaTime, MoveOffsetInterpSpeed);

	// --- Interpolate camera ---
	const float TargetBoomLength = TargetStance.Camera.BoomLength + CurrentMoveOffset.X;
	const FVector TargetSocketOffset = TargetStance.Camera.SocketOffset + FVector(0.f, CurrentMoveOffset.Y, 0.f);
	const float TargetFOV = TargetStance.Camera.FOV;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetBoomLength, DeltaTime, InterpSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaTime, InterpSpeed);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, InterpSpeed));

	// --- Update active stance for external queries ---
	ActiveStance = TargetStance;
}

const FAZ_StanceDefinition& UAZ_PawnCameraMovementComponent::ResolveStance(bool bIsAiming, bool bIsCrouching) const
{
	if (bIsCrouching && bIsAiming)
	{
		return CrouchAimStance;
	}
	if (bIsCrouching)
	{
		return CrouchStance;
	}
	if (bIsAiming)
	{
		return AimStance;
	}
	return DefaultStance;
}

FVector2D UAZ_PawnCameraMovementComponent::ComputeMoveOffset(const FAZ_CameraMoveOffset& Offsets, const FVector& LocalMoveInput) const
{
	FVector2D Result = FVector2D::ZeroVector;

	// Forward/Back (X axis of local input)
	if (LocalMoveInput.X > 0.1f)
	{
		Result += Offsets.Forward * LocalMoveInput.X;
	}
	else if (LocalMoveInput.X < -0.1f)
	{
		Result += Offsets.Backward * FMath::Abs(LocalMoveInput.X);
	}

	// Left/Right (Y axis of local input)
	if (LocalMoveInput.Y > 0.1f)
	{
		Result += Offsets.Right * LocalMoveInput.Y;
	}
	else if (LocalMoveInput.Y < -0.1f)
	{
		Result += Offsets.Left * FMath::Abs(LocalMoveInput.Y);
	}

	return Result;
}

float UAZ_PawnCameraMovementComponent::GetActiveMaxSpeed() const
{
	float Speed = ActiveStance.Movement.MaxSpeed;
	if (bIsSprinting)
	{
		Speed *= SprintSpeedMultiplier;
	}
	return Speed;
}
