// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "GameFramework/Character.h"


UAZ_AT_PlayMontageAndWaitForEvent::UAZ_AT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), MontageToPlay(nullptr), AnimRootMotionTranslationScale(0)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
}

void UAZ_AT_PlayMontageAndWaitForEvent::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;
	TWeakObjectPtr AZ_AbilitySystemComponent = GetTargetASC();

	if (AZ_AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			// Bind to event callback
			EventHandle = AZ_AbilitySystemComponent->AddGameplayEventTagContainerDelegate(EventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UAZ_AT_PlayMontageAndWaitForEvent::OnGameplayEvent));

			if (AZ_AbilitySystemComponent->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection) > 0.f)
			{
				// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return;
				}

				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UAZ_AT_PlayMontageAndWaitForEvent::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this, &UAZ_AT_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &UAZ_AT_PlayMontageAndWaitForEvent::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				if (Character && (Character->GetLocalRole() == ROLE_Authority ||
					(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAZ_AT_PlayMontageAndWaitForEvent call to PlayMontage failed!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UAZ_AT_PlayMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAZ_AT_PlayMontageAndWaitForEvent called in Ability %s failed to play montage %s; Task Instance Name %s."), *Ability->GetName(), *GetNameSafe(MontageToPlay), *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			//ABILITY_LOG(Display, TEXT("%s: OnCancelled"), *GetName());
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	SetWaitingOnAvatar();
}

void UAZ_AT_PlayMontageAndWaitForEvent::ExternalCancel()
{
	Super::ExternalCancel();
}

FString UAZ_AT_PlayMontageAndWaitForEvent::GetDebugString() const
{
	return Super::GetDebugString();
}

void UAZ_AT_PlayMontageAndWaitForEvent::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

UAZ_AT_PlayMontageAndWaitForEvent* UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(UGameplayAbility* OwningAbility, FName TaskInstanceName,
                                                                                                 UAnimMontage* MontageToPlay, FGameplayTagContainer EventTags, float Rate, FName StartSection, bool bStopWhenAbilityEnds, float AnimRootMotionTranslationScale)
{
	UAZ_AT_PlayMontageAndWaitForEvent* NewTask = NewAbilityTask<UAZ_AT_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	NewTask->MontageToPlay = MontageToPlay;
	NewTask->EventTags = EventTags;
	NewTask->Rate = Rate;
	NewTask->StartSection = StartSection;
	NewTask->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	NewTask->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	return NewTask;
}

bool UAZ_AT_PlayMontageAndWaitForEvent::StopPlayingMontage() const
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (AbilitySystemComponent.IsValid() && Ability)
	{
		if (AbilitySystemComponent->GetAnimatingAbility() == Ability
			&& AbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			AbilitySystemComponent->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

UAZ_AbilitySystemComponent* UAZ_AT_PlayMontageAndWaitForEvent::GetTargetASC() const
{
	return Cast<UAZ_AbilitySystemComponent>(AbilitySystemComponent);
}

void UAZ_AT_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (InMontage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && (Character->GetLocalRole() == ROLE_Authority ||
				(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}

		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UAZ_AT_PlayMontageAndWaitForEvent::OnAbilityCancelled() const
{
	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UAZ_AT_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	EndTask();
}

void UAZ_AT_PlayMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag InEventTag, const FGameplayEventData* InPayload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *InPayload;
		TempData.EventTag = InEventTag;

		EventReceived.Broadcast(InEventTag, TempData);
	}
}

