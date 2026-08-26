
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "Animation/AZ_CmcAnimTypes.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "AnimationWarpingTypes.h"
#include "BoneControllers/AnimNode_OrientationWarping.h"
#include "Animation/AnimNodeReference.h"
#include "AZ_CmcAnimInstance.generated.h"

class AAZ_CmcCharacterBase;
class UAnimationAsset;
class UAZ_LocomotionStateMachine;
class UAZ_ObstacleSensorComponent;
class UChooserTable;
class UPoseSearchDatabase;
struct FAnimNodeReference;
struct FAnimNode_OffsetRootBone;
struct FMotionMatchingAnimNodeReference;

UCLASS(Abstract, Blueprintable)
class AZ_API UAZ_CmcAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UAZ_CmcAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;


	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_Logic(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_Trajectory(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_EssentialValues(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_States();

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_MovementDirection();


	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	void Update_MotionMatching(const FAnimNodeReference& Node);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	void Update_MotionMatching_PostSelection(const FAnimNodeReference& Node);

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	bool EnableSteering(const FAnimNodeReference& Node) const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	TArray<UPoseSearchDatabase*> Get_DatabasesToSearch() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe, DeprecatedFunction,
		DeprecationMessage = "RootTransform is read natively now. This setter is dead and will be removed."))
	void SetOffsetRootTransform(const FTransform& InOffsetRootTransform);


	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsMoving() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsPivoting() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool ShouldTurnInPlace() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_TrajectoryTurnAngle() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector CalculateRelativeAccelerationAmount() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector2D Get_LeanAmount() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector2D Get_AOValue() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_DynamicPlayRate(float MinPlayRate = 0.8f, float MaxPlayRate = 1.2f) const;


	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsStarting() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsOnGround() const { return MovementMode == EAZ_MovementMode::OnGround; }

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool ShouldSpinTransition() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FQuat Get_DesiredFacing() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_AO_Yaw() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FAZ_MovementDirectionThresholds Get_MovementDirectionThresholds() const;


	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_MMBlendTime() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_MMNotifyRecencyTimeOut() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EPoseSearchInterruptMode Get_MMInterruptMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOffsetRootBoneMode Get_OffsetRootRotationMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOffsetRootBoneMode Get_OffsetRootTranslationMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationHalfLife() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationRadius() const { return OffsetRootTranslationRadius; }

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOrientationWarpingSpace Get_OrientationWarpingWarpingSpace() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	bool AllowFootPinning() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	AAZ_CmcCharacterBase* GetCmcCharacter() const { return Cached_Character; }


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim")
	FAZ_CmcAnimContract CharacterProperties;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FTransformTrajectory Trajectory;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectory_WorldCollisionResults TrajectoryCollision;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_PastVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_FutureVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	float PreviousDesiredControllerYaw = 0.f;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform CharacterTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform CharacterTransform_LastFrame = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform RootTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Acceleration_LastFrame = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	float AccelerationAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	bool bHasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Velocity_LastFrame = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential", meta = (ForceUnits = "cm/s"))
	float Speed2D = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	bool bHasVelocity = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector VelocityAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector RelativeAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector LastNonZeroVelocity = FVector::ZeroVector;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementMode MovementMode_LastFrame = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_RotationMode RotationMode_LastFrame = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementState MovementState = EAZ_MovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementState MovementState_LastFrame = EAZ_MovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Gait Gait_LastFrame = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Stance Stance = EAZ_Stance::Standing;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Stance Stance_LastFrame = EAZ_Stance::Standing;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TArray<FName> CurrentDatabaseTags;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	bool bCurrentAssetLooping = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TObjectPtr<UObject> CurrentSelectedAnim;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TObjectPtr<const UPoseSearchDatabase> CurrentSelectedDatabase;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	float SearchCost = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction")
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction", meta = (ForceUnits = "deg"))
	float MovementDirectionAngle = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction")
	bool bLeftFootDown = false;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|V2")
	FAZ_v2_ChooserContext ChooserContext;

	UPROPERTY(BlueprintReadWrite, Transient, Category = "AZ|Cmc|Anim|V2")
	FAZ_BlendStackInputs BlendStackInputs;

	UPROPERTY(BlueprintReadWrite, Transient, Category = "AZ|Cmc|Anim|V2")
	FAZ_ChooserOutputs ChooserOutputs;

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim|V2", meta = (BlueprintThreadSafe, AutoCreateRefTerm = "Candidates"))
	void SetBlendStackAnimFromChooser(
		bool bForceBlend,
		FAnimNodeReference BlendStackNode,
		FAZ_ChooserOutputs ChooserOut,
		UAnimationAsset* ChosenAnim,
		const TArray<UObject*>& Candidates);

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|V2", meta = (BlueprintThreadSafe))
	UChooserTable* GetLocomotionChooser() const { return LocomotionChooser; }

	bool IsPlayingImpactReaction() const { return LatchedReaction != EAZ_ObstacleReaction::None; }


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Lean")
	FVector2D LeanAmount = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Lean")
	float LeanAlpha = 0.f;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Combat")
	bool bCombatReady = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Combat")
	float CombatReadyAlpha = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Combat", meta = (ClampMin = "0"))
	float CombatReadyBlendInSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Combat", meta = (ClampMin = "0"))
	float CombatReadyBlendOutSpeed = 6.f;


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|GrabIK")
	float GrabIKAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|GrabIK")
	FVector GrabIKTarget_HandR = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|GrabIK")
	FVector GrabIKTarget_HandL = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|GrabIK")
	FRotator GrabBodyShakeRot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|GrabIK")
	FRotator GrabHeadShakeRot = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "1"))
	float GrabIKBlendSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "0.5", ClampMax = "1"))
	float GrabIKReachScale = 0.97f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "1"))
	float GrabIKTargetInterpSpeed = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "0"))
	float GrabBodyShakeAmplitudeDeg = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "0.1"))
	float GrabBodyShakeFrequency = 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "0"))
	float GrabHeadShakeAmplitudeDeg = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK", meta = (ClampMin = "0.1"))
	float GrabHeadShakeFrequency = 16.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK")
	FName GrabIKGrabberBoneForHandR = TEXT("GrabIK_HandR");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|GrabIK")
	FName GrabIKGrabberBoneForHandL = TEXT("GrabIK_HandL");


	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector PredictedFutureVelocity = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float TrajectoryFutureLookahead = 0.2f;

	UPROPERTY(BlueprintReadWrite, Category = "AZ|Cmc|Anim|StateMachine")
	bool bSuppressLocomotion = false;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectoryData TrajectoryGenerationData_Idle;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectoryData TrajectoryGenerationData_Moving;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "s"))
	float TrajectoryHistorySamplingInterval = -1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ClampMin = "2"))
	int32 TrajectoryHistoryCount = 30;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "s"))
	float TrajectoryPredictionSamplingInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ClampMin = "2"))
	int32 TrajectoryPredictionCount = 15;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	bool bHandleTrajectoryCollisions = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	bool bTrajectoryApplyGravity = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "cm"))
	float FloorCollisionsOffset = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "cm"))
	float MaxObstacleHeight = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D PastVelocityWindow = FVector2D(-0.3f, -0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D CurrentVelocityWindow = FVector2D(0.0f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D FutureVelocityWindow = FVector2D(0.4f, 0.5f);


public:

	bool IsStopClipSelected() const { return bStopClipSelected_GT; }

	float GetStopClipDepictedSpeed() const { return StopClipSpeed_GT; }

	float GetStopClipSampleTime() const { return StopClipSampleTime_GT; }


	bool IsTurnInPlaceClipSelected() const { return bTipClipSelected_GT; }

	float GetTurnInPlaceClipFraction() const { return TipClipFraction_GT; }

	float GetTipRootYaw() const { return TipRootYaw_GT; }

protected:
	bool bStopActive = false;
	bool bStopActive_LastFrame = false;

	bool bStopClipSelected_GT = false;
	float StopClipSpeed_GT = 0.f;
	float StopClipSampleTime_GT = 0.f;

	bool bTurnInPlaceActive = false;
	bool bTurnInPlaceActive_LastFrame = false;

	bool bTipClipSelected_GT = false;
	float TipClipFraction_GT = 0.f;
	float TipRootYaw_GT = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "s"))
	float TurnInPlaceFacingConvergeTime = 0.5f;

	float CurrentSelectedTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Selection", meta = (ClampMin = "0", ClampMax = "1"))
	float OneShotKeepAliveFractionTunable = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Selection", meta = (ClampMin = "0", ClampMax = "1"))
	float StopKeepAliveFractionTunable = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean")
	float LeanInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean", meta = (ForceUnits = "deg/s"))
	float LeanTurnRateReference = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float HasVelocityThreshold = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds")
	float IsMovingVelocityTolerance = 0.1f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_OrientToMovement = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_Strafe = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_Aiming = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float TurnInPlaceAngleThreshold = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean")
	FVector2D LeanSpeedRangeIn = FVector2D(165.f, 375.f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean")
	FVector2D LeanSpeedRangeOut = FVector2D(0.5f, 1.f);


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	bool bInvertFootPhase = false;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction", meta = (ForceUnits = "cm/s"))
	float DirectionHoldSpeed = 10.f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName FootSpeedCurveL = TEXT("FootSpeed_L");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName FootSpeedCurveR = TEXT("FootSpeed_R");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName MoveDataSpeedCurve = TEXT("MoveData_Speed");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName PlayRateWarpingCurve = TEXT("Enable_PlayRateWarping");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName DisableAOCurve = TEXT("Disable_AO");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves", meta = (ClampMin = "0"))
	float FootPlantedSpeedThreshold = 1.f;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Databases")
	TArray<FAZ_DatabaseGate> DatabaseGates;

	mutable TArray<FName> MatchedGateLabels;

	bool bWarnedEmptyGateUnion = false;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float StartingFutureSpeedMargin = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float SpinTransitionAngleThreshold = 130.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float SpinTransitionMinSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "s"))
	float DesiredFacingSampleTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|MotionMatching")
	FName PivotDatabaseTag = TEXT("Pivots");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	FName MontageSlotName = TEXT("DefaultSlot");


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_Cardinal = { 60.0, 60.0, 120.0, 120.0 };

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_SideLoop = { 60.0, 60.0, 140.0, 140.0 };

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_SideTight = { 40.0, 40.0, 140.0, 140.0 };


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Ground = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_JustLanded = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Rising = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Falling = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "cm/s"))
	float MMRisingVelocityZ = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Walk = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Run = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Sprint = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float OffsetRootHalfLife_Idle = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float OffsetRootHalfLife_Moving = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	float OffsetRootTranslationRadius = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	bool bOffsetRootBoneEnabled = true;


	UPROPERTY(Transient)
	TObjectPtr<UAZ_LocomotionStateMachine> StateMachine;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|V2")
	TObjectPtr<UChooserTable> LocomotionChooser;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|V2")
	FName PoseHistoryTag = TEXT("PoseHistory");


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakMinTime = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakMaxTime = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakAlmostCompleteThreshold = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|V2", meta = (ClampMin = "0", ForceUnits = "s"))
	float TransitionAlmostCompleteThreshold = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|V2", meta = (ClampMin = "0", ClampMax = "1"))
	float MoveIntentDeadzone = 0.1f;


	UPROPERTY(Transient)
	TWeakObjectPtr<UAZ_ObstacleSensorComponent> Cached_ObstacleSensor;

	UPROPERTY(Transient)
	EAZ_ObstacleReaction LatchedReaction = EAZ_ObstacleReaction::None;

	UPROPERTY(Transient) EAZ_StateMachineState LastPushedSMState = EAZ_StateMachineState::IdleLoop;
	UPROPERTY(Transient) EAZ_Stance            LastPushedStance  = EAZ_Stance::Standing;
	UPROPERTY(Transient) EAZ_Gait              LastPushedGait    = EAZ_Gait::Walk;
	UPROPERTY(Transient) EAZ_MovementDirection LastPushedDir     = EAZ_MovementDirection::F;
	UPROPERTY(Transient) bool                  LastPushedLeftFootDown = false;
	UPROPERTY(Transient) EAZ_ObstacleReaction  LastPushedReaction = EAZ_ObstacleReaction::None;

	uint32 TransitionSerial = 0;
	uint32 LastPushedTransitionSerial = 0;

	float PendingBlendOut = 0.f;

	float PendingStartAngleDeg = 0.f;

	bool LastGroundedLeftFootDown = false;
	bool LastGroundedIsMoving     = false;


	bool bMontageActive_GT = false;

	mutable bool bMontageJustReleased_GT = false;

	bool bRmOwnsStarts_GT = false;

	EAZ_ObstacleReaction SensorReaction_GT = EAZ_ObstacleReaction::None;

	TWeakObjectPtr<UObject> LastLoggedSelectedAnim;
	float SecondsSinceSelectionChange = 0.f;
	int32 SelectionChangeIndex = 0;
	float RatioSum = 0.f;
	int32 RatioCount = 0;
	float RatioMin = 0.f;
	float RatioMax = 0.f;

	void PublishSelection(UObject* InAnim, const UPoseSearchDatabase* InDatabase, float InTime,
		bool bInLoop, float InCost, const TArray<FName>& InFallbackTags);

	void Update_LocomotionStateMachine(float DeltaSeconds);

	void Update_GrabIK(float DeltaSeconds);


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Debug")
	bool bDebugAnim = false;


	UPROPERTY(Transient)
	TObjectPtr<AAZ_CmcCharacterBase> Cached_Character;

	FAnimNode_OffsetRootBone* FindOffsetRootBoneNode();

	void KeepPlayingOneShotSearchable(
		const FMotionMatchingAnimNodeReference& MotionMatchingNode, TArray<UPoseSearchDatabase*>& Pool) const;


	FTransform OffsetRootTransform = FTransform::Identity;
	bool bHasOffsetRootTransform = false;

	void DrawDebugAnimOverlay() const;

	void LogMovementFeelOncePerSecond(float DeltaSeconds) const;

	float DebugAccumulator = 0.f;
	bool bLoggedInit = false;
};
