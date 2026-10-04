// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Animation/AnimNode_AZWeaponGrip.h"   // FAZ_WeaponGripMarkers
#include "AnimNode_AZWeaponBodyClearance.generated.h"

class UPhysicsAsset;

/**
 * Weapon body clearance - keeps the held weapon out of the character's body.
 *
 *   The weapon's stock (Markers.StockFront -> Markers.StockButt, Markers.StockRadius, relative to WeaponBoneName) is
 *   tested against the body capsules of the character's Physics Asset (torso, clavicles, neck, head, pelvis, thighs;
 *   radii scaled to the real body). When the stock is inside, the RIGHT HAND - the weapon hangs on it - is moved by the
 *   smallest push that clears it; the right arm follows by two-bone IK, the hand keeps its rotation, the weapon rides
 *   the hand. While aiming (ShoulderContactAlpha) the butt end may press ButtPocketDepth into the shoulder pocket.
 *
 *   Then the RIGHT ARM is kept out of the weapon (bSolveRightArm, needs Markers.Field): the forearm and upper arm,
 *   tapered capsules with the sleeve, are sampled against the weapon's exact surface; the elbow swings out and the
 *   wrist turns the hand + weapon, the cheapest pair that clears (see the Arm settings).
 *
 *   Place it BEFORE AZ Weapon Grip: the left hand and the fingers then hold the weapon where this node put it.
 *   Inert without stock markers (weapons without StockFront / StockButt sockets, e.g. the M16 and the pistol today).
 */
USTRUCT(BlueprintInternalUseOnly)
struct AZ_API FAnimNode_AZWeaponBodyClearance : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** Stock markers of the held weapon relative to WeaponBoneName (bind to the anim instance's WeaponGripMarkers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (PinShownByDefault))
	FAZ_WeaponGripMarkers Markers;

	/** The bone the weapon hangs on (bind to WeaponGripBone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (PinShownByDefault))
	FName WeaponBoneName = NAME_None;

	/** Master weight: the weapon is in the hands (bind to WeaponGripAlpha). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (PinShownByDefault, ClampMin = "0", ClampMax = "1"))
	float ClearanceAlpha = 0.f;

	/** 1 while aiming (bind to AimAlpha): the butt end may rest in the shoulder pocket instead of being pushed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (PinShownByDefault, ClampMin = "0", ClampMax = "1"))
	float ShoulderContactAlpha = 0.f;

	/** Physics Asset capsules are thicker than the body (clothes, simulation margin); scale their radii by this. */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float BodyRadiusScale = 0.8f;

	/** Extra gap (cm) kept between the stock and the body. */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float Margin = 0.3f;

	/** Bodies of the Physics Asset that count as "the body" (others - arms, hands, lower legs - are ignored). */
	UPROPERTY(EditAnywhere, Category = "Body")
	TArray<FName> BodyBones = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"),
		TEXT("spine_05"), TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("neck_01"), TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r") };

	/** Bodies forming the shoulder pocket: while aiming the butt end may press into them by ButtPocketDepth. */
	UPROPERTY(EditAnywhere, Category = "Body")
	TArray<FName> ShoulderPocketBones = { TEXT("clavicle_r"), TEXT("spine_04"), TEXT("spine_05") };

	/** Length (cm) of the butt end that may press into the shoulder pocket while aiming. */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float ButtContactLength = 10.f;

	/** How deep (cm) the butt end may press into the shoulder pocket while aiming (clothes and soft tissue give);
	 *  deeper than that the weapon is pushed out like anywhere else. */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float ButtPocketDepth = 1.f;

	/** Treat the weapon as SHOULDERED whenever the incoming animation already holds the butt end at the shoulder
	 *  pocket, aiming or not (sets that carry the weapon shouldered all the time - the Winchester on RifleMega Rifle01,
	 *  2026-09-28): the pocket is allowed and the wrist is not turned, exactly as while aiming. Sprint / reload poses
	 *  (butt away from the shoulder) fall out by themselves. */
	UPROPERTY(EditAnywhere, Category = "Body")
	bool bDetectShoulderedFromPose = true;

	/** Butt-end-to-pocket gap (cm, surface to surface) at or below which the weapon counts as fully shouldered ... */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float ShoulderedNearCm = 3.f;

	/** ... and at or above which it counts as not shouldered (eased in between). */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float ShoulderedFarCm = 10.f;

	/** Largest push (cm) of the right hand / weapon. */
	UPROPERTY(EditAnywhere, Category = "Push", meta = (ClampMin = "0"))
	float MaxPushCm = 12.f;

	/** Smoothing of the push (1/s); 0 = immediate. Only takes the edge off single-frame changes: the push follows the
	 *  pose, and a slower value showed as the weapon sliding into place after the pose was already there. */
	UPROPERTY(EditAnywhere, Category = "Push", meta = (ClampMin = "0"))
	float PushInterpSpeed = 40.f;

	/** Right arm pole: pushed this far (cm) along the input pose's own elbow direction. */
	UPROPERTY(EditAnywhere, Category = "Push", meta = (ClampMin = "1"))
	float ElbowPoleDistance = 30.f;

	/** Weight curve of the right hand on the weapon (1 = the hand holds its grip; baked into the clips, see AZ Weapon
	 *  Grip). The re-grip (Markers.RightHandCorrection) follows it, so a reload hand that leaves the grip is left alone.
	 *  A clip without the curve counts as 1. */
	UPROPERTY(EditAnywhere, Category = "Re-grip")
	FName RightGripCurveName = TEXT("AZ_Grip_R");

	/** Keep the right arm out of the weapon, measured on the weapon's exact surface (Markers.Field): the elbow swings
	 *  out (about the shoulder->hand axis, always away from the torso) and the wrist turns the hand + weapon (muzzle
	 *  toward the arm's side, never while aiming) - the cheapest combination that clears the arm, every frame. */
	UPROPERTY(EditAnywhere, Category = "Arm")
	bool bSolveRightArm = true;

	/** Arm thickness with the sleeve (cm): upper arm at the shoulder. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0"))
	float ArmShoulderRadius = 5.5f;

	/** Arm thickness with the sleeve (cm) at the elbow. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0"))
	float ArmElbowRadius = 4.3f;

	/** Forearm thickness with the sleeve (cm) at the wrist. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0"))
	float ArmWristRadius = 3.f;

	/** Forearm length (cm) next to the wrist that may touch the weapon: the hand holds it there, the cuff rests on it. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0"))
	float ArmGripZone = 7.f;

	/** Sleeve contact (cm) accepted anywhere on the arm. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0"))
	float ArmContact = 0.5f;

	/** The upper-arm test starts this far along shoulder->elbow ... */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0", ClampMax = "1"))
	float UpperArmTestStart = 0.35f;

	/** ... and this far while aiming: the butt legitimately rests against the inner upper arm next to the shoulder. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0", ClampMax = "1"))
	float UpperArmTestStartAiming = 0.5f;

	/** Largest elbow swing (deg). */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0", ClampMax = "120"))
	float MaxElbowSwingDeg = 75.f;

	/** Largest wrist turn of the hand + weapon (deg); scaled to 0 while aiming (the weapon stays on the sight line). */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "0", ClampMax = "60"))
	float MaxWristTurnDeg = 30.f;

	/** The correction PATH: elbow swing and wrist turn grow together in the ratio ElbowSwingCostDeg : WristTurnCostDeg,
	 *  and the first point on that path that clears the arm is used (unique, so it never flips between two solutions;
	 *  a 2-D cost minimum twitched, 2026-09-28). Measured on the M16 relaxed set with the Winchester: 40 : 15 gives
	 *  the standing idle ~50 deg of elbow + ~19 of wrist, walking ~42 + ~16, running ~22 + ~8, crouching ~28 + ~11. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "1"))
	float ElbowSwingCostDeg = 40.f;

	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "1"))
	float WristTurnCostDeg = 15.f;

	/** Largest change speed of the applied angles (deg/s). The solution follows the pose (no easing); this only stops a
	 *  single-frame pop when the solution itself jumps. */
	UPROPERTY(EditAnywhere, Category = "Arm", meta = (ClampMin = "1"))
	float ArmMaxRateDegPerSec = 720.f;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightUpperArm;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightLowerArm;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference RightHand;

	/** Torso reference for "away from the body" (elbow swing side, wrist turn side). */
	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference TorsoBone;

	FAnimNode_AZWeaponBodyClearance();

	// FAnimNode_Base
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
	// FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	struct FBodyCapsule
	{
		FBoneReference Bone;
		FVector A = FVector::ZeroVector;     // bone space
		FVector B = FVector::ZeroVector;
		float Radius = 0.f;
		bool bShoulderPocket = false;
	};

	TArray<FBodyCapsule> Capsules;
	TWeakObjectPtr<const UPhysicsAsset> CachedPhysicsAsset;
	bool bCapsulesDirty = true;
	FBoneReference WeaponBone;
	bool bWeaponBoneDirty = true;

	FVector CurrentPush = FVector::ZeroVector;
	float LastPenetration = 0.f;
	FName LastPenetrationBone = NAME_None;

	/** Applied (signed) right-arm corrections and what the solver saw. */
	float ArmSwingDeg = 0.f;
	float ArmWristDeg = 0.f;
	float ElbowOutSign = 0.f;
	float LastArmPenetration = 0.f;   // before the correction (cm, > 0 = arm inside the weapon beyond the contact)
	float LastArmResidual = 0.f;      // after the solved correction
	int32 LastArmEvaluations = 0;
	float DetectedShoulderAlpha = 0.f;   // eased "the animation holds the butt at the shoulder"
	float LastShoulderAlpha = 0.f;       // effective: max(ShoulderContactAlpha, DetectedShoulderAlpha)

	void BuildCapsules(const FBoneContainer& RequiredBones, const UPhysicsAsset* PhysicsAsset);
};
