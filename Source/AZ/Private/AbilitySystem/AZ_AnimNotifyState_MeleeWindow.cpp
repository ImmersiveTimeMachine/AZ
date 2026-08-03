// Copyright Artur. AZ project.

#include "AbilitySystem/AZ_AnimNotifyState_MeleeWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AZ_GameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UAZ_AnimNotifyState_MeleeWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendEvent(MeshComp, ResolveBeginTag());
}

void UAZ_AnimNotifyState_MeleeWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	// Fires on the natural end AND on interruption/blend-out/section jump — that second case is the whole
	// reason this is a state. A close with no matching open is harmless: StopHitWindow is idempotent.
	SendEvent(MeshComp, ResolveEndTag());
}

FGameplayTag UAZ_AnimNotifyState_MeleeWindow::ResolveBeginTag() const
{
	return BeginEventTag.IsValid() ? BeginEventTag : FAZ_GameplayTags::Get().Event_Montage_Melee_WindowBegin;
}

FGameplayTag UAZ_AnimNotifyState_MeleeWindow::ResolveEndTag() const
{
	return EndEventTag.IsValid() ? EndEventTag : FAZ_GameplayTags::Get().Event_Montage_Melee_WindowEnd;
}

void UAZ_AnimNotifyState_MeleeWindow::SendEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& Tag)
{
	if (!MeshComp || !Tag.IsValid())
	{
		return;
	}
	const UWorld* World = MeshComp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;   // anim editor preview plays notifies too
	}
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = Tag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Tag, Payload);
}

FString UAZ_AnimNotifyState_MeleeWindow::GetNotifyName_Implementation() const
{
	return TEXT("AZ Melee Window");
}
