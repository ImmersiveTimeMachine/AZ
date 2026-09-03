// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_StrikeInteraction.h"

#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

// PSI strike driver (TryStrikeSearch ONLY — Experimental API quarantine, same rule as GA_ChalkieGrab)
#include "PoseSearch/MultiAnimAsset.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "PoseSearch/PoseSearchLibrary.h"

namespace
{
	/** The parent's rotation-only warp twin (file-local there too). Removed alongside WarpTargetName on the
	 *  pair path so a stale LMB lunge target can never steer the paired heavy. */
	FName StrikeFacingWarpTargetName(const FName& WarpTargetName)
	{
		return FName(*(WarpTargetName.ToString() + TEXT("_Facing")));
	}
}

UAZ_GA_StrikeInteraction::UAZ_GA_StrikeInteraction()
{
}

void UAZ_GA_StrikeInteraction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// ---- PRECHECK. Anything short of a live, strikeable Chalkie inside the search window is the parent's
	// warped heavy, untouched — the key must always throw something.
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	AActor* Target = nullptr;
	const TCHAR* Skip = nullptr;
	if (!StrikeDatabase)
	{
		Skip = TEXT("StrikeDatabase unset");
	}
	else if (!StrikeMontage)
	{
		Skip = TEXT("StrikeMontage unset");
	}
	else if (!Avatar || !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		Skip = TEXT("no avatar / authority");
	}
	else
	{
		Target = FindWarpTarget();
		const UAbilitySystemComponent* TargetASC = Target ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
		// A victim still inside a previous pair's knockback IS strikeable: GA_HitReact retriggers on
		// Event.Strike.Victim (its old drive is released generation-scoped), so chained jabs re-pair
		// instead of degrading to the warped fallback for the whole recoil + recover hold.
		FGameplayTagContainer Unstrikeable;
		Unstrikeable.AddTag(Tags.State_Combat_Grabbing);
		Unstrikeable.AddTag(Tags.Character_Dead);
		Unstrikeable.AddTag(Tags.Character_Dying);
		if (!Target)
		{
			Skip = TEXT("no hostile inside WarpSearchDistance");
		}
		else if (!Cast<AAZ_PawnMoverInfectedCharacter>(Target))
		{
			Skip = TEXT("target is not a Chalkie");
		}
		else if (!TargetASC)
		{
			Skip = TEXT("target has no ASC");
		}
		else if (TargetASC->HasAnyMatchingGameplayTags(Unstrikeable))
		{
			Skip = TEXT("target grabbing / dying");
		}
	}
	if (Skip)
	{
		UE_LOG(LogTemp, Display, TEXT("[Strike] %s FALLBACK reason=%s -> warped heavy"), *GetNameSafe(Avatar), Skip);
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}

	// ---- PAIR PATH: the parent's activation front (commit, GEs, latches), minus its montage/warp tail —
	// that runs in BeginStrike once the victim is still, or in the parent if the search says no.
	UAZ_GameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsOnActivate)
	{
		if (!*EffectClass) continue;
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
		if (Spec.IsValid()) ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
	bIsMovingLatched = ResolveAvatarIsMoving();
	WarpTargetLatched = Target;
	TargetGapLatched = FVector::Dist2D(Target->GetActorLocation(), Avatar->GetActorLocation());
	bCancellable = false;
	SetCancelWindowTag(false);
	ActiveMontage = nullptr;
	StrikeTarget = Target;
	bPairLive = false;
	bContactReached = false;
	PairContactTime = 0.f;

	// NO ROOTING MOVE HERE (measured 2026-09-03). The catch roots the PREY before its search because the
	// search and the prey's own rooting live on different actors and different ticks. Here the close-in and
	// the search are the SAME tick on the SAME body: a 250ms zero-velocity override queued first was still
	// live when the close-in (also OverrideVelocity) arrived, and it WON — the victim stood for 0.25s, then
	// covered half the close-in, and the hook opened at gap 162 instead of ~127: four whiffs out of four.
	// One velocity owner on the victim: the close-in, from this tick. (A walking Chalkie is aligned from
	// where it is right now; the close-in replaces its walk on the next sim tick.)
	UE_LOG(LogTemp, Display, TEXT("[Strike] %s -> %s gap=%.0fcm: searching"),
		*GetNameSafe(Avatar), *GetNameSafe(Target), TargetGapLatched);
	BeginStrike();
}

void UAZ_GA_StrikeInteraction::TryBeginStrikeWhenStill(int32 Attempt)
{
	if (!IsActive())
	{
		return;
	}
	const AActor* Target = StrikeTarget.Get();
	if (!Target)
	{
		FallbackToWarpedHeavy(TEXT("victim vanished while rooting"));
		return;
	}
	constexpr float StillSpeed = 15.f;   // cm/s: below this the search sees a stationary victim
	constexpr int32 MaxWaitTicks = 4;    // the 250ms zero-velocity bridge covers this comfortably
	const float VictimSpeed = Target->GetVelocity().Size2D();
	if (VictimSpeed > StillSpeed && Attempt < MaxWaitTicks)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Attempt]()
		{
			TryBeginStrikeWhenStill(Attempt + 1);
		}));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[Strike] search after %d tick(s): victim speed %.0f cm/s"), Attempt, VictimSpeed);
	BeginStrike();
}

void UAZ_GA_StrikeInteraction::BeginStrike()
{
	if (!IsActive())
	{
		return;
	}
	AActor* Target = StrikeTarget.Get();
	FAZ_StrikePair Pair;
	if (!Target || !TryStrikeSearch(Target, Pair))
	{
		FallbackToWarpedHeavy(TEXT("search (see the line above)"));
		return;
	}
	PlayPairedStrike(Pair);
}

void UAZ_GA_StrikeInteraction::FallbackToWarpedHeavy(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Display, TEXT("[Strike] %s FALLBACK reason=%s -> warped heavy"), *GetNameSafe(GetAvatarActorFromActorInfo()), Reason);
	StrikeTarget = nullptr;
	// The parent's whole activation: it re-runs the commit (no cost/cooldown on the fist rail, so harmless)
	// and plays whichever slot its SelectMontage picks — every slot on the BP child is the warped heavy.
	Super::ActivateAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr);
}

bool UAZ_GA_StrikeInteraction::TryStrikeSearch(AActor* Target, FAZ_StrikePair& Out)
{
	// ---- Experimental-API quarantine: every PoseSearch interaction call lives HERE and nowhere else.
	// Role names are the contract with PSS_AZ_Strike's Skeletons array — the schema owns them.
	static const FName AttackerRole(TEXT("Attacker"));
	static const FName VictimRole(TEXT("Victim"));
	static const FName PoseHistoryTag(TEXT("PoseHistory"));

	auto Fallback = [](const TCHAR* Reason) -> bool
	{
		UE_LOG(LogTemp, Warning, TEXT("[Strike] search FALLBACK reason=%s"), Reason);
		return false;
	};

	AActor* Avatar = GetAvatarActorFromActorInfo();
	AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Target);
	USkeletalMeshComponent* VictimMesh = Infected ? Infected->GetMesh() : nullptr;
	UAnimInstance* VictimAnim = VictimMesh ? VictimMesh->GetAnimInstance() : nullptr;
	// Our PoseHistory lives on whichever mesh runs UAZ_MoverAnimInstance — on the MetaHuman hero that is not
	// necessarily the main mesh (body + face + garments each carry components), so find it by class.
	USkeletalMeshComponent* HeroMesh = nullptr;
	UAnimInstance* HeroAnim = nullptr;
	if (Avatar)
	{
		TInlineComponentArray<USkeletalMeshComponent*> HeroMeshes(Avatar);
		for (USkeletalMeshComponent* Mesh : HeroMeshes)
		{
			if (Mesh && Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()))
			{
				HeroMesh = Mesh;
				HeroAnim = Mesh->GetAnimInstance();
				break;
			}
		}
	}
	if (!Avatar || !Infected || !VictimAnim || !HeroAnim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Strike] hero anim=%s victim anim=%s"), *GetNameSafe(HeroAnim), *GetNameSafe(VictimAnim));
		return Fallback(TEXT("missing anim instance"));
	}

	// One query, both actors, fixed roles: we are the Attacker, the Chalkie the Victim (the catch schema is
	// the reverse pair — PSS_AZ_Strike binds the roles to the swapped skeletons).
	FPoseSearchMotionMatchMultiQuery Query;
	Query.Database = StrikeDatabase;
	FPoseSearchAnimContextRoles& SelfRoles = Query.AnimContextsRoles.AddDefaulted_GetRef();
	SelfRoles.AnimContext = HeroAnim;
	SelfRoles.Roles.Add(AttackerRole);
	FPoseSearchAnimContextRoles& VictimRoles = Query.AnimContextsRoles.AddDefaulted_GetRef();
	VictimRoles.AnimContext = VictimAnim;
	VictimRoles.Roles.Add(VictimRole);

	TArray<FPoseSearchBlueprintResult> Results;
	UPoseSearchInteractionLibrary::MotionMatchMulti(
		{ Query }, PoseHistoryTag, FPoseSearchContinuingProperties(), Results);

	const FPoseSearchBlueprintResult* SelfResult = nullptr;
	const FPoseSearchBlueprintResult* VictimResult = nullptr;
	for (const FPoseSearchBlueprintResult& R : Results)
	{
		const AActor* ResultActor = UPoseSearchLibrary::GetActor(R);
		UE_LOG(LogTemp, Display, TEXT("[Strike] search actor=%s role=%s anim=%s t=%.2f rate=%.2f cost=%.1f"),
			*GetNameSafe(ResultActor), *R.Role.ToString(), *GetNameSafe(R.SelectedAnim),
			R.SelectedTime, R.WantedPlayRate, R.SearchCost);
		if (ResultActor == Avatar) { SelfResult = &R; }
		else if (ResultActor == Target) { VictimResult = &R; }
	}
	if (!SelfResult || !VictimResult)
	{
		return Fallback(TEXT("results did not cover both actors"));
	}
	const UMultiAnimAsset* Psia = Cast<UMultiAnimAsset>(SelfResult->SelectedAnim);
	if (!Psia || SelfResult->SearchCost >= BIG_NUMBER)
	{
		return Fallback(TEXT("empty search (no PSIA / cost=MAX) - pose history or index"));
	}
	if (SelfResult->SelectedTime > StrikeEntryMaxTime)
	{
		return Fallback(TEXT("SelectedTime past the wind-up - SamplingRange not honored?"));
	}
	// One owner per fact: our editor-assigned StrikeMontage owns WHAT we play; the PSIA must agree, else
	// the alignment would be computed for content we are not going to run.
	if (Psia->GetAnimationAsset(AttackerRole) != StrikeMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Strike] PSIA attacker anim '%s' != StrikeMontage '%s'"),
			*GetNameSafe(Psia->GetAnimationAsset(AttackerRole)), *GetNameSafe(StrikeMontage));
		return Fallback(TEXT("PSIA/StrikeMontage mismatch"));
	}
	UAnimMontage* VictimMontage = Cast<UAnimMontage>(Psia->GetAnimationAsset(VictimRole));
	if (!VictimMontage)
	{
		return Fallback(TEXT("PSIA victim item is not a montage"));
	}
	const int32 ReactIndex = VictimMontage->GetSectionIndex(ReactSectionName);
	if (ReactIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Strike] victim montage '%s' has no section '%s'"), *VictimMontage->GetName(), *ReactSectionName.ToString());
		return Fallback(TEXT("victim montage has no React section"));
	}
	float ReactStart = 0.f, ReactEnd = 0.f;
	VictimMontage->GetSectionStartAndEndTime(ReactIndex, ReactStart, ReactEnd);
	if (ReactStart <= SelfResult->SelectedTime + 0.05f)
	{
		return Fallback(TEXT("entry frame at or past contact"));
	}
	Out.VictimMontage = VictimMontage;
	Out.StartTime = SelfResult->SelectedTime;
	Out.WantedPlayRate = SelfResult->WantedPlayRate;
	Out.ContactTime = ReactStart;
	Out.CloseSeconds = ReactStart - SelfResult->SelectedTime;

	// ---- Aligned targets: MESH-space world transforms each actor should reach within CloseSeconds to be
	// consistently aligned to the PSIA at the entry frame (header contract) — i.e. where each root sits AT
	// CONTACT, the clips' own root motion over the close-in included. The item weights (attacker 1/1,
	// victim 0/0) make the hero the anchor: his aligned transform should be his own authored travel to
	// contact, nothing else — logged as "hero d", expected ≈ the heavy's root advance to 0.50 (~45cm).
	FTransform VictimAligned = FTransform::Identity;
	UPoseSearchInteractionLibrary::CalculateFullAlignedTransform(*VictimResult, Out.CloseSeconds, /*bWarpUsingRootBone*/ false, VictimAligned);
	FTransform HeroAligned = FTransform::Identity;
	UPoseSearchInteractionLibrary::CalculateFullAlignedTransform(*SelfResult, Out.CloseSeconds, /*bWarpUsingRootBone*/ false, HeroAligned);
	for (int32 i = 0; i < SelfResult->ActorRootTransforms.Num(); ++i)
	{
		UE_LOG(LogTemp, Display, TEXT("[Strike] raw ActorRootTransforms[%d]=(%.0f,%.0f,%.0f yaw %.0f)"), i,
			SelfResult->ActorRootTransforms[i].GetLocation().X, SelfResult->ActorRootTransforms[i].GetLocation().Y,
			SelfResult->ActorRootTransforms[i].GetLocation().Z, SelfResult->ActorRootTransforms[i].Rotator().Yaw);
	}

	// Mesh-space -> actor-space, each with its OWN rig offset (the hero's mesh rides -90 yaw / -92 Z, the
	// Chalkie's -90 yaw / +10 X / -94 Z — not the same, unlike the catch's delta check assumed).
	const FTransform VictimActorRelMesh = Infected->GetActorTransform().GetRelativeTransform(VictimMesh->GetComponentTransform());
	const FTransform HeroActorRelMesh = Avatar->GetActorTransform().GetRelativeTransform(HeroMesh->GetComponentTransform());
	const FTransform VictimActorTarget = VictimActorRelMesh * VictimAligned;
	const FTransform HeroActorTarget = HeroActorRelMesh * HeroAligned;
	const FVector VictimFrom = Infected->GetActorLocation();
	Out.VictimDisplacement = (VictimActorTarget.GetLocation() - VictimFrom) * FVector(1.f, 1.f, 0.f);
	Out.VictimFacing = VictimActorTarget.GetRotation().GetForwardVector().GetSafeNormal2D();
	const float VictimYawDelta = FRotator::NormalizeAxis(
		static_cast<float>(VictimActorTarget.Rotator().Yaw - Infected->GetActorRotation().Yaw));
	const float HeroDelta2D = FVector::Dist2D(HeroActorTarget.GetLocation(), Avatar->GetActorLocation());
	const float HeroYawDelta = FRotator::NormalizeAxis(
		static_cast<float>(HeroActorTarget.Rotator().Yaw - Avatar->GetActorRotation().Yaw));
	UE_LOG(LogTemp, Display, TEXT("[Strike] align victim (%.0f,%.0f yaw %.0f) -> (%.0f,%.0f yaw %.0f) d=%.0fcm dyaw=%+.0f | hero d=%.0fcm dyaw=%+.0f (expect ~ the heavy's own travel to contact, ~0 yaw) | entry t=%.2f rate=%.2f contact=%.2f close=%.2fs | root-root at contact=%.0fcm"),
		VictimFrom.X, VictimFrom.Y, Infected->GetActorRotation().Yaw,
		VictimActorTarget.GetLocation().X, VictimActorTarget.GetLocation().Y, VictimActorTarget.Rotator().Yaw,
		Out.VictimDisplacement.Size2D(), VictimYawDelta, HeroDelta2D, HeroYawDelta,
		Out.StartTime, Out.WantedPlayRate, Out.ContactTime, Out.CloseSeconds,
		FVector::Dist2D(VictimActorTarget.GetLocation(), HeroActorTarget.GetLocation()));
	if (Out.VictimDisplacement.Size2D() > MaxCloseInDistance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Strike] aligned victim target %.0fcm away > MaxCloseInDistance %.0f"), Out.VictimDisplacement.Size2D(), MaxCloseInDistance);
		return Fallback(TEXT("aligned target too far for the close-in"));
	}
	return true;
}

void UAZ_GA_StrikeInteraction::PlayPairedStrike(const FAZ_StrikePair& Pair)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	AActor* Target = StrikeTarget.Get();
	if (!Avatar || !Target || !Pair.VictimMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 1. NO WARP ON THE PAIR — the PSIA owns the geometry. Clear anything a previous LMB swing registered.
	if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
	{
		Warping->RemoveWarpTarget(WarpTargetName);
		Warping->RemoveWarpTarget(StrikeFacingWarpTargetName(WarpTargetName));
	}

	// 2. CLOSE-IN on the victim, through the sim, ending AT CONTACT (its root-motion drive starts there —
	// GA_HitReact defers it to the React section, so the two never overlap on the capsule).
	if (UAZ_PawnMoverComponent* VictimMover = Target->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		if (!Pair.VictimDisplacement.IsNearlyZero(1.f) && Pair.CloseSeconds > KINDA_SMALL_NUMBER)
		{
			const TSharedPtr<FLayeredMove_LinearVelocity> CloseMove = MakeShared<FLayeredMove_LinearVelocity>();
			CloseMove->Velocity = Pair.VictimDisplacement / Pair.CloseSeconds;
			CloseMove->DurationMs = Pair.CloseSeconds * 1000.f;
			CloseMove->MixMode = EMoveMixMode::OverrideVelocity;
			VictimMover->QueueLayeredMove(CloseMove);
		}
	}

	// 3. OUR HALF at the entry frame. Same task/tag/binding shape as the parent so the hit window, cancel
	// window and beat all work unchanged; root motion drives what is left of the trimmed montage and
	// bReleaseRootMotionOnHit ends it at contact (no carry-through into the knockback).
	FGameplayTagContainer EventTags;
	EventTags.AddTag(Tags.Event_Montage_Melee_WindowBegin);
	EventTags.AddTag(Tags.Event_Montage_Melee_WindowEnd);
	EventTags.AddTag(Tags.Event_Combat_BeatEnd);
	EventTags.AddTag(Tags.Event_Combat_CancelOpen);
	EventTags.AddTag(Tags.Event_Combat_CancelClose);
	ActiveMontage = StrikeMontage;
	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("MeleeMontage"), StrikeMontage, EventTags,
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->EventReceived.AddDynamic(this, &ThisClass::OnMontageEvent);
	MontageTask->ReadyForActivation();
	UAnimInstance* HeroAnim = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (HeroAnim && Pair.StartTime > KINDA_SMALL_NUMBER)
	{
		HeroAnim->Montage_SetPosition(StrikeMontage, Pair.StartTime);
	}
	if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		const float Remaining = FMath::Max(0.1f, StrikeMontage->GetPlayLength() - Pair.StartTime);
		RootMotionGen = Mover->DriveRootMotion(Remaining);
	}

	// 4. THE VICTIM'S HALF — synchronous, same frame as ours. GA_HitReact takes it from here (montage at
	// the same entry frame, Staggered + StruckPair up, facing us, drive at contact).
	FGameplayEventData Payload;
	Payload.EventTag = Tags.Event_Strike_Victim;
	Payload.Instigator = Avatar;
	Payload.Target = Target;
	Payload.OptionalObject = Pair.VictimMontage;
	Payload.EventMagnitude = Pair.StartTime;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, Tags.Event_Strike_Victim, Payload);
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	bPairLive = TargetASC && TargetASC->HasMatchingGameplayTag(Tags.State_Combat_StruckPair);
	PairContactTime = Pair.ContactTime;
	UE_LOG(LogTemp, Display, TEXT("[Strike] pair %s: hero %s@%.2f + victim %s@%.2f, contact@%.2f, close-in %.0fcm over %.2fs"),
		bPairLive ? TEXT("LIVE") : TEXT("REFUSED (victim did not take it - it reacts normally on the hit)"),
		*StrikeMontage->GetName(), Pair.StartTime, *Pair.VictimMontage->GetName(), Pair.StartTime,
		Pair.ContactTime, Pair.VictimDisplacement.Size2D(), Pair.CloseSeconds);

	// 5. PROBES (the failure-axis instruments): who moved, and where the fist actually is at contact.
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<const AActor> WeakSelf = Avatar;
		const TWeakObjectPtr<const AActor> WeakVictim = Target;
		const TWeakObjectPtr<const USkeletalMeshComponent> WeakHeroMesh = GetAvatarMesh();
		const FVector VictimStart = Target->GetActorLocation();
		const FVector HeroStart = Avatar->GetActorLocation();
		const FName Knuckle = KnuckleSocket_R;
		const FName Chest = VictimChestSocket;
		auto Probe = [WeakSelf, WeakVictim, WeakHeroMesh, VictimStart, HeroStart, Knuckle, Chest](const TCHAR* Tag, bool bContact)
		{
			const AActor* Self = WeakSelf.Get();
			const AActor* Victim = WeakVictim.Get();
			if (!Self || !Victim) { return; }
			UE_LOG(LogTemp, Display, TEXT("[Strike] victim@%s moved=%.0fcm hero moved=%.0fcm root-root=%.0fcm victim vel=%.0fcm/s"), Tag,
				FVector::Dist2D(Victim->GetActorLocation(), VictimStart), FVector::Dist2D(Self->GetActorLocation(), HeroStart),
				FVector::Dist2D(Self->GetActorLocation(), Victim->GetActorLocation()), Victim->GetVelocity().Size2D());
			if (bContact)
			{
				const USkeletalMeshComponent* HeroMesh = WeakHeroMesh.Get();
				const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Victim);
				const USkeletalMeshComponent* VictimMesh = Infected ? Infected->GetMesh() : nullptr;
				if (HeroMesh && VictimMesh)
				{
					const FVector Fist = HeroMesh->GetSocketLocation(Knuckle);
					const FVector ChestLoc = VictimMesh->GetSocketLocation(Chest);
					UE_LOG(LogTemp, Display, TEXT("[Strike] contact@%s fist->chest=%.0fcm (planar %.0f, dz %.0f; authored ~8cm inside the torso axis)"), Tag,
						FVector::Dist(Fist, ChestLoc), FVector::Dist2D(Fist, ChestLoc), Fist.Z - ChestLoc.Z);
				}
			}
		};
		const float ContactIn = FMath::Max(0.01f, Pair.ContactTime - Pair.StartTime);
		World->GetTimerManager().SetTimer(ProbeMidTimer, FTimerDelegate::CreateLambda([Probe, ContactIn]() { Probe(*FString::Printf(TEXT("%.2fs"), ContactIn * 0.5f), false); }), ContactIn * 0.5f, false);
		World->GetTimerManager().SetTimer(ContactProbeTimer, FTimerDelegate::CreateWeakLambda(this, [this, Probe, ContactIn]()
		{
			bContactReached = true;
			Probe(*FString::Printf(TEXT("%.2fs"), ContactIn), true);
		}), ContactIn, false);
	}
}

void UAZ_GA_StrikeInteraction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProbeMidTimer);
		World->GetTimerManager().ClearTimer(ContactProbeTimer);
	}
	// A pair that ends BEFORE contact (we were hit, grabbed, or died mid-swing) must not leave the victim
	// playing a knockback for a punch that never landed. From contact on the hit is real and the reaction
	// stays — it belongs to the victim's ability now.
	if (bPairLive && !bContactReached)
	{
		if (AActor* Target = StrikeTarget.Get())
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
			{
				const FGameplayTagContainer ReactionTags(FAZ_GameplayTags::Get().Ability_Combat_HitReact);
				TargetASC->CancelAbilities(&ReactionTags);
				UE_LOG(LogTemp, Display, TEXT("[Strike] ABORT: pair ended before contact (cancelled=%d) - victim %s released"),
					bWasCancelled, *GetNameSafe(Target));
			}
		}
	}
	bPairLive = false;
	bContactReached = false;
	StrikeTarget = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
