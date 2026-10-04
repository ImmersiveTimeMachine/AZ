// Copyright Artur. AZ project.

#include "Animation/AnimNode_AZWeaponBodyClearance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TwoBoneIK.h"
#include "AZ_ConsoleVariables.h"
#include "Weapon/AZ_WeaponGripField.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AZWeaponBodyClearance)

namespace AZBodyClearance
{
	static constexpr int32 PushIterations = 6;

	// Right-arm solver (2026-09-28). Model and numbers were checked offline on the M16 relaxed set carrying the
	// Winchester (Saved/wgs/arm_poses.json, scratchpad armmodel.py): the old swing-only solver needed 81-98 deg at the
	// standing idle (the stock enters the forearm 7-12 cm from the wrist, where a swing about the shoulder->hand axis
	// has almost no leverage) and picked the side by cost, so idle and running ended on opposite sides of the stock.
	//
	// The two corrections grow TOGETHER along one ray - elbow = R * ElbowSwingCostDeg, wrist = R * WristTurnCostDeg - and
	// the solution is the FIRST R that clears the arm. A 2-D cost minimum was tried first: along the "arm just clear"
	// boundary the cost is almost flat, so at the standing idle two solutions 0.5 % apart (elbow 54 / wrist 16.2 and
	// elbow 57 / wrist 14.5) won on alternate frames and the elbow twitched (PIE 2026-09-28). The first crossing along a
	// fixed ray is unique and moves continuously with the pose (idle 50.4..50.7 deg over the whole clip), and costs ~35
	// arm evaluations a frame instead of ~250.
	static constexpr int32 ArmSamples = 10;              // per arm segment
	static constexpr double RayStep = 0.04;              // scan step of R (1.6 deg of elbow at the default 40)
	static constexpr int32 RayBisections = 6;            // then bisection inside the step -> 0.025 deg
	static constexpr double SideHysteresisCm = 0.5;
	static constexpr double BodyTolerance = 0.2;         // the wrist turn may not push the weapon further into the body

	/** Distance-weighted blend helper. */
	static double Lerp(double A, double B, double T) { return A + (B - A) * T; }
}

FAnimNode_AZWeaponBodyClearance::FAnimNode_AZWeaponBodyClearance()
{
	RightUpperArm.BoneName = TEXT("upperarm_r");
	RightLowerArm.BoneName = TEXT("lowerarm_r");
	RightHand.BoneName = TEXT("hand_r");
	TorsoBone.BoneName = TEXT("spine_04");
}

void FAnimNode_AZWeaponBodyClearance::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(alpha %.2f, shoulder %.2f, capsules %d, penetration %.1f cm in %s, push %.1f cm, arm %.1f -> %.1f cm, elbow %.1f wrist %.1f deg)"),
		ClearanceAlpha, LastShoulderAlpha, Capsules.Num(), LastPenetration, *LastPenetrationBone.ToString(), CurrentPush.Size(),
		LastArmPenetration, LastArmResidual, ArmSwingDeg, ArmWristDeg);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_AZWeaponBodyClearance::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	RightUpperArm.Initialize(RequiredBones);
	RightLowerArm.Initialize(RequiredBones);
	RightHand.Initialize(RequiredBones);
	TorsoBone.Initialize(RequiredBones);
	WeaponBone.BoneName = WeaponBoneName;
	WeaponBone.Initialize(RequiredBones);
	bWeaponBoneDirty = false;
	bCapsulesDirty = true;
}

void FAnimNode_AZWeaponBodyClearance::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	if (WeaponBone.BoneName != WeaponBoneName)
	{
		WeaponBone.BoneName = WeaponBoneName;
		bWeaponBoneDirty = true;
	}
	if (ClearanceAlpha <= UE_KINDA_SMALL_NUMBER)
	{
		// No weapon in the hands: nothing to keep out; start clean when the next one comes.
		ArmSwingDeg = ArmWristDeg = 0.f;
		ElbowOutSign = 0.f;
	}
}

bool FAnimNode_AZWeaponBodyClearance::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// Needs the stock markers (body push, arm clearance) OR a right-hand re-grip (weapons without a stock, e.g. the M16).
	return (Markers.bHasStock || !Markers.RightHandCorrection.Equals(FTransform::Identity, 1e-4))
		&& WeaponBoneName != NAME_None
		&& RightUpperArm.IsValidToEvaluate(RequiredBones)
		&& RightLowerArm.IsValidToEvaluate(RequiredBones)
		&& RightHand.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_AZWeaponBodyClearance::BuildCapsules(const FBoneContainer& RequiredBones, const UPhysicsAsset* PhysicsAsset)
{
	Capsules.Reset();
	CachedPhysicsAsset = PhysicsAsset;
	bCapsulesDirty = false;
	if (!PhysicsAsset)
	{
		return;
	}
	for (const TObjectPtr<USkeletalBodySetup>& Body : PhysicsAsset->SkeletalBodySetups)
	{
		if (!Body || !BodyBones.Contains(Body->BoneName))
		{
			continue;
		}
		const bool bPocket = ShoulderPocketBones.Contains(Body->BoneName);
		for (const FKSphylElem& Sphyl : Body->AggGeom.SphylElems)
		{
			FBodyCapsule Capsule;
			Capsule.Bone.BoneName = Body->BoneName;
			Capsule.Bone.Initialize(RequiredBones);
			if (!Capsule.Bone.IsValidToEvaluate(RequiredBones))
			{
				continue;
			}
			const FVector HalfAxis = Sphyl.Rotation.RotateVector(FVector(0.0, 0.0, 0.5 * Sphyl.Length));
			Capsule.A = Sphyl.Center - HalfAxis;
			Capsule.B = Sphyl.Center + HalfAxis;
			Capsule.Radius = Sphyl.Radius;
			Capsule.bShoulderPocket = bPocket;
			Capsules.Add(Capsule);
		}
		for (const FKSphereElem& Sphere : Body->AggGeom.SphereElems)
		{
			FBodyCapsule Capsule;
			Capsule.Bone.BoneName = Body->BoneName;
			Capsule.Bone.Initialize(RequiredBones);
			if (!Capsule.Bone.IsValidToEvaluate(RequiredBones))
			{
				continue;
			}
			Capsule.A = Capsule.B = Sphere.Center;
			Capsule.Radius = Sphere.Radius;
			Capsule.bShoulderPocket = bPocket;
			Capsules.Add(Capsule);
		}
	}
}

void FAnimNode_AZWeaponBodyClearance::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	if (bWeaponBoneDirty)
	{
		WeaponBone.Initialize(RequiredBones);
		bWeaponBoneDirty = false;
	}
	const USkeletalMeshComponent* Mesh = Output.AnimInstanceProxy ? Output.AnimInstanceProxy->GetSkelMeshComponent() : nullptr;
	const UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	if (bCapsulesDirty || CachedPhysicsAsset.Get() != PhysicsAsset)
	{
		BuildCapsules(RequiredBones, PhysicsAsset);
	}
	const float DeltaSeconds = Output.AnimInstanceProxy ? Output.AnimInstanceProxy->GetDeltaSeconds() : 0.f;
	const bool bWeaponValid = WeaponBone.IsValidToEvaluate(RequiredBones) && ClearanceAlpha > UE_KINDA_SMALL_NUMBER;

	const FCompactPoseBoneIndex UpperIndex = RightUpperArm.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex LowerIndex = RightLowerArm.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex HandIndex = RightHand.GetCompactPoseIndex(RequiredBones);
	FTransform UpperCS = Output.Pose.GetComponentSpaceTransform(UpperIndex);
	FTransform LowerCS = Output.Pose.GetComponentSpaceTransform(LowerIndex);
	FTransform HandCS = Output.Pose.GetComponentSpaceTransform(HandIndex);

	// Stock segment in component space (front -> butt), before any push.
	const FTransform WeaponCS = bWeaponValid
		? Output.Pose.GetComponentSpaceTransform(WeaponBone.GetCompactPoseIndex(RequiredBones))
		: FTransform::Identity;

	// ---- 0) right-hand re-grip: the hand moves onto the weapon's natural hold (Markers.RightHandCorrection, in the hand's
	// own space, solved offline with the hand's grasp) while the WEAPON KEEPS ITS ANIMATED PLACE; the right arm follows by
	// two-bone IK (the elbow keeps its side). Weighted by the right hand's grip curve, so a hand that leaves the grip
	// (reload) is left alone. Everything below then works on the re-gripped hand.
	const FCompactPoseBoneIndex WeaponIndex = bWeaponValid ? WeaponBone.GetCompactPoseIndex(RequiredBones) : FCompactPoseBoneIndex(INDEX_NONE);
	bool bRegripped = false;
	if (bWeaponValid && WeaponIndex != HandIndex && !Markers.RightHandCorrection.Equals(FTransform::Identity, 1e-4))
	{
		bool bHasCurve = false;
		const float CurveWeight = Output.Curve.Get(RightGripCurveName, bHasCurve, 1.f);
		const float Weight = ClearanceAlpha * FMath::Clamp(bHasCurve ? CurveWeight : 1.f, 0.f, 1.f);
		if (Weight > UE_KINDA_SMALL_NUMBER)
		{
			const FTransform Target = Markers.RightHandCorrection * HandCS;
			const FVector Goal = FMath::Lerp(HandCS.GetLocation(), Target.GetLocation(), static_cast<double>(Weight));
			const FQuat GoalRotation = FQuat::Slerp(HandCS.GetRotation(), Target.GetRotation(), Weight).GetNormalized();
			const FVector ShoulderToHandMid = 0.5 * (UpperCS.GetLocation() + HandCS.GetLocation());
			FVector ElbowOut = LowerCS.GetLocation() - ShoulderToHandMid;
			if (!ElbowOut.Normalize())
			{
				ElbowOut = FVector(0.0, 1.0, -1.0).GetSafeNormal();
			}
			AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, LowerCS.GetLocation() + ElbowOut * ElbowPoleDistance, Goal,
				false, 1.0, 1.0);
			HandCS.SetRotation(GoalRotation);
			bRegripped = true;
		}
	}
	const FTransform InputHandCS = HandCS;
	// The weapon rides the right hand (its bone is the hand or a child of it): keep that relation through every change
	// of the hand below.
	const FTransform WeaponInHand = WeaponCS.GetRelativeTransform(InputHandCS);
	const FVector Front = WeaponCS.TransformPosition(Markers.StockFront);
	const FVector Butt = WeaponCS.TransformPosition(Markers.StockButt);
	const double StockLength = FVector::Dist(Front, Butt);
	const FVector Forward = (Front - Butt).GetSafeNormal();
	// Fraction of the stock (from the front) where the butt end begins; only relevant while aiming.
	const double ButtStart = StockLength > 1.0 ? FMath::Clamp(1.0 - ButtContactLength / StockLength, 0.0, 1.0) : 1.0;

	// Body capsules in component space.
	struct FCapsuleCS { FVector A; FVector B; double Radius; bool bPocket; FName Bone; };
	TArray<FCapsuleCS, TInlineAllocator<24>> Body;
	if (bWeaponValid && Markers.bHasStock)      // no stock markers: no body push (only the re-grip above runs)
	{
		for (const FBodyCapsule& Capsule : Capsules)
		{
			const FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(Capsule.Bone.GetCompactPoseIndex(RequiredBones));
			Body.Add({ BoneCS.TransformPosition(Capsule.A), BoneCS.TransformPosition(Capsule.B),
				Capsule.Radius * BodyRadiusScale + Markers.StockRadius + Margin, Capsule.bShoulderPocket, Capsule.Bone.BoneName });
		}
	}

	// Shouldered = aiming OR the incoming animation already holds the butt end at the shoulder pocket (surface gap of
	// the last ButtContactLength of the stock to the pocket capsules, eased).
	{
		double Gap = TNumericLimits<double>::Max();
		if (bDetectShoulderedFromPose && bWeaponValid && StockLength > 1.0)
		{
			const FVector ButtEndStart = Butt + Forward * FMath::Min(static_cast<double>(ButtContactLength), StockLength);
			for (const FCapsuleCS& Capsule : Body)
			{
				if (Capsule.bPocket)
				{
					FVector OnStock, OnBody;
					FMath::SegmentDistToSegmentSafe(ButtEndStart, Butt, Capsule.A, Capsule.B, OnStock, OnBody);
					Gap = FMath::Min(Gap, FVector::Dist(OnStock, OnBody) - Capsule.Radius);
				}
			}
		}
		const double Far = FMath::Max(ShoulderedFarCm, ShoulderedNearCm + 0.1f);
		const float Target = Gap < TNumericLimits<double>::Max()
			? static_cast<float>(1.0 - FMath::SmoothStep(static_cast<double>(ShoulderedNearCm), Far, Gap)) : 0.f;
		DetectedShoulderAlpha = DeltaSeconds > 0.f ? FMath::FInterpTo(DetectedShoulderAlpha, Target, DeltaSeconds, 10.f) : Target;
		LastShoulderAlpha = FMath::Max(FMath::Clamp(ShoulderContactAlpha, 0.f, 1.f), DetectedShoulderAlpha);
	}
	const float Shouldered = LastShoulderAlpha;

	// Deepest penetration of a stock segment (+ Offset) into the body, with the shoulder-pocket allowance.
	auto BodyPenetration = [&](const FVector& StockFront, const FVector& StockButt, FVector* OutNormal, FName* OutBone) -> double
	{
		const double Length = FVector::Dist(StockFront, StockButt);
		double Worst = 0.0;
		for (const FCapsuleCS& Capsule : Body)
		{
			FVector OnStock, OnBody;
			FMath::SegmentDistToSegmentSafe(StockFront, StockButt, Capsule.A, Capsule.B, OnStock, OnBody);
			double Penetration = Capsule.Radius - FVector::Dist(OnStock, OnBody);
			if (Capsule.bPocket && Shouldered > 0.f && Length > 1.0
				&& FVector::Dist(OnStock, StockFront) / Length >= ButtStart)
			{
				// Aiming: the butt end may press a little into the shoulder pocket - never through the chest (2026-09-28:
				// skipping the pocket entirely let the stock sit 6-9 cm inside the clavicle / upper chest).
				Penetration -= ButtPocketDepth * Shouldered;
			}
			if (Penetration <= Worst)
			{
				continue;
			}
			Worst = Penetration;
			if (OutNormal)
			{
				*OutNormal = (OnStock - OnBody).GetSafeNormal();
				if (OutNormal->IsNearlyZero())
				{
					*OutNormal = Forward;
				}
			}
			if (OutBone)
			{
				*OutBone = Capsule.Bone;
			}
		}
		return Worst;
	};

	// ---- 1) weapon out of the body: smallest push of the right hand, resolved along the separation normals.
	FVector Push = FVector::ZeroVector;
	double Deepest = 0.0;
	FName DeepestBone = NAME_None;
	if (bWeaponValid && Body.Num() > 0)
	{
		for (int32 Iteration = 0; Iteration < AZBodyClearance::PushIterations; ++Iteration)
		{
			FVector WorstNormal = FVector::ZeroVector;
			FName WorstBone = NAME_None;
			const double Worst = BodyPenetration(Front + Push, Butt + Push, &WorstNormal, &WorstBone);
			if (Worst <= 0.0)
			{
				break;
			}
			if (Worst > Deepest)
			{
				Deepest = Worst;
				DeepestBone = WorstBone;
			}
			Push += WorstNormal * Worst;
			if (Push.Size() > MaxPushCm)
			{
				Push = Push.GetClampedToMaxSize(MaxPushCm);
				break;
			}
		}
	}
	LastPenetration = static_cast<float>(Deepest);
	LastPenetrationBone = DeepestBone;
	CurrentPush = PushInterpSpeed > 0.f && DeltaSeconds > 0.f
		? FMath::VInterpTo(CurrentPush, Push * ClearanceAlpha, DeltaSeconds, PushInterpSpeed)
		: Push * ClearanceAlpha;

	bool bModified = bRegripped;
	if (CurrentPush.SizeSquared() >= 1e-4)
	{
		// Move the right hand (the weapon hangs on it) by the push; the right arm follows by two-bone IK.
		const FQuat HandRotation = HandCS.GetRotation();
		const FVector Goal = HandCS.GetLocation() + CurrentPush;
		const FVector ShoulderToHandMid = 0.5 * (UpperCS.GetLocation() + HandCS.GetLocation());
		FVector ElbowOut = LowerCS.GetLocation() - ShoulderToHandMid;
		if (!ElbowOut.Normalize())
		{
			ElbowOut = FVector(0.0, 1.0, -1.0).GetSafeNormal();
		}
		const FVector Pole = LowerCS.GetLocation() + ElbowOut * ElbowPoleDistance;
		AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, Pole, Goal, false, 1.0, 1.0);
		HandCS.SetRotation(HandRotation);
		bModified = true;
	}

	// ---- 2) right arm out of the weapon: elbow swing + wrist turn, measured on the weapon's exact surface.
	const UAZ_WeaponGripField* Field = Markers.Field.Get();
	const bool bArm = bSolveRightArm && bWeaponValid && Field && Field->IsValidField();
	double TargetSwing = 0.0, TargetWrist = 0.0;
	LastArmPenetration = LastArmResidual = 0.f;
	LastArmEvaluations = 0;
	FVector WristUpCS = FVector::UpVector;
	if (bArm)
	{
		const FVector Shoulder = UpperCS.GetLocation();
		const FVector Elbow = LowerCS.GetLocation();
		const FVector Hand = HandCS.GetLocation();
		const FVector ShoulderAxis = (Hand - Shoulder).GetSafeNormal();
		const FTransform FieldInHand = Markers.FieldInBone * WeaponInHand;
		const double UpperStart = FMath::Lerp(UpperArmTestStart, UpperArmTestStartAiming, Shouldered);
		const double MaxWrist = MaxWristTurnDeg * (1.0 - Shouldered);
		const FVector StockFrontInHand = WeaponInHand.TransformPosition(Markers.StockFront);
		const FVector StockButtInHand = WeaponInHand.TransformPosition(Markers.StockButt);
		const double BodyBefore = Body.Num() > 0
			? BodyPenetration(HandCS.TransformPosition(StockFrontInHand), HandCS.TransformPosition(StockButtInHand), nullptr, nullptr)
			: 0.0;

		auto SwungElbow = [&](double SignedDeg)
		{
			return Shoulder + FQuat(ShoulderAxis, FMath::DegreesToRadians(SignedDeg)).RotateVector(Elbow - Shoulder);
		};
		// Wrist turn axis: the component-space up, made perpendicular to the forearm (a turn about the vertical at the
		// wrist; tested against free wrist-bend directions offline - one direction solves every clip at ~equal cost).
		auto WristAxis = [&](const FVector& SwungElbowPos)
		{
			const FVector Forearm = (Hand - SwungElbowPos).GetSafeNormal();
			FVector Axis = FVector::UpVector - Forearm * (Forearm | FVector::UpVector);
			return Axis.Normalize() ? Axis : FVector::ForwardVector;
		};
		auto TurnedHand = [&](const FVector& Axis, double SignedDeg)
		{
			FTransform Turned = HandCS;
			Turned.SetRotation((FQuat(Axis, FMath::DegreesToRadians(SignedDeg)) * HandCS.GetRotation()).GetNormalized());
			return Turned;
		};
		// Deepest arm penetration (cm beyond the accepted sleeve contact) for a swung elbow and a (turned) hand.
		auto ArmPenetration = [&](const FVector& E, const FTransform& TurnedHandCS)
		{
			++LastArmEvaluations;
			const FTransform FieldCS = FieldInHand * TurnedHandCS;
			double Worst = -1e9;
			const FVector ToHand = Hand - E;
			const double ForearmLength = ToHand.Size();
			const double TMax = ForearmLength > ArmGripZone ? (ForearmLength - ArmGripZone) / ForearmLength : 0.0;
			for (int32 Index = 0; Index <= AZBodyClearance::ArmSamples; ++Index)
			{
				const double T = TMax * Index / AZBodyClearance::ArmSamples;
				const FVector P = E + ToHand * T;
				const double Radius = AZBodyClearance::Lerp(ArmElbowRadius, ArmWristRadius, T);
				Worst = FMath::Max(Worst, Radius - Field->SampleUnbounded(FieldCS.InverseTransformPosition(P)) - ArmContact);
			}
			for (int32 Index = 0; Index <= AZBodyClearance::ArmSamples; ++Index)
			{
				const double T = UpperStart + (1.0 - UpperStart) * Index / AZBodyClearance::ArmSamples;
				const FVector P = Shoulder + (E - Shoulder) * T;
				const double Radius = AZBodyClearance::Lerp(ArmShoulderRadius, ArmElbowRadius, T);
				Worst = FMath::Max(Worst, Radius - Field->SampleUnbounded(FieldCS.InverseTransformPosition(P)) - ArmContact);
			}
			return Worst;
		};

		const double Penetration0 = ArmPenetration(Elbow, HandCS);
		LastArmPenetration = static_cast<float>(Penetration0);
		if (Penetration0 > 0.0)
		{
			// Sides, from anatomy (not from the cost - the cost made idle and running pick opposite sides): the elbow
			// swings AWAY from the torso; the wrist turns the muzzle toward the arm's own side.
			const FVector Torso = TorsoBone.IsValidToEvaluate(RequiredBones)
				? Output.Pose.GetComponentSpaceTransform(TorsoBone.GetCompactPoseIndex(RequiredBones)).GetLocation()
				: Shoulder - FVector(0.0, 0.0, 20.0);
			auto HorizontalDistance = [&](const FVector& P) { return FVector::Dist2D(P, Torso); };
			const double OutPlus = HorizontalDistance(SwungElbow(10.0)), OutMinus = HorizontalDistance(SwungElbow(-10.0));
			if (FMath::Abs(OutPlus - OutMinus) > AZBodyClearance::SideHysteresisCm || ElbowOutSign == 0.f)
			{
				ElbowOutSign = OutPlus >= OutMinus ? 1.f : -1.f;
			}
			const double SwingSign = ElbowOutSign;
			// The turn direction is judged on the body's FORWARD (perpendicular to the arm side), not on the weapon's
			// own direction: a weapon already pointing toward the arm's side made that test flip sign mid-clip (strafe
			// right, replica 2026-09-28: elbow 74 deg <-> wrist -27 deg toward the body).
			const FVector ArmSide = FVector(Shoulder.X - Torso.X, Shoulder.Y - Torso.Y, 0.0).GetSafeNormal();
			const FVector BodyForward = (ArmSide ^ FVector::UpVector).GetSafeNormal();
			auto WristSignFor = [&](const FVector& Axis)
			{
				const FVector Moved = FQuat(Axis, FMath::DegreesToRadians(5.0)).RotateVector(BodyForward) - BodyForward;
				return (Moved | ArmSide) >= 0.0 ? 1.0 : -1.0;
			};

			// Feasible = the arm is clear AND the turned weapon is no deeper in the body than before the turn.
			auto Feasible = [&](const FVector& E, const FVector& Axis, double SignedWrist, double& OutPenetration)
			{
				const FTransform Turned = TurnedHand(Axis, SignedWrist);
				OutPenetration = ArmPenetration(E, Turned);
				if (OutPenetration > 0.0)
				{
					return false;
				}
				if (Body.Num() > 0 && SignedWrist != 0.0)
				{
					const double BodyAfter = BodyPenetration(Turned.TransformPosition(StockFrontInHand),
						Turned.TransformPosition(StockButtInHand), nullptr, nullptr);
					if (BodyAfter > FMath::Max(BodyBefore, 0.0) + AZBodyClearance::BodyTolerance)
					{
						OutPenetration = FMath::Max(OutPenetration, BodyAfter);
						return false;
					}
				}
				return true;
			};

			// One ray through both corrections (see the namespace comment): elbow = R * ElbowSwingCostDeg,
			// wrist = R * WristTurnCostDeg (the wrist share goes to 0 while aiming, MaxWrist). First feasible R wins.
			const double RayElbow = FMath::Max(1.0, static_cast<double>(ElbowSwingCostDeg));
			const double RayWrist = MaxWrist > 0.0 ? static_cast<double>(WristTurnCostDeg) * (MaxWrist / FMath::Max(1.0f, MaxWristTurnDeg)) : 0.0;
			auto At = [&](double R, double& OutSwing, double& OutWrist, double& OutPenetration)
			{
				const double Swing = FMath::Min(R * RayElbow, static_cast<double>(MaxElbowSwingDeg));
				const double WristAbs = FMath::Min(R * RayWrist, MaxWrist);
				const FVector E = SwungElbow(SwingSign * Swing);
				const FVector Axis = WristAxis(E);
				OutSwing = SwingSign * Swing;
				OutWrist = WristSignFor(Axis) * WristAbs;
				return Feasible(E, Axis, OutWrist, OutPenetration);
			};
			const double RMax = FMath::Max(MaxElbowSwingDeg / RayElbow, RayWrist > 0.0 ? MaxWrist / RayWrist : 0.0);
			double Swing = 0.0, Wrist = 0.0, Penetration = Penetration0;
			double LeastPenetration = Penetration0, LeastSwing = 0.0, LeastWrist = 0.0;   // fallback: nothing on the ray clears
			double Below = 0.0, Found = -1.0;
			for (double R = AZBodyClearance::RayStep; R <= RMax + AZBodyClearance::RayStep * 0.5; R += AZBodyClearance::RayStep)
			{
				const double Clamped = FMath::Min(R, RMax);
				if (At(Clamped, Swing, Wrist, Penetration))
				{
					Found = Clamped;
					break;
				}
				if (Penetration < LeastPenetration)
				{
					LeastPenetration = Penetration;
					LeastSwing = Swing;
					LeastWrist = Wrist;
				}
				Below = Clamped;
			}
			if (Found >= 0.0)
			{
				double Lo = Below, Hi = Found;
				for (int32 Iteration = 0; Iteration < AZBodyClearance::RayBisections; ++Iteration)
				{
					const double Mid = 0.5 * (Lo + Hi);
					double MidSwing = 0.0, MidWrist = 0.0, MidPenetration = 0.0;
					(At(Mid, MidSwing, MidWrist, MidPenetration) ? Hi : Lo) = Mid;
				}
				At(Hi, Swing, Wrist, Penetration);
				TargetSwing = Swing;
				TargetWrist = Wrist;
				LastArmResidual = static_cast<float>(Penetration);
			}
			else
			{
				TargetSwing = LeastSwing;
				TargetWrist = LeastWrist;
				LastArmResidual = static_cast<float>(LeastPenetration);
			}
		}
	}
	// Follow the solution (it tracks the pose continuously); the rate only stops a single-frame pop.
	const double MaxStep = ArmMaxRateDegPerSec * FMath::Max(DeltaSeconds, 1.f / 120.f);
	ArmSwingDeg = static_cast<float>(ArmSwingDeg + FMath::Clamp(TargetSwing * ClearanceAlpha - ArmSwingDeg, -MaxStep, MaxStep));
	ArmWristDeg = static_cast<float>(ArmWristDeg + FMath::Clamp(TargetWrist * ClearanceAlpha - ArmWristDeg, -MaxStep, MaxStep));
	if (!FMath::IsNearlyZero(ArmSwingDeg, 0.01f) || !FMath::IsNearlyZero(ArmWristDeg, 0.01f))
	{
		const FVector Shoulder = UpperCS.GetLocation();
		const FVector Hand = HandCS.GetLocation();
		const FQuat Swing((Hand - Shoulder).GetSafeNormal(), FMath::DegreesToRadians(ArmSwingDeg));
		const FVector NewElbow = Shoulder + Swing.RotateVector(LowerCS.GetLocation() - Shoulder);
		UpperCS.SetRotation((Swing * UpperCS.GetRotation()).GetNormalized());
		LowerCS.SetLocation(NewElbow);
		LowerCS.SetRotation((Swing * LowerCS.GetRotation()).GetNormalized());
		const FVector Forearm = (Hand - NewElbow).GetSafeNormal();
		WristUpCS = FVector::UpVector - Forearm * (Forearm | FVector::UpVector);
		if (!WristUpCS.Normalize())
		{
			WristUpCS = FVector::ForwardVector;
		}
		HandCS.SetRotation((FQuat(WristUpCS, FMath::DegreesToRadians(ArmWristDeg)) * HandCS.GetRotation()).GetNormalized());
		bModified = true;
	}

	if (AZCVars::GetWeaponDebug() >= 2)
	{
		static double NextLogTime = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now >= NextLogTime)
		{
			NextLogTime = Now + AZCVars::GetWeaponDebugInterval();
			UE_LOG(LogTemp, Display, TEXT("[Body Clearance] body %.1f (%s) push %.1f | arm %.1f -> %.1f cm, target elbow %.1f wrist %.1f, applied %.1f / %.1f deg, evals %d, shoulder %.2f"),
				Deepest, *DeepestBone.ToString(), CurrentPush.Size(), LastArmPenetration, LastArmResidual, TargetSwing, TargetWrist,
				ArmSwingDeg, ArmWristDeg, LastArmEvaluations, LastShoulderAlpha);
		}
	}
	if (AZCVars::GetWeaponDebug() >= 3 && Output.AnimInstanceProxy && bWeaponValid)
	{
		const FTransform& ToWorld = Output.AnimInstanceProxy->GetComponentTransform();
		Output.AnimInstanceProxy->AnimDrawDebugLine(ToWorld.TransformPosition(Front + CurrentPush),
			ToWorld.TransformPosition(Butt + CurrentPush), FColor::Orange, false, -1.f, 2.f * Markers.StockRadius, SDPG_Foreground);
		for (const FCapsuleCS& Capsule : Body)
		{
			Output.AnimInstanceProxy->AnimDrawDebugLine(ToWorld.TransformPosition(Capsule.A), ToWorld.TransformPosition(Capsule.B),
				Capsule.bPocket ? FColor::Cyan : FColor::Silver, false, -1.f, 0.5f, SDPG_Foreground);
		}
		Output.AnimInstanceProxy->AnimDrawDebugLine(ToWorld.TransformPosition(LowerCS.GetLocation()), ToWorld.TransformPosition(HandCS.GetLocation()),
			LastArmResidual > 0.f ? FColor::Red : FColor::Green, false, -1.f, 2.f * ArmWristRadius, SDPG_Foreground);
	}

	if (!bModified)
	{
		return;
	}
	OutBoneTransforms.Add(FBoneTransform(UpperIndex, UpperCS));
	OutBoneTransforms.Add(FBoneTransform(LowerIndex, LowerCS));
	OutBoneTransforms.Add(FBoneTransform(HandIndex, HandCS));
	if (bRegripped)
	{
		// The re-grip changed the hand -> weapon relation: the weapon bone gets its own transform (it still rides every
		// push / wrist turn of the hand, from its re-gripped place).
		OutBoneTransforms.Add(FBoneTransform(WeaponIndex, WeaponInHand * HandCS));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
