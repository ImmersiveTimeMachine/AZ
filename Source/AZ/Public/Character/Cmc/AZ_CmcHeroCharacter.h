
#pragma once

#include "CoreMinimal.h"
#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "AZ_CmcHeroCharacter.generated.h"

class UAZ_GameplayAbility;
class UCameraComponent;
class UChooserTable;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_CmcHeroCharacter : public AAZ_CmcCharacterBase
{
	GENERATED_BODY()

public:
	AAZ_CmcHeroCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void FillAnimContract(FAZ_CmcAnimContract& Out) const override;

	virtual void SetGrabFacingTarget(const AActor* Target) override { GrabFacingTarget = Target; }
	virtual const AActor* GetGrabFacingTarget() const override { return GrabFacingTarget.Get(); }
	virtual void SetGrabOutcomeFraming(bool bInOutcomeFraming) override { bGrabOutcomeFraming = bInOutcomeFraming; }
	virtual void SetGrabIKReleased(bool bInReleased) override { bGrabIKReleased = bInReleased; }
	virtual bool IsGrabIKReleased() const override { return bGrabIKReleased; }

	UFUNCTION(BlueprintPure, Category = "AZ|Input")
	UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UCameraComponent* GetCamera() const { return Camera; }

	bool OwnsRootMotionStarts(EAZ_Gait InGait) const;

protected:
	void InitAbilitySystem();

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);

	void ApplyMovementFeelParams(float DeltaSeconds);


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart")
	TArray<FAZ_RootMotionStartClip> RootMotionStartClips;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart")
	bool bRootMotionStarts = false;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart", meta = (ForceUnits = "cm/s"))
	float RootMotionStartMaxSpeed = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart", meta = (ForceUnits = "s"))
	float RootMotionStartBlendIn = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart", meta = (ForceUnits = "s"))
	float RootMotionStartBlendOut = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart", meta = (ForceUnits = "s"))
	float RootMotionStartCooldown = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStart")
	FName RootMotionStartSlot = TEXT("DefaultSlot");

	bool TryPlayRootMotionStart(const FVector& WorldInputDir);

	float LastRootMotionStartTime = -1.f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStop")
	TArray<FAZ_RootMotionStopClip> RootMotionStopClips;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|RootMotionStop")
	bool bRootMotionStops = false;

	bool TryPlayRootMotionStop();

	bool bRootMotionStopActive = false;

	float LastRootMotionStopTime = -1.f;
	bool bStopActive_LastFrameHero = false;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Chooser")
	TObjectPtr<UChooserTable> RootMotionStartChooser;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Chooser")
	TObjectPtr<UChooserTable> RootMotionStopChooser;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	EAZ_Gait ChooserGait = EAZ_Gait::Walk;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	EAZ_StartDirection ChooserDirection = EAZ_StartDirection::Fwd;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	EAZ_Stance ChooserStance = EAZ_Stance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	EAZ_RotationMode ChooserRotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	float ChooserSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement|Chooser")
	bool bChooserLeftFootDown = true;

	UAnimSequence* EvaluateLocomotionChooser(UChooserTable* Table);


	void UpdateTurnInPlaceLock(float DeltaSeconds);

	void ReleaseTurnInPlaceLock(const TCHAR* Reason, bool bSnapCapsule);


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp")
	bool bInputRampEnabled = false;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float InputRampStartScale = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp", meta = (ForceUnits = "s", ClampMin = "0.05"))
	float InputRampSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp", meta = (ForceUnits = "s", ClampMin = "0.02"))
	float InputRampReleaseSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp", meta = (ForceUnits = "deg", ClampMin = "0", ClampMax = "180"))
	float InputRampRetriggerAngle = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|InputRamp", meta = (ForceUnits = "cm/s"))
	float InputRampBaseMinAnalog = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace")
	bool bTurnInPlaceLock = false;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace", meta = (ForceUnits = "deg"))
	float TurnInPlaceEnterAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace", meta = (ForceUnits = "deg"))
	float TurnInPlaceExitAngle = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace", meta = (ForceUnits = "cm/s"))
	float TurnInPlaceEnterMaxSpeed = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace", meta = (ForceUnits = "s"))
	float TurnInPlaceTimeout = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace", meta = (ForceUnits = "deg"))
	float TurnInPlaceRetargetAngle = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace")
	bool bTurnInPlaceSnapCapsuleOnRelease = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|TurnInPlace")
	int32 TurnInPlaceMinHoldFrames = 2;

	bool bTipLockActive = false;
	float TipTargetYaw = 0.f;
	float TipLockElapsed = 0.f;
	int32 TipEnterCandidateFrames = 0;
	FVector LastMoveInputDir = FVector::ZeroVector;
	float LastMoveInputTime = -1.f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop")
	bool bStopCurveBraking = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop")
	float StopCurveMaxBraking = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "cm/s"))
	float StopCurveMaxHandoverStep = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopCurveMaxHandoverClipTime = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopCurveConvergenceTime = 0.1f;

	bool bStopCurveEngaged = false;

	bool bStopCurveRejected = false;

	void ResolveGaitAndStanceFromTags();


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelWithInput = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelNoInput = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float WalkBrakingDecel = 190.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float RunBrakingDecel = 375.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float SprintBrakingDecel = 615.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	bool bGaitScaledBraking = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float MaxAccelerationBase = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float MaxAccelerationAtTopSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float AccelTaperSpeedMin = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float AccelTaperSpeedMax = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float GroundFrictionMax = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float GroundFrictionMin = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float FrictionTaperSpeedMax = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float GroundedRotationRateYaw = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float RunRotationRateYaw = 115.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float SprintRotationRateYaw = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	bool bGaitScaledRotationRate = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float FallingRotationRateYaw = 200.f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime")
	bool bFacingTimeRotation = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "s", ClampMin = "0.01"))
	float IdleFacingTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "s", ClampMin = "0.01"))
	float WalkRunFacingTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "s", ClampMin = "0.01"))
	float SprintFacingTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "deg", ClampMin = "0", ClampMax = "180"))
	float CameraSnapShortenStartAngle = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "deg", ClampMin = "0", ClampMax = "180"))
	float CameraSnapShortenFullAngle = 135.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "s", ClampMin = "0"))
	float CameraSnapShortenMaxSeconds = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|FacingTime", meta = (ForceUnits = "deg/s", ClampMin = "1"))
	float MaxFacingRotationRateYaw = 900.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRateYaw = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRatePitch = 1.f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|GAS")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> GrabbedAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> HitReactAbilityClass;

	TWeakObjectPtr<const AActor> GrabFacingTarget;
	bool bGrabOutcomeFraming = false;
	bool bGrabIKReleased = false;
};
