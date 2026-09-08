// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_WeaponAnimationProfile.generated.h"

class UBlendSpace;
class UAnimSequence;
class UPoseSearchDatabase;

/** Weapon animation tuning and optional pools; the existing main CHT remains the only clip selector. */
UCLASS(BlueprintType)
class AZ_API UAZ_WeaponAnimationProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Null pools retain the main chooser's selected clip; weapon profiles never borrow an unarmed database. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> WalkLocoDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> RunLocoDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeWalkDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeRunDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeCrouchDatabase = nullptr;

	/** Enable only after verifying that this profile's transitions end at their matching loop's frame zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bPhaseLockedTransitionToLoop = false;

	/** The current MHC graph contains unarmed additive lean assets. Most weapon profiles must bypass them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bUseUnarmedLeans = false;

	/** THREE-PHASE JUMP. Enable for profiles whose takeoff clips are CLOSED (a baked landing and only a few
	 *  tenths of a second of air, like the P01 rm_W2_*_Jump pack): at the takeoff's apex handoff the SM
	 *  advances to InAirLoop and the chooser's InAirLoop rows push a LOOPING air cycle for the rest of the
	 *  fall, so a drop longer than the takeoff clip cannot freeze on its last frame. Touchdown still selects
	 *  the land clip on real floor contact, unchanged.
	 *  False = the unarmed TWO-PHASE jump: TransitionToInAir persists for the whole airborne duration and the
	 *  takeoff clip's own open-ended fall tail covers the descent.
	 *  Requires InAirLoop rows in CHT_v2 matching this profile's weapon, and those rows' clips must LOOP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bUseAirLoop = false;

	/** Opt in only after verifying the listed moving loops' depicted-speed curves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	bool bUseLoopPlayRate = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	FName SpeedCurveName = TEXT("Speed");
	/** Explicit moving-loop ownership; idle curves can contain nonzero values and must not qualify. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	TArray<TObjectPtr<UAnimSequence>> PlayRateLoopAssets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0.01"))
	float LoopPlayRateMin = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0.01"))
	float LoopPlayRateMax = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LoopPlayRateMinDepictedSpeed = 1.f;

	/** Optional authored lowered-weapon pose, layered above spine_02 during relaxed locomotion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Relaxed")
	TObjectPtr<UAnimSequence> RelaxedUpperBodyPose = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Relaxed", meta = (ClampMin = "0"))
	float RelaxedPoseBlendSpeed = 8.f;

	/** Mesh-space additive aim offsets. X = local aim yaw, Y = local aim pitch, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UBlendSpace> StandingAimOffset = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UBlendSpace> CrouchingAimOffset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimInterpSpeed = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimBlendInSpeed = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimBlendOutSpeed = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxAimYaw = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float MaxAimPitch = 90.f;
};
