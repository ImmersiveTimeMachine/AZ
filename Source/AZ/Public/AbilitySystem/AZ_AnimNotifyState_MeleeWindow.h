// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AZ_AnimNotifyState_MeleeWindow.generated.h"

/**
 * THE strike window: one draggable bar on the montage timeline that opens and closes the hit detector.
 *
 * Replaces the notify PAIR (a WindowBegin notify + a WindowEnd notify placed independently). Three
 * reasons the state is the correct primitive, all of them bugs we hit with the pair:
 *  - They cannot desync. Two notifies are two objects; moving or deleting one leaves a window that never
 *    opens (dead swing) or never closes (a detector ticking into the next animation). Here the window IS
 *    the object, and its length is what you dragged.
 *  - The engine closes it on interruption. Active notify states get NotifyEnd when the montage is stopped,
 *    blended out or jumps section — an End NOTIFY simply never fires in those cases.
 *  - It reads as a window in the editor instead of two ticks you must remember are a pair.
 *
 * It only ANNOUNCES; it never detects. The sweep lives on the ability task because notify-state objects
 * are shared by every montage instance playing the clip (per-swing state here would be per-CLIP state —
 * the classic engine trap), and because damage must stay server-authoritative while notifies also run on
 * clients. Animation owns the timing; gameplay owns the consequences.
 *
 * Tags default to Event.Montage.Melee.WindowBegin/End, resolved lazily: native tags are registered AFTER
 * CDO constructors run, so they cannot be defaulted in the ctor (same reason GA_HitReact patches its
 * triggers at grant time). Leave them unset to get the defaults; set them to drive a different consumer.
 */
UCLASS(meta = (DisplayName = "AZ Melee Window"))
class AZ_API UAZ_AnimNotifyState_MeleeWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** Sent on the first frame of the window. Unset = Event.Montage.Melee.WindowBegin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ")
	FGameplayTag BeginEventTag;

	/** Sent when the window closes — including an early close caused by an interruption. Unset =
	 *  Event.Montage.Melee.WindowEnd. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ")
	FGameplayTag EndEventTag;

private:
	/** Resolved tags (authored override, else the native default). */
	FGameplayTag ResolveBeginTag() const;
	FGameplayTag ResolveEndTag() const;

	/** Fire Tag on the mesh's owner. No-ops outside a game world so the anim editor's preview, which
	 *  plays notifies too, cannot open a hit window on a preview actor. */
	static void SendEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& Tag);
};
