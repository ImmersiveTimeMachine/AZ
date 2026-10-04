// Copyright Artur. AZ project.
//
// UAZ_MoverAnimInstance - the LEFT HAND DURING A WEAPON SWITCH (draw / holster).
//
// The equipment owns the switch; its phase snapshot (FAZ_WeaponSwitchPresentation) names the clip, the weapon that is
// physically presented and the montage that plays it. That exact montage - not "the most recent montage" - owns the
// left hand (FAZ_WeaponSwitchReach, read by AZ Weapon Grip). The master WeaponGripAlpha (right hand, body clearance) is
// never changed here.
//
// THE CLIP MOVES THE ARM. The grip only closes on the weapon where the clip's own hand touches it (draw) and lets go where
// the clip's hand leaves it (holster): the switch clip's own AZ_Grip_L at its montage time (M16: draw 1.00 -> 1.15 s
// standing, 1.30 -> 1.57 s crouched; holster 0 -> 0.13 s). A clip without the curve gets the same windows from its own
// geometry. A predictive path and a per-frame leg push were tried and removed - they fought the animation and each other
// (the hand jerked, 2026-10-03). Any correction of the arm's path is BAKED INTO THE SWITCH CLIP offline (user decision
// 2026-10-03): az.Weapon.RecordSwitch records the live situation (the grip node's input pose) for that solver.
// Design and measurements: docs/design-briefs/left-hand-weapon-switch-h1-h2-checkpoint.md section 5.

#include "Animation/AZ_MoverAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "HAL/IConsoleManager.h"
#include "Weapon/AZ_Weapon.h"

static TAutoConsoleVariable<int32> CVarAZWeaponReach(
	TEXT("az.Weapon.Reach"), 1,
	TEXT("Left hand during a weapon draw / holster: 1 = the grip follows the switch clip's own timing (closes where its hand")
	TEXT(" touches the gun, lets go where it leaves it), 0 = no grip during the switch (the clip's hand only),")
	TEXT(" 2 = 1 + a log line every frame."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAZWeaponSwitchFullBody(
	TEXT("az.Weapon.SwitchFullBody"), 1,
	TEXT("1 = a crouched weapon switch while standing still plays the switch clip on the whole body (its own legs and pelvis -")
	TEXT(" the crouch switch clips are full-body motions), 0 = upper body only over the locomotion legs."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAZWeaponRecordSwitch(
	TEXT("az.Weapon.RecordSwitch"), 0,
	TEXT("1 = record every weapon switch phase: the AZ Weapon Grip node's input pose each frame (the switch clip over the")
	TEXT(" live legs) -> Saved/NaturalGrip/SwitchRecordings/<clip>_<time>.json, for the offline switch-arm solver."),
	ECVF_Default);

namespace AZWeaponReach
{
	static const FName GripCurveL(TEXT("AZ_Grip_L"));

	/** Component-space transform of Bone in Clip (bones without a track keep the reference pose). */
	static FTransform ClipBoneCS(const UAnimSequence* Clip, const FReferenceSkeleton& Ref, int32 Bone, const FAnimExtractContext& Context,
		TMap<int32, FTransform>& Memo)
	{
		if (const FTransform* Found = Memo.Find(Bone))
		{
			return *Found;
		}
		FTransform Local = Ref.GetRefBonePose()[Bone];
		Clip->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), Context, false);
		const int32 Parent = Ref.GetParentIndex(Bone);
		const FTransform Result = Parent == INDEX_NONE ? Local : Local * ClipBoneCS(Clip, Ref, Parent, Context, Memo);
		Memo.Add(Bone, Result);
		return Result;
	}

	/** The weapon's LeftHandGrip as it hangs once settled in the hand: relative to its mesh (= the root at rest), on the
	 *  hero's in-hand socket, expressed in that socket's bone (az_weapon_r). */
	static bool GripInCarryBone(const AAZ_Weapon* Weapon, const USkeletalMeshComponent* HeroMesh, FTransform& OutGripInBone, FName& OutBone)
	{
		const USkeletalMeshSocket* Socket = Weapon && HeroMesh ? HeroMesh->GetSocketByName(Weapon->RelaxedSocketName) : nullptr;
		if (!Socket)
		{
			return false;
		}
		const USceneComponent* MeshRef = Weapon->GetWeaponMesh3P() ? static_cast<const USceneComponent*>(Weapon->GetWeaponMesh3P()) : Weapon->GetRootComponent();
		if (!MeshRef)
		{
			return false;
		}
		TInlineComponentArray<USceneComponent*> Components(Weapon);
		for (const USceneComponent* Component : Components)
		{
			if (Component && Component->DoesSocketExist(Weapon->LeftHandGripSocket))
			{
				FTransform GripInRoot = Component->GetSocketTransform(Weapon->LeftHandGripSocket, RTS_World).GetRelativeTransform(MeshRef->GetComponentTransform());
				GripInRoot.SetScale3D(FVector::OneVector);
				FTransform SocketLocal = Socket->GetSocketLocalTransform();
				SocketLocal.SetScale3D(FVector::OneVector);
				OutGripInBone = GripInRoot * SocketLocal;
				OutBone = Socket->BoneName;
				return true;
			}
		}
		return false;
	}
}

void UAZ_MoverAnimInstance::UpdateWeaponSwitchReach(float DeltaSeconds, const FAZ_WeaponSwitchPresentation* Switch)
{
	using namespace AZWeaponReach;
	const FAZ_WeaponSwitchReach Prev = WeaponSwitchReach;
	const int32 ReachMode = CVarAZWeaponReach.GetValueOnGameThread();

	// 1. The phase's own montage (validated by its phase id) and the clip time it plays.
	const UAnimSequenceBase* Clip = nullptr;
	float ClipTime = 0.f;
	float MontageWeight = 0.f;
	if (Switch && Switch->Animation.IsValid())
	{
		const AAZ_Weapon* Coordinator = Switch->Coordinator.Get();
		const UAnimMontage* Montage = Coordinator ? Coordinator->GetEquipmentAnimationMontage(Switch->PhaseId) : nullptr;
		const FAnimMontageInstance* Instance = Montage ? GetActiveInstanceForMontage(Montage) : nullptr;
		if (Instance && Instance->IsActive())
		{
			const float Position = Instance->GetPosition();
			for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
			{
				const int32 Segment = Track.AnimTrack.GetSegmentIndexAtTime(Position);
				float Time = 0.f;
				const UAnimSequenceBase* Played = Track.AnimTrack.AnimSegments.IsValidIndex(Segment)
					? Track.AnimTrack.AnimSegments[Segment].GetAnimationData(Position, Time) : nullptr;
				if (Played && Played == Switch->Animation.Get())
				{
					Clip = Played;
					ClipTime = Time;
					MontageWeight = FMath::Clamp(Instance->GetWeight(), 0.f, 1.f);
					break;
				}
			}
		}
	}
	const bool bOwned = Clip != nullptr;

	// Not owned: the last weight stays while the ownership fades (the grip node mixes it out, no jump).
	FAZ_WeaponSwitchReach Next = Prev;
	Next.bRecord = false;
	if (bOwned)
	{
		// 2. The clip's own grip weight at this time.
		const float T = ClipTime;
		float Grip = 0.f;
		if (ReachMode <= 0)
		{
			Grip = 0.f;
		}
		else if (Clip->HasCurveData(GripCurveL))
		{
			Grip = FMath::Clamp(Clip->EvaluateCurveData(GripCurveL, FAnimExtractContext(static_cast<double>(T))), 0.f, 1.f);
		}
		else if (const AAZ_Weapon* Weapon = Switch->PresentedWeapon.Get())
		{
			// no curve: the same windows from the clip's own geometry
			const TPair<TObjectKey<UAnimSequenceBase>, TObjectKey<UClass>> Key(TObjectKey<UAnimSequenceBase>(Clip), TObjectKey<UClass>(Weapon->GetClass()));
			const FSwitchGripWindow* Window = SwitchGripWindows.Find(Key);
			if (!Window)
			{
				FSwitchGripWindow Built;
				BuildSwitchGripWindow(*Switch, Weapon, Built);
				Window = &SwitchGripWindows.Add(Key, Built);
				UE_LOG(LogTemp, Display, TEXT("[Reach] %s %s has no AZ_Grip_L: two-handed %d, contact %.3f s, release %.3f s (measured on the clip)"),
					Switch->bHolster ? TEXT("holster") : TEXT("draw"), *GetNameSafe(Clip), Built.bTwoHanded, Built.Contact, Built.Release);
			}
			if (Window->bTwoHanded)
			{
				Grip = Switch->bHolster
					? 1.f - FMath::SmoothStep(0.f, FMath::Max(WeaponReachReleaseTime, Window->Release), T)
					: FMath::SmoothStep(Window->Contact, Window->Contact + WeaponReachSettleTime, T);
			}
		}
		// Holster: once let go of, the hand stays the clip's for the rest of the phase (the crouch holster's own curve
		// returns to 1 at 1.1 s with the gun already on the back).
		if (Switch->bHolster)
		{
			if (SwitchReleasedPhase == Switch->PhaseId)
			{
				Grip = 0.f;
			}
			else if (Grip <= 0.02f && MontageWeight >= 0.99f)
			{
				SwitchReleasedPhase = Switch->PhaseId;
				Grip = 0.f;
			}
		}
		// While the montage blends in, the held pose underneath keeps the normal grip; the master weight says the weapon
		// is in the hands at all.
		Next.Alpha = WeaponGripAlpha * ((1.f - MontageWeight) + MontageWeight * Grip);

		// 3. Recording for the offline switch-arm solver.
		if (CVarAZWeaponRecordSwitch.GetValueOnGameThread() > 0)
		{
			Next.bRecord = true;
			Next.RecordPhaseId = Switch->PhaseId;
			Next.RecordClip = Clip->GetFName();
			Next.RecordWeapon = Switch->PresentedWeapon.IsValid() ? Switch->PresentedWeapon->GetClass()->GetFName() : NAME_None;
			Next.RecordClipTime = T;
			Next.RecordMontageWeight = MontageWeight;
			Next.bRecordHolster = Switch->bHolster;
			Next.bRecordCrouching = Switch->bCrouching;
		}
	}

	// 4. Ownership: the phase owns the hand while its montage plays; taken over / handed back over a short blend.
	const float OwnershipRate = WeaponReachOwnershipBlendTime > 0.f ? 1.f / WeaponReachOwnershipBlendTime : 1000.f;
	Next.Ownership = FMath::FInterpConstantTo(Prev.Ownership, bOwned ? 1.f : 0.f, DeltaSeconds, OwnershipRate);

	// 5. The switch clip on the WHOLE body: a crouched switch while the hero stands still. The crouch switch clips are
	// full-body motions (the hero half-rises to take the rifle off the back: pelvis 40 -> 56 cm, the torso leans); layered
	// as upper body over the live crouch legs the torso folds over the left knee and the left arm passes through the thigh
	// / calf by 11-13 cm (recorded 2026-10-04), while over its own legs the clip is clean. Moving, the legs stay
	// locomotion's. Played by AZ Weapon Switch Full Body (right after the RifleFire layer in the AnimGraph).
	const bool bStill = !ChooserContext.bIsMoving && ChooserContext.Speed2D < WeaponSwitchFullBodyMaxSpeed;
	const bool bFullBody = bOwned && Switch->bCrouching && bStill && CVarAZWeaponSwitchFullBody.GetValueOnGameThread() > 0;
	if (bOwned)
	{
		Next.FullBodyClip = Cast<UAnimSequence>(Clip);
		Next.FullBodyTime = ClipTime;
	}
	const float FullBodyRate = WeaponSwitchFullBodyBlendTime > 0.f ? 1.f / WeaponSwitchFullBodyBlendTime : 1000.f;
	Next.FullBodyAlpha = FMath::FInterpConstantTo(Prev.FullBodyAlpha, bFullBody ? 1.f : 0.f, DeltaSeconds, FullBodyRate);
	if (Next.FullBodyAlpha <= 0.f)
	{
		Next.FullBodyClip = nullptr;
	}
	if (!bOwned && Next.Ownership <= 0.f && Next.FullBodyAlpha <= 0.f)
	{
		Next = FAZ_WeaponSwitchReach();
	}
	WeaponSwitchReach = Next;

	if (ReachMode >= 2 && (bOwned || Next.Ownership > 0.f || Next.FullBodyAlpha > 0.f))
	{
		UE_LOG(LogTemp, Display, TEXT("[Reach] %s %s t=%.3f w=%.2f own %.2f alpha %.2f master %.2f full body %.2f%s"),
			Switch && Switch->bHolster ? TEXT("holster") : TEXT("draw"), *GetNameSafe(Clip), ClipTime, MontageWeight,
			Next.Ownership, Next.Alpha, WeaponGripAlpha, Next.FullBodyAlpha, Next.bRecord ? TEXT(" [recording]") : TEXT(""));
	}
}

void UAZ_MoverAnimInstance::BuildSwitchGripWindow(const FAZ_WeaponSwitchPresentation& Switch, const AAZ_Weapon* Weapon,
	FSwitchGripWindow& Out) const
{
	using namespace AZWeaponReach;
	Out = FSwitchGripWindow();
	const UAnimSequence* Seq = Switch.Animation.Get();
	const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
	FTransform GripInBone;
	FName CarryBone;
	if (!Seq || !Skeleton || !GripInCarryBone(Weapon, GetSkelMeshComponent(), GripInBone, CarryBone))
	{
		return;
	}
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	const int32 HandIndex = Ref.FindBoneIndex(TEXT("hand_l"));
	const int32 BoneIndex = Ref.FindBoneIndex(CarryBone);
	const float Length = Seq->GetPlayLength();
	if (HandIndex == INDEX_NONE || BoneIndex == INDEX_NONE || Length <= 0.1f)
	{
		return;
	}
	// The clip's left hand relative to the clip's weapon bone.
	auto HandOnGun = [&](float Time)
	{
		TMap<int32, FTransform> Memo;
		const FAnimExtractContext Context(static_cast<double>(FMath::Clamp(Time, 0.f, Length)));
		return ClipBoneCS(Seq, Ref, HandIndex, Context, Memo).GetRelativeTransform(ClipBoneCS(Seq, Ref, BoneIndex, Context, Memo)).GetLocation();
	};
	// Two-handed where it matters (draw: at its end, holster: at its start) = its hold is near the weapon's grip.
	const FVector Hold = HandOnGun(Switch.bHolster ? 0.f : Length);
	Out.bTwoHanded = FVector::Dist(Hold, GripInBone.GetLocation()) <= WeaponReachHoldTolerance;
	if (!Out.bTwoHanded)
	{
		return;
	}
	if (!Switch.bHolster)
	{
		// The hand meets the gun: from the end backwards, the last time it is still beyond the contact radius of its hold.
		constexpr float Dt = 1.f / 30.f;
		const float Earliest = FMath::Min(Switch.ClipAttachTime + 0.2f, Length);
		Out.Contact = Earliest;
		float After = Length;
		float AfterDistance = 0.f;
		for (float T = Length - Dt; T >= Earliest - 1e-4f; T -= Dt)
		{
			const float Distance = FVector::Dist(HandOnGun(T), Hold);
			if (Distance > WeaponReachContactRadius)
			{
				Out.Contact = T + (Distance - WeaponReachContactRadius) / FMath::Max(Distance - AfterDistance, 1e-3f) * (After - T);
				break;
			}
			After = T;
			AfterDistance = Distance;
		}
	}
	else
	{
		// The hand leaves the gun: the first time it is 3 cm from its starting hold.
		constexpr float Dt = 1.f / 60.f;
		constexpr float Leave = 3.f;
		float Before = 0.f;
		float BeforeDistance = 0.f;
		for (float T = Dt; T <= Length; T += Dt)
		{
			const float Distance = FVector::Dist(HandOnGun(T), Hold);
			if (Distance > Leave)
			{
				Out.Release = Before + (Leave - BeforeDistance) / FMath::Max(Distance - BeforeDistance, 1e-3f) * (T - Before);
				break;
			}
			Before = T;
			BeforeDistance = Distance;
		}
	}
}
