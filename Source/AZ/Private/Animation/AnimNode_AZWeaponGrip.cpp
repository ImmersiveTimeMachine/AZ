// Copyright Artur. AZ project.

#include "Animation/AnimNode_AZWeaponGrip.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStats.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "AZ_ConsoleVariables.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AZWeaponGrip)

namespace AZWeaponGrip
{
	static const TCHAR* const Sides[] = { TEXT("l"), TEXT("r") };
	static const TCHAR* const Fingers[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
}

FAnimNode_AZWeaponGrip::FAnimNode_AZWeaponGrip()
{
	LeftUpperArm.BoneName = TEXT("upperarm_l");
	LeftLowerArm.BoneName = TEXT("lowerarm_l");
	LeftHand.BoneName = TEXT("hand_l");
	RightHand.BoneName = TEXT("hand_r");
	RightUpperArm.BoneName = TEXT("upperarm_r");
	RightLowerArm.BoneName = TEXT("lowerarm_r");
	for (int32 Side = 0; Side < NumSides; ++Side)
	{
		for (int32 Finger = 0; Finger < NumFingers; ++Finger)
		{
			FFingerChain& Chain = Chains[Side][Finger];
			Chain.Parent.BoneName = Finger == 0
				? FName(*FString::Printf(TEXT("hand_%s"), AZWeaponGrip::Sides[Side]))
				: FName(*FString::Printf(TEXT("%s_metacarpal_%s"), AZWeaponGrip::Fingers[Finger], AZWeaponGrip::Sides[Side]));
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				Chain.Bones[Joint].BoneName = FName(*FString::Printf(TEXT("%s_0%d_%s"), AZWeaponGrip::Fingers[Finger], Joint + 1, AZWeaponGrip::Sides[Side]));
			}
		}
	}
}

void FAnimNode_AZWeaponGrip::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Grip %s on %s, alpha %.2f, L %.2f R %.2f, finger markers 0x%03x, elbow swing %.1f deg)"),
		*GetNameSafe(GripPose), *WeaponBoneName.ToString(), GripAlpha, LastLeftAlpha, LastRightAlpha, Markers.FingerMask,
		LastElbowSwingDeg);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_AZWeaponGrip::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	LeftUpperArm.Initialize(RequiredBones);
	LeftLowerArm.Initialize(RequiredBones);
	LeftHand.Initialize(RequiredBones);
	RightHand.Initialize(RequiredBones);
	RightUpperArm.Initialize(RequiredBones);
	RightLowerArm.Initialize(RequiredBones);
	for (int32 Side = 0; Side < NumSides; ++Side)
	{
		for (int32 Finger = 0; Finger < NumFingers; ++Finger)
		{
			FFingerChain& Chain = Chains[Side][Finger];
			Chain.Parent.Initialize(RequiredBones);
			for (FBoneReference& Bone : Chain.Bones)
			{
				Bone.Initialize(RequiredBones);
			}
		}
	}
	WeaponBone.BoneName = WeaponBoneName;
	WeaponBone.Initialize(RequiredBones);
	bWeaponBoneDirty = false;
}

void FAnimNode_AZWeaponGrip::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	// The weapon can change (and so its attach bone) while the bone container stays the same.
	if (WeaponBone.BoneName != WeaponBoneName)
	{
		WeaponBone.BoneName = WeaponBoneName;
		bWeaponBoneDirty = true;
	}
	if (CachedGripPose.Get() != GripPose)
	{
		CacheGripPose();
	}
}

void FAnimNode_AZWeaponGrip::CacheGripPose()
{
	CachedGripPose = GripPose;
	const USkeleton* PoseSkeleton = GripPose ? GripPose->GetSkeleton() : nullptr;
	for (int32 Side = 0; Side < NumSides; ++Side)
	{
		for (int32 Finger = 0; Finger < NumFingers; ++Finger)
		{
			FFingerChain& Chain = Chains[Side][Finger];
			Chain.bHasGrip = PoseSkeleton != nullptr;
			Chain.bIKReady = false;
			float Angle[3] = { 0.f, 0.f, 0.f };
			for (int32 Joint = 0; Joint < 3 && Chain.bHasGrip; ++Joint)
			{
				// The grip pose may be authored on a compatible skeleton (SK_AZ_Master): bones are matched by name.
				const FReferenceSkeleton& RefSkeleton = PoseSkeleton->GetReferenceSkeleton();
				const int32 SkeletonIndex = RefSkeleton.FindBoneIndex(Chain.Bones[Joint].BoneName);
				if (SkeletonIndex == INDEX_NONE)
				{
					Chain.bHasGrip = false;
					break;
				}
				FTransform Local;
				GripPose->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex), FAnimExtractContext(0.0), false);
				Chain.GripRotation[Joint] = Local.GetRotation().GetNormalized();

				// Flexion axis of this joint = the axis that turns the reference (open) pose into the grip pose (the grip
				// solver closes every joint about exactly such an axis).
				Chain.RefRotation[Joint] = RefSkeleton.GetRefBonePose()[SkeletonIndex].GetRotation().GetNormalized();
				FQuat Delta = (Chain.RefRotation[Joint].Inverse() * Chain.GripRotation[Joint]).GetNormalized();
				if (Delta.W < 0.0)
				{
					Delta = -Delta;
				}
				FVector Axis;
				float DeltaAngle = 0.f;
				Delta.ToAxisAndAngle(Axis, DeltaAngle);
				Chain.FlexAxis[Joint] = Axis;
				Angle[Joint] = DeltaAngle;
			}
			if (!Chain.bHasGrip)
			{
				continue;
			}
			// Joints the grip pose barely bends have no reliable axis of their own: they borrow the most bent joint's.
			const float MinAngle = FMath::DegreesToRadians(4.f);
			int32 Strongest = 0;
			for (int32 Joint = 1; Joint < 3; ++Joint)
			{
				Strongest = Angle[Joint] > Angle[Strongest] ? Joint : Strongest;
			}
			if (Angle[Strongest] < MinAngle)
			{
				continue;                                     // a straight finger: grip pose only, no IK
			}
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				if (Angle[Joint] < MinAngle)
				{
					Chain.FlexAxis[Joint] = Chain.FlexAxis[Strongest];
				}
				// Signed angle of the grip pose about the (possibly borrowed) axis.
				const FQuat Delta = (Chain.RefRotation[Joint].Inverse() * Chain.GripRotation[Joint]).GetNormalized();
				Chain.GripAngle[Joint] = 2.f * FMath::Atan2(FVector(Delta.X, Delta.Y, Delta.Z) | Chain.FlexAxis[Joint], Delta.W);
			}
			Chain.DistalRatio = FMath::Abs(Chain.GripAngle[1]) > MinAngle
				? FMath::Clamp(Chain.GripAngle[2] / Chain.GripAngle[1], 0.3f, 1.3f) : 0.75f;
			Chain.bIKReady = true;
		}
	}
}

void FAnimNode_AZWeaponGrip::SolveFingerIK(const FFingerChain& Chain, const FTransform& ParentCS, const FVector Translations[3],
	const FVector& TargetCS, FQuat OutRotations[3]) const
{
	// Three natural degrees of freedom: base flexion, middle flexion (the tip follows it by DistalRatio) and base
	// spread (abduction about the palm normal = FlexAxis x finger direction, both in the base joint's rest frame).
	FVector SpreadAxis = Chain.FlexAxis[0] ^ Translations[1].GetSafeNormal();
	const bool bHasSpread = SpreadAxis.Normalize();
	auto Rotations = [&Chain, &SpreadAxis, bHasSpread](const double X[3], FQuat Out[3])
	{
		const FQuat Spread = bHasSpread ? FQuat(SpreadAxis, X[2]) : FQuat::Identity;
		Out[0] = (Chain.RefRotation[0] * Spread * FQuat(Chain.FlexAxis[0], X[0])).GetNormalized();
		Out[1] = (Chain.RefRotation[1] * FQuat(Chain.FlexAxis[1], X[1])).GetNormalized();
		Out[2] = (Chain.RefRotation[2] * FQuat(Chain.FlexAxis[2], Chain.DistalRatio * X[1])).GetNormalized();
	};
	auto Pad = [&](const double X[3])
	{
		FQuat Rot[3];
		Rotations(X, Rot);
		FTransform BoneCS = ParentCS;
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			BoneCS = FTransform(Rot[Joint], Translations[Joint]) * BoneCS;
		}
		return BoneCS.TransformPosition(Translations[2] * FingertipExtension);
	};

	// Limits: natural joint ranges (rad, relative to the open reference pose), narrowed by FingerIKMaxChangeDeg
	// around the grip pose when that setting is tighter.
	const double Change = FMath::DegreesToRadians(FMath::Max(FingerIKMaxChangeDeg, 1.f));
	const double Lo[3] = { FMath::Max(-0.35, Chain.GripAngle[0] - Change), FMath::Max(-0.10, Chain.GripAngle[1] - Change), bHasSpread ? -0.40 : 0.0 };
	const double Hi[3] = { FMath::Min(1.70, Chain.GripAngle[0] + Change), FMath::Min(1.95, Chain.GripAngle[1] + Change), bHasSpread ? 0.40 : 0.0 };
	double X[3] = { FMath::Clamp<double>(Chain.GripAngle[0], Lo[0], Hi[0]), FMath::Clamp<double>(Chain.GripAngle[1], Lo[1], Hi[1]), 0.0 };
	auto Cost = [&](const double P[3]) { return (Pad(P) - TargetCS).SizeSquared(); };
	double E = Cost(X);

	// Damped Gauss-Newton with backtracking: a step is taken only if it brings the pad closer, so the result is never
	// worse than the grip pose.
	constexpr double H = 0.01;
	for (int32 Iteration = 0; Iteration < 12 && E > 0.0025; ++Iteration)
	{
		const FVector P = Pad(X);
		const FVector Err = TargetCS - P;
		FVector J[3];
		for (int32 K = 0; K < 3; ++K)
		{
			double Xh[3] = { X[0], X[1], X[2] };
			Xh[K] += H;
			J[K] = (Pad(Xh) - P) / H;
		}
		double A[3][3];
		double G[3];
		for (int32 R = 0; R < 3; ++R)
		{
			for (int32 C = 0; C < 3; ++C)
			{
				A[R][C] = J[R] | J[C];
			}
			G[R] = J[R] | Err;
		}
		const double Damping = 1e-3 * (A[0][0] + A[1][1] + A[2][2]) + 1e-6;
		for (int32 R = 0; R < 3; ++R)
		{
			A[R][R] += Damping;
		}
		if (!bHasSpread)
		{
			A[2][0] = A[2][1] = A[0][2] = A[1][2] = 0.0;
			A[2][2] = 1.0;
			G[2] = 0.0;
		}
		// 3x3 solve (Cramer).
		const double Det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
			+ A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
		if (FMath::Abs(Det) < 1e-14)
		{
			break;
		}
		double D[3];
		for (int32 K = 0; K < 3; ++K)
		{
			double M[3][3];
			for (int32 R = 0; R < 3; ++R)
			{
				for (int32 C = 0; C < 3; ++C)
				{
					M[R][C] = C == K ? G[R] : A[R][C];
				}
			}
			const double DetK = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
				+ M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
			D[K] = FMath::Clamp(DetK / Det, -0.35, 0.35);
		}
		bool bImproved = false;
		for (double Step = 1.0; Step > 0.06; Step *= 0.5)
		{
			double Candidate[3];
			for (int32 K = 0; K < 3; ++K)
			{
				Candidate[K] = FMath::Clamp(X[K] + Step * D[K], Lo[K], Hi[K]);
			}
			const double CandidateCost = Cost(Candidate);
			if (CandidateCost < E)
			{
				X[0] = Candidate[0];
				X[1] = Candidate[1];
				X[2] = Candidate[2];
				E = CandidateCost;
				bImproved = true;
				break;
			}
		}
		if (!bImproved)
		{
			break;
		}
	}
	Rotations(X, OutRotations);
}

float FAnimNode_AZWeaponGrip::AvoidStockWithRightArm(float Weight, const FTransform& WeaponCS, FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms) const
{
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	if (!Markers.bHasStock || Weight <= UE_KINDA_SMALL_NUMBER || !RightUpperArm.IsValidToEvaluate(RequiredBones)
		|| !RightLowerArm.IsValidToEvaluate(RequiredBones) || !RightHand.IsValidToEvaluate(RequiredBones))
	{
		return 0.f;
	}
	const FCompactPoseBoneIndex UpperIndex = RightUpperArm.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex LowerIndex = RightLowerArm.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex HandIndex = RightHand.GetCompactPoseIndex(RequiredBones);
	FTransform UpperCS = Output.Pose.GetComponentSpaceTransform(UpperIndex);
	FTransform LowerCS = Output.Pose.GetComponentSpaceTransform(LowerIndex);
	const FTransform HandCS = Output.Pose.GetComponentSpaceTransform(HandIndex);
	const FVector StockA = WeaponCS.TransformPosition(Markers.StockFront);
	const FVector StockB = WeaponCS.TransformPosition(Markers.StockButt);
	if (AZCVars::GetWeaponDebug() >= 3)
	{
		const FTransform& ToWorld = Output.AnimInstanceProxy->GetComponentTransform();
		Output.AnimInstanceProxy->AnimDrawDebugLine(ToWorld.TransformPosition(StockA), ToWorld.TransformPosition(StockB), FColor::Yellow,
			false, -1.f, 2.f * Markers.StockRadius, SDPG_Foreground);
	}
	const FVector Shoulder = UpperCS.GetLocation();
	const FVector Elbow = LowerCS.GetLocation();
	const FVector Hand = HandCS.GetLocation();
	const FVector Axis = (Hand - Shoulder).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return 0.f;
	}

	// Clearance (cm, >= 0 = free) of the arm swung by Deg about the shoulder->hand axis. The wrist end of the forearm is
	// left out (ForearmTestFraction): the hand legitimately holds the stock's wrist.
	auto Clearance = [&](double Deg)
	{
		const FVector SwungElbow = Shoulder + FQuat(Axis, FMath::DegreesToRadians(Deg)).RotateVector(Elbow - Shoulder);
		const FVector ForearmEnd = SwungElbow + (Hand - SwungElbow) * ForearmTestFraction;
		FVector OnArm, OnStock;
		FMath::SegmentDistToSegmentSafe(SwungElbow, ForearmEnd, StockA, StockB, OnArm, OnStock);
		const double Forearm = FVector::Dist(OnArm, OnStock) - (ForearmRadius + Markers.StockRadius);
		FMath::SegmentDistToSegmentSafe(Shoulder, SwungElbow, StockA, StockB, OnArm, OnStock);
		const double Upper = FVector::Dist(OnArm, OnStock) - (UpperArmRadius + Markers.StockRadius);
		return FMath::Min(Forearm, Upper);
	};
	double BestClearance = Clearance(0.0);
	if (BestClearance >= 0.0)
	{
		return 0.f;
	}
	// Smallest swing that frees the arm; the side used last frame is tried first so the elbow never flips sides.
	const double PreferredSign = LastElbowSwingDeg < 0.f ? -1.0 : 1.0;
	double Best = 0.0;
	for (int32 Degrees = 1; Degrees <= FMath::FloorToInt(MaxElbowSwingDeg); ++Degrees)
	{
		bool bFree = false;
		for (const double Sign : { PreferredSign, -PreferredSign })
		{
			const double Candidate = Sign * Degrees;
			const double Value = Clearance(Candidate);
			if (Value > BestClearance)
			{
				BestClearance = Value;
				Best = Candidate;
			}
			if (Value >= 0.0)
			{
				bFree = true;
				break;
			}
		}
		if (bFree)
		{
			break;
		}
	}
	const double Applied = Best * Weight;
	if (FMath::IsNearlyZero(Applied))
	{
		return 0.f;
	}
	const FQuat Swing(Axis, FMath::DegreesToRadians(Applied));
	UpperCS.SetRotation((Swing * UpperCS.GetRotation()).GetNormalized());
	LowerCS.SetLocation(Shoulder + Swing.RotateVector(Elbow - Shoulder));
	LowerCS.SetRotation((Swing * LowerCS.GetRotation()).GetNormalized());
	OutBoneTransforms.Add(FBoneTransform(UpperIndex, UpperCS));
	OutBoneTransforms.Add(FBoneTransform(LowerIndex, LowerCS));
	OutBoneTransforms.Add(FBoneTransform(HandIndex, HandCS));    // the hand (and the weapon on it) stays exactly where it was
	return static_cast<float>(Applied);
}

bool FAnimNode_AZWeaponGrip::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return GripPose != nullptr
		&& GripAlpha > UE_KINDA_SMALL_NUMBER
		&& WeaponBoneName != NAME_None
		&& LeftUpperArm.IsValidToEvaluate(RequiredBones)
		&& LeftLowerArm.IsValidToEvaluate(RequiredBones)
		&& LeftHand.IsValidToEvaluate(RequiredBones)
		&& RightHand.IsValidToEvaluate(RequiredBones);
}

float FAnimNode_AZWeaponGrip::HandAlpha(const FBlendedCurve& Curve, FName CurveName) const
{
	bool bHasCurve = false;
	const float Value = Curve.Get(CurveName, bHasCurve, DefaultHandAlpha);
	return GripAlpha * FMath::Clamp(bHasCurve ? Value : DefaultHandAlpha, 0.f, 1.f);
}

void FAnimNode_AZWeaponGrip::AppendFingers(int32 Side, float GripBlendFactor, const FTransform& HandCS, const FTransform& WeaponCS,
	bool bWeaponValid, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) const
{
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex HandIndex = (Side == 0 ? LeftHand : RightHand).GetCompactPoseIndex(RequiredBones);
	for (int32 Finger = 0; Finger < NumFingers; ++Finger)
	{
		const FFingerChain& Chain = Chains[Side][Finger];
		if (!Chain.bHasGrip || !Chain.Parent.IsValidToEvaluate(RequiredBones)
			|| !Chain.Bones[0].IsValidToEvaluate(RequiredBones) || !Chain.Bones[1].IsValidToEvaluate(RequiredBones)
			|| !Chain.Bones[2].IsValidToEvaluate(RequiredBones))
		{
			continue;
		}
		// Parent in component space, re-derived from the (possibly IK-moved) hand: metacarpals keep their local.
		const FCompactPoseBoneIndex ParentIndex = Chain.Parent.GetCompactPoseIndex(RequiredBones);
		FTransform ParentCS = ParentIndex == HandIndex
			? HandCS
			: Output.Pose.GetLocalSpaceTransform(ParentIndex) * HandCS;

		// Target rotations: the grip pose, re-bent by the fingertip IK when the weapon has this finger's marker.
		FQuat TargetRotation[3] = { Chain.GripRotation[0], Chain.GripRotation[1], Chain.GripRotation[2] };
		const int32 MarkerIndex = Side * NumFingers + Finger;
		if (bWeaponValid && Chain.bIKReady && FingerIKAlpha > UE_KINDA_SMALL_NUMBER && (Markers.FingerMask & (1 << MarkerIndex)) != 0
			&& Markers.FingerTargets.IsValidIndex(MarkerIndex))
		{
			FVector Translations[3];
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				Translations[Joint] = Output.Pose.GetLocalSpaceTransform(Chain.Bones[Joint].GetCompactPoseIndex(RequiredBones)).GetTranslation();
			}
			FQuat Solved[3];
			SolveFingerIK(Chain, ParentCS, Translations, WeaponCS.TransformPosition(Markers.FingerTargets[MarkerIndex]), Solved);
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				TargetRotation[Joint] = FQuat::Slerp(Chain.GripRotation[Joint], Solved[Joint], FingerIKAlpha).GetNormalized();
			}
		}
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			const FBoneReference& Bone = Chain.Bones[Joint];
			const FCompactPoseBoneIndex BoneIndex = Bone.GetCompactPoseIndex(RequiredBones);
			FTransform Local = Output.Pose.GetLocalSpaceTransform(BoneIndex);
			Local.SetRotation(FQuat::Slerp(Local.GetRotation(), TargetRotation[Joint], GripBlendFactor).GetNormalized());
			const FTransform BoneCS = Local * ParentCS;
			OutBoneTransforms.Add(FBoneTransform(BoneIndex, BoneCS));
			ParentCS = BoneCS;
		}
		// az.Weapon.Debug 3: marker = green, fingertip pad = red (world space).
		if (AZCVars::GetWeaponDebug() >= 3 && bWeaponValid && (Markers.FingerMask & (1 << MarkerIndex)) != 0
			&& Markers.FingerTargets.IsValidIndex(MarkerIndex))
		{
			const FTransform& ToWorld = Output.AnimInstanceProxy->GetComponentTransform();
			const FVector PadCS = ParentCS.TransformPosition(
				Output.Pose.GetLocalSpaceTransform(Chain.Bones[2].GetCompactPoseIndex(RequiredBones)).GetTranslation() * FingertipExtension);
			const FVector MarkerCS = WeaponCS.TransformPosition(Markers.FingerTargets[MarkerIndex]);
			Output.AnimInstanceProxy->AnimDrawDebugSphere(ToWorld.TransformPosition(MarkerCS), 0.4f, 8, FColor::Green, false, -1.f, 0.f, SDPG_Foreground);
			Output.AnimInstanceProxy->AnimDrawDebugSphere(ToWorld.TransformPosition(PadCS), 0.3f, 8, FColor::Red, false, -1.f, 0.f, SDPG_Foreground);
		}
	}
}

void FAnimNode_AZWeaponGrip::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	if (bWeaponBoneDirty)
	{
		WeaponBone.Initialize(RequiredBones);
		bWeaponBoneDirty = false;
	}
	LastLeftAlpha = HandAlpha(Output.Curve, LeftCurveName);
	LastRightAlpha = HandAlpha(Output.Curve, RightCurveName);

	const bool bWeaponValid = WeaponBone.IsValidToEvaluate(RequiredBones);
	const FTransform WeaponCS = bWeaponValid
		? Output.Pose.GetComponentSpaceTransform(WeaponBone.GetCompactPoseIndex(RequiredBones))
		: FTransform::Identity;

	// ---- left hand onto the weapon's grip (current-frame weapon bone -> no lag)
	FTransform LeftHandCS = Output.Pose.GetComponentSpaceTransform(LeftHand.GetCompactPoseIndex(RequiredBones));
	if (LastLeftAlpha > UE_KINDA_SMALL_NUMBER && bWeaponValid)
	{
		const FCompactPoseBoneIndex UpperIndex = LeftUpperArm.GetCompactPoseIndex(RequiredBones);
		const FCompactPoseBoneIndex LowerIndex = LeftLowerArm.GetCompactPoseIndex(RequiredBones);
		const FCompactPoseBoneIndex HandIndex = LeftHand.GetCompactPoseIndex(RequiredBones);
		FTransform UpperCS = Output.Pose.GetComponentSpaceTransform(UpperIndex);
		FTransform LowerCS = Output.Pose.GetComponentSpaceTransform(LowerIndex);
		FTransform HandCS = LeftHandCS;

		const FTransform TargetCS = LeftHandInWeaponBone * WeaponCS;
		const FVector Goal = FMath::Lerp(HandCS.GetLocation(), TargetCS.GetLocation(), LastLeftAlpha);
		const FQuat GoalRotation = FQuat::Slerp(HandCS.GetRotation(), TargetCS.GetRotation(), LastLeftAlpha).GetNormalized();

		// Pole: keep the input pose's own elbow side.
		const FVector ShoulderToHandMid = 0.5 * (UpperCS.GetLocation() + HandCS.GetLocation());
		FVector ElbowOut = LowerCS.GetLocation() - ShoulderToHandMid;
		if (!ElbowOut.Normalize())
		{
			ElbowOut = FVector(0.0, -1.0, -1.0).GetSafeNormal();
		}
		const FVector Pole = LowerCS.GetLocation() + ElbowOut * ElbowPoleDistance;

		AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, Pole, Goal, false, 1.0, 1.0);
		HandCS.SetRotation(GoalRotation);
		OutBoneTransforms.Add(FBoneTransform(UpperIndex, UpperCS));
		OutBoneTransforms.Add(FBoneTransform(LowerIndex, LowerCS));
		OutBoneTransforms.Add(FBoneTransform(HandIndex, HandCS));
		LeftHandCS = HandCS;
	}

	// ---- right arm out of the stock (the right hand and the weapon on it do not move)
	LastElbowSwingDeg = bWeaponValid && LastRightAlpha > UE_KINDA_SMALL_NUMBER
		? AvoidStockWithRightArm(LastRightAlpha, WeaponCS, Output, OutBoneTransforms)
		: 0.f;

	// ---- fingers of both hands: grip pose + fingertip IK onto the weapon's markers
	if (LastLeftAlpha > UE_KINDA_SMALL_NUMBER)
	{
		AppendFingers(0, LastLeftAlpha, LeftHandCS, WeaponCS, bWeaponValid, Output, OutBoneTransforms);
	}
	if (LastRightAlpha > UE_KINDA_SMALL_NUMBER)
	{
		const FTransform RightHandCS = Output.Pose.GetComponentSpaceTransform(RightHand.GetCompactPoseIndex(RequiredBones));
		AppendFingers(1, LastRightAlpha, RightHandCS, WeaponCS, bWeaponValid, Output, OutBoneTransforms);
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
