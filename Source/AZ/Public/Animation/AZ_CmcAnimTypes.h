
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "AZ_CmcAnimTypes.generated.h"

class UPoseSearchDatabase;

USTRUCT(BlueprintType)
struct AZ_API FAZ_CmcAnimContract
{
	GENERATED_BODY()


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FTransform ActorTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract", meta = (ForceUnits = "cm/s"))
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector InputAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FRotator AimingRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector BasedMovementDelta = FVector::ZeroVector;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	float CurrentMaxAcceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	float CurrentMaxDeceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract", meta = (ForceUnits = "cm/s"))
	float CurrentMaxSpeed = 0.f;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector GroundLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector GroundNormal = FVector::UpVector;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	EAZ_Gait SelectionGait = EAZ_Gait::Walk;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	bool bStopActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	bool bStopIsAnimated = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim", meta = (ForceUnits = "cm/s"))
	float StopEntrySpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim", meta = (ForceUnits = "cm"))
	float StopRemainingDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim", meta = (ForceUnits = "cm"))
	float StopPlannedDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float StopProgress = 0.f;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	bool bTurnInPlaceActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim", meta = (ForceUnits = "deg"))
	float TurnInPlaceTargetYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float WalkSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float RunSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float SprintSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_Stance Stance = EAZ_Stance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FAZ_PlayerInputState InputState;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	bool bJustLanded = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector LandVelocity = FVector::ZeroVector;


	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FGameplayTagContainer OwnedTags;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_DatabaseGate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Gate")
	FName Label;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_MovementMode> MovementModes;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_Stance> Stances;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_MovementState> MovementStates;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_Gait> Gaits;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<TObjectPtr<UPoseSearchDatabase>> Databases;


	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_StateMachineState> States;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_StartDirection> StartDirections;

	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_RotationMode> RotationModes;

	UPROPERTY(EditAnywhere, Category = "Gate")
	bool bExclusive = false;

	bool Matches(const EAZ_MovementMode InMode, const EAZ_Stance InStance,
	             const EAZ_MovementState InState, const EAZ_Gait InGait,
	             const EAZ_StateMachineState InSMState = EAZ_StateMachineState::IdleLoop,
	             const EAZ_StartDirection InStartDir = EAZ_StartDirection::Fwd,
	             const EAZ_RotationMode InRotationMode = EAZ_RotationMode::OrientToMovement) const
	{
		return (MovementModes.IsEmpty()   || MovementModes.Contains(InMode))
			&& (Stances.IsEmpty()         || Stances.Contains(InStance))
			&& (MovementStates.IsEmpty()  || MovementStates.Contains(InState))
			&& (Gaits.IsEmpty()           || Gaits.Contains(InGait))
			&& (States.IsEmpty()          || States.Contains(InSMState))
			&& (StartDirections.IsEmpty() || StartDirections.Contains(InStartDir))
			&& (RotationModes.IsEmpty()   || RotationModes.Contains(InRotationMode));
	}
};
