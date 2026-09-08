#include "AbilitySystem/Abilities/AZ_GA_FirearmAim.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Player/AZ_PlayerController.h"

namespace
{
	const UAZ_Inv_CommonUI_EquipmentComponent* ResolveAimEquipment(const FGameplayAbilityActorInfo* ActorInfo, const UObject* Source)
	{
		const AAZ_PawnMoverHeroCharacter* Hero = ActorInfo ? Cast<AAZ_PawnMoverHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const AAZ_PlayerController* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
		if (!Controller || Controller->IsInventoryInputCaptured()) return nullptr;
		const auto* Equipment = Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
		return Equipment && Equipment->IsActiveWeaponSource(Source) ? Equipment : nullptr;
	}
}

UAZ_GA_FirearmAim::UAZ_GA_FirearmAim()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAZ_GA_FirearmAim::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	ActivationBlockedTags.AddTag(Tags.Character_Dead);
	ActivationBlockedTags.AddTag(Tags.Character_Dying);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Staggered);
	ActivationBlockedTags.AddTag(Tags.State_Combat_StruckPair);
	ActivationBlockedTags.AddTag(Tags.Ability_State_MeleeAttacking);
	CancelAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
	BlockAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
}

bool UAZ_GA_FirearmAim::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	return ResolveAimEquipment(ActorInfo, GetSourceObject(Handle, ActorInfo)) != nullptr;
}

void UAZ_GA_FirearmAim::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAZ_AbilitySystemComponent* ASC = ActorInfo ? Cast<UAZ_AbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	// GAS has already applied activation/block relationships. Revalidate the source
	// and menu here without repeating CanActivateAbility against the active ability.
	const auto* Equipment = ActorInfo ? ResolveAimEquipment(ActorInfo, GetSourceObject(Handle, ActorInfo)) : nullptr;
	if (!ASC || !Equipment || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	AimedItemId = Equipment->GetActiveItem()->GetInstanceId();
	// These counted contributions belong only to this activation. Other actions' fight
	// requests survive our release; equipment selection itself never raises rifle aim.
	ASC->AddStateTag(Tags.Ability_State_Aiming);
	ASC->AddStateTag(Tags.Movement_Strafe);
	bOwnsAimState = true;

	UE_LOG(LogTemp, Display, TEXT("[Aim] begin item=%s"), *AimedItemId.ToString());

	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitRelease->OnRelease.AddDynamic(this, &ThisClass::OnAimInputReleased);
	WaitRelease->ReadyForActivation();
}

void UAZ_GA_FirearmAim::OnAimInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_FirearmAim::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) return;
	if (UAZ_AbilitySystemComponent* ASC = ActorInfo ? Cast<UAZ_AbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr)
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		// End/cancel is never permission to resume from a button held across a switch,
		// menu or grab. The controller also only starts aim on a real Started edge.
		ASC->ClearWeaponInput(FGameplayTagContainer(Tags.Input_Action_Aim));
		if (bOwnsAimState)
		{
			bOwnsAimState = false;
			ASC->RemoveStateTag(Tags.Ability_State_Aiming);
			ASC->RemoveStateTag(Tags.Movement_Strafe);
			UE_LOG(LogTemp, Display, TEXT("[Aim] end item=%s cancelled=%d"), *AimedItemId.ToString(), bWasCancelled);
		}
	}
	bOwnsAimState = false;
	AimedItemId.Invalidate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
