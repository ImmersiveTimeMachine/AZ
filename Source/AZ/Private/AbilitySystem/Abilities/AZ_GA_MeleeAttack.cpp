// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Character/AZ_HeroPawn.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AZ_GameplayTags.h" 

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
	
	bIsMovingLatched = false;

	if (const AAZ_HeroPawn* Pawn = GetHeroPawnFromActorInfo())
		if (USkeletalMeshComponent* Mesh = Pawn->GetMainMesh())
			if (UAZ_MoverAnimInstance* Anim = Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()))
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

}

void UAZ_GA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
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
