// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_ChalkieGrab.h"

#include "Character/AZ_PawnMoverHeroCharacter.h"  // releasing the victim's grab hand-IK on contact
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"  // ResolveShoveDescriptor - one owner for the shove's timing

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
	// NOTE: this CDO patch is inert for the containers GAS reads off the INSTANCE — kept only so the tag
	// is visible when inspecting the class. The real application is the explicit loose tag in
	// ActivateAbility (bAppliedGrabbingTag). See UAZ_GameplayAbility::DeclareAbilityTags.
	if (CDO && !CDO->ActivationOwnedTags.HasTagExact(GrabbingTag))
	{
		CDO->ActivationOwnedTags.AddTag(GrabbingTag);
	}
}

void UAZ_GA_ChalkieGrab::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();

	const FAZ_GameplayTags& T = FAZ_GameplayTags::Get();
	CancelAbilitiesWithTag.AddTag(T.Ability_Combat_Melee);   // the grab IS the attack — drop the swing
	ActivationBlockedTags.AddTag(T.State_Combat_Staggered);
	ActivationBlockedTags.AddTag(T.Character_Dead);
	ActivationBlockedTags.AddTag(T.Character_Dying);
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

	// PAIRED route first: one sectioned montage drives the whole grab and the player's montage follows
	// it. Reject a montage missing the sections we steer by — driving a section that doesn't exist fails
	// silently (the montage just runs to its end), which reads in PIE as "the grab ignored my mash".
	CachedPairedMontage = PairedGrabMontage.LoadSynchronous();
	if (CachedPairedMontage)
	{
		const bool bHasCatch = CachedPairedMontage->GetSectionIndex(CatchSection) != INDEX_NONE;
		const bool bHasWrestle = CachedPairedMontage->GetSectionIndex(WrestleSection) != INDEX_NONE;
		const bool bHasFail = CachedPairedMontage->GetSectionIndex(FailSection) != INDEX_NONE;
		if (!bHasCatch || !bHasWrestle || !bHasFail)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grab] paired montage '%s' REJECTED: Catch(%s)=%d Wrestle(%s)=%d Fail(%s)=%d — falling back to the v1 loop/exit pair"),
				*GetNameSafe(CachedPairedMontage), *CatchSection.ToString(), bHasCatch,
				*WrestleSection.ToString(), bHasWrestle, *FailSection.ToString(), bHasFail);
			CachedPairedMontage = nullptr;
		}
	}

	UAnimMontage* Loop = CachedPairedMontage
		? nullptr
		: UAZ_GA_MeleeAttack::FindAnimSetMontage(Avatar, TEXT("GrabLoopMontage"));

	// Pre-check BEFORE firing the event: a prey already in another Chalkie's grab must not double-grab.
	// (The grab token upstream makes this near-impossible; this is the belt to that suspender.)
	if (!Avatar || !TargetASC || (!CachedPairedMontage && !Loop) || TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] %s ABORT pre-check: Avatar=%d Target=%s TargetASC=%d Paired=%d Loop=%d AlreadyGrabbed=%d"),
			*GetNameSafe(Avatar), Avatar != nullptr, *GetNameSafe(Target), TargetASC != nullptr,
			CachedPairedMontage != nullptr, Loop != nullptr,
			TargetASC ? TargetASC->HasMatchingGameplayTag(Tags.State_Grabbed) : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GrabTarget = Target;
	// ★ EVERY PER-GRAB LATCH RESETS HERE. InstancedPerActor means ONE instance serves every grab this
	// Chalkie ever performs, so anything left set survives into the next one. bOutcomeHandedToHero is the
	// expensive case: stale-true, it makes HandOffShoveToReaction early-out, so the second escape sends no
	// Event.Combat.GrabShoved, its watchdog is also suppressed (same guard), and the shove silently falls
	// back to the paired montage's 2.3s / 39.6cm section instead of the 3.1s / 117cm knockback — i.e.
	// "the first knockback is fine and the next one is short". EndAbility is NOT the place for this: an
	// abnormal end can skip it, and the next activation must not inherit anything either way.
	bResolved = false;
	bResolvedEscaped = false;
	bOutcomeHandedToHero = false;
	ChosenEscapeIndex = 0;

	// Publish the prey to the pawn so the anim layer can aim the grab hand-IK at the victim's grip
	// sockets. The anim instance must not reach into GAS or the AI tree for this — same reason the hero
	// reads its grabber off SetGrabFacingTarget rather than off the ability. Cleared in EndAbility.
	if (AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Avatar))
	{
		Infected->SetGrabTarget(Target);
	}

	// Explicit loose tag (removed in EndAbility): the flinch carve-out keys on this — a Chalkie
	// mid-grab is armored against the stagger montage that would break the hold on the slot.
	if (UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo())
	{
		// Absolute count (loose tags are COUNTED) — a leaked count here leaves the Chalkie permanently
		// armored against its own hit reactions, i.e. an enemy that can never be staggered again.
		SelfASC->SetLooseGameplayTagCount(Tags.State_Combat_Grabbing, 1);
		bAppliedGrabbingTag = true;
	}

	// Verdict listeners BEFORE the event fires — the player ability could in principle resolve fast.
	WaitEscapedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_GrabEscaped);
	WaitEscapedTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnEscaped);
	WaitEscapedTask->ReadyForActivation();
	WaitTimeoutTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_GrabTimeout);
	WaitTimeoutTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnTimeout);
	WaitTimeoutTask->ReadyForActivation();
	// The victim's escape animation tells US when the shove connects (its notify, forwarded here).
	WaitShoveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_Grab_Shove);
	WaitShoveTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnHeroShove);
	WaitShoveTask->ReadyForActivation();

	// COLLISION CARVE-OUT (user 2026-07-24: "closer"): the PAIR ignores each other's movement collision
	// for the hold. On the paired route this is not optional — shared-origin clips put both capsules at
	// ONE transform, i.e. fully interpenetrating, which solid collision would fight every tick.
	if (UPrimitiveComponent* ChalkieRoot = Cast<UPrimitiveComponent>(Avatar->GetRootComponent()))
	{
		ChalkieRoot->IgnoreActorWhenMoving(Target, true);
	}
	if (UPrimitiveComponent* HeroRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		HeroRoot->IgnoreActorWhenMoving(Avatar, true);
	}
	bAppliedMoveIgnore = true;

	// CLOSE-IN: slide the grabber onto the prey THROUGH the Mover sim (a short layered velocity move,
	// the same mechanism the knockback uses). The ATTACKER travels; the player keeps their position.
	//
	// Stops at GrabHoldDistance, which is ~92 and NOT 0 — the two actors are rotationally OPPOSED here,
	// so co-locating them puts the clips' baked body offsets back to back. See the property's comment
	// before changing it; this was tested and reverted on 2026-08-04.
	if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		const FVector HeroLoc = Target->GetActorLocation();
		const FVector DirToChalkie = (Avatar->GetActorLocation() - HeroLoc).GetSafeNormal2D();
		const FVector ContactPoint = HeroLoc + DirToChalkie * GrabHoldDistance;
		const FVector Displacement = (ContactPoint - Avatar->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (!Displacement.IsNearlyZero(1.f) && GrabCloseSeconds > KINDA_SMALL_NUMBER)
		{
			const TSharedPtr<FLayeredMove_LinearVelocity> CloseMove = MakeShared<FLayeredMove_LinearVelocity>();
			CloseMove->Velocity = Displacement / GrabCloseSeconds;
			CloseMove->DurationMs = GrabCloseSeconds * 1000.f;
			CloseMove->MixMode = EMoveMixMode::OverrideVelocity;
			Mover->QueueLayeredMove(CloseMove);
		}
	}

	// PAIRED: our montage must ALREADY BE PLAYING before the player's ability activates, because that
	// ability calls MontageSync_Follow against it and the engine requires both montages to be live
	// (AnimInstance.h:735). SendGameplayEventToActor below is synchronous, so this ordering is the whole
	// handshake. A failed catch ends the ability, which stops this montage (bStopWhenAbilityEnds).
	if (CachedPairedMontage)
	{
		StartLoopMontage();
		if (!LoopMontageTask)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

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
	// Loop is deliberately null on the paired route — report whichever montage is actually driving.
	const UAnimMontage* HoldMontage = CachedPairedMontage ? CachedPairedMontage : Loop;
	UE_LOG(LogTemp, Display, TEXT("[Grab] %s HOLD started on %s (%s %s, %.1fs)"),
		*GetNameSafe(Avatar), *GetNameSafe(Target),
		CachedPairedMontage ? TEXT("paired") : TEXT("loop"), *GetNameSafe(HoldMontage),
		HoldMontage ? HoldMontage->GetPlayLength() : 0.f);

	// PACK STEP-BACK (user 2026-07-24): the rest of the pack recoils off the seized prey — the moment
	// reads as THE grab, not another day in the mosh pit. Subsystem plays each engaged Chalkie's own
	// variant KB montage for the beat.
	if (UAZ_HordeSubsystem* Horde = GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
	{
		AAIController* SelfAI = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;
		Horde->NotifyGrabStarted(Cast<AAZ_InfectedAIController>(SelfAI), Target, PackStepBackSeconds);
	}

	// V1 route only: the loop montage, self-looped until a verdict stops it. (The paired montage was
	// already started above, before the catch event, so the follower had something to sync to.)
	if (!CachedPairedMontage)
	{
		CachedLoopMontage = Loop;
		StartLoopMontage();
		if (!LoopMontageTask)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
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

UAnimInstance* UAZ_GA_ChalkieGrab::GetOwnAnimInstance() const
{
	const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* Mesh = Infected ? Infected->GetMesh() : nullptr;
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UAZ_GA_ChalkieGrab::SetGrabStage(const FGameplayTag& NewStage)
{
	if (ActiveStageTag == NewStage)
	{
		return;
	}
	UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo();
	if (!SelfASC)
	{
		return;
	}
	// Absolute counts on both sides of the swap: stages change several times per grab
	// (Catch -> Wrestle -> outcome), so this is the site where a drifting count would accumulate fastest.
	if (ActiveStageTag.IsValid())
	{
		SelfASC->SetLooseGameplayTagCount(ActiveStageTag, 0);
	}
	ActiveStageTag = NewStage;
	if (ActiveStageTag.IsValid())
	{
		SelfASC->SetLooseGameplayTagCount(ActiveStageTag, 1);
	}
}

void UAZ_GA_ChalkieGrab::StartLoopMontage()
{
	LoopMontageTask = nullptr;
	UAnimMontage* Montage = CachedPairedMontage ? CachedPairedMontage : CachedLoopMontage;
	if (!Montage)
	{
		return;
	}
	// Paired starts at the catch section; v1 starts wherever the clip starts.
	const FName StartSection = CachedPairedMontage ? CatchSection : NAME_None;
	// The outcome sections carry Event.Grab.OutcomeBegin on their first frame. The task subscribes BY TAG
	// and silently drops anything unlisted, so this container is what makes the shove drivable at all.
	FGameplayTagContainer GrabEventTags;
	GrabEventTags.AddTag(FAZ_GameplayTags::Get().Event_Grab_OutcomeBegin);

	LoopMontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("GrabLoop"), Montage, GrabEventTags,
		/*Rate*/ 1.f, StartSection, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);
	if (!LoopMontageTask)
	{
		return;
	}
	// Replay on natural end/blend-out. NOT OnInterrupted — the exit montage preempts deliberately.
	LoopMontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnLoopMontageEnded);
	LoopMontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnLoopMontageEnded);
	LoopMontageTask->EventReceived.AddDynamic(this, &UAZ_GA_ChalkieGrab::OnGrabMontageEvent);
	LoopMontageTask->ReadyForActivation();

	UAnimInstance* AnimInstance = GetOwnAnimInstance();
	if (!AnimInstance)
	{
		return;
	}
	if (CachedPairedMontage)
	{
		// The hold loop is authored INTO the montage (Wrestle links to itself), so nothing to force here.
		// Re-assert it anyway: a resolve queues an outcome onto that link, and a replay after an
		// interruption would otherwise inherit the queued outcome and skip the hold entirely.
		AnimInstance->Montage_SetNextSection(WrestleSection, WrestleSection, CachedPairedMontage);
		SetGrabStage(FAZ_GameplayTags::Get().State_Grab_Catching);

		// Catching -> Wrestling when the catch clip runs out. A timer rather than a notify: the stage tag
		// is bookkeeping for other systems, so it must not depend on notify authoring inside the montage.
		const int32 CatchIndex = CachedPairedMontage->GetSectionIndex(CatchSection);
		const float CatchLength = (CatchIndex != INDEX_NONE) ? CachedPairedMontage->GetSectionLength(CatchIndex) : 0.f;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StageTimer);
			if (CatchLength > KINDA_SMALL_NUMBER)
			{
				World->GetTimerManager().SetTimer(StageTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (!bResolved && IsActive())
					{
						SetGrabStage(FAZ_GameplayTags::Get().State_Grab_Wrestling);
					}
				}), CatchLength, false);
			}
			else
			{
				SetGrabStage(FAZ_GameplayTags::Get().State_Grab_Wrestling);
			}
		}
	}
	else
	{
		// Section self-loop (primary loop mechanism; the replay binding is the belt to this suspender).
		const FName LoopSection = CachedLoopMontage->GetSectionName(0);
		AnimInstance->Montage_SetNextSection(LoopSection, LoopSection, CachedLoopMontage);
	}
}

void UAZ_GA_ChalkieGrab::OnGrabMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (EventTag != FAZ_GameplayTags::Get().Event_Grab_OutcomeBegin || !CachedPairedMontage)
	{
		return;
	}
	// Hand the outcome to the hero: camera phase on both, and on an escape the index of the standalone
	// montage it should now play under its own steam. Sent on the frame the bodies leave the hold pose.
	NotifyHeroOutcomeBegan();

	// FAIL keeps the paired route intact: the bite is authored as a two-body scene and the hero follows
	// into its own half. Only the ESCAPE diverges.
	if (!bResolvedEscaped)
	{
		return;
	}

	// The SHOVE itself is no longer ours to time. It lands when the hero's escape animation says it
	// does — measured at 1.35s into a 2.30s clip, because both NAAT escape clips hold still and only
	// then depart 39.6cm. Firing here instead would discard the entire wind-up and launch the knockback
	// before the hero has touched anything. Until that event arrives we keep playing our authored
	// Pushed/Kicked section, which is exactly the wind-up.
	//
	// WATCHDOG, not the mechanism: if the notify is missing (montage unassigned, clip re-authored) the
	// escape must still resolve rather than hang on a signal that is never coming.
	if (UWorld* World = GetWorld())
	{
		// Section length when we can resolve one, else a plain constant. GetSectionLength(INDEX_NONE)
		// indexes the section array directly and would crash, so the lookup is guarded rather than
		// chained — a missing section here is exactly the degraded case this watchdog exists for.
		float Fallback = 2.3f;
		if (const UAnimInstance* Anim = CachedPairedMontage ? GetOwnAnimInstance() : nullptr)
		{
			const int32 Idx = CachedPairedMontage->GetSectionIndex(Anim->Montage_GetCurrentSection(CachedPairedMontage));
			if (Idx != INDEX_NONE)
			{
				Fallback = CachedPairedMontage->GetSectionLength(Idx);
			}
		}
		World->GetTimerManager().SetTimer(ShoveWatchdog, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive() && !bOutcomeHandedToHero)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Grab] shove notify never arrived — firing the knockback on the watchdog"));
				HandOffShoveToReaction();
			}
		}), FMath::Max(0.2f, Fallback), false);
	}
}

void UAZ_GA_ChalkieGrab::OnHeroShove(FGameplayEventData Payload)
{
	// The hero's escape montage reached its contact frame. THIS is the knockback's cue.
	HandOffShoveToReaction();
}

void UAZ_GA_ChalkieGrab::NotifyHeroOutcomeBegan()
{
	if (!GrabTarget.IsValid())
	{
		return;
	}
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	FGameplayEventData Payload;
	Payload.EventTag = Tags.Event_Grab_OutcomeBegin;
	Payload.Instigator = GetAvatarActorFromActorInfo();
	Payload.Target = GrabTarget.Get();
	// WHICH escape, by index. The victim no longer learns the outcome by having a section jump mirrored
	// onto it, so the choice has to travel as data — and it stays the SERVER's choice, made here.
	// Negative = the fail path (no escape montage to play, camera phase only).
	Payload.EventMagnitude = bResolvedEscaped ? static_cast<float>(ChosenEscapeIndex) : -1.f;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GrabTarget.Get(), Tags.Event_Grab_OutcomeBegin, Payload);
}

void UAZ_GA_ChalkieGrab::HandOffShoveToReaction()
{
	if (bOutcomeHandedToHero)
	{
		return;   // OutcomeBegin fires once per section, but a replayed montage could re-enter
	}
	bOutcomeHandedToHero = true;
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShoveWatchdog);
	}

	// ---- ORDER IS LOAD-BEARING. ----

	// (1) LET GO. The hands were still pushing until this frame; from here the two bodies separate by
	// 117cm and any IK left running visibly stretches after the other actor. Our own grip target and the
	// hero's IK gate both drop here, one line each, and their blend speeds fade the rest.
	if (AAZ_PawnMoverInfectedCharacter* Self = Cast<AAZ_PawnMoverInfectedCharacter>(GetAvatarActorFromActorInfo()))
	{
		Self->SetGrabTarget(nullptr);
	}
	if (AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GrabTarget.Get()))
	{
		Hero->SetGrabIKReleased(true);
	}

	// (2) DROP THE GRAB ARMOR before triggering the reaction. State.Combat.Grabbing is in GA_HitReact's
	// ActivationBlockedTags (rule 8: a mid-grab Chalkie plays no flinch), and those containers are read
	// off the ABILITY INSTANCE — so leaving it up makes the shove silently not activate, with no error.
	if (bAppliedGrabbingTag)
	{
		bAppliedGrabbingTag = false;
		if (UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo())
		{
			SelfASC->SetLooseGameplayTagCount(Tags.State_Combat_Grabbing, 0);
		}
	}

	// (3) Stop our half so the slot is free for the knockback. bInterrupt defaults true, which routes to
	// the montage task's OnInterrupted — deliberately NOT bound on the loop task. Bind it, or stop with
	// bInterrupt=false, and this re-enters OnLoopMontageEnded -> EndAbility from inside Resolve.
	if (UAnimInstance* AnimInstance = GetOwnAnimInstance())
	{
		AnimInstance->Montage_Stop(0.2f, CachedPairedMontage);
	}

	// (4) Hand the body to the reaction ability. Synchronous: GA_HitReact has activated, taken the slot,
	// queued its own generation-scoped root-motion drive and set the Staggered gate before this returns.
	// Nothing below may assume our montage or our drive still exists.
	FGameplayEventData ShovePayload;
	ShovePayload.EventTag = Tags.Event_Combat_GrabShoved;
	ShovePayload.Instigator = GrabTarget.Get();
	ShovePayload.Target = GetAvatarActorFromActorInfo();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetAvatarActorFromActorInfo(), Tags.Event_Combat_GrabShoved, ShovePayload);

	// STAY ALIVE across the shove. Not for the animation — GA_HitReact owns that outright now — but for
	// two lifetimes that used to end with the outcome section: the BT task is latent on THIS ability, so
	// ending now would release the attack/grab tokens and unlock crowd rotation while the Chalkie is
	// still being hurled backwards; and EndAbility restores the pair-collision carve-out, which must not
	// happen while the two capsules are still coincident.
	// LONGEST possible reaction, not the one that was picked: the reaction ability rolls its own random
	// entry and we cannot see which. Re-rolling here to "match" would produce a second, different draw —
	// the hold must simply never end before the knockback does, because it restores pair collision and
	// releases the crowd tokens.
	const float HoldSeconds = UAZ_GA_HitReact::GetShoveHoldSeconds(GetAvatarActorFromActorInfo());
	UE_LOG(LogTemp, Display, TEXT("[Grab] shove handed to GA_HitReact; holding the grab %.2fs for tokens + collision"),
		HoldSeconds);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HandoffTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}), FMath::Max(0.1f, HoldSeconds), false);
	}
}

void UAZ_GA_ChalkieGrab::OnLoopMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (bResolved)
	{
		// PAIRED: the outcome section just finished (it links to nothing), so the grab is over — the whole
		// thing lived in one montage and there is no separate exit clip to wait on.
		if (CachedPairedMontage && IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return;
	}
	if (IsActive())
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
	bResolvedEscaped = bPlayerEscaped;
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

	// PAIRED: the outcome is a SECTION of the montage already running, not a new montage. We are the
	// LEADER — we are the only side that may steer, and the player's montage mirrors this jump through
	// MontageSync_Follow (which only mirrors while both sit in a same-named section, hence identical
	// section tables on the two assets).
	//
	// QUEUED, not immediate: Grab_To_Wrestle both starts and ends on the hold pose but passes ~9cm away
	// at its midpoint, so cutting mid-clip pops on both bodies while switching at the loop boundary is
	// seamless. Costs up to one wrestle cycle of latency; the press-time camera jolt covers it.
	if (CachedPairedMontage)
	{
		FName Outcome = FailSection;
		if (bPlayerEscaped && EscapeSections.Num() > 0)
		{
			ChosenEscapeIndex = FMath::RandRange(0, EscapeSections.Num() - 1);
			Outcome = EscapeSections[ChosenEscapeIndex];
		}
		if (CachedPairedMontage->GetSectionIndex(Outcome) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grab] outcome section '%s' missing from '%s' — ending the hold instead"),
				*Outcome.ToString(), *GetNameSafe(CachedPairedMontage));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		if (UAnimInstance* AnimInstance = GetOwnAnimInstance())
		{
			AnimInstance->Montage_SetNextSection(WrestleSection, Outcome, CachedPairedMontage);
			SetGrabStage(FAZ_GameplayTags::Get().State_Grab_Resolving);
			UE_LOG(LogTemp, Display, TEXT("[Grab] outcome queued: %s -> %s"),
				*WrestleSection.ToString(), *Outcome.ToString());
			return;   // OnLoopMontageEnded ends the ability once that section plays out
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// V1 (unpaired) route. There is no outcome NOTIFY on this path — the exit is a separate montage, not
	// a section — so the camera phase is announced here instead, at the equivalent moment.
	NotifyHeroOutcomeBegan();

	// The ESCAPE goes through the SAME reaction hand-off as the paired one rather than through
	// GrabEscapeMontage: every anim set points that field at AM_Zombie_KB_Atk_1, which is an _IPC capture
	// measuring 0.0cm of travel, so the v1 escape has never actually separated the two bodies.
	// GA_HitReact's descriptor route plays a clip that does.
	if (bPlayerEscaped)
	{
		HandOffShoveToReaction();
		return;
	}
	PlayExitMontage(TEXT("GrabEndMontage"));
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
		World->GetTimerManager().ClearTimer(StageTimer);
		World->GetTimerManager().ClearTimer(HandoffTimer);
		World->GetTimerManager().ClearTimer(ShoveWatchdog);
	}
	// Stage tag off before the grabbing tag — both are loose tags we own, and leaving a stage behind
	// would tell the crowd brain a grab is still running on this Chalkie.
	SetGrabStage(FGameplayTag());

	// Release the anim layer's grip target on EVERY exit path, or the hands keep reaching for prey that
	// has already been freed (the IK alpha would stay pinned at 1 forever).
	if (AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(GetAvatarActorFromActorInfo()))
	{
		Infected->SetGrabTarget(nullptr);
	}
	if (bAppliedGrabbingTag)
	{
		bAppliedGrabbingTag = false;
		if (UAbilitySystemComponent* SelfASC = GetAbilitySystemComponentFromActorInfo())
		{
			SelfASC->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Combat_Grabbing, 0);
		}
	}
	// No root-motion release here any more: the shove's drive belongs to GA_HitReact, which owns it for
	// its own lifetime through its own generation. Releasing from here would be a second owner reaching
	// for someone else's move.
	CachedPairedMontage = nullptr;

	// The scene is over — tell the player, on EVERY exit path. Abnormal ends (BT abort, death, external
	// cancel) must not leave them frozen; RESOLVED ends matter just as much on the paired route, where
	// the player stays grabbed and synced through the outcome section and is waiting for exactly this
	// signal to hand control back. Harmless if they already ended: the tag check below fails and nothing
	// is sent.
	// ...EXCEPT when we already handed the outcome over. Then the hero is deliberately still holding
	// State.Grabbed while it plays its own shove/kick, and this signal would END its ability outright —
	// dropping the tag, unfreezing movement and camera mid-animation, roughly 2.3s early. On the escape
	// path the victim owns its own exit; here we would just be cutting it off.
	if (GrabTarget.IsValid() && !bOutcomeHandedToHero)
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
