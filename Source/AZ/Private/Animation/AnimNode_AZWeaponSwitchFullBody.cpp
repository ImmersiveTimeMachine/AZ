// Copyright Artur. AZ project.

#include "Animation/AnimNode_AZWeaponSwitchFullBody.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Animation/AnimNode_AZWeaponGrip.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AZWeaponSwitchFullBody)

void FAnimNode_AZWeaponSwitchFullBody::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_AZWeaponSwitchFullBody::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
}

void FAnimNode_AZWeaponSwitchFullBody::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	// The weapon switch (game thread, already updated for this frame).
	Clip.Reset();
	ClipTime = 0.f;
	Weight = 0.f;
	if (const UAZ_MoverAnimInstance* Owner = Context.AnimInstanceProxy ? Cast<UAZ_MoverAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()) : nullptr)
	{
		const FAZ_WeaponSwitchReach& Switch = Owner->GetWeaponSwitchReach();
		Clip = Switch.FullBodyClip;
		ClipTime = Switch.FullBodyTime;
		Weight = FMath::Clamp(Switch.FullBodyAlpha, 0.f, 1.f);
	}
}

void FAnimNode_AZWeaponSwitchFullBody::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	const UAnimSequence* Sequence = Clip.Get();
	if (!Sequence || Weight <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	FPoseContext ClipPose(Output);
	FAnimationPoseData ClipData(ClipPose);
	Sequence->GetAnimationPose(ClipData, FAnimExtractContext(static_cast<double>(ClipTime)));
	for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		if (Bone.GetInt() == 0)
		{
			continue;   // the root: OffsetRootBone / the capsule own it
		}
		Output.Pose[Bone].BlendWith(ClipPose.Pose[Bone], Weight);
	}
}

void FAnimNode_AZWeaponSwitchFullBody::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Switch full body %s t=%.2f weight %.2f)"), *GetNameSafe(Clip.Get()), ClipTime, Weight);
	DebugData.AddDebugItem(DebugLine);
	Source.GatherDebugData(DebugData);
}
