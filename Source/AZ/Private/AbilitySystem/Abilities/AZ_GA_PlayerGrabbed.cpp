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

	Grabber = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	UE_LOG(LogTemp, Display, TEXT("[Grab] GA_PlayerGrabbed ACTIVE (grabber=%s)"), *GetNameSafe(Grabber.Get()));
	Presses = 0;
	bResolved = false;

	// Being caught interrupts whatever the player was doing (mid-punch grab stops the punch + montage).
	// State.Grabbed goes on EXPLICITLY (loose tag, removed in EndAbility) — ActivationOwnedTags is not
	// reliable for BP tuning children: runtime CDO patches never reach their instances (proven in the
	// 2026-07-24 PIE log — instance activated with an empty container while the trigger, which lives in
	// the ASC's event map from grant time, kept working).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CancelAbilities(nullptr, nullptr, this);
		ASC->AddLooseGameplayTag(FAZ_GameplayTags::Get().State_Grabbed);
		bAppliedGrabbedTag = true;
	}

	// Face the grabber: ProduceInput points the body's OrientationIntent at this target while grabbed,
	// so the rooted hero turns to meet the bite (cleared on every exit in EndAbility).
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		Hero->SetGrabFacingTarget(Grabber.Get());
	}

	// Tunables with test overrides (az.Grab.*).
	const int32 PressesOverride = AZCVars::GetGrabPressesToEscape();
	const float WindowOverride = AZCVars::GetGrabWindowSeconds();
	PressesNeededEffective = PressesOverride > 0 ? PressesOverride : PressesToEscape;
	WindowSecondsEffective = WindowOverride > 0.f ? WindowOverride : WindowSeconds;

	// The struggle pose: ONE pick from the pool per grab (random or fixed index), self-looped for the
	// whole hold. Replay-on-end keeps the hold unbreakable: no locomotion/flinch shows through mid-grab.
	CachedStruggleMontage = nullptr;
	if (StruggleMontages.Num() > 0)
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
	// The grab died on the other side — free ourselves quietly (no verdict to send).
	FinishGrab(/*bEscaped*/ true, /*bNotifyGrabber*/ false);
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
			ASC->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Grabbed);
		}
	}
	if (AAZ_PawnMoverHeroCharacter* Hero = GetHeroPawnFromActorInfo())
	{
		Hero->SetGrabFacingTarget(nullptr);
	}
	Grabber = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
