#include "AbilitySystem/Abilities/AZ_GA_Crouch.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/AZ_HeroCharacter.h"
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
	// bTestAlreadyReleased=true: on the server the activation arrives via replication and the
	// client may ALREADY have released a fast tap — the task then fires immediately instead
	// of waiting forever for an event that already passed.
	UAbilityTask_WaitInputRelease* WaitRelease =
			UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ true);
	WaitRelease->OnRelease.AddDynamic(this, &UAZ_GA_Crouch::OnCrouchInputReleased);
	WaitRelease->ReadyForActivation();   // ← the line everyone forgets in C++ (BP latent nodes call it for you)

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// NO EndAbility here — staying active IS the feature (the held tag is the crouch state).
}

bool UAZ_GA_Crouch::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	// Can always activate to toggle — Crouch/UnCrouch handles validation internally
	return true;
}

void UAZ_GA_Crouch::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_Crouch::OnCrouchInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			   /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);

}
