// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.h"

#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_WaitInputPressWithTags.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AZ_ConsoleVariables.h"
#include "AZ_GameplayTags.h"
#include "Camera/AZ_GrabCameraShakes.h"
#include "Character/AZ_GrabAnchorLayeredMove.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UAZ_GA_PlayerGrabbed::UAZ_GA_PlayerGrabbed()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// The one ability allowed to run under State.Grabbed (it IS the grab).
	bActivatableWhileGrabbed = true;
	// Camera feel: native shake classes as code fallbacks (class refs, not content paths). CONTENT
	// assets — StruggleMontage (AM_Hero_Struggle) and ShakeIntensityCurve (Curve_GrabShakeIntensity) —
	// are EDITOR-ASSIGNED in BP_GA_PlayerGrabbed's defaults (user rule 2026-07-24: no hardcoded /Game/
	// paths in C++). Both degrade gracefully when unset: no montage = root-only grab, no curve = scale 1.
	RumbleShakeClass = UAZ_CameraShake_GrabRumble::StaticClass();
	JoltShakeClass = UAZ_CameraShake_GrabJolt::StaticClass();
}

APlayerController* UAZ_GA_PlayerGrabbed::GetOwnerPlayerController() const
{
	return CurrentActorInfo ? Cast<APlayerController>(CurrentActorInfo->PlayerController.Get()) : nullptr;
}

float UAZ_GA_PlayerGrabbed::ComputeShakeScale() const
{
	const UCurveFloat* Curve = ShakeIntensityCurve.Get();   // LoadSynchronous happened at activation
	if (!Curve || WindowSecondsEffective <= 0.f)
	{
		return 1.f;
	}
	const UWorld* World = GetWorld();
	const float Progress = World
		? FMath::Clamp(float(World->GetTimeSeconds() - HoldStartTimeSeconds) / WindowSecondsEffective, 0.f, 1.f)
		: 0.f;
	return FMath::Max(0.f, Curve->GetFloatValue(Progress));
}

void UAZ_GA_PlayerGrabbed::ConfigureCDO(UClass* GrantClass)
{
	UClass* TargetClass = GrantClass ? GrantClass : UAZ_GA_PlayerGrabbed::StaticClass();
	UAZ_GA_PlayerGrabbed* CDO = Cast<UAZ_GA_PlayerGrabbed>(TargetClass->GetDefaultObject());
	if (!CDO)
	{
		return;
	}
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (!CDO->AbilityTriggers.ContainsByPredicate(
		[&Tags](const FAbilityTriggerData& T) { return T.TriggerTag == Tags.Event_Grabbed; }))
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = Tags.Event_Grabbed;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		CDO->AbilityTriggers.Add(Trigger);
	}
	if (!CDO->ActivationOwnedTags.HasTagExact(Tags.State_Grabbed))
	{
		CDO->ActivationOwnedTags.AddTag(Tags.State_Grabbed);
	}
}

void UAZ_GA_PlayerGrabbed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] GA_PlayerGrabbed: commit FAILED"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ★ EVENT-TRIGGERED ONLY. Nothing may start this ability except a grabber's Event.Grabbed.
	//
	// The grabbed spec carries the Input.Action.Interact dynamic tag so E-presses reach the MASH while it
	// is running — but that same tag makes it a candidate for ordinary input activation, and
	// AbilityInputTagHeld activates every non-active spec carrying the tag
	// (AZ_AbilitySystemComponent.cpp:72-79). Every input action is bound to Triggered->Held as well as
	// Started->Pressed, so simply HOLDING E in open space activated this with no trigger data: no
	// grabber, no paired montage, no anchor — but State.Grabbed applied and the escape window armed.
	// The player froze themselves for up to WindowSeconds with a "GRABBED!" prompt and nothing holding
	// them. (The grant-site comment claims only Pressed can reach the spec; that is true of
	// AbilityInputTagPressed and false of Held.)
	//
	// Guarding on the PAYLOAD rather than the input path: this ability is meaningless without a grabber,
	// so a missing instigator is the honest test regardless of how activation was reached.
	const AActor* EventInstigator = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	if (!EventInstigator)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;   // ends BEFORE State.Grabbed is applied — nothing to unwind
	}

	Grabber = EventInstigator;
	UE_LOG(LogTemp, Display, TEXT("[Grab] GA_PlayerGrabbed ACTIVE (grabber=%s)"), *GetNameSafe(Grabber.Get()));
	Presses = 0;
	bResolved = false;
	// Same reuse trap as the grabber (InstancedPerActor): stale-true, this makes OnOutcomeBegan early-out,
	// so the second escape never takes over its own montage — the hero stays a follower, its escape
	// montage never plays, and the contact notify that drives the knockback never fires.
	bSelfDrivingOutcome = false;

	// Being caught interrupts whatever the player was doing (mid-punch grab stops the punch + montage).
	// State.Grabbed goes on EXPLICITLY (loose tag, removed in EndAbility) — ActivationOwnedTags is not
	// reliable for BP tuning children: runtime CDO patches never reach their instances (proven in the
	// 2026-07-24 PIE log — instance activated with an empty container while the trigger, which lives in
	// the ASC's event map from grant time, kept working).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CancelAbilities(nullptr, nullptr, this);
		// Absolute count, not Add/Remove: loose tags are COUNTED, and State.Grabbed is the gate that
		// freezes movement, camera and every ability without bActivatableWhileGrabbed. A count left at 1
		// after the paired Remove would freeze the player permanently with no input that can clear it.
		// Not reachable today (this ability does not set bRetriggerInstancedAbility, and the grabber-side
		// pre-check plus the one-grabber token stop a second grab landing on an already-grabbed victim) —
		// but the failure is a softlock, so it is written to be safe by construction rather than by five
		// guards elsewhere continuing to hold.
		ASC->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Grabbed, 1);
		bAppliedGrabbedTag = true;
	}

	// Face the grabber: ProduceInput points the body's OrientationIntent at this target while grabbed,
	// so the rooted hero turns to meet the bite (cleared on every exit in EndAbility).
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		Hero->SetGrabFacingTarget(Grabber.Get());

		// Instrumentation ([Grab] face): the victim's yaw error to the grabber at the catch, and again 0.3s
		// later — by then the catch section is well under way, so the second number is "was the body square
		// when it mattered". Pass line: |err| < 5 at 0.3s from any start angle (GrabbedFacingTime).
		auto FaceErrorDeg = [](const AActor* Victim, const AActor* Holder) -> float
		{
			if (!Victim || !Holder) { return 0.f; }
			const FVector ToHolder = (Holder->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D();
			return ToHolder.IsNearlyZero() ? 0.f
				: FRotator::NormalizeAxis(static_cast<float>(ToHolder.Rotation().Yaw - Victim->GetActorRotation().Yaw));
		};
		auto Dist2D = [](const AActor* A, const AActor* B) -> float
		{
			return (A && B) ? static_cast<float>(FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation())) : -1.f;
		};
		UE_LOG(LogTemp, Display, TEXT("[Grab] face %s: at catch err=%+.0fdeg dist=%.0fcm"),
			*GetNameSafe(Hero), FaceErrorDeg(Hero, Grabber.Get()), Dist2D(Hero, Grabber.Get()));
		if (UWorld* World = GetWorld())
		{
			const TWeakObjectPtr<const AActor> WeakHero = Hero;
			const TWeakObjectPtr<const AActor> WeakHolder = Grabber.Get();
			FTimerHandle FaceProbe;
			World->GetTimerManager().SetTimer(FaceProbe, FTimerDelegate::CreateLambda([WeakHero, WeakHolder, FaceErrorDeg, Dist2D]()
			{
				// Pass: |err| < 5 AND dist within ~10cm of the at-catch value (the rooted body must not slide).
				UE_LOG(LogTemp, Display, TEXT("[Grab] face %s: @0.3s err=%+.0fdeg dist=%.0fcm (pass: |err| < 5, dist ~= at catch)"),
					*GetNameSafe(WeakHero.Get()), FaceErrorDeg(WeakHero.Get(), WeakHolder.Get()), Dist2D(WeakHero.Get(), WeakHolder.Get()));
			}), 0.3f, false);
		}
	}

	// PAIRED route: our half of the shared-origin montage, bound to the grabber's as the follower. The
	// animation owns placement completely, so there is nothing to anchor — no socket lock, no mesh lift.
	// Fall back to the v1 socket anchor only when no paired montage is assigned.
	const bool bPaired = StartPairedFollow();
	if (!bPaired)
	{
		StartGrabAnchor();
	}

	// Tunables with test overrides (az.Grab.*).
	const int32 PressesOverride = AZCVars::GetGrabPressesToEscape();
	const float WindowOverride = AZCVars::GetGrabWindowSeconds();
	PressesNeededEffective = PressesOverride > 0 ? PressesOverride : PressesToEscape;
	WindowSecondsEffective = WindowOverride > 0.f ? WindowOverride : WindowSeconds;

	// The struggle pose: ONE pick from the pool per grab (random or fixed index), self-looped for the
	// whole hold. Replay-on-end keeps the hold unbreakable: no locomotion/flinch shows through mid-grab.
	CachedStruggleMontage = nullptr;
	if (!bPaired && StruggleMontages.Num() > 0)
	{
		const int32 Pick = bRandomStruggleMontage
			? FMath::RandRange(0, StruggleMontages.Num() - 1)
			: FMath::Clamp(StruggleMontageIndex, 0, StruggleMontages.Num() - 1);
		CachedStruggleMontage = StruggleMontages[Pick].LoadSynchronous();
	}
	UE_LOG(LogTemp, Display, TEXT("[Grab] mash armed: %d presses / %.1fs window; struggle montage=%s (pool %d, %s)"),
		PressesNeededEffective, WindowSecondsEffective, *GetNameSafe(CachedStruggleMontage),
		StruggleMontages.Num(), bRandomStruggleMontage ? TEXT("random") : TEXT("indexed"));
	StartStruggleMontage();
	// No montage is not fatal — the lock + mash still work (root-only grab, per design fallback).

	StartMashTask();

	HoldStartTimeSeconds = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		FinishGrab(/*bEscaped*/ false, /*bNotifyGrabber*/ true);
	}), WindowSecondsEffective, false);

	// Camera feel (local viewer): the continuous struggle rumble for the whole hold. The boom pull-in
	// happens in the pawn's CameraGrabbed framing mode (State.Grabbed precedence in UpdateCameraForMode).
	ShakeIntensityCurve.LoadSynchronous();   // one sync load; ComputeShakeScale reads the cached asset
	if (APlayerController* PC = GetOwnerPlayerController())
	{
		if (RumbleShakeClass)
		{
			PC->ClientStartCameraShake(RumbleShakeClass, ComputeShakeScale());
		}
	}

	// A grabber that dies / gets cancelled mid-hold frees us without a verdict.
	WaitReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FAZ_GameplayTags::Get().Event_GrabRelease);
	WaitReleaseTask->EventReceived.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnGrabberReleased);
	WaitReleaseTask->ReadyForActivation();

	// Our own escape montage's contact notify, forwarded to the grabber as its knockback cue.
	WaitShoveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FAZ_GameplayTags::Get().Event_Grab_Shove);
	WaitShoveTask->EventReceived.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnShoveNotify);
	WaitShoveTask->ReadyForActivation();

	// Camera phase change, sent on BOTH outcomes (the escape unfollows as well; the bite does not).
	WaitOutcomeTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FAZ_GameplayTags::Get().Event_Grab_OutcomeBegin);
	WaitOutcomeTask->EventReceived.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnOutcomeBegan);
	WaitOutcomeTask->ReadyForActivation();

	UpdateStruggleHUD();
}

void UAZ_GA_PlayerGrabbed::StartStruggleMontage()
{
	MontageTask = nullptr;
	if (!CachedStruggleMontage)
	{
		return;
	}
	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("StruggleLoop"), CachedStruggleMontage, FGameplayTagContainer(),
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);
	if (!MontageTask)
	{
		return;
	}
	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnStruggleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnStruggleMontageEnded);
	MontageTask->ReadyForActivation();

	// Section self-loop (primary mechanism; the replay binding backs it up).
	if (const AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		if (UAnimInstance* AnimInstance = Hero->GetMesh() ? Hero->GetMesh()->GetAnimInstance() : nullptr)
		{
			const FName LoopSection = CachedStruggleMontage->GetSectionName(0);
			AnimInstance->Montage_SetNextSection(LoopSection, LoopSection, CachedStruggleMontage);
		}
	}
}

void UAZ_GA_PlayerGrabbed::OnStruggleMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (!bResolved && IsActive())
	{
		StartStruggleMontage();
	}
}

bool UAZ_GA_PlayerGrabbed::StartPairedFollow()
{
	CachedPairedMontage = PairedGrabbedMontage.LoadSynchronous();
	if (!CachedPairedMontage)
	{
		return false;
	}

	const AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo();
	UAnimInstance* OwnAnim = Hero && Hero->GetMesh() ? Hero->GetMesh()->GetAnimInstance() : nullptr;
	const AActor* GrabberActor = Grabber.Get();
	const USkeletalMeshComponent* GrabberMesh = GrabberActor
		? GrabberActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* LeaderAnim = GrabberMesh ? GrabberMesh->GetAnimInstance() : nullptr;

	// The leader's montage must ALREADY be playing — GA_ChalkieGrab starts it before firing Event.Grabbed
	// precisely so this call has something to bind to (AnimInstance.h:735 "both montages must be playing").
	UAnimMontage* LeaderMontage = LeaderAnim ? LeaderAnim->GetCurrentActiveMontage() : nullptr;
	if (!OwnAnim || !LeaderAnim || !LeaderMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] paired follow SKIPPED: ownAnim=%d leaderAnim=%d leaderMontage=%s"),
			OwnAnim != nullptr, LeaderAnim != nullptr, *GetNameSafe(LeaderMontage));
		CachedPairedMontage = nullptr;
		return false;
	}

	const float Started = OwnAnim->Montage_Play(CachedPairedMontage, 1.f);
	if (Started <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] paired follow FAILED: %s would not play (slot missing from the hero ABP?)"),
			*GetNameSafe(CachedPairedMontage));
		CachedPairedMontage = nullptr;
		return false;
	}
	if (CachedPairedMontage->GetSectionIndex(CatchSection) != INDEX_NONE)
	{
		OwnAnim->Montage_JumpToSection(CatchSection, CachedPairedMontage);
	}

	// From here the leader owns position, play rate and every section jump. We never steer.
	OwnAnim->MontageSync_Follow(CachedPairedMontage, LeaderAnim, LeaderMontage);

	UE_LOG(LogTemp, Display, TEXT("[Grab] paired follow ARMED: %s follows %s on %s"),
		*GetNameSafe(CachedPairedMontage), *GetNameSafe(LeaderMontage), *GetNameSafe(GrabberActor));
	return true;
}

void UAZ_GA_PlayerGrabbed::StopPairedFollow()
{
	if (!CachedPairedMontage)
	{
		return;
	}
	const AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo();
	if (UAnimInstance* OwnAnim = Hero && Hero->GetMesh() ? Hero->GetMesh()->GetAnimInstance() : nullptr)
	{
		OwnAnim->MontageSync_StopFollowing(CachedPairedMontage);
		// Do NOT stop the montage here: on a normal resolve the leader has already jumped us into an
		// outcome section and we want it to play out. An abnormal end blends it out through the ASC.
	}
	CachedPairedMontage = nullptr;
}

void UAZ_GA_PlayerGrabbed::StartGrabAnchor()
{
	if (!bAnchorToGrabber)
	{
		return;
	}
	AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo();
	AActor* GrabberActor = const_cast<AActor*>(Grabber.Get());
	if (!Hero || !GrabberActor)
	{
		return;
	}
	UAZ_PawnMoverComponent* Mover = Hero->GetMoverComponent();
	USkeletalMeshComponent* HeroMesh = Hero->GetMesh();
	USkeletalMeshComponent* GrabberMesh = GrabberActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mover || !HeroMesh || !GrabberMesh)
	{
		return;
	}

	// Fail LOUD on a bad socket name. GetSocketTransform silently falls back to the component transform
	// when the socket is missing — that would drag the hero to the Chalkie's feet and look like a physics
	// bug rather than a typo. Refuse the lock instead and say why.
	if (!GrabberMesh->DoesSocketExist(GrabberAnchorSocket) || !HeroMesh->DoesSocketExist(GrabbedAnchorSocket))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] anchor SKIPPED: grabber '%s' socket=%s(%d), hero socket=%s(%d)"),
			*GetNameSafe(GrabberActor), *GrabberAnchorSocket.ToString(),
			GrabberMesh->DoesSocketExist(GrabberAnchorSocket),
			*GrabbedAnchorSocket.ToString(), HeroMesh->DoesSocketExist(GrabbedAnchorSocket));
		return;
	}

	const TSharedPtr<FLayeredMove_AZ_GrabAnchor> Anchor = MakeShared<FLayeredMove_AZ_GrabAnchor>();
	Anchor->AnchorMesh = GrabberMesh;
	Anchor->AnchorSocket = GrabberAnchorSocket;
	Anchor->GrabbedMesh = HeroMesh;
	Anchor->GrabbedSocket = GrabbedAnchorSocket;
	Anchor->AnchorSpaceOffset = AnchorSocketOffset;
	Anchor->MaxCorrectionSpeed = AnchorMaxCorrectionSpeed;
	Mover->QueueLayeredMove(Anchor);
	bAnchorQueued = true;

	// HEIGHT is a separate channel: the layered move above is XY-only because the capsule is floor-snapped
	// every tick. The lift rides the mesh instead, so the hero can hang off the ground in the hold.
	if (bAnchorMatchHeight)
	{
		Hero->SetGrabMeshAnchor(GrabberMesh, GrabberAnchorSocket, GrabbedAnchorSocket, AnchorSocketOffset);
	}

	UE_LOG(LogTemp, Display, TEXT("[Grab] anchored: hero %s -> %s %s (offset %s, max %.0f cm/s, height match %d)"),
		*GrabbedAnchorSocket.ToString(), *GetNameSafe(GrabberActor), *GrabberAnchorSocket.ToString(),
		*AnchorSocketOffset.ToCompactString(), AnchorMaxCorrectionSpeed, bAnchorMatchHeight);
}

void UAZ_GA_PlayerGrabbed::StopGrabAnchor()
{
	if (!bAnchorQueued)
	{
		return;
	}
	bAnchorQueued = false;
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		if (UAZ_PawnMoverComponent* Mover = Hero->GetMoverComponent())
		{
			Mover->CancelFeaturesWithTag(FAZ_GameplayTags::Get().Mover_GrabAnchor, /*bRequireExactMatch*/ false);
		}
		// Both channels release together — the mesh eases back down to its authored offset from here.
		Hero->ClearGrabMeshAnchor();
	}
}

void UAZ_GA_PlayerGrabbed::StartMashTask()
{
	// The task ends itself after ONE press (replicated-event rebind constraint — see its header), so
	// the mash loop is task-per-press: count, then arm the next one.
	MashTask = UAZ_AT_WaitInputPressWithTags::WaitInputPressWithTags(
		this, FGameplayTagContainer(), FGameplayTagContainer(), /*bTestAlreadyPressed*/ false);
	if (MashTask)
	{
		MashTask->OnPress.AddDynamic(this, &UAZ_GA_PlayerGrabbed::OnMashPress);
		MashTask->ReadyForActivation();
	}
}

void UAZ_GA_PlayerGrabbed::OnMashPress(float TimeWaited)
{
	if (bResolved)
	{
		return;
	}
	++Presses;
	UE_LOG(LogTemp, Display, TEXT("[Grab] mash press %d/%d"), Presses, PressesNeededEffective);
	UpdateStruggleHUD();

	// Camera feel: the wrench jolt, plus a rumble RESTART at the curve's current scale (the rumble is
	// bSingleInstance, so this retunes the running shake — the ramp toward the window's end).
	if (APlayerController* PC = GetOwnerPlayerController())
	{
		const float Scale = ComputeShakeScale();
		if (JoltShakeClass)
		{
			PC->ClientStartCameraShake(JoltShakeClass, Scale);
		}
		if (RumbleShakeClass)
		{
			PC->ClientStartCameraShake(RumbleShakeClass, Scale);
		}
	}

	if (Presses >= PressesNeededEffective)
	{
		FinishGrab(/*bEscaped*/ true, /*bNotifyGrabber*/ true);
	}
	else
	{
		StartMashTask();
	}
}

void UAZ_GA_PlayerGrabbed::OnGrabberReleased(FGameplayEventData Payload)
{
	// Already resolved = we were riding out the outcome section on the paired route, and this is the
	// leader telling us the scene is done. (FinishGrab would early-out on bResolved and strand us until
	// the safety timer.)
	if (bResolved)
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return;
	}
	// The grab died on the other side mid-hold — free ourselves quietly (no verdict to send).
	FinishGrab(/*bEscaped*/ true, /*bNotifyGrabber*/ false);
}

void UAZ_GA_PlayerGrabbed::OnOutcomeBegan(FGameplayEventData Payload)
{
	// THE PAYOFF IS A DIFFERENT SHOT. Both outcomes reach here (escape and bite) while State.Grabbed is
	// still up, so the pawn keeps its grab framing precedence — we just tell it WHICH grab framing.
	// Without this the whole escape plays behind a 130cm over-the-shoulder boom with the struggle rumble
	// still running, which is a good hold shot and an unwatchable payoff.
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		Hero->SetGrabOutcomeFraming(true);
	}
	if (bStopRumbleOnOutcome)
	{
		if (APlayerController* PC = GetOwnerPlayerController())
		{
			if (RumbleShakeClass)
			{
				// Fade, not cut: the struggle is over, but snapping the shake off mid-frame reads as a bug.
				PC->ClientStopCameraShake(RumbleShakeClass, /*bImmediately*/ false);
			}
		}
	}
	// ESCAPE: take the montage over completely. EventMagnitude carries the grabber's choice as an index
	// into EscapeMontages (negative = the bite, which stays synced and needs nothing here).
	//
	// A STANDALONE montage, not the paired montage's Push/Kick section, and that is the whole point: a
	// montage FOLLOWER is SetPosition'd every frame and can skip notifies, so the contact notify this
	// escape depends on is only reliable once we stop following. Playing our own asset also removes the
	// section-mirror race — we no longer need the leader to have jumped us into the right section first.
	const int32 Index = FMath::RoundToInt(Payload.EventMagnitude);
	if (Index < 0 || bSelfDrivingOutcome || !IsActive())
	{
		return;
	}
	bSelfDrivingOutcome = true;

	AAZ_PawnMoverHeroCharacter* HeroPawn = GetHeroPawnFromActorInfo();
	UAnimInstance* OwnAnim = HeroPawn && HeroPawn->GetMesh() ? HeroPawn->GetMesh()->GetAnimInstance() : nullptr;
	if (!OwnAnim || EscapeMontages.Num() == 0)
	{
		return;   // unauthored: stay synced and let the grabber's watchdog resolve it
	}
	UAnimMontage* Escape = EscapeMontages[FMath::Clamp(Index, 0, EscapeMontages.Num() - 1)].LoadSynchronous();
	if (!Escape)
	{
		return;
	}

	// Unfollow BEFORE playing: sync copies the leader's position unconditionally every tick, so a live
	// link would fight (and win against) whatever we start here.
	if (CachedPairedMontage)
	{
		OwnAnim->MontageSync_StopFollowing(CachedPairedMontage);
		OwnAnim->Montage_Stop(0.2f, CachedPairedMontage);
		CachedPairedMontage = nullptr;
	}

	if (OwnAnim->Montage_Play(Escape, 1.f) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grab] escape montage %s would not play (FullBody slot missing?)"),
			*GetNameSafe(Escape));
		return;
	}
	// We own the ending now — the grabber deliberately stops sending Event.GrabRelease once it has
	// handed the outcome over, so nothing else would ever free us.
	FOnMontageEnded EndDelegate;
	EndDelegate.BindWeakLambda(this, [this](UAnimMontage* /*M*/, bool /*bInterrupted*/)
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	});
	OwnAnim->Montage_SetEndDelegate(EndDelegate, Escape);

	UE_LOG(LogTemp, Display, TEXT("[Grab] outcome framing ON (rumble %s); hero self-driving %s [idx %d]"),
		bStopRumbleOnOutcome ? TEXT("stopped") : TEXT("kept"), *GetNameSafe(Escape), Index);
}

void UAZ_GA_PlayerGrabbed::OnShoveNotify(FGameplayEventData Payload)
{
	// Our escape animation reached its contact frame. We are the only side that can hear this — the
	// notify fires on OUR montage and lands on OUR ASC — so forward it to the grabber, which owns the
	// knockback and the IK release. Doing it this way keeps the animator in control of WHEN the shove
	// lands: move the notify, and the Chalkie flies at the new time with no code change.
	if (!Grabber.IsValid())
	{
		return;
	}
	FGameplayEventData Out;
	Out.EventTag = FAZ_GameplayTags::Get().Event_Grab_Shove;
	Out.Instigator = GetAvatarActorFromActorInfo();
	Out.Target = Grabber.Get();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		const_cast<AActor*>(Grabber.Get()), FAZ_GameplayTags::Get().Event_Grab_Shove, Out);
	UE_LOG(LogTemp, Display, TEXT("[Grab] shove contact -> %s"), *GetNameSafe(Grabber.Get()));
}

void UAZ_GA_PlayerGrabbed::FinishGrab(bool bEscaped, bool bNotifyGrabber)
{
	if (bResolved || !IsActive())
	{
		return;
	}
	bResolved = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowTimer);
	}

	if (bNotifyGrabber && Grabber.IsValid())
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		const FGameplayTag Verdict = bEscaped ? Tags.Event_GrabEscaped : Tags.Event_GrabTimeout;
		FGameplayEventData Payload;
		Payload.EventTag = Verdict;
		Payload.Instigator = GetAvatarActorFromActorInfo();
		Payload.Target = Grabber.Get();
		// The grabber's ASC is on its avatar; the event triggers GA_ChalkieGrab's wait tasks.
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			const_cast<AActor*>(Grabber.Get()), Verdict, Payload);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x6A17, 2.f, bEscaped ? FColor::Green : FColor::Red,
			bEscaped ? TEXT("STRUGGLE: ESCAPED!") : TEXT("STRUGGLE: OVERPOWERED..."));
	}

	// PAIRED: the mash is over but the SCENE is not. Our half of the escape/bite clip is a section of the
	// montage we are following, and the leader only jumps to it now — ending here would cut the hero out
	// of their own escape animation. Stay grabbed and synced; the grabber's Event.GrabRelease (sent from
	// its EndAbility once the outcome section finishes) hands control back.
	if (CachedPairedMontage && bNotifyGrabber)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				UE_LOG(LogTemp, Warning, TEXT("[Grab] outcome tail timed out — releasing the player without a grabber signal"));
				if (IsActive())
				{
					EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
				}
			}), PairedOutcomeMaxSeconds, false);
		}
		return;
	}

	// EndAbility drops State.Grabbed (ActivationOwnedTags) -> movement/camera/abilities unfreeze.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_PlayerGrabbed::UpdateStruggleHUD() const
{
	// v1 meter: on-screen debug line updated in place (stable key). The real UProgressBar widget
	// replaces this in the HUD pass — this keeps the mechanic fully testable without UI assets.
	if (GEngine)
	{
		const int32 Filled = FMath::Clamp(Presses, 0, PressesNeededEffective);
		FString Bar;
		for (int32 i = 0; i < PressesNeededEffective; ++i)
		{
			Bar += (i < Filled) ? TEXT("#") : TEXT("-");
		}
		GEngine->AddOnScreenDebugMessage(0x6A17, 60.f, FColor::Yellow,
			FString::Printf(TEXT("GRABBED! MASH [E]  %s  (%d/%d)"), *Bar, Filled, PressesNeededEffective));
	}
}

void UAZ_GA_PlayerGrabbed::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowTimer);
	}
	// Fade the rumble out on EVERY exit path (escape, timeout, release, cancel). The camera framing
	// glides back by itself once State.Grabbed drops below.
	if (APlayerController* PC = GetOwnerPlayerController())
	{
		if (RumbleShakeClass)
		{
			PC->ClientStopCameraShake(RumbleShakeClass, /*bImmediately*/ false);
		}
	}
	// Paired removal of the explicit State.Grabbed (see ActivateAbility) — unfreezes movement, camera,
	// and the ability surface on every exit path exactly once.
	if (bAppliedGrabbedTag)
	{
		bAppliedGrabbedTag = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			// Absolute 0 — see the set site. A decrement can only ever be as correct as the count that
			// preceded it; setting the value cannot leave the player frozen whatever happened upstream.
			ASC->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Grabbed, 0);
		}
	}
	// Release the sync and the socket lock BEFORE the facing target: all of these must go on every exit
	// path (escape, timeout, grabber death, external cancel) or the hero keeps being driven by a grab
	// that is over. Both are no-ops on the route that didn't use them.
	StopPairedFollow();
	StopGrabAnchor();
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		Hero->SetGrabFacingTarget(nullptr);
		Hero->SetGrabOutcomeFraming(false);   // next grab starts on the tight hold framing again
	}
	Grabber = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
