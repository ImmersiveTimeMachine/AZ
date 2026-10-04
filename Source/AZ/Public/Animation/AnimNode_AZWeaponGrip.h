// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AZWeaponGrip.generated.h"

class UAnimSequence;
class UAZ_WeaponGripField;

/**
 * Per-weapon grip markers, relative to the bone the weapon hangs on (gathered by the anim instance from the weapon's
 * sockets every frame, see AAZ_Weapon::GetGripMarkersInBone).
 *   - Surface field : the weapon's baked signed-distance lattice + where the weapon mesh sits relative to the bone.
 *                     When present the fingers close on the surface in real time (the markers below are not used).
 *   - Fingertip pads: sockets Grip_<L|R>_<Thumb|Index|Middle|Ring|Pinky>; index = Side * 5 + Finger (Side 0 = left).
 *   - Stock capsule : sockets StockFront -> StockButt, radius AAZ_Weapon::StockRadius (weapon-vs-body clearance, and the
 *                     arm fallback when the weapon has no surface field).
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

	/** Baked signed-distance lattice of the weapon mesh (AAZ_Weapon::GripField); null = marker IK only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	TObjectPtr<UAZ_WeaponGripField> Field = nullptr;

	/** The weapon mesh (the field's space) relative to the bone the weapon hangs on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FTransform FieldInBone = FTransform::Identity;

	/** Right-hand re-grip of this weapon in hand_r's own space (HandNew = RightHandCorrection * HandAnimated): the hand
	 *  moves onto the weapon's natural hold while the weapon keeps its animated place (AAZ_Weapon::RightHandGripCorrection,
	 *  applied by AZ Weapon Body Clearance). Identity = the animation's hold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FTransform RightHandCorrection = FTransform::Identity;

	/** The grip pose's right-hand fingers are the solved grasp of the re-gripped hand (AAZ_Weapon::bBakedRightHandGrasp):
	 *  AZ Weapon Grip applies them as they are, ring / pinky metacarpals included, with no real-time fitting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	bool bBakedRightFingers = false;

	/** Same for the left (support) hand, whose hold is the weapon's LeftHandGrip socket (AAZ_Weapon::bBakedLeftHandGrasp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	bool bBakedLeftFingers = false;
};

/**
 * The left hand during a weapon SWITCH (UAZ_MoverAnimInstance::GetWeaponSwitchReach, game thread; see
 * AZ_MoverAnimInstance_WeaponReach.cpp and docs/design-briefs/left-hand-weapon-switch-h1-h2-checkpoint.md section 5).
 *
 * The switch clip moves the arm; the grip only closes on the weapon where the clip's own hand touches it (draw) and lets
 * go where the clip's hand leaves it (holster): Alpha is the exact switch clip's AZ_Grip_L at its montage time, read from
 * the clip itself - the pose's AZ_Grip_L is not used meanwhile (upper-body layers fading out at the switch start
 * override the clip's curve with their own 1). Ownership mixes that with the normal grip (GripAlpha x AZ_Grip_L), so a
 * change of owner never jumps. The master GripAlpha (right hand, body clearance) is never changed by the switch.
 * Any correction of the arm's path belongs in the switch clip itself (baked offline), not here.
 */
struct FAZ_WeaponSwitchReach
{
	/** 0..1: how much the switch owns the left hand (eased by the anim instance; 0 = the normal grip). */
	float Ownership = 0.f;
	/** IK weight of the left hand onto the weapon's grip while the switch owns it (0 = the clip's own hand). */
	float Alpha = 0.f;

	// ---- the switch clip on the WHOLE body (AZ Weapon Switch Full Body): a crouched switch while standing still plays
	// the clip's own legs and pelvis - it was authored as a full-body motion (see AnimNode_AZWeaponSwitchFullBody.h)
	const class UAnimSequence* FullBodyClip = nullptr;
	float FullBodyTime = 0.f;
	float FullBodyAlpha = 0.f;

	// ---- recording (az.Weapon.RecordSwitch 1): the node writes its INPUT pose every frame of a switch phase
	bool bRecord = false;
	FGuid RecordPhaseId;
	FName RecordClip = NAME_None;
	FName RecordWeapon = NAME_None;
	float RecordClipTime = 0.f;
	float RecordMontageWeight = 0.f;
	bool bRecordHolster = false;
	bool bRecordCrouching = false;
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
 *   - SOLVED RIGHT GRASP (Markers.bBakedRightFingers): the grip pose's right fingers were solved offline for the
 *     re-gripped hand (anatomical joints, the real skin against the weapon, Tools/wgs/natgrip): applied as they are,
 *     ring / pinky metacarpals included; no real-time fitting for that hand.
 *   - The RIGHT ARM is not touched here: the re-grip (Markers.RightHandCorrection) and keeping the arm out of the weapon
 *     (elbow + wrist) are AZ Weapon Body Clearance's job, which runs before this node.
 *   - Per-hand weight = GripAlpha x the animation curve AZ_Grip_L / AZ_Grip_R (1 = that hand holds the weapon,
 *     baked into clips by Tools/az_grip_apply.py MODE curves), or DefaultHandAlpha when the playing animation has
 *     no such curve.
 *   - WEAPON SWITCH: the switch owns the left hand (FAZ_WeaponSwitchReach): the clip's own grip timing, left hand only.
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

	/** Real-time fit of the AUTHORED grip pose to the weapon's surface (Markers.Field). The grip pose gives the shape
	 *  (index on the trigger, fingers around the wrist, ...); every frame each finger keeps that shape and only curls
	 *  a little more or less as a whole (all its joints together) until its phalanges touch the surface without
	 *  entering it - so the hand stays natural AND on the weapon in every clip. 0 = off (grip pose as authored). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0", ClampMax = "1"))
	float FingerContactAlpha = 1.f;

	/** Most a finger may curl beyond the authored pose to reach the surface (deg, applied to each of its joints). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0", ClampMax = "60"))
	float FingerAdaptCloseDeg = 25.f;

	/** Most a finger may open from the authored pose to get out of the weapon (deg, applied to each of its joints). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0", ClampMax = "60"))
	float FingerAdaptOpenDeg = 30.f;

	/** Proximal / middle / distal phalanx radius (cm) of the four fingers (skin included). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact")
	FVector FingerRadii = FVector(0.85f, 0.75f, 0.65f);

	/** Proximal / middle / distal phalanx radius (cm) of the thumb. */
	UPROPERTY(EditAnywhere, Category = "Finger Contact")
	FVector ThumbRadii = FVector(1.0f, 0.85f, 0.75f);

	/** Skin compression allowed at a contact (cm). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0"))
	float ContactSkin = 0.15f;

	/** Fit step (deg); smaller = finer contact, more samples. */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0.25", ClampMax = "10"))
	float ContactStepDeg = 1.5f;

	/** A finger whose nearest phalanx is closer than this (cm) counts as touching and is not curled further. */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0"))
	float ContactTouchDistance = 0.1f;

	/** The tip joint moves this fraction of the base / middle joints' change (human DIP / PIP coupling). */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0", ClampMax = "1.5"))
	float DistalCoupling = 0.75f;

	/** Smoothing of the solved joint angles (1/s); 0 = none. */
	UPROPERTY(EditAnywhere, Category = "Finger Contact", meta = (ClampMin = "0"))
	float ContactInterpSpeed = 18.f;

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
		FQuat ParentGripRotation = FQuat::Identity;  // the metacarpal in the grip pose (palm arch of a solved grasp)
		bool bHasParentGrip = false;         // Parent is a metacarpal and the grip pose has it
	};

	FFingerChain Chains[NumSides][NumFingers];
	FBoneReference WeaponBone;
	bool bWeaponBoneDirty = true;

	/** The weapon switch of this update (copied from the anim instance in UpdateInternal). */
	FAZ_WeaponSwitchReach Reach;
	float LastSwitchOwnership = 0.f;

	/** az.Weapon.RecordSwitch: the input pose of one switch phase, written to Saved/NaturalGrip/SwitchRecordings when
	 *  the phase ends (the data the offline switch-arm solver bakes from). */
	TArray<FBoneReference> RecordBones;
	bool bRecordBonesDirty = true;
	FGuid RecordingPhase;
	FName RecordingClip = NAME_None;
	FString RecordingHeader;
	TArray<FString> RecordingFrames;
	double RecordingStartTime = 0.0;
	void FlushRecording();


	/** The grip pose whose finger rotations are cached in Chains (re-sampled when GripPose changes). */
	TWeakObjectPtr<UAnimSequence> CachedGripPose;
	float LastLeftAlpha = 0.f;
	float LastRightAlpha = 0.f;

	/** +1 / -1 per side: which side of the (index_01 - hand) x (pinky_01 - hand) plane the fingers curl toward. */
	float PalmSign[NumSides] = { 1.f, 1.f };
	bool bPalmSignReady[NumSides] = { false, false };
	/** Smoothed fit angle (rad) per side / finger: how much the whole finger curls beyond (+) / opens from (-) the pose. */
	mutable float ContactAngles[NumSides][NumFingers][2] = {};
	mutable bool bContactAnglesValid[NumSides][NumFingers] = {};
	mutable int32 LastContactFingers = 0;

	void CacheGripPose();
	/** Real-time closing of one finger on Markers.Field. False when the finger cannot be solved (no field / bones). */
	bool SolveFingerContact(int32 Side, int32 Finger, const FVector& PalmNormalCS, const FTransform& ParentCS,
		const FVector Translations[3], const FQuat& ClipBaseRotation, const FTransform& WeaponCS, float DeltaSeconds,
		FQuat OutRotations[3]) const;
	float HandAlpha(const FBlendedCurve& Curve, FName CurveName) const;
	void AppendFingers(int32 Side, float GripBlendFactor, const FTransform& HandCS, const FTransform& WeaponCS, bool bWeaponValid,
		FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) const;
	/** Two-parameter flexion IK of one finger: returns the three local rotations that put its pad on TargetCS. */
	void SolveFingerIK(const FFingerChain& Chain, const FTransform& ParentCS, const FVector Translations[3],
		const FVector& TargetCS, FQuat OutRotations[3]) const;
};
