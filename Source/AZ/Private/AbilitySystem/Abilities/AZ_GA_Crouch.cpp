#include "AbilitySystem/Abilities/AZ_GA_Crouch.h"

#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Character/AZ_HeroCharacter.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


UAZ_GA_Crouch::UAZ_GA_Crouch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAZ_GA_Crouch::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	// Crouch is a toggle: ignore the activation press and wait for the NEXT press.
	// The GAS task carries that edge to authority, including a fast second tap.
	// Retain the existing reflected callback name for loaded Blueprint compatibility.
	UAbilityTask_WaitInputPress* WaitPress =
		UAbilityTask_WaitInputPress::WaitInputPress(this, /*bTestAlreadyPressed*/ false);
	WaitPress->OnPress.AddDynamic(this, &UAZ_GA_Crouch::OnCrouchInputReleased);
	WaitPress->ReadyForActivation();
	// Remaining active owns Movement.Crouching; key release no longer ends it.
}

bool UAZ_GA_Crouch::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Avatar gate (mirrors GA_PawnJump's opt-in pattern): only pawns that can actually crouch — a
	// CharacterMoverComponent (v2 Mover pawn) or an ACharacter (legacy CMC). Without this, pressing
	// crouch while driving a vehicle activated the GA and held a meaningless Movement.Crouching tag
	// on the PlayerState ASC (audit P1-13).
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return false;
	}
	return Avatar->FindComponentByClass<UCharacterMoverComponent>() != nullptr
		|| Avatar->IsA<ACharacter>();
}

void UAZ_GA_Crouch::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_Crouch::OnCrouchInputReleased(float TimeHeld)
{
	// Legacy reflected name; now called by the next-press task to toggle standing.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			   /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);

}
