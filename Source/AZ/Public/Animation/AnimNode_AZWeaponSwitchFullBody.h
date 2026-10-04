// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_AZWeaponSwitchFullBody.generated.h"

class UAnimSequence;

/**
 * Weapon switch full body - plays the weapon-switch clip on the WHOLE body while the anim instance asks for it
 * (UAZ_MoverAnimInstance: a crouched draw / holster while standing still, FAZ_WeaponSwitchReach::FullBody*).
 *
 *   The crouch switch clips are full-body motions (the hero half-rises to take the rifle off the back: pelvis 40 -> 56 cm,
 *   the torso leans). Layered as upper body over the live crouch legs, the torso folds over the left knee and the left arm
 *   passes through the thigh / calf (-11 to -13 cm, recorded 2026-10-04); over its own legs the clip is clean. So while
 *   the hero does not move, the clip's own legs and pelvis are used: the authored animation, unchanged. Moving, the legs
 *   stay locomotion's (the weight fades out).
 *
 *   Every bone but the root (OffsetRootBone and the capsule own it) is blended toward the clip's local pose by the weight;
 *   curves are not touched. Place it right after the layered blend of the RifleFire slot (before foot placement and the
 *   weapon grip / clearance nodes). Inert (pass-through) at weight 0.
 */
USTRUCT(BlueprintInternalUseOnly)
struct AZ_API FAnimNode_AZWeaponSwitchFullBody : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	/** This update's request (copied from the anim instance in Update_AnyThread). */
	TWeakObjectPtr<const UAnimSequence> Clip;
	float ClipTime = 0.f;
	float Weight = 0.f;
};
