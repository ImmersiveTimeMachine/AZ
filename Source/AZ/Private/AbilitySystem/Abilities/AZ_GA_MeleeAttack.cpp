// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AZ_InfectedAnimInstance.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AZ_GameplayTags.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GenericTeamAgentInterface.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"   // Mover_AnimRootMotion
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"

UAZ_GA_MeleeAttack::UAZ_GA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DamageEffect = UAZ_GE_Damage::StaticClass();
}

USkeletalMeshComponent* UAZ_GA_MeleeAttack::GetAvatarMesh() const
{
	// Explicit casts, not FindComponentByClass — the hero carries extra skeletal meshes (equipment
	// proxies); each pawn's GetMesh() is the authoritative body.
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(Avatar))
	{
		return Hero->GetMesh();
	}
	if (const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Avatar))
	{
		return Infected->GetMesh();
	}
	return nullptr;
}

bool UAZ_GA_MeleeAttack::ResolveAvatarIsMoving() const
{
	const USkeletalMeshComponent* Mesh = GetAvatarMesh();
	const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (const UAZ_MoverAnimInstance* HeroAnim = Cast<UAZ_MoverAnimInstance>(Anim))
	{
		return HeroAnim->ChooserContext.bIsMoving;   // hero: INTENT (trajectory), per v2 doctrine
	}
	if (const UAZ_InfectedAnimInstance* InfectedAnim = Cast<UAZ_InfectedAnimInstance>(Anim))
	{
		return InfectedAnim->IsMoving();             // infected: commanded-motion ground speed
	}
	return false;
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

	bIsMovingLatched = ResolveAvatarIsMoving();

	// CDO ctors run before native-tag registration — resolve the default hit-window tag lazily.
	if (!HitWindowEventTag.IsValid())
	{
		HitWindowEventTag = FAZ_GameplayTags::Get().Event_Montage_Melee_Hit;
	}

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
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())   // pawn-class-agnostic: hero AND infected
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			if (Avatar->GetLocalRole() != ROLE_SimulatedProxy)   // proxy follows the replicated transform
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
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
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

UAnimMontage* UAZ_GA_MeleeAttack::FindAnimSetMontage(const AActor* Avatar, FName MontageProperty)
{
	if (!Avatar)
	{
		return nullptr;
	}
	const FObjectProperty* SetProperty = CastField<FObjectProperty>(Avatar->GetClass()->FindPropertyByName(TEXT("AnimSet")));
	const UObject* AnimSet = SetProperty ? SetProperty->GetObjectPropertyValue_InContainer(Avatar) : nullptr;
	if (!AnimSet)
	{
		return nullptr;
	}
	const FObjectProperty* Field = CastField<FObjectProperty>(AnimSet->GetClass()->FindPropertyByName(MontageProperty));
	return Field ? Cast<UAnimMontage>(Field->GetObjectPropertyValue_InContainer(AnimSet)) : nullptr;
}

void UAZ_GA_MeleeAttack::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_MeleeAttack::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// THE HIT WINDOW (S1 damage spine). Server-authoritative: the predicted client plays the montage
	// but only the authority deals damage (results replicate back via attributes/tags).
	if (EventTag != HitWindowEventTag || !DamageEffect)
	{
		return;
	}
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		return;
	}

	// Forward sphere sweep from the avatar's center — a fat capsule-height cone-substitute. Socket-accurate
	// per-hand sweeps are a P3 refinement; range/radius are generous on purpose (zombie-arm reach).
	const FVector Forward = Avatar->GetActorForwardVector();
	const FVector Start = Avatar->GetActorLocation();
	const FVector End = Start + Forward * MeleeRange;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeSweep), /*bTraceComplex*/ false, Avatar);
	Avatar->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(MeleeRadius), Params);

	TArray<AActor*> AlreadyHit;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Avatar || AlreadyHit.Contains(Target))
		{
			continue;
		}
		AlreadyHit.Add(Target);

		// Team filter: only hostiles take the hit (no horde friendly-fire, no smacking your co-op partner).
		if (FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, DamageAmount);
			Spec.Data->GetContext().AddHitResult(Hit);   // death/hit-react abilities read direction from this
			if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
}
