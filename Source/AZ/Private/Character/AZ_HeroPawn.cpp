#include "Character/AZ_HeroPawn.h"

#include "AbilitySystemComponent.h"
#include "Character/AZ_MoverInputCmd.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "Character/AZ_PawnCameraMovementComponent.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Player/AZ_PlayerState.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/BasedMovementUtils.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "NetworkPredictionComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

AAZ_HeroPawn::AAZ_HeroPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// --- Tick ---
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// --- Replication ---
	bReplicates = true;
	SetReplicatingMovement(false); // Mover handles movement replication

	// --- Rotation (match old character: yaw from controller, no pitch/roll) ---
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// --- Collision root (capsule) — match old: radius 25, half-height 90 ---
	UCapsuleComponent* CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CapsuleComp->InitCapsuleSize(25.f, 90.f);
	CapsuleComp->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComp->SetCanEverAffectNavigation(true);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CapsuleComp->SetGenerateOverlapEvents(false);
	SetRootComponent(CapsuleComp);

	// --- Skeletal Mesh — match old: offset -90 (half-height), collision channels ---
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CapsuleComp);
	MeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	MeshComponent->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- Mover Component (replaces CharacterMovementComponent) ---
	CharacterMoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("CharacterMoverComponent"));
	CharacterMoverComponent->SetUpdatedComponent(CapsuleComp);
	CharacterMoverComponent->SetHandleJump(true);
	CharacterMoverComponent->SetHandleStanceChanges(true);
	CharacterMoverComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// --- Network Prediction (required for networked Mover replication) ---
	NetworkPredictionComponent = CreateDefaultSubobject<UNetworkPredictionComponent>(TEXT("NetworkPredictionComponent"));

	// --- Camera — match old character values ---
	ThirdPersonCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonCameraBoom"));
	ThirdPersonCameraBoom->SetupAttachment(CapsuleComp);
	ThirdPersonCameraBoom->TargetArmLength = 220.f;
	ThirdPersonCameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	ThirdPersonCameraBoom->bUsePawnControlRotation = true;
	ThirdPersonCameraBoom->bEnableCameraLag = true;
	ThirdPersonCameraBoom->CameraLagSpeed = 8.f;
	ThirdPersonCameraBoom->CameraLagMaxDistance = 50.f;
	ThirdPersonCameraBoom->bEnableCameraRotationLag = true;
	ThirdPersonCameraBoom->CameraRotationLagSpeed = 10.f;
	ThirdPersonCameraBoom->bDoCollisionTest = true;
	ThirdPersonCameraBoom->ProbeSize = 12.f;
	ThirdPersonCameraBoom->ProbeChannel = ECC_Camera;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonCameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	// --- Motion Matching Trajectory (Mover-native, replaces CharacterTrajectoryComponent) ---
	MoverTrajectoryPredictor = CreateDefaultSubobject<UMoverTrajectoryPredictor>(TEXT("MoverTrajectoryPredictor"));

	// --- Equipment ---
	EquipmentManagerComponent = CreateDefaultSubobject<UAZ_EquipmentManagerComponent>(TEXT("EquipmentManager"));

	// --- Camera + Movement State Manager ---
	CameraMovementComponent = CreateDefaultSubobject<UAZ_PawnCameraMovementComponent>(TEXT("CameraMovementComponent"));

	// --- Default attribute set class ---
	DefaultAttributeSetClass = nullptr;
}

// ============================================================
// Lifecycle
// ============================================================

void AAZ_HeroPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The Mover component may have been added in BP; find it if not created in constructor
	if (!CharacterMoverComponent)
	{
		CharacterMoverComponent = FindComponentByClass<UCharacterMoverComponent>();
	}
}

void AAZ_HeroPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayerCameraManager->ViewPitchMax = 89.f;
		PC->PlayerCameraManager->ViewPitchMin = -89.f;
	}

	if (CharacterMoverComponent)
	{
		// Register this pawn as the Mover's input producer
		CharacterMoverComponent->InputProducer = this;

		// Wire up Mover trajectory predictor for PoseSearch
		if (MoverTrajectoryPredictor)
		{
			MoverTrajectoryPredictor->Setup(CharacterMoverComponent);
		}
	}
}

void AAZ_HeroPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Populate thread-safe state proxy for AnimInstance (game thread only)
	if (CharacterMoverComponent)
	{
		MoverStateProxy.Velocity = CharacterMoverComponent->GetVelocity();
		MoverStateProxy.GroundSpeed = FVector(MoverStateProxy.Velocity.X, MoverStateProxy.Velocity.Y, 0.f).Size();
		MoverStateProxy.bIsOnGround = CharacterMoverComponent->IsOnGround();
		MoverStateProxy.bIsFalling = CharacterMoverComponent->IsFalling();
		MoverStateProxy.bIsCrouching = CharacterMoverComponent->IsCrouching();

		// Floor hit for GroundNormal / GroundLocation (used by foot IK slope warping)
		FHitResult FloorHit;
		if (CharacterMoverComponent->TryGetFloorCheckHitResult(FloorHit))
		{
			MoverStateProxy.GroundNormal = FloorHit.ImpactNormal;
			MoverStateProxy.GroundLocation = FloorHit.ImpactPoint;
		}
		else
		{
			MoverStateProxy.GroundNormal = FVector::UpVector;
			MoverStateProxy.GroundLocation = FVector::ZeroVector;
		}
	}

	if (AZ_AbilitySystemComponent)
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		MoverStateProxy.bIsSprinting = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Sprinting);
		MoverStateProxy.bIsAiming = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Aiming);
	}
}

// ============================================================
// Controller / PlayerState — GAS Init
// ============================================================

void AAZ_HeroPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side: grab ASC from PlayerState
	if (AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());

		if (AZ_AbilitySystemComponent)
		{
			AZ_AbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// Create attribute set
			if (DefaultAttributeSetClass)
			{
				AttributeSet = NewObject<UAttributeSet>(this, DefaultAttributeSetClass);
				AZ_AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
			}

			InitDefaultAttributes();
			InitDefaultAbilities();

			OnAscRegistered.Broadcast(AZ_AbilitySystemComponent.Get());
		}
	}
}

void AAZ_HeroPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client-side: grab ASC from PlayerState
	if (AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());

		if (AZ_AbilitySystemComponent)
		{
			AZ_AbilitySystemComponent->InitAbilityActorInfo(PS, this);

			if (DefaultAttributeSetClass && !AttributeSet)
			{
				AttributeSet = NewObject<UAttributeSet>(this, DefaultAttributeSetClass);
				AZ_AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
			}

			OnAscRegistered.Broadcast(AZ_AbilitySystemComponent.Get());
		}
	}
}

// ============================================================
// IAbilitySystemInterface / CombatInterface
// ============================================================

UAbilitySystemComponent* AAZ_HeroPawn::GetAbilitySystemComponent() const
{
	return AZ_AbilitySystemComponent.Get();
}

UAbilitySystemComponent* AAZ_HeroPawn::GetASC() const
{
	return AZ_AbilitySystemComponent.Get();
}

FOnASCRegistered& AAZ_HeroPawn::GetOnASCRegisteredDelegate()
{
	return OnAscRegistered;
}

// ============================================================
// GAS Helpers
// ============================================================

void AAZ_HeroPawn::InitDefaultAttributes()
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void AAZ_HeroPawn::ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, float Level) const
{
	if (!GameplayEffectClass || !AZ_AbilitySystemComponent)
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = AZ_AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = AZ_AbilitySystemComponent->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	AZ_AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void AAZ_HeroPawn::InitDefaultAbilities() const
{
	if (!AZ_AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	UObject* SourceObject = const_cast<AAZ_HeroPawn*>(this);

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, SourceObject);
		AZ_AbilitySystemComponent->GiveAbility(Spec);
	}

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, SourceObject);
		AZ_AbilitySystemComponent->GiveAbility(Spec);
	}

	// Input-bound abilities — grant with InputTag so controller can bind them
	AZ_AbilitySystemComponent->GrantAbilitiesWithInputTag(CharacterInputAbilities);
}

// ============================================================
// Enhanced Input
// ============================================================

void AAZ_HeroPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_HeroPawn::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnMoveCompleted);
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_HeroPawn::OnLookTriggered);
		Input->BindAction(LookInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnLookCompleted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AAZ_HeroPawn::OnJumpStarted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnJumpReleased);
	}
}

void AAZ_HeroPawn::OnMoveTriggered(const FInputActionValue& Value)
{
	// Enhanced Input IA_Move: X = Right/Left (A/D), Y = Forward/Back (W/S)
	// RotateVector expects: X = Forward, Y = Right
	const FVector2D Input2D = Value.Get<FVector2D>();
	CachedMoveInputIntent.X = FMath::Clamp(Input2D.Y, -1.f, 1.f);  // Forward/Back
	CachedMoveInputIntent.Y = FMath::Clamp(Input2D.X, -1.f, 1.f);  // Right/Left
	CachedMoveInputIntent.Z = 0.f;

}

void AAZ_HeroPawn::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void AAZ_HeroPawn::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X * LookRateYaw);
	AddControllerPitchInput(-LookVector.Y * LookRatePitch);
}

void AAZ_HeroPawn::OnLookCompleted(const FInputActionValue& Value)
{
	// No-op — look is applied immediately in OnLookTriggered
}

void AAZ_HeroPawn::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void AAZ_HeroPawn::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

// ============================================================
// IMoverInputProducerInterface — ProduceInput
// ============================================================

void AAZ_HeroPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput(static_cast<float>(SimTimeMs), InputCmdResult);
}

void AAZ_HeroPawn::OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult)
{
	// --- Standard character inputs (movement, orientation, jump) ---
	FCharacterDefaultInputs& CharInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (!GetController())
	{
		return;
	}

	// Control rotation (camera)
	CharInputs.ControlRotation = FRotator::ZeroRotator;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		CharInputs.ControlRotation = PC->GetControlRotation();
	}

	// Directional movement intent, rotated into world space by control rotation
	// Clamp magnitude to 1.0 to prevent diagonal speed boost
	FVector ClampedIntent = CachedMoveInputIntent;
	if (ClampedIntent.SizeSquared() > 1.f)
	{
		ClampedIntent.Normalize();
	}
	const FVector WorldMoveIntent = CharInputs.ControlRotation.RotateVector(ClampedIntent);
	CharInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMoveIntent);

	// Orientation: face camera when aiming (strafe), face movement otherwise
	const bool bAiming = AZ_AbilitySystemComponent
		? AZ_AbilitySystemComponent->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Aiming)
		: false;

	if (bAiming)
	{
		// Strafe orientation: face camera forward (flattened to horizontal)
		const FVector CameraForward = FRotationMatrix(CharInputs.ControlRotation).GetUnitAxis(EAxis::X);
		CharInputs.OrientationIntent = FVector(CameraForward.X, CameraForward.Y, 0.f).GetSafeNormal();
	}
	else
	{
		constexpr float RotationMagMin = 1e-3f;
		if (WorldMoveIntent.SizeSquared() >= RotationMagMin * RotationMagMin)
		{
			CharInputs.OrientationIntent = WorldMoveIntent.GetSafeNormal();
		}
		else
		{
			CharInputs.OrientationIntent = FVector::ZeroVector;
		}
	}

	// Jump
	CharInputs.bIsJumpPressed = bIsJumpPressed;
	CharInputs.bIsJumpJustPressed = bIsJumpJustPressed;
	CharInputs.SuggestedMovementMode = NAME_None;

	// Convert to base-relative if standing on a moving platform
	CharInputs.bUsingMovementBase = false;
	if (CharacterMoverComponent)
	{
		if (UPrimitiveComponent* MovementBase = CharacterMoverComponent->GetMovementBase())
		{
			FName BaseBoneName = CharacterMoverComponent->GetMovementBaseBoneName();
			FVector RelativeMove, RelativeOrient;

			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, BaseBoneName, CharInputs.GetMoveInput(), RelativeMove);
			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, BaseBoneName, CharInputs.OrientationIntent, RelativeOrient);

			CharInputs.SetMoveInput(CharInputs.GetMoveInputType(), RelativeMove);
			CharInputs.OrientationIntent = RelativeOrient;

			CharInputs.bUsingMovementBase = true;
			CharInputs.MovementBase = MovementBase;
			CharInputs.MovementBaseBoneName = BaseBoneName;
		}
	}

	// --- AZ custom ability inputs (GAS tag → Mover flag handshake) ---
	FAZ_MoverInputCmd& AZInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverInputCmd>();

	if (AZ_AbilitySystemComponent)
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		AZInputs.bIsSprinting = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Sprinting);
		AZInputs.bIsAiming = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Aiming);
		AZInputs.bIsCrouching = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Movement_Crouching);
		AZInputs.bIsDashing = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Dashing);
	}

	// Consume temporal inputs
	bIsJumpJustPressed = false;
}
