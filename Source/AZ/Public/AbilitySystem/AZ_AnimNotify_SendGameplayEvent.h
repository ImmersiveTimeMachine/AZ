// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AZ_AnimNotify_SendGameplayEvent.generated.h"

/**
 * Frame-accurate anim -> GAS bridge: drop on any montage/sequence frame, pick a tag, and the owning
 * actor's ASC receives that GameplayEvent at exactly that frame. THE sender for attack hit windows
 * (Event.Montage.Melee.Hit -> GA_MeleeAttack::OnMontageEvent sweeps + damages), combo windows, fire
 * points, and any future anim-led gameplay beat. Rail doctrine: this notify is what makes a clip
 * "anim-led" — the clip owns the timing, gameplay listens.
 */
UCLASS(meta = (DisplayName = "AZ Send Gameplay Event"))
class AZ_API UAZ_AnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** The gameplay event to send (e.g. Event.Montage.Melee.Hit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ")
	FGameplayTag EventTag;
};
