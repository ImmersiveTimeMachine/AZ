// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Character/AZ_HeroPawn.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AZ_GameplayTags.h"
#include "GameplayEffect.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"   // Mover_AnimRootMotion
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"

UAZ_GA_MeleeAttack::UAZ_GA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UAZ_GA_MeleeAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return true; //ToDo: HasAmmo();
}

void UAZ_GA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Data-driven GEs on activation (e.g. GE_CombatReady -> refresh the fists-up stance every punch). The GE owns
	// its duration + refresh-on-reapply stacking; ApplyGameplayEffectSpecToOwner routes prediction/authority.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsOnActivate)
	{
		if (!*EffectClass) continue;
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
		if (Spec.IsValid()) ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}

	bIsMovingLatched = false;

	if (const auto* Pawn = GetHeroPawnFromActorInfo())
		if (const USkeletalMeshComponent* Mesh = Pawn->GetMesh())
			if (const UAZ_MoverAnimInstance* Anim = Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()))
				bIsMovingLatched = Anim->ChooserContext.bIsMoving;

	UAnimMontage* Montage = SelectMontage();
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	FGameplayTagContainer EventTags;
	if (HitWindowEventTag.IsValid()) EventTags.AddTag(HitWindowEventTag);

	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("MeleeMontage"), Montage, EventTags,
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->EventReceived.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageEvent);
	MontageTask->ReadyForActivation();   // C++ must call this manually (see the task's header comment)

	// Stage 2: drive the capsule with the punch montage's root motion. The FullBody slot overrides
	// the pose, so RootMotionFromEverything extracts the MONTAGE delta into the attribute; a live
	// FLayeredMove_RootMotionAttribute makes the Mover follow it (OverrideAll). Mirrors the
	// transition-clip bridge in AZ_MoverAnimInstance.cpp:112-116. Scoped to the montage length;
	// cancelled in EndAbility so an interrupted punch doesn't overrun the capsule.
	if (const AAZ_PawnMoverHeroCharacter* Pawn = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAZ_PawnMoverComponent* Mover = Pawn->GetMoverComponent())
		{
			if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)   // proxy follows the replicated transform
			{
				Mover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);  // replace, don't stack
				const TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
				RMMove->DurationMs = Montage->GetPlayLength() * 1000.f;   // Rate is 1.0
				Mover->QueueLayeredMove(RMMove);
			}
		}
	}
}

void UAZ_GA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Stop driving the capsule with the punch RM. No-op if none was queued; on an interrupted
	// punch this cancels the layered move so it doesn't overrun its DurationMs. Self-heals for
	// locomotion (the AnimInstance re-queues transition RM moves each frame as needed).
	if (const AAZ_PawnMoverHeroCharacter* Pawn = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAZ_PawnMoverComponent* Mover = Pawn->GetMoverComponent())
		{
			Mover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
		}
	}

	// (later) remove Ability.State.MeleeAttacking loose tag here.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_MeleeAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UAZ_GA_MeleeAttack::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
}

UAnimMontage* UAZ_GA_MeleeAttack::SelectMontage() const
{
	if (bIsMovingLatched)
		return (Hand == EAZ_MeleeHand::Left) ? PunchMove_L : PunchMove_R;
	return (Hand == EAZ_MeleeHand::Left) ? PunchIdle_L : PunchIdle_R;
}

void UAZ_GA_MeleeAttack::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_MeleeAttack::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// Pass 1: stub. Later: this is the HIT WINDOW — trace from the fist + apply a Damage GE here.
	
}
