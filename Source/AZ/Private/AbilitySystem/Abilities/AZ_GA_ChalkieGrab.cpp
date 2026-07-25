// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_ChalkieGrab.h"

#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetMontage
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AI/AZ_HordeSubsystem.h"
#include "AI/AZ_InfectedAIController.h"   // AZ_ChalkieBBKeys
#include "AZ_GameplayTags.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Engine/World.h"
#include "MoverTypes.h"
#include "TimerManager.h"

UAZ_GA_ChalkieGrab::UAZ_GA_ChalkieGrab()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// AI ability: activated by the BT on the server; montages replicate through the ASC.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	DamageEffect = UAZ_GE_Damage::StaticClass();
}

void UAZ_GA_ChalkieGrab::ConfigureCDO(UClass* GrantClass)
{
	UClass* TargetClass = GrantClass ? GrantClass : UAZ_GA_ChalkieGrab::StaticClass();
	UAZ_GA_ChalkieGrab* CDO = Cast<UAZ_GA_ChalkieGrab>(TargetClass->GetDefaultObject());
	const FGameplayTag& GrabbingTag = FAZ_GameplayTags::Get().State_Combat_Grabbing;
	if (CDO && !CDO->ActivationOwnedTags.HasTagExact(GrabbingTag))
	{
		CDO->ActivationOwnedTags.AddTag(GrabbingTag);
	}
}

void UAZ_GA_ChalkieGrab::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const AAIController* AI = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;
	const UBlackboardComponent* BB = AI ? AI->GetBlackboardComponent() : nullptr;
	AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(AZ_ChalkieBBKeys::TargetActor)) : nullptr;
	UAbilitySystemComponent* TargetASC = Target ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;

	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	UAnimMontage* Loop = UAZ_GA_MeleeAttack::FindAnimSetMontage(Avatar, TEXT("GrabLoopMontage"));
	// Pre-check BEFORE firing the event: a prey already in another Chalkie's grab must not double-grab.
	// (The grab token upstream makes this near-impossible; this is the belt to that suspender.)
	if (!Avatar || !TargetASC || !Loop || TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] %s ABORT pre-check: Avatar=%d Target=%s TargetASC=%d Loop=%d AlreadyGrabbed=%d"),
			*GetNameSafe(Avatar), Avatar != nullptr, *GetNameSafe(Target), TargetASC != nullptr, Loop != nullptr,
			TargetASC ? TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed) : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GrabTarget = Target;
	bResolved = false;

	// Explicit loose tag (removed in EndAbility): the flinch carve-out keys on this — a Chalkie
	// mid-grab is armored against the stagger montage that would break the hold on the slot.
	if (UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo())
	{
		SelfASC->AddLooseGameplayTag(Tags.State_Combat_Grabbing);
		bAppliedGrabbingTag = true;
	}

	// Verdict listeners BEFORE the event fires — the player ability could in principle resolve fast.
	WaitEscapedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_GrabEscaped);
	WaitEscapedTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnEscaped);
	WaitEscapedTask->ReadyForActivation();
	WaitTimeoutTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_GrabTimeout);
	WaitTimeoutTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnTimeout);
	WaitTimeoutTask->ReadyForActivation();

	// Catch the player: the event triggers GA_PlayerGrabbed on the PLAYER's own avatar.
	FGameplayEventData Payload;
	Payload.EventTag = Tags.Event_Grabbed;
	Payload.Instigator = Avatar;
	Payload.Target = Target;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, Tags.Event_Grabbed, Payload);

	// Activation is synchronous — if the grab didn't take (blocked, failed commit), walk away cleanly.
	if (!TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		FGameplayTagContainer Owned;
		TargetASC->GetOwnedGameplayTags(Owned);
		UE_LOG(LogTemp, Warning, TEXT("[Grab] %s ABORT: Event.Grabbed sent to %s but State.Grabbed NOT on targetASC=%s(%p); owned='%s'"),
			*GetNameSafe(Avatar), *GetNameSafe(Target), *GetNameSafe(TargetASC), TargetASC, *Owned.ToStringSimple());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[Grab] %s HOLD started on %s (loop %s, %.1fs)"),
		*GetNameSafe(Avatar), *GetNameSafe(Target), *GetNameSafe(Loop), Loop->GetPlayLength());

	// PACK STEP-BACK (user 2026-07-24): the rest of the pack recoils off the seized prey — the moment
	// reads as THE grab, not another day in the mosh pit. Subsystem plays each engaged Chalkie's own
	// variant KB montage for the beat.
	if (UAZ_HordeSubsystem* Horde = GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
	{
		AAIController* SelfAI = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;
		Horde->NotifyGrabStarted(Cast<AAZ_InfectedAIController>(SelfAI), Target, PackStepBackSeconds);
	}

	// The hold: the loop montage, self-looped until a verdict stops it. Replay-on-end binding makes the
	// hold UNBREAKABLE — during a grab, nothing but the two grab anims may show (user rule).
	CachedLoopMontage = Loop;
	StartLoopMontage();
	if (!LoopMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// COLLISION CARVE-OUT (user 2026-07-24: "closer"): the PAIR ignores each other's movement collision
	// for the hold, so the close-in can reach a GrabHoldDistance INSIDE capsule contact (~70cm) for a
	// TLOU-tight clinch. Mutual + paired-restored in EndAbility on every exit.
	if (UPrimitiveComponent* ChalkieRoot = Cast<UPrimitiveComponent>(Avatar->GetRootComponent()))
	{
		ChalkieRoot->IgnoreActorWhenMoving(Target, true);
	}
	if (UPrimitiveComponent* HeroRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		HeroRoot->IgnoreActorWhenMoving(Avatar, true);
	}
	bAppliedMoveIgnore = true;

	// CLOSE-IN (user 2026-07-24: distant grabs look fake): slide the grabber to GrabHoldDistance from
	// the prey THROUGH the Mover sim — a short layered velocity move, the same mechanism the knockback
	// uses. Attachment intent without attachment: once in contact both actors are rooted and mutually
	// facing, so the pose holds itself and the move simply expires (nothing to detach on release).
	if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		const FVector HeroLoc = Target->GetActorLocation();
		const FVector DirToChalkie = (Avatar->GetActorLocation() - HeroLoc).GetSafeNormal2D();
		const FVector ContactPoint = HeroLoc + DirToChalkie * GrabHoldDistance;
		const FVector Displacement = (ContactPoint - Avatar->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (!DirToChalkie.IsNearlyZero() && !Displacement.IsNearlyZero(1.f) && GrabCloseSeconds > KINDA_SMALL_NUMBER)
		{
			const TSharedPtr<FLayeredMove_LinearVelocity> CloseMove = MakeShared<FLayeredMove_LinearVelocity>();
			CloseMove->Velocity = Displacement / GrabCloseSeconds;
			CloseMove->DurationMs = GrabCloseSeconds * 1000.f;
			CloseMove->MixMode = EMoveMixMode::OverrideVelocity;
			Mover->QueueLayeredMove(CloseMove);
		}
	}

	// Silent-player safety: a verdict that never arrives must not wedge the Chalkie OR the BT.
	GetWorld()->GetTimerManager().SetTimer(SafetyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bResolved && IsActive())
		{
			Resolve(/*bPlayerEscaped*/ true);
		}
	}), MaxHoldSeconds, false);
}

void UAZ_GA_ChalkieGrab::StartLoopMontage()
{
	LoopMontageTask = nullptr;
	if (!CachedLoopMontage)
	{
		return;
	}
	LoopMontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("GrabLoop"), CachedLoopMontage, FGameplayTagContainer(),
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);
	if (!LoopMontageTask)
	{
		return;
	}
	// Replay on natural end/blend-out. NOT OnInterrupted — the exit montage preempts deliberately.
	LoopMontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnLoopMontageEnded);
	LoopMontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnLoopMontageEnded);
	LoopMontageTask->ReadyForActivation();

	// Section self-loop (primary loop mechanism; the replay binding is the belt to this suspender).
	if (const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAnimInstance* AnimInstance = Infected->GetMesh() ? Infected->GetMesh()->GetAnimInstance() : nullptr)
		{
			const FName LoopSection = CachedLoopMontage->GetSectionName(0);
			AnimInstance->Montage_SetNextSection(LoopSection, LoopSection, CachedLoopMontage);
		}
	}
}

void UAZ_GA_ChalkieGrab::OnLoopMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (!bResolved && IsActive())
	{
		StartLoopMontage();
	}
}

void UAZ_GA_ChalkieGrab::OnEscaped(FGameplayEventData Payload)
{
	Resolve(/*bPlayerEscaped*/ true);
}

void UAZ_GA_ChalkieGrab::OnTimeout(FGameplayEventData Payload)
{
	Resolve(/*bPlayerEscaped*/ false);
}

void UAZ_GA_ChalkieGrab::Resolve(bool bPlayerEscaped)
{
	if (bResolved || !IsActive())
	{
		return;
	}
	bResolved = true;
	UE_LOG(LogTemp, Display, TEXT("[Grab] %s RESOLVE: player %s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), bPlayerEscaped ? TEXT("ESCAPED") : TEXT("overpowered (timeout)"));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyTimer);
	}

	if (!bPlayerEscaped)
	{
		// The attack lands: heavy chunk through the normal S1 damage spine (attacker owns the damage,
		// so a lethal chunk credits the Chalkie exactly like a claw kill would).
		if (UAbilitySystemComponent* TargetASC = GrabTarget.IsValid()
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GrabTarget.Get()) : nullptr)
		{
			if (DamageEffect)
			{
				FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, FailDamageAmount);
					if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
					{
						SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
					}
				}
			}
		}
	}

	PlayExitMontage(bPlayerEscaped ? TEXT("GrabEscapeMontage") : TEXT("GrabEndMontage"));
}

void UAZ_GA_ChalkieGrab::PlayExitMontage(FName MontageProperty)
{
	UAnimMontage* Exit = UAZ_GA_MeleeAttack::FindAnimSetMontage(GetAvatarActorFromActorInfo(), MontageProperty);
	if (!Exit)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	// Same slot as the loop -> playing this preempts the hold pose in one blend.
	ExitMontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("GrabExit"), Exit, FGameplayTagContainer(),
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);
	if (!ExitMontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	ExitMontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnExitMontageFinished);
	ExitMontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnExitMontageFinished);
	ExitMontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnExitMontageFinished);
	ExitMontageTask->ReadyForActivation();
}

void UAZ_GA_ChalkieGrab::OnExitMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_ChalkieGrab::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyTimer);
	}
	if (bAppliedGrabbingTag)
	{
		bAppliedGrabbingTag = false;
		if (UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo())
		{
			SelfASC->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Combat_Grabbing);
		}
	}

	// Abnormal end mid-hold (BT abort, death, external cancel): the player must NEVER stay frozen —
	// tell their ability the grab is gone. Resolved exits already freed them (they sent the verdict).
	if (!bResolved && GrabTarget.IsValid())
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GrabTarget.Get()))
		{
			if (TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed))
			{
				FGameplayEventData Payload;
				Payload.EventTag = Tags.Event_GrabRelease;
				Payload.Instigator = GetAvatarActorFromActorInfo();
				Payload.Target = GrabTarget.Get();
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GrabTarget.Get(), Tags.Event_GrabRelease, Payload);
			}
		}
	}
	// Restore pair collision (paired with the carve-out at hold start). Overlap at restore time is
	// fine — the next movement depenetrates, and the escape stagger covers the separation visually.
	if (bAppliedMoveIgnore)
	{
		bAppliedMoveIgnore = false;
		AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if (AvatarActor && GrabTarget.IsValid())
		{
			if (UPrimitiveComponent* ChalkieRoot = Cast<UPrimitiveComponent>(AvatarActor->GetRootComponent()))
			{
				ChalkieRoot->IgnoreActorWhenMoving(GrabTarget.Get(), false);
			}
			if (UPrimitiveComponent* HeroRoot = Cast<UPrimitiveComponent>(GrabTarget->GetRootComponent()))
			{
				HeroRoot->IgnoreActorWhenMoving(AvatarActor, false);
			}
		}
	}
	GrabTarget = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
