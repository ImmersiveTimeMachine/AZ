// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"

#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetCombatMontage / FindAnimSetMontage / ReadConfigFloat / FindBeatEndNotifyTime
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAZ_GA_HitReact::UAZ_GA_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Reactions originate on the authority (vitals + horde both run server-side); montage replicates
	// through the ASC via the task, same as GA_Death.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// A second punch mid-stagger RESTARTS the reaction (EndAbility + fresh activation) instead of being
	// swallowed. Stun-lock on one Chalkie is accepted; difficulty comes from the pack (fight rulebook).
	bRetriggerInstancedAbility = true;
}

void UAZ_GA_HitReact::ConfigureOnCDO(UClass* GrantClass)
{
	// Native tags register after CDO ctors, so triggers/tags cannot be set in the constructor — this is
	// the GA_Death grant-time pattern. Each patch is guarded by its SPECIFIC tag (audit #10): a future
	// addition here must not be silently skipped on a CDO an earlier session already patched.
	UClass* TargetClass = GrantClass ? GrantClass : UAZ_GA_HitReact::StaticClass();
	UAZ_GA_HitReact* CDO = Cast<UAZ_GA_HitReact>(TargetClass->GetDefaultObject());
	if (!CDO)
	{
		return;
	}
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	for (const FGameplayTag& TriggerTag : { Tags.Event_Combat_HitReact, Tags.Event_Combat_StepBack })
	{
		if (!CDO->AbilityTriggers.ContainsByPredicate(
			[&TriggerTag](const FAbilityTriggerData& T) { return T.TriggerTag == TriggerTag; }))
		{
			FAbilityTriggerData Trigger;
			Trigger.TriggerTag = TriggerTag;
			Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
			CDO->AbilityTriggers.Add(Trigger);
		}
	}

	// The Staggered gate IS this ability being active — set/cleared by GAS, no timers to desync.
	if (!CDO->ActivationOwnedTags.HasTagExact(Tags.State_Combat_Staggered))
	{
		CDO->ActivationOwnedTags.AddTag(Tags.State_Combat_Staggered);
	}
	// GRAB ARMOR (rule 8): a mid-grab Chalkie plays no flinch MOTION. Only the motion — damage-lock,
	// scream and melee-cancel live in HandleDamaged, which runs before the event is even sent.
	if (!CDO->ActivationBlockedTags.HasTagExact(Tags.State_Combat_Grabbing))
	{
		CDO->ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	}
}

void UAZ_GA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	// Resolve the clip + timing by trigger. ONE resolve site — HandleDamaged no longer knows montages.
	Desc = FAZ_CombatMontage();
	const bool bStepBack = TriggerEventData && TriggerEventData->EventTag == Tags.Event_Combat_StepBack;
	if (bStepBack)
	{
		// Payload-supplied clip. Montage tasks want a mutable montage; the payload field is const by
		// signature only — these are shared assets we do not mutate.
		Desc.Montage = const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()));
		// Caller beat (crowd pacing) — may SHORTEN the clip's own beat, never lengthen it: the BeatEnd
		// notify still fires at the authored time and acts as the ceiling.
		Desc.ActiveSeconds = TriggerEventData->EventMagnitude;
	}
	else
	{
		// Per-clip descriptor from the anim set (arch step B); legacy fallback = bare montage field +
		// the pawn's FlinchRootMotionSeconds, so un-authored variants keep pre-B behaviour exactly.
		if (!UAZ_GA_MeleeAttack::FindAnimSetCombatMontage(Avatar, TEXT("HitReact"), Desc))
		{
			Desc.Montage = UAZ_GA_MeleeAttack::FindAnimSetMontage(Avatar, TEXT("HitReactMontage"));
			Desc.ActiveSeconds = UAZ_GA_MeleeAttack::ReadConfigFloat(Avatar, TEXT("FlinchRootMotionSeconds"), 0.f);
		}
	}

	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!Avatar || !AnimInstance || !Desc.IsSet())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Montage task: BeatEnd is the only event we wait for; completion callbacks are the lifecycle.
	FGameplayTagContainer EventTags;
	EventTags.AddTag(Tags.Event_Combat_BeatEnd);
	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("HitReactMontage"), Desc.Montage, EventTags, 1.f, NAME_None,
		/*bStopWhenAbilityEnds*/ true, 1.f);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// NOT bound: OnBlendOut. Ending there would drop the Staggered gate at blend START; waiting for
	// OnCompleted makes the gate naturally span beat + blend — what a viewer reads as "still staggering".
	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->EventReceived.AddDynamic(this, &UAZ_GA_HitReact::OnMontageEvent);
	MontageTask->ReadyForActivation();

	// Capsule follows the recoil for the descriptor's RM window (KnockBack_Chase clips walk back in
	// during their second half — driving past the peak turns a knockback into a stroll-back-at-you).
	if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		RootMotionGen = Mover->DriveRootMotion(Desc.ResolveRootMotion());
	}

	// CLOCKS. Primary: the BeatEnd notify authored on the montage (the timeline itself). The cut timer
	// is armed ONLY when that notify doesn't own the beat — un-authored variant, or a caller-shortened
	// step-back — and does the identical stop. The watchdog ends the ability if every event path failed.
	const float NotifyBeat = UAZ_GA_MeleeAttack::FindBeatEndNotifyTime(Desc.Montage);
	const float Beat = Desc.ResolveBeat();
	const bool bNotifyOwnsBeat = NotifyBeat > 0.f && Beat >= NotifyBeat - 0.05f;
	if (!bNotifyOwnsBeat && Desc.IsCutEarly())
	{
		GetWorld()->GetTimerManager().SetTimer(BeatCutTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { StopMontageAtBeat(); }), Beat, false);
	}
	GetWorld()->GetTimerManager().SetTimer(Watchdog,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}), Desc.ResolveGate() + 0.5f, false);
}

void UAZ_GA_HitReact::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (EventTag == FAZ_GameplayTags::Get().Event_Combat_BeatEnd)
	{
		StopMontageAtBeat();
	}
}

void UAZ_GA_HitReact::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_HitReact::StopMontageAtBeat()
{
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && Desc.Montage && AnimInstance->Montage_IsPlaying(Desc.Montage))
	{
		// Blend-out starts here; the task's OnCompleted fires when it finishes → EndAbility → tag drops.
		AnimInstance->Montage_Stop(Desc.BlendOutTime, Desc.Montage);
	}
}

void UAZ_GA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeatCutTimer);
		World->GetTimerManager().ClearTimer(Watchdog);
	}
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			// Generation-scoped: no-ops if something newer (a fresh reaction, an attack) took the bridge.
			Mover->ReleaseRootMotion(RootMotionGen);
		}
	}
	RootMotionGen = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
