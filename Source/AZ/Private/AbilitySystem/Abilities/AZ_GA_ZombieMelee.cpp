// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"

#include "Animation/AnimMontage.h"

UAZ_GA_ZombieMelee::UAZ_GA_ZombieMelee()
{
	// AI ability: activated by the BT on the server; no prediction needed. Montage still replicates
	// to clients through the ASC.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	DamageAmount = 10.f;
	MeleeRange = 110.f;   // warp-search shape only now — what's worth lunging at
	MeleeRadius = 50.f;
	SweepSphereRadius = 15.f;   // claws rake wider than a fist
}

void UAZ_GA_ZombieMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Hand = FMath::RandBool() ? EAZ_MeleeHand::Left : EAZ_MeleeHand::Right;

	// The bite's length now lives ON THE CLIP: an Event.Combat.BeatEnd notify authored on the attack
	// montages at the bite beat. The base handles everything — ends the ability on the notify, defaults
	// the RM window to it, and arms the watchdog. This replaced a BiteSeconds timer plus a static
	// generation map that guarded against stale timers poisoning re-activations (audit finding #2) —
	// with an event there is nothing to go stale, so both were deleted outright.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

UAnimMontage* UAZ_GA_ZombieMelee::SelectMontage() const
{
	return FindAnimSetMontage(GetAvatarActorFromActorInfo(),
		Hand == EAZ_MeleeHand::Left ? TEXT("AttackMontage_L") : TEXT("AttackMontage_R"));
}
