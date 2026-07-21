// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"

#include "Animation/AnimMontage.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "Engine/World.h"
#include "MoverTypes.h"
#include "TimerManager.h"

UAZ_GA_ZombieMelee::UAZ_GA_ZombieMelee()
{
	// AI ability: activated by the BT on the server; no prediction needed. Montage still replicates
	// to clients through the ASC.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	DamageAmount = 10.f;
	MeleeRange = 110.f;   // claw contact ~2.0m total — longer than a fist, shorter than a spear
	MeleeRadius = 50.f;
}

void UAZ_GA_ZombieMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Hand = FMath::RandBool() ? EAZ_MeleeHand::Left : EAZ_MeleeHand::Right;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The base queued its RM override for the FULL montage length (8-10s clawing cycles) — re-queue at
	// the bite length so a missed cancel can never root a zombie for seconds (replace-don't-stack).
	if (IsActive())
	{
		if (const AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
			{
				if (Avatar->GetLocalRole() != ROLE_SimulatedProxy)
				{
					Mover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
					const TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
					RMMove->DurationMs = BiteSeconds * 1000.f;
					Mover->QueueLayeredMove(RMMove);
				}
			}
		}
	}

	// Timed bite of the long clawing cycle: end the ability (the montage task stops the montage with a
	// blend-out) after BiteSeconds. GENERATION-GUARDED (audit finding #2): the ability is
	// InstancedPerActor, so a stale timer from an INTERRUPTED bite would otherwise see the NEXT
	// activation as "active" and end it early — every interrupted bite poisoned its successor.
	// (Static map = LC glue; becomes a member FTimerHandle cleared in EndAbility at the batch.)
	if (IsActive())
	{
		static TMap<TWeakObjectPtr<const UAZ_GA_ZombieMelee>, uint64> GBiteGenerations;
		const uint64 ThisGeneration = ++GBiteGenerations.FindOrAdd(this, 0);
		FTimerHandle BiteTimer;
		GetWorld()->GetTimerManager().SetTimer(BiteTimer, FTimerDelegate::CreateWeakLambda(this, [this, ThisGeneration]()
		{
			const uint64* Current = GBiteGenerations.Find(this);
			if (Current && *Current == ThisGeneration && IsActive())
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
