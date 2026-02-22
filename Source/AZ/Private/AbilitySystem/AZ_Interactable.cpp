// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AZ_Interactable.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"


bool IAZ_Interactable::IsAvailableForInteraction_Implementation(UPrimitiveComponent* InteractionComponent) const
{
	return false;
}

float IAZ_Interactable::GetInteractionDuration_Implementation(UPrimitiveComponent* InteractionComponent) const
{
	return 0.0f;
}

void IAZ_Interactable::GetPreInteractSyncType_Implementation(bool& bShouldSync, EAbilityTaskNetSyncType& Type, UPrimitiveComponent* InteractionComponent) const
{
	bShouldSync = false;
	Type = EAbilityTaskNetSyncType::OnlyServerWait;
}

void IAZ_Interactable::GetPostInteractSyncType_Implementation(bool& bShouldSync, EAbilityTaskNetSyncType& Type, UPrimitiveComponent* InteractionComponent) const
{
	bShouldSync = false;
	Type = EAbilityTaskNetSyncType::OnlyServerWait;
}

FSimpleMulticastDelegate* IAZ_Interactable::GetTargetCancelInteractionDelegate(UPrimitiveComponent* InteractionComponent)
{
	return nullptr;
}

void IAZ_Interactable::RegisterInteracter_Implementation(UPrimitiveComponent* InteractionComponent, AActor* InteractingActor)
{
	if (Interacters.Contains(InteractionComponent))
	{
		TArray<AActor*>& InteractingActors = Interacters[InteractionComponent];
		if (!InteractingActors.Contains(InteractingActor))
		{
			InteractingActors.Add(InteractingActor);
		}
	}
	else
	{
		TArray<AActor*> InteractingActors;
		InteractingActors.Add(InteractingActor);
		Interacters.Add(InteractionComponent, InteractingActors);
	}
}

void IAZ_Interactable::UnregisterInteracter_Implementation(UPrimitiveComponent* InteractionComponent, AActor* InteractingActor)
{
	if (Interacters.Contains(InteractionComponent))
	{
		TArray<AActor*>& InteractingActors = Interacters[InteractionComponent];
		InteractingActors.Remove(InteractingActor);
	}
}

void IAZ_Interactable::InteractableCancelInteraction_Implementation(UPrimitiveComponent* InteractionComponent)
{
	if (Interacters.Contains(InteractionComponent))
	{
		FGameplayTagContainer InteractAbilityTagContainer;
		InteractAbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Interaction"));

		TArray<AActor*>& InteractingActors = Interacters[InteractionComponent];
		for (AActor* InteractingActor : InteractingActors)
		{
			UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InteractingActor);

			if (ASC)
			{
				ASC->CancelAbilities(&InteractAbilityTagContainer);
			}
		}

		InteractingActors.Empty();
	}
}
