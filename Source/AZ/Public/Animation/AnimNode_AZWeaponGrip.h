// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AZWeaponGrip.generated.h"

class UAnimSequence;

/**
 * Per-weapon grip markers, relative to the bone the weapon hangs on (gathered by the anim instance from the weapon's
 * sockets every frame, see AAZ_Weapon::GetGripMarkersInBone).
 *   - Fingertip pads: sockets Grip_<L|R>_<Thumb|Index|Middle|Ring|Pinky>; index = Side * 5 + Finger (Side 0 = left).
 *   - Stock capsule : sockets StockFront -> StockButt, radius AAZ_Weapon::StockRadius (arm-vs-stock avoidance).
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponGripMarkers
{
	GENERATED_BODY()

	/** 10 fingertip targets (see the struct comment for the order); entry i is used when bit (1 << i) of FingerMask is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	TArray<FVector> FingerTargets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	int32 FingerMask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector StockFront = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector StockButt = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float StockRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	bool bHasStock = false;
};

/**
 * Weapon grip - holds any weapon the way its grip data says, on top of any animation.
 *
 *   - LEFT HAND: two-bone IK (upperarm_l / lowerarm_l / hand_l) onto the weapon's grip, position AND rotation.
 *     The target is given relative to the bone the weapon hangs on (LeftHandInWeaponBone, WeaponBoneName) and is
 *     resolved against the CURRENT pose's component-space transform of that bone, so the hand never lags the
 *     weapon (reloads move the weapon through az_weapon_r every frame).
 *   - FINGERS of both hands: local rotations from the weapon's 1-frame grip pose (solved on the weapon's surface
 *     by Tools/az_grip_solve2.py), blended over the animation's fingers.
 *   - FINGER IK (real time): every finger whose fingertip marker exists on the weapon (Markers) is re-bent so its pad
 *     lands on the marker. Only natural flexion: each joint turns about its own flexion axis (the grip pose relative
 *     to the reference pose), two parameters per finger (base, middle; tip follows the middle in the grip pose's
 *     ratio). The grip pose is the start and the fallback.
 *   - ARM vs STOCK: the right arm swings about the shoulder->hand axis (hand and weapon stay put) until the forearm
 *     and upper arm, with sleeve thickness, clear the stock capsule (Markers.StockFront/StockButt).
 *   - Per-hand weight = GripAlpha x the animation curve AZ_Grip_L / AZ_Grip_R (1 = that hand holds the weapon,
 *     baked into clips by Tools/az_grip_apply.py MODE curves), or DefaultHandAlpha when the playing animation has
 *     no such curve.
 *
 * Bones default to the MetaHuman names; the grip pose may live on a compatible skeleton (bones matched by name).
 */
USTRUCT(BlueprintInternalUseOnly)
struct AZ_API FAnimNode_AZWeaponGrip : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 1-frame pose whose finger bones hold this weapon (per weapon, e.g. AS_Grip_Winchester). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (PinShownByDefault))
	TObjectPtr<UAnimSequence> GripPose = nullptr;

	/** hand_l target relative to WeaponBoneName = weapon attach socket * the weapon mesh's LeftHandGrip socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (PinShownByDefault))
	FTransform LeftHandInWeaponBone = FTransform::Identity;

	/** The bone the weapon is attached to (its attach socket's bone, e.g. az_weapon_r). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (PinShownByDefault))
	FName WeaponBoneName = NAME_None;

	/** Master weight: weapon in hands, eased in/out by the anim instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (PinShownByDefault, ClampMin = "0", ClampMax = "1"))
	float GripAlpha = 0.f;

	/** Fingertip + stock markers of the held weapon, relative to WeaponBoneName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (PinShownByDefault))
	FAZ_WeaponGripMarkers Markers;

	/** Weight of the fingertip IK over the grip pose's fingers (0 = grip pose only). */
	UPROPERTY(EditAnywhere, Category = "Finger IK", meta = (ClampMin = "0", ClampMax = "1"))
	float FingerIKAlpha = 1.f;

	/** Fingertip pad = end of the distal bone, extrapolated by this fraction of the middle->distal bone offset. */
	UPROPERTY(EditAnywhere, Category = "Finger IK", meta = (ClampMin = "0"))
	float FingertipExtension = 0.85f;

	/** How far (deg) the IK may bend a joint away from the grip pose. */
	UPROPERTY(EditAnywhere, Category = "Finger IK", meta = (ClampMin = "0"))
	float FingerIKMaxChangeDeg = 45.f;

	/** Forearm radius including the sleeve (cm) for the arm-vs-stock avoidance. */
	UPROPERTY(EditAnywhere, Category = "Arm vs Stock", meta = (ClampMin = "0"))
	float ForearmRadius = 5.f;

	/** Upper arm radius including the sleeve (cm). */
	UPROPERTY(EditAnywhere, Category = "Arm vs Stock", meta = (ClampMin = "0"))
	float UpperArmRadius = 6.f;

	/** Part of the forearm (from the elbow) tested against the stock: the wrist holds the stock, so it must not count. */
	UPROPERTY(EditAnywhere, Category = "Arm vs Stock", meta = (ClampMin = "0", ClampMax = "1"))
	float ForearmTestFraction = 0.7f;

	/** Largest elbow swing (deg) about the shoulder->hand axis. */
	UPROPERTY(EditAnywhere, Category = "Arm vs Stock", meta = (ClampMin = "0", ClampMax = "90"))
	float MaxElbowSwingDeg = 45.f;

	UPROPERTY(EditAnywhere, Category = "Grip")
	FName LeftCurveName = TEXT("AZ_Grip_L");

	UPROPERTY(EditAnywhere, Category = "Grip")
	FName RightCurveName = TEXT("AZ_Grip_R");

	/** Hand weight when the playing animation carries no grip curve (e.g. a set without RifleMega grip data). */
	UPROPERTY(EditAnywhere, Category = "Grip", meta = (ClampMin = "0", ClampMax = "1"))
	float DefaultHandAlpha = 1.f;

	/** Elbow pole: pushed this far (cm) out of the shoulder-hand line along the input pose's own elbow direction. */
	UPROPERTY(EditAnywhere, Category = "Grip", meta = (ClampMin = "1"))
	float ElbowPoleDistance = 30.f;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference LeftUpperArm;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference LeftLowerArm;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference LeftHand;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightHand;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightUpperArm;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightLowerArm;

	FAnimNode_AZWeaponGrip();

	// FAnimNode_Base
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
	// FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	static constexpr int32 NumSides = 2;     // 0 = left, 1 = right
	static constexpr int32 NumFingers = 5;   // thumb, index, middle, ring, pinky

	struct FFingerChain
	{
		FBoneReference Parent;               // <finger>_metacarpal_<side>, or hand_<side> for the thumb
		FBoneReference Bones[3];             // <finger>_01/02/03_<side>
		FQuat GripRotation[3] = { FQuat::Identity, FQuat::Identity, FQuat::Identity };
		bool bHasGrip = false;               // the grip pose has all three bones
		// Finger IK: joint rotation(phi) = RefRotation * Quat(FlexAxis, phi); the grip pose sits at phi = GripAngle.
		FQuat RefRotation[3] = { FQuat::Identity, FQuat::Identity, FQuat::Identity };
		FVector FlexAxis[3] = { FVector::UnitZ(), FVector::UnitZ(), FVector::UnitZ() };
		float GripAngle[3] = { 0.f, 0.f, 0.f };
		float DistalRatio = 0.75f;           // phi(03) = DistalRatio * phi(02)
		bool bIKReady = false;               // flexion axes are known (the grip pose bends this finger)
	};

	FFingerChain Chains[NumSides][NumFingers];
	FBoneReference WeaponBone;
	bool bWeaponBoneDirty = true;

	/** The grip pose whose finger rotations are cached in Chains (re-sampled when GripPose changes). */
	TWeakObjectPtr<UAnimSequence> CachedGripPose;
	float LastLeftAlpha = 0.f;
	float LastRightAlpha = 0.f;

	float LastElbowSwingDeg = 0.f;

	void CacheGripPose();
	float HandAlpha(const FBlendedCurve& Curve, FName CurveName) const;
	void AppendFingers(int32 Side, float GripBlendFactor, const FTransform& HandCS, const FTransform& WeaponCS, bool bWeaponValid,
		FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) const;
	/** Two-parameter flexion IK of one finger: returns the three local rotations that put its pad on TargetCS. */
	void SolveFingerIK(const FFingerChain& Chain, const FTransform& ParentCS, const FVector Translations[3],
		const FVector& TargetCS, FQuat OutRotations[3]) const;
	/** Right arm swing about the shoulder->hand axis so the arm clears the stock. Returns the applied angle (deg). */
	float AvoidStockWithRightArm(float Weight, const FTransform& WeaponCS, FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms) const;
};
