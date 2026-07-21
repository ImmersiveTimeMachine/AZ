// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"

#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAZ_GA_ZombieMelee::UAZ_GA_ZombieMelee()
{
	// AI ability: activated by the BT on the server; no prediction needed. Montage still replicates
	// to clients through the ASC.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	DamageAmount = 10.f;
	MeleeRange = 170.f;
	MeleeRadius = 70.f;
}

void UAZ_GA_ZombieMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Hand = FMath::RandBool() ? EAZ_MeleeHand::Left : EAZ_MeleeHand::Right;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Timed bite of the long clawing cycle: end the ability (the montage task stops the montage with a
	// blend-out) after BiteSeconds. Super may already have ended us on a missing montage — IsActive guards.
	if (IsActive())
	{
		FTimerHandle BiteTimer;
		GetWorld()->GetTimerManager().SetTimer(BiteTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}), BiteSeconds, false);
	}
}

UAnimMontage* UAZ_GA_ZombieMelee::SelectMontage() const
{
	return FindAnimSetMontage(GetAvatarActorFromActorInfo(),
		Hand == EAZ_MeleeHand::Left ? TEXT("AttackMontage_L") : TEXT("AttackMontage_R"));
}
