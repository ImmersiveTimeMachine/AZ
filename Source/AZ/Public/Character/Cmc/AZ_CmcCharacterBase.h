
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "Animation/AZ_CmcAnimTypes.h"
#include "Character/AZ_CombatAvatar.h"
#include "Character/AZ_JumpRequester.h"
#include "AZ_CmcCharacterBase.generated.h"

class UAbilitySystemComponent;
class UMotionWarpingComponent;

UCLASS(Abstract, config = Game, BlueprintType)
class AZ_API AAZ_CmcCharacterBase
	: public ACharacter
	, public IAbilitySystemInterface
	, public IGameplayTagAssetInterface
	, public IGenericTeamAgentInterface
	, public IAZ_JumpRequester
	, public IAZ_CombatAvatar
{
	GENERATED_BODY()

public:
	AAZ_CmcCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return nullptr; }

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }

	virtual void SetJumpPressed(bool bPressed) override;
	virtual bool CanRequestJump() const override { return CanJump(); }

	virtual USkeletalMeshComponent* GetCombatMesh() const override { return GetMesh(); }

	UFUNCTION(BlueprintCallable, Category = "AZ|Movement")
	void SetGait(EAZ_Gait NewGait);

	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	EAZ_Gait GetCurrentGait() const { return CurrentGait; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	bool IsJustLanded() const { return bJustLanded; }

	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	FVector GetLandVelocity() const { return LandVelocity; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	FRotator GetAimRotation() const;


	UFUNCTION(BlueprintCallable, Category = "AZ|Anim")
	virtual void FillAnimContract(FAZ_CmcAnimContract& Out) const;

protected:
	void WireModularMeshFollowers();

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Pawn")
	TArray<FName> ModularFollowerExclusions { FName("Face") };

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float WalkSpeed = 165.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float RunSpeed = 375.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float SprintSpeed = 585.f;


	EAZ_Gait SelectionGait = EAZ_Gait::Walk;

	bool bStopBandLatched = false;

	EAZ_Gait LatchedStopBand = EAZ_Gait::Walk;


	bool bStopActive = false;

	bool bStopIsAnimated = false;

	float StopEntrySpeed = 0.f;

	FVector StopDirection = FVector::ZeroVector;

	FVector StopStartLocation = FVector::ZeroVector;

	float StopPlannedDistance = 0.f;

	float StopBrakingDecel = 0.f;

	float StopElapsed = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopTimeSeconds = 0.93f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "cm/s"))
	float StopAnimEnterSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopFloorTimeSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop")
	bool bStopTimeBraking = true;

	float GetStopRemainingDistance() const;

	float GetStopProgress() const;

	EAZ_Gait BandForSpeed(float Speed2D) const;

	void UpdateSelectionGait();

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float CrouchSpeed = 90.f;

	EAZ_Gait CurrentGait = EAZ_Gait::Run;


	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement", meta = (ForceUnits = "s"))
	float JustLandedDuration = 0.3f;

	bool bJustLanded = false;
	FVector LandVelocity = FVector::ZeroVector;
	FTimerHandle JustLandedTimer;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 0;

	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
