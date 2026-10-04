// Copyright Artur. AZ project.

#include "Animation/AnimNode_AZWeaponGrip.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStats.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TwoBoneIK.h"
#include "AZ_ConsoleVariables.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Weapon/AZ_WeaponGripField.h"

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
	DebugLine += FString::Printf(TEXT("(Grip %s on %s, alpha %.2f, L %.2f R %.2f, finger markers 0x%03x, contact field %s fingers %d, switch %.2f)"),
		*GetNameSafe(GripPose), *WeaponBoneName.ToString(), GripAlpha, LastLeftAlpha, LastRightAlpha, Markers.FingerMask,
		*GetNameSafe(Markers.Field), LastContactFingers, LastSwitchOwnership);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_AZWeaponGrip::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	LeftUpperArm.Initialize(RequiredBones);
	LeftLowerArm.Initialize(RequiredBones);
	LeftHand.Initialize(RequiredBones);
	RightHand.Initialize(RequiredBones);
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
	bRecordBonesDirty = true;
}

void FAnimNode_AZWeaponGrip::FlushRecording()
{
	if (RecordingFrames.Num() > 0)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("NaturalGrip/SwitchRecordings");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = Dir / FString::Printf(TEXT("%s_%s.json"), *RecordingClip.ToString(), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		const FString Json = FString::Printf(TEXT("{%s,\"frames\":[\n%s\n]}\n"), *RecordingHeader, *FString::Join(RecordingFrames, TEXT(",\n")));
		const bool bSaved = FFileHelper::SaveStringToFile(Json, *Path);
		UE_LOG(LogTemp, Display, TEXT("[Reach] recorded %d frames of %s -> %s (%s)"), RecordingFrames.Num(), *RecordingClip.ToString(), *Path,
			bSaved ? TEXT("saved") : TEXT("FAILED"));
	}
	RecordingFrames.Reset();
	RecordingHeader.Reset();
	RecordingClip = NAME_None;
	RecordingPhase.Invalidate();
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

	// The weapon switch (game thread, already updated for this frame).
	Reach = FAZ_WeaponSwitchReach();
	if (const UAZ_MoverAnimInstance* Owner = Context.AnimInstanceProxy ? Cast<UAZ_MoverAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()) : nullptr)
	{
		Reach = Owner->GetWeaponSwitchReach();
	}
	if (RecordingPhase.IsValid() && (!Reach.bRecord || Reach.RecordPhaseId != RecordingPhase))
	{
		FlushRecording();   // the recorded phase ended (the next one starts its own file on its first evaluated frame)
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
			// The metacarpal too (a solved grasp arches the palm: ring / pinky metacarpals flex with their fingers).
			Chain.bHasParentGrip = false;
			if (Finger > 0)
			{
				const int32 ParentSkeletonIndex = PoseSkeleton->GetReferenceSkeleton().FindBoneIndex(Chain.Parent.BoneName);
				if (ParentSkeletonIndex != INDEX_NONE)
				{
					FTransform Local;
					GripPose->GetBoneTransform(Local, FSkeletonPoseBoneIndex(ParentSkeletonIndex), FAnimExtractContext(0.0), false);
					Chain.ParentGripRotation = Local.GetRotation().GetNormalized();
					Chain.bHasParentGrip = true;
				}
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

	// Palm side per hand for the real-time contact closing: the fingers curl toward the palm, and the grip pose curls
	// them, so the grip pose's fingertips lie on the palm side of the open (reference) ones. The sign is taken against
	// the (index_01 - hand) x (pinky_01 - hand) plane, which the contact solver rebuilds every frame from the live hand.
	for (int32 Side = 0; Side < NumSides; ++Side)
	{
		bPalmSignReady[Side] = false;
		PalmSign[Side] = 1.f;
		if (!PoseSkeleton)
		{
			continue;
		}
		const FReferenceSkeleton& Ref = PoseSkeleton->GetReferenceSkeleton();
		const int32 HandIndex = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("hand_%s"), AZWeaponGrip::Sides[Side])));
		if (HandIndex == INDEX_NONE)
		{
			continue;
		}
		// Reference transform of a bone relative to the hand (walks the parents up to the hand).
		auto InHand = [&Ref, HandIndex](int32 Index)
		{
			FTransform T = FTransform::Identity;
			while (Index != INDEX_NONE && Index != HandIndex)
			{
				T = T * Ref.GetRefBonePose()[Index];
				Index = Ref.GetParentIndex(Index);
			}
			return T;
		};
		const int32 Index01 = Ref.FindBoneIndex(Chains[Side][1].Bones[0].BoneName);
		const int32 Pinky01 = Ref.FindBoneIndex(Chains[Side][4].Bones[0].BoneName);
		if (Index01 == INDEX_NONE || Pinky01 == INDEX_NONE)
		{
			continue;
		}
		const FVector PlaneNormal = (InHand(Index01).GetLocation() ^ InHand(Pinky01).GetLocation()).GetSafeNormal();
		FVector Curl = FVector::ZeroVector;
		for (int32 Finger = 1; Finger < NumFingers; ++Finger)
		{
			const FFingerChain& Chain = Chains[Side][Finger];
			const int32 ParentIndex = Ref.FindBoneIndex(Chain.Parent.BoneName);
			int32 BoneIndices[3];
			bool bFound = Chain.bHasGrip && ParentIndex != INDEX_NONE;
			for (int32 Joint = 0; Joint < 3 && bFound; ++Joint)
			{
				BoneIndices[Joint] = Ref.FindBoneIndex(Chain.Bones[Joint].BoneName);
				bFound = BoneIndices[Joint] != INDEX_NONE;
			}
			if (!bFound)
			{
				continue;
			}
			FTransform Open = InHand(ParentIndex);
			FTransform Grip = Open;
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				const FTransform& RefLocal = Ref.GetRefBonePose()[BoneIndices[Joint]];
				Open = RefLocal * Open;
				Grip = FTransform(Chain.GripRotation[Joint], RefLocal.GetTranslation()) * Grip;
			}
			const FVector TipOffset = Ref.GetRefBonePose()[BoneIndices[2]].GetTranslation() * FingertipExtension;
			Curl += Grip.TransformPosition(TipOffset) - Open.TransformPosition(TipOffset);
		}
		if (Curl.Size() > 0.5 && !PlaneNormal.IsNearlyZero())
		{
			PalmSign[Side] = (Curl | PlaneNormal) >= 0.0 ? 1.f : -1.f;
			bPalmSignReady[Side] = true;
		}
	}
	for (int32 Side = 0; Side < NumSides; ++Side)
	{
		for (int32 Finger = 0; Finger < NumFingers; ++Finger)
		{
			bContactAnglesValid[Side][Finger] = false;
		}
	}
}

namespace AZWeaponGrip
{
	/** Fit one finger of the BASE shape (the playing clip's own fingers, or an authored pose) onto the weapon's surface
	 *  field: the whole finger curls a little more or less (all joints together, the tip by Node.DistalCoupling) until it
	 *  touches without entering. See FAnimNode_AZWeaponGrip::FingerContactAlpha. */
	static bool FitFingerToSurface(const FAnimNode_AZWeaponGrip& Node, bool bThumb, const FQuat InBase[3],
		const FVector& PalmNormalCS, const FTransform& ParentCS, const FVector Translations[3], const FTransform& WeaponCS,
		float DeltaSeconds, float& Smoothed, bool& bSmoothedValid, FQuat OutRotations[3])
	{
	const UAZ_WeaponGripField* Field = Node.Markers.Field;
	if (!Field || !Field->IsValidField() || PalmNormalCS.IsNearlyZero())
	{
		return false;
	}

	const FQuat Base[3] = { InBase[0], InBase[1], InBase[2] };

	// Hinge axis of every joint of the authored finger: perpendicular to its bone and to the palm normal, signed so
	// that a positive angle curls toward the palm; stored in the joint's parent frame so it turns with the joints
	// before it. The thumb keeps its base (CMC) joint and adapts its two outer joints.
	FVector AxisLocal[3];
	{
		FTransform Parent = ParentCS;
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			const FTransform Bone = FTransform(Base[Joint], Translations[Joint]) * Parent;
			const FVector Child = Joint < 2
				? (FTransform(Base[Joint + 1], Translations[Joint + 1]) * Bone).GetLocation()
				: Bone.TransformPosition(Translations[2] * Node.FingertipExtension);
			FVector Axis = (Child - Bone.GetLocation()).GetSafeNormal() ^ PalmNormalCS;
			if (!Axis.Normalize())
			{
				return false;
			}
			AxisLocal[Joint] = Parent.InverseTransformVectorNoScale(Axis);
			Parent = Bone;
		}
	}
	// One parameter per finger: the whole finger curls by T beyond the authored pose (T < 0 opens it); the tip joint
	// moves Node.DistalCoupling x T. The shape the artist gave stays; only how tight it is changes.
	const float Weight[3] = { bThumb ? 0.f : 1.f, 1.f, Node.DistalCoupling };
	auto Rotations = [&](float T, FQuat Out[3])
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			Out[Joint] = Weight[Joint] > 0.f
				? (FQuat(AxisLocal[Joint], Weight[Joint] * T) * Base[Joint]).GetNormalized()
				: Base[Joint];
		}
	};
	// Nearest clearance (cm) over the finger's phalanx capsules: min of (field distance - radius); < 0 = inside.
	const FVector Radii = bThumb ? Node.ThumbRadii : Node.FingerRadii;
	auto Clearance = [&](float T)
	{
		FQuat R[3];
		Rotations(T, R);
		FVector Points[4];
		FTransform Bone = ParentCS;
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			Bone = FTransform(R[Joint], Translations[Joint]) * Bone;
			Points[Joint] = Bone.GetLocation();
		}
		Points[3] = Bone.TransformPosition(Translations[2] * Node.FingertipExtension);
		float Best = TNumericLimits<float>::Max();
		for (int32 Segment = 0; Segment < 3; ++Segment)
		{
			for (int32 Sample = 0; Sample <= 2; ++Sample)
			{
				const FVector P = FMath::Lerp(Points[Segment], Points[Segment + 1], 0.5f * Sample);
				const FVector InField = Node.Markers.FieldInBone.InverseTransformPosition(WeaponCS.InverseTransformPosition(P));
				Best = FMath::Min(Best, Field->Sample(InField) - static_cast<float>(Radii[Segment]));
			}
		}
		return Best;
	};

	// Smallest change from the authored pose that puts the finger ON the surface: inside -> open until out; floating
	// -> curl until touching (never into the weapon); touching -> leave it as authored.
	const float Step = FMath::DegreesToRadians(Node.ContactStepDeg);
	const float OpenLimit = -FMath::DegreesToRadians(Node.FingerAdaptOpenDeg);
	const float CloseLimit = FMath::DegreesToRadians(Node.FingerAdaptCloseDeg);
	float T = 0.f;
	float C = Clearance(T);
	if (C < -Node.ContactSkin)
	{
		while (C < -Node.ContactSkin && T - Step >= OpenLimit)
		{
			T -= Step;
			C = Clearance(T);
		}
	}
	else if (C > Node.ContactTouchDistance)
	{
		while (T + Step <= CloseLimit)
		{
			const float Next = Clearance(T + Step);
			if (Next < -Node.ContactSkin)
			{
				break;
			}
			T += Step;
			C = Next;
			if (C <= Node.ContactTouchDistance)
			{
				break;
			}
		}
	}

	// Smooth over time (contacts change as the clip moves the hand; no popping between steps).
	if (!bSmoothedValid || Node.ContactInterpSpeed <= 0.f || DeltaSeconds <= 0.f)
	{
		Smoothed = T;
		bSmoothedValid = true;
	}
	else
	{
		Smoothed = FMath::FInterpTo(Smoothed, T, DeltaSeconds, Node.ContactInterpSpeed);
	}
	Rotations(Smoothed, OutRotations);
	return true;
}
}

namespace AZWeaponGrip
{
	/** Put one finger's pad on a semantic target (e.g. the index on the trigger), starting from the BASE shape (the
	 *  clip's own finger): three small changes are searched - base joint flexion, middle joint flexion (tip coupled)
	 *  and base spread about the palm normal - minimising the pad-to-target distance plus a preference for small
	 *  changes, with the weapon's surface field keeping the finger out of the metal. */
	static bool FitFingerToTarget(const FAnimNode_AZWeaponGrip& Node, bool bThumb, const FQuat InBase[3], const FVector& PalmNormalCS,
		const FTransform& ParentCS, const FVector Translations[3], const FTransform& WeaponCS, const FVector& TargetCS,
		float DeltaSeconds, float Smoothed[2], bool& bSmoothedValid, FQuat OutRotations[3])
	{
		if (PalmNormalCS.IsNearlyZero())
		{
			return false;
		}
		const UAZ_WeaponGripField* Field = Node.Markers.Field;
		const bool bField = Field && Field->IsValidField();
		FVector AxisLocal[3];
		{
			FTransform Parent = ParentCS;
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				const FTransform Bone = FTransform(InBase[Joint], Translations[Joint]) * Parent;
				const FVector Child = Joint < 2
					? (FTransform(InBase[Joint + 1], Translations[Joint + 1]) * Bone).GetLocation()
					: Bone.TransformPosition(Translations[2] * Node.FingertipExtension);
				FVector Axis = (Child - Bone.GetLocation()).GetSafeNormal() ^ PalmNormalCS;
				if (!Axis.Normalize())
				{
					return false;
				}
				AxisLocal[Joint] = Parent.InverseTransformVectorNoScale(Axis);
				Parent = Bone;
			}
		}
		const FVector SpreadLocal = ParentCS.InverseTransformVectorNoScale(PalmNormalCS).GetSafeNormal();
		auto Rotations = [&](double Base, double Middle, double Spread, FQuat Out[3])
		{
			Out[0] = (FQuat(SpreadLocal, Spread) * FQuat(AxisLocal[0], Base) * InBase[0]).GetNormalized();
			Out[1] = (FQuat(AxisLocal[1], Middle) * InBase[1]).GetNormalized();
			Out[2] = (FQuat(AxisLocal[2], Node.DistalCoupling * Middle) * InBase[2]).GetNormalized();
		};
		auto Chain = [&](double Base, double Middle, double Spread, FVector Points[4])
		{
			FQuat R[3];
			Rotations(Base, Middle, Spread, R);
			FTransform Bone = ParentCS;
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				Bone = FTransform(R[Joint], Translations[Joint]) * Bone;
				Points[Joint] = Bone.GetLocation();
			}
			Points[3] = Bone.TransformPosition(Translations[2] * Node.FingertipExtension);
		};
		auto Cost = [&](double Base, double Middle, double Spread, bool bWithField)
		{
			FVector Points[4];
			Chain(Base, Middle, Spread, Points);
			double C = (Points[3] - TargetCS).SizeSquared() + 0.5 * (Base * Base + Middle * Middle + Spread * Spread);
			if (bWithField && bField)
			{
				for (int32 Segment = 0; Segment < 3; ++Segment)
				{
					for (int32 Sample = 0; Sample <= 2; ++Sample)
					{
						const FVector P = FMath::Lerp(Points[Segment], Points[Segment + 1], 0.5f * Sample);
						const FVector InField = Node.Markers.FieldInBone.InverseTransformPosition(WeaponCS.InverseTransformPosition(P));
						const float Radius = static_cast<float>(bThumb ? Node.ThumbRadii[Segment] : Node.FingerRadii[Segment]);
						const double Inside = -(Field->Sample(InField) - Radius) - Node.ContactSkin;
						if (Inside > 0.0)
						{
							C += 20.0 * Inside * Inside;
						}
					}
				}
			}
			return C;
		};
		// Coarse grid (distance only), then coordinate descent with the surface term.
		const double Open = -FMath::DegreesToRadians(Node.FingerAdaptOpenDeg);
		const double Close = FMath::DegreesToRadians(FMath::Max(Node.FingerAdaptCloseDeg, 40.f));
		const double SpreadLimit = FMath::DegreesToRadians(20.0);
		double Best[3] = { 0.0, 0.0, 0.0 };
		double BestCost = Cost(0.0, 0.0, 0.0, false);
		const double Coarse = FMath::DegreesToRadians(5.0);
		for (double B = Open; B <= Close + 1e-6; B += Coarse)
		{
			for (double M = Open; M <= Close + 1e-6; M += Coarse)
			{
				for (double S = -SpreadLimit; S <= SpreadLimit + 1e-6; S += Coarse)
				{
					const double C = Cost(B, M, S, false);
					if (C < BestCost)
					{
						BestCost = C;
						Best[0] = B;
						Best[1] = M;
						Best[2] = S;
					}
				}
			}
		}
		BestCost = Cost(Best[0], Best[1], Best[2], true);
		const double Lo[3] = { Open, Open, -SpreadLimit };
		const double Hi[3] = { Close, Close, SpreadLimit };
		for (double Step = FMath::DegreesToRadians(2.5); Step >= FMath::DegreesToRadians(0.4); Step *= 0.5)
		{
			bool bImproved = true;
			for (int32 Pass = 0; Pass < 20 && bImproved; ++Pass)
			{
				bImproved = false;
				for (int32 K = 0; K < 3; ++K)
				{
					for (const double Sign : { -1.0, 1.0 })
					{
						double Trial[3] = { Best[0], Best[1], Best[2] };
						Trial[K] = FMath::Clamp(Trial[K] + Sign * Step, Lo[K], Hi[K]);
						const double C = Cost(Trial[0], Trial[1], Trial[2], true);
						if (C < BestCost - 1e-9)
						{
							BestCost = C;
							Best[0] = Trial[0];
							Best[1] = Trial[1];
							Best[2] = Trial[2];
							bImproved = true;
						}
					}
				}
			}
		}
		if (!bSmoothedValid || Node.ContactInterpSpeed <= 0.f || DeltaSeconds <= 0.f)
		{
			Smoothed[0] = static_cast<float>(Best[0]);
			Smoothed[1] = static_cast<float>(Best[1]);
			bSmoothedValid = true;
		}
		else
		{
			Smoothed[0] = FMath::FInterpTo(Smoothed[0], static_cast<float>(Best[0]), DeltaSeconds, Node.ContactInterpSpeed);
			Smoothed[1] = FMath::FInterpTo(Smoothed[1], static_cast<float>(Best[1]), DeltaSeconds, Node.ContactInterpSpeed);
		}
		Rotations(Smoothed[0], Smoothed[1], Best[2], OutRotations);
		return true;
	}
}

bool FAnimNode_AZWeaponGrip::SolveFingerContact(int32 Side, int32 Finger, const FVector& PalmNormalCS, const FTransform& ParentCS,
	const FVector Translations[3], const FQuat& ClipBaseRotation, const FTransform& WeaponCS, float DeltaSeconds,
	FQuat OutRotations[3]) const
{
	(void)ClipBaseRotation;
	const FFingerChain& Chain = Chains[Side][Finger];
	return AZWeaponGrip::FitFingerToSurface(*this, Finger == 0, Chain.GripRotation, PalmNormalCS, ParentCS, Translations,
		WeaponCS, DeltaSeconds, ContactAngles[Side][Finger][0], bContactAnglesValid[Side][Finger], OutRotations);
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

bool FAnimNode_AZWeaponGrip::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// a held weapon with grip data, or a weapon switch owning the left hand (its reach, and the left-arm clearance even
	// before the incoming weapon has grip data)
	const bool bGrip = GripPose != nullptr && WeaponBoneName != NAME_None && GripAlpha > UE_KINDA_SMALL_NUMBER;
	const bool bSwitch = Reach.Ownership > UE_KINDA_SMALL_NUMBER || Reach.bRecord;
	if (!bSwitch)
	{
		LastSwitchOwnership = 0.f;
	}
	return (bGrip || bSwitch)
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

	// A weapon with a solved grasp for this hand (AAZ_Weapon::bBakedRightHandGrasp / bBakedLeftHandGrasp): the grip pose IS
	// the grasp of the hand on its hold (the right hand re-gripped by AZ Weapon Body Clearance, the left hand IK'd onto
	// LeftHandGrip above) - applied as it is, metacarpals included (the palm arch); no real-time fitting, so no clip
	// finger shape and no per-frame search leak into it.
	const bool bBaked = Side == 1 ? Markers.bBakedRightFingers : Markers.bBakedLeftFingers;

	// Real-time contact closing: palm normal of the LIVE hand (knuckles from the clip's metacarpals), signed to the
	// palm side (PalmSign, from the grip pose).
	FVector PalmNormalCS = FVector::ZeroVector;
	const bool bContact = !bBaked && bWeaponValid && FingerContactAlpha > UE_KINDA_SMALL_NUMBER && Markers.Field != nullptr
		&& bPalmSignReady[Side];
	if (bContact)
	{
		auto KnuckleCS = [&](const FFingerChain& Chain, FVector& Out)
		{
			if (!Chain.Parent.IsValidToEvaluate(RequiredBones) || !Chain.Bones[0].IsValidToEvaluate(RequiredBones))
			{
				return false;
			}
			const FTransform Metacarpal = Output.Pose.GetLocalSpaceTransform(Chain.Parent.GetCompactPoseIndex(RequiredBones)) * HandCS;
			Out = Metacarpal.TransformPosition(Output.Pose.GetLocalSpaceTransform(Chain.Bones[0].GetCompactPoseIndex(RequiredBones)).GetTranslation());
			return true;
		};
		FVector IndexKnuckle, PinkyKnuckle;
		if (KnuckleCS(Chains[Side][1], IndexKnuckle) && KnuckleCS(Chains[Side][4], PinkyKnuckle))
		{
			const FVector Wrist = HandCS.GetLocation();
			PalmNormalCS = ((IndexKnuckle - Wrist) ^ (PinkyKnuckle - Wrist)).GetSafeNormal() * PalmSign[Side];
		}
	}
	const float DeltaSeconds = Output.AnimInstanceProxy ? Output.AnimInstanceProxy->GetDeltaSeconds() : 0.f;
	int32 ContactFingers = 0;

	for (int32 Finger = 0; Finger < NumFingers; ++Finger)
	{
		const FFingerChain& Chain = Chains[Side][Finger];
		if (!Chain.bHasGrip || !Chain.Parent.IsValidToEvaluate(RequiredBones)
			|| !Chain.Bones[0].IsValidToEvaluate(RequiredBones) || !Chain.Bones[1].IsValidToEvaluate(RequiredBones)
			|| !Chain.Bones[2].IsValidToEvaluate(RequiredBones))
		{
			continue;
		}
		// Parent in component space, re-derived from the (possibly IK-moved) hand: metacarpals keep their local, except in
		// a solved grasp, which arches the palm (the grip pose's metacarpal).
		const FCompactPoseBoneIndex ParentIndex = Chain.Parent.GetCompactPoseIndex(RequiredBones);
		FTransform ParentCS = HandCS;
		if (ParentIndex != HandIndex)
		{
			FTransform ParentLocal = Output.Pose.GetLocalSpaceTransform(ParentIndex);
			if (bBaked && Chain.bHasParentGrip)
			{
				ParentLocal.SetRotation(FQuat::Slerp(ParentLocal.GetRotation(), Chain.ParentGripRotation, GripBlendFactor).GetNormalized());
				ParentCS = ParentLocal * HandCS;
				OutBoneTransforms.Add(FBoneTransform(ParentIndex, ParentCS));
			}
			else
			{
				ParentCS = ParentLocal * HandCS;
			}
		}

		// Target rotations: the grip pose, re-bent by the fingertip IK when the weapon has this finger's marker.
		FQuat TargetRotation[3] = { Chain.GripRotation[0], Chain.GripRotation[1], Chain.GripRotation[2] };
		const int32 MarkerIndex = Side * NumFingers + Finger;
		bool bContactSolved = false;
		if (bContact && !PalmNormalCS.IsNearlyZero())
		{
			FVector Translations[3];
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				Translations[Joint] = Output.Pose.GetLocalSpaceTransform(Chain.Bones[Joint].GetCompactPoseIndex(RequiredBones)).GetTranslation();
			}
			// Base shape = the playing clip's OWN fingers (natural mocap, per clip: hip carry, aim, crouch...), fitted
			// onto the weapon's surface. The authored grip pose is no longer the finger shape when a field exists.
			FQuat ClipRotation[3];
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				ClipRotation[Joint] = Output.Pose.GetLocalSpaceTransform(Chain.Bones[Joint].GetCompactPoseIndex(RequiredBones)).GetRotation();
			}
			FQuat Contact[3];
			// A finger with a marker on the weapon has a semantic target (the right index -> the trigger, the left thumb
			// -> along the forend's side): put its pad there; every other finger just rests on the surface.
			const bool bHasTarget = (Markers.FingerMask & (1 << MarkerIndex)) != 0 && Markers.FingerTargets.IsValidIndex(MarkerIndex);
			const bool bFitted = bHasTarget
				? AZWeaponGrip::FitFingerToTarget(*this, Finger == 0, ClipRotation, PalmNormalCS, ParentCS, Translations, WeaponCS,
					WeaponCS.TransformPosition(Markers.FingerTargets[MarkerIndex]), DeltaSeconds, ContactAngles[Side][Finger],
					bContactAnglesValid[Side][Finger], Contact)
				: AZWeaponGrip::FitFingerToSurface(*this, Finger == 0, ClipRotation, PalmNormalCS, ParentCS, Translations, WeaponCS,
					DeltaSeconds, ContactAngles[Side][Finger][0], bContactAnglesValid[Side][Finger], Contact);
			if (bFitted)
			{
				for (int32 Joint = 0; Joint < 3; ++Joint)
				{
					TargetRotation[Joint] = FQuat::Slerp(ClipRotation[Joint], Contact[Joint], FingerContactAlpha).GetNormalized();
				}
				bContactSolved = true;
				++ContactFingers;
			}
		}
		if (!bBaked && !bContactSolved && bWeaponValid && Chain.bIKReady && FingerIKAlpha > UE_KINDA_SMALL_NUMBER && (Markers.FingerMask & (1 << MarkerIndex)) != 0
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
	LastContactFingers += ContactFingers;
}

void FAnimNode_AZWeaponGrip::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	if (bWeaponBoneDirty)
	{
		WeaponBone.Initialize(RequiredBones);
		bWeaponBoneDirty = false;
	}
	// ---- az.Weapon.RecordSwitch: this node's INPUT pose (the switch clip's upper body over the live legs, after foot
	// placement and the real layering) - what the offline switch-arm solver bakes the clip's correction against.
	if (Reach.bRecord)
	{
		if (RecordingPhase != Reach.RecordPhaseId)
		{
			FlushRecording();
			RecordingPhase = Reach.RecordPhaseId;
			RecordingClip = Reach.RecordClip;
			RecordingStartTime = FPlatformTime::Seconds();
			RecordingHeader = FString::Printf(TEXT("\"clip\":\"%s\",\"weapon\":\"%s\",\"holster\":%s,\"crouching\":%s,\"weaponBone\":\"%s\",\"leftHandInWeaponBone\":[%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.6f]"),
				*Reach.RecordClip.ToString(), *Reach.RecordWeapon.ToString(), Reach.bRecordHolster ? TEXT("true") : TEXT("false"),
				Reach.bRecordCrouching ? TEXT("true") : TEXT("false"), *WeaponBoneName.ToString(),
				LeftHandInWeaponBone.GetLocation().X, LeftHandInWeaponBone.GetLocation().Y, LeftHandInWeaponBone.GetLocation().Z,
				LeftHandInWeaponBone.GetRotation().X, LeftHandInWeaponBone.GetRotation().Y, LeftHandInWeaponBone.GetRotation().Z, LeftHandInWeaponBone.GetRotation().W);
		}
		if (bRecordBonesDirty)
		{
			static const TCHAR* const Names[] = {
				TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
				TEXT("neck_01"), TEXT("head"), TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("upperarm_twist_01_l"), TEXT("upperarm_twist_02_l"),
				TEXT("lowerarm_l"), TEXT("lowerarm_twist_01_l"), TEXT("lowerarm_twist_02_l"), TEXT("hand_l"),
				TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("az_weapon_r"),
				TEXT("thigh_l"), TEXT("thigh_twist_01_l"), TEXT("calf_l"), TEXT("calf_twist_01_l"), TEXT("calf_twist_02_l"), TEXT("foot_l"), TEXT("ball_l"),
				TEXT("thigh_r"), TEXT("thigh_twist_01_r"), TEXT("calf_r"), TEXT("calf_twist_01_r"), TEXT("calf_twist_02_r"), TEXT("foot_r"), TEXT("ball_r") };
			RecordBones.Reset();
			for (const TCHAR* Name : Names)
			{
				FBoneReference Bone(Name);
				if (Bone.Initialize(RequiredBones))
				{
					RecordBones.Add(Bone);
				}
			}
			bRecordBonesDirty = false;
		}
		FString Frame = FString::Printf(TEXT("{\"t\":%.4f,\"clipTime\":%.4f,\"w\":%.3f,\"own\":%.3f,\"alpha\":%.3f,\"master\":%.3f,\"bones\":{"),
			FPlatformTime::Seconds() - RecordingStartTime, Reach.RecordClipTime, Reach.RecordMontageWeight, Reach.Ownership, Reach.Alpha, GripAlpha);
		for (int32 I = 0; I < RecordBones.Num(); ++I)
		{
			const FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(RecordBones[I].GetCompactPoseIndex(RequiredBones));
			const FVector L = BoneCS.GetLocation();
			const FQuat Q = BoneCS.GetRotation();
			Frame += FString::Printf(TEXT("%s\"%s\":[%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f]"), I ? TEXT(",") : TEXT(""),
				*RecordBones[I].BoneName.ToString(), L.X, L.Y, L.Z, Q.X, Q.Y, Q.Z, Q.W);
		}
		Frame += TEXT("}}");
		RecordingFrames.Add(MoveTemp(Frame));
	}

	LastLeftAlpha = HandAlpha(Output.Curve, LeftCurveName);
	LastRightAlpha = HandAlpha(Output.Curve, RightCurveName);
	// A weapon switch OWNS the left hand by Reach.Ownership (tau): its weight (the switch clip's own AZ_Grip_L) replaces
	// the pose's AZ_Grip_L, which upper-body layers fading out at the switch start override with their own 1. The two
	// weights are mixed, so a change of owner never jumps.
	const float Tau = FMath::Clamp(Reach.Ownership, 0.f, 1.f);
	LastSwitchOwnership = Tau;
	const bool bWeaponValid = GripPose != nullptr && WeaponBone.IsValidToEvaluate(RequiredBones);
	const FTransform WeaponCS = bWeaponValid
		? Output.Pose.GetComponentSpaceTransform(WeaponBone.GetCompactPoseIndex(RequiredBones))
		: FTransform::Identity;
	const float NormalAlpha = bWeaponValid ? LastLeftAlpha : 0.f;
	const float SwitchAlpha = bWeaponValid && Tau > 0.f ? FMath::Clamp(Reach.Alpha, 0.f, 1.f) : 0.f;
	const float LeftAlpha = FMath::Lerp(NormalAlpha, SwitchAlpha, Tau);
	LastLeftAlpha = LeftAlpha;

	// ---- left hand onto the weapon's grip (current-frame weapon bone -> no lag)
	FTransform LeftHandCS = Output.Pose.GetComponentSpaceTransform(LeftHand.GetCompactPoseIndex(RequiredBones));
	if (LeftAlpha > UE_KINDA_SMALL_NUMBER)
	{
		const FCompactPoseBoneIndex UpperIndex = LeftUpperArm.GetCompactPoseIndex(RequiredBones);
		const FCompactPoseBoneIndex LowerIndex = LeftLowerArm.GetCompactPoseIndex(RequiredBones);
		const FCompactPoseBoneIndex HandIndex = LeftHand.GetCompactPoseIndex(RequiredBones);
		FTransform UpperCS = Output.Pose.GetComponentSpaceTransform(UpperIndex);
		FTransform LowerCS = Output.Pose.GetComponentSpaceTransform(LowerIndex);
		FTransform HandCS = LeftHandCS;

		const FTransform TargetCS = LeftHandInWeaponBone * WeaponCS;
		const FVector Goal = FMath::Lerp(HandCS.GetLocation(), TargetCS.GetLocation(), LeftAlpha);
		const FQuat GoalRotation = FQuat::Slerp(HandCS.GetRotation(), TargetCS.GetRotation(), LeftAlpha).GetNormalized();

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

	// ---- fingers of both hands: grip pose, closed on the weapon's surface field (or fingertip IK onto its markers)
	LastContactFingers = 0;
	if (GripPose && LeftAlpha > UE_KINDA_SMALL_NUMBER)
	{
		AppendFingers(0, LeftAlpha, LeftHandCS, WeaponCS, bWeaponValid, Output, OutBoneTransforms);
	}
	if (GripPose && LastRightAlpha > UE_KINDA_SMALL_NUMBER)
	{
		const FTransform RightHandCS = Output.Pose.GetComponentSpaceTransform(RightHand.GetCompactPoseIndex(RequiredBones));
		AppendFingers(1, LastRightAlpha, RightHandCS, WeaponCS, bWeaponValid, Output, OutBoneTransforms);
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
