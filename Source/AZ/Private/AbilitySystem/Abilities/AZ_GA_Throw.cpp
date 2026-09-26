// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Throw.h"

#include "AZ_GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "Player/AZ_PlayerController.h"
#include "TimerManager.h"
#include "Throwables/AZ_ThrowLaunchSolver.h"
#include "Throwables/AZ_ThrowPreviewComponent.h"
#include "Throwables/AZ_ThrowableHandComponent.h"
#include "Throwables/AZ_ThrowPresentationProfile.h"
#include "Throwables/AZ_ThrowableDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "Throwables/AZ_ThrowableProjectile.h"
#include "UObject/Class.h"

namespace
{
	bool HasIgnitionTool(const UAZ_Inv_CommonUI_InventoryComponent* Inventory, const UAZ_ThrowableDefinition* Definition)
	{
		if (!Definition || !Definition->RequiredIgnitionTool.IsValid()) return true;
		if (!Inventory) return false;
		for (const auto* Candidate : Inventory->GetItems())
		{
			if (IsValid(Candidate) && Candidate->GetTotalStackCount() > 0
				&& Candidate->GetLocation() == EAZ_InventoryItemLocation::Backpack
				&& Candidate->GetItemManifest().GetItemTypeTag().MatchesTagExact(Definition->RequiredIgnitionTool)) return true;
		}
		return false;
	}
}

UAZ_GA_Throw::UAZ_GA_Throw()
{
	// InstancedPerActor: the action carries live state (phase, frozen aim, reservation id) across several
	// frames and tasks, so it cannot be a shared CDO invocation.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bServerRespectsRemoteAbilityCancellation = true;
}

void UAZ_GA_Throw::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	// Preparing is its own state: nothing is committed or spent while aiming, and other systems must be able
	// to tell "lining up a throw" apart from "the throw is happening".
	ActivationOwnedTags.AddTag(Tags.Ability_State_ThrowPreparing);
	// Same refusals the firearm uses. A dead, grabbed or staggered character does not line up a throw.
	ActivationBlockedTags.AddTag(Tags.Character_Dead);
	ActivationBlockedTags.AddTag(Tags.Character_Dying);
	ActivationBlockedTags.AddTag(Tags.Character_Stunned);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Staggered);
	ActivationBlockedTags.AddTag(Tags.State_Combat_StruckPair);
	ActivationBlockedTags.AddTag(Tags.Ability_State_Reloading);
	// Self-blocking, the jump precedent: the action stays ACTIVE for the whole hold, so this is what
	// guarantees a held button can never start a second throw on top of the one in progress.
	ActivationBlockedTags.AddTag(Tags.Ability_State_ThrowPreparing);
	ActivationBlockedTags.AddTag(Tags.Ability_State_MeleeAttacking);
	// Sprinting is cancelled rather than blocking: the player asked to aim, and refusing the press because
	// they happened to be running would read as a dropped input.
	CancelAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
	BlockAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
	// Default the cue to the registered tag. Left to the asset it would silently be an empty container,
	// which PlayMontageAndWaitForEvent treats as "every event", so any unrelated notify would launch.
	if (!ReleaseEventTag.IsValid())
	{
		ReleaseEventTag = Tags.Event_Throw_Release;
	}
}

void UAZ_GA_Throw::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The hero is a Mover APawn, NOT an ACharacter — casting to ACharacter here would fail on every
	// activation and the whole action would silently never run.
	const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	const AAZ_PlayerController* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
	const UAZ_QuickBarComponent* QuickBar = Controller ? Controller->FindComponentByClass<UAZ_QuickBarComponent>() : nullptr;
	UAZ_Inv_CommonUI_InventoryComponent* Inventory =
		Controller ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	// ★ No IsInventoryInputCaptured() guard. It belonged to the old entry model, where a click on RMB started
	// the aim and had to be told apart from a click on the inventory UI. Entry is now READYING the item —
	// which the player does FROM that very UI, while it still owns input — so the guard rejected every
	// legitimate entry (measured 2026-09-18: uiCaptured=1 on every activation, the ready pose never played).
	// There is no click left to misread: the throw and cancel presses are routed separately, and the
	// controller already drops those while the menu is up.
	if (!Hero || !QuickBar || !Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] activation refused: hero=%d ctrl=%d quickbar=%d inventory=%d"),
			Hero ? 1 : 0, Controller ? 1 : 0, QuickBar ? 1 : 0, Inventory ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Resolve the ready item by CAPABILITY, never by category: a unique knife is a weapon and a stone is
	// neither potion nor firearm, so the fragment's presence is the only correct test.
	SourceItemId = QuickBar->GetReadyItemId();
	const UAZ_Inv_CommonUI_InventoryItem* Item = Inventory->FindItemById(SourceItemId);
	const auto* Fragment = Item
		? Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_ThrowableFragment>() : nullptr;
	Definition = Fragment ? Fragment->ThrowableDefinition.Get() : nullptr;
	Profile = Definition ? Definition->DefaultProfile.Get() : nullptr;
	const bool bHasIgnitionTool = HasIgnitionTool(Inventory, Definition);
	if (!Definition || !Profile || !Profile->IsUsable() || !bHasIgnitionTool)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] activation refused: item=%s found=%d fragment=%d definition=%d profile=%d usable=%d ignitionTool=%d"),
			*SourceItemId.ToString(), Item ? 1 : 0, Fragment ? 1 : 0, Definition ? 1 : 0, Profile ? 1 : 0,
			Profile && Profile->IsUsable() ? 1 : 0, bHasIgnitionTool ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Identity only. The RESERVATION now happens at the wind-up, not here: readying a grenade enters this
	// state and STAYS in it, so reserving on activation would hold a unit for as long as the grenade is
	// selected rather than for as long as a throw is actually being committed.
	ThrowActionId = FGuid::NewGuid();

	Phase = EAZ_ThrowPhase::Preparing;
	// The wind-up owns the upper-body slot from here until the action ends.
	SetHandActionOwnership(true);
	bPendingCommit = false;
	bCancelRequested = false;
	AcceptedAim = Controller->GetControlRotation();
	AcceptedAimDistance = Definition->FarArcDistance;   // replaced by the first preview tick

	// ★ NO release listener. Hold-to-aim is gone: readying the grenade enters this state and the player stays
	// in it, so there is no held button whose release could mean anything. WaitInputRelease would be actively
	// dangerous here — with bTestAlreadyReleased it fires immediately for an ability nobody pressed, latching
	// a commit that throws the grenade on its own about a second later. The throw is now an explicit press,
	// routed in from the controller as RequestThrow().

	// Start: carriage -> ready pose. Entering Loop directly would skip that motion entirely.
	PresentationTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("ThrowStart"), Profile->StartMontage, FGameplayTagContainer(), 1.f, NAME_None,
		/*bStopWhenAbilityEnds*/ true);
	PresentationTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->OnCancelled.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->ReadyForActivation();

	if (UWorld* World = GetWorld())
	{
		// The measured ready seam, where Start's last pose matches every continuation's first to within
		// 0.009cm. A release before this latches one intent and is consumed here.
		//
		// ★ Fired one BLEND EARLY, so the continuation's fade-in lands ON the seam instead of after it.
		// Starting exactly at ReadySeamTime leaves a hole: Start has finished contributing and the incoming
		// montage is still ramping from zero, so the slot sags toward the base pose and snaps back — the
		// visible hitch at the end of the draw (reported 2026-09-18, and NOT fixed by removing Start's own
		// blend-out, which only moved the hole). Because the two poses match at the seam, a crossfade
		// straddling it is invisible; a gap never is.
		const float LoopBlendIn = Profile->LoopMontage ? Profile->LoopMontage->BlendIn.GetBlendTime() : 0.f;
		World->GetTimerManager().SetTimer(ReadySeamTimer, FTimerDelegate::CreateUObject(this, &UAZ_GA_Throw::OnReadySeam),
			FMath::Max(0.01f, Profile->ReadySeamTime - LoopBlendIn), false);
		// The preview is live immediately: the player is aiming from frame one and must not wait out Start
		// to see where the throw goes.
		World->GetTimerManager().SetTimer(PreviewTimer, FTimerDelegate::CreateUObject(this, &UAZ_GA_Throw::TickPreview),
			1.f / FMath::Max(1.f, PreviewFrequency), true, 0.f);
	}
}

void UAZ_GA_Throw::TickPreview()
{
	if (Phase != EAZ_ThrowPhase::Preparing && Phase != EAZ_ThrowPhase::Aiming)
	{
		return;
	}
	const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	const AAZ_PlayerController* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}
	AcceptedAim = Controller->GetControlRotation();

	// ---- 1. Choose the arc, from the AIM ---------------------------------------------------------
	// ★ Provisional selection with hysteresis on EVERY tick, not deferred to commit: the two release clips
	// leave the hand 106cm apart vertically, so previewing with the wrong one draws an arc starting a metre
	// from where the object actually goes. "Choose once at commit" means LOCK once; the choice itself has to
	// be live while aiming or the preview lies.
	//
	// ★ And it is measured from what the player is AIMING AT — a straight ray from the view point, measured
	// to the THROWER — never from where the current arc's throw lands. That was circular: Close lands short,
	// a short landing reads as Close, and the action latched to Close permanently (measured 2026-09-16:
	// every throw came out arc=0 |v|=900 whatever the player pointed at, including straight up).
	FVector ViewLocation = Hero->GetPawnViewLocation();
	FRotator ViewRotation = AcceptedAim;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const float AimDistance = UAZ_ThrowLaunchSolver::MeasureAimDistance(
		this, ViewLocation, AcceptedAim, Hero->GetActorLocation(), Hero,
		Definition->FarArcDistance * AimRangeFactor);
	AcceptedAimDistance = AimDistance;
	const EAZ_ThrowArc Previous = Arc;
	const EAZ_ThrowArc Candidate = Definition->SelectArc(AimDistance, Arc);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Candidate == Arc)
	{
		PendingArc = Arc;            // the aim came back; nothing is pending
	}
	else
	{
		if (Candidate != PendingArc)
		{
			PendingArc = Candidate;  // a new candidate starts its clock
			PendingArcSince = Now;
		}
		else if (Now - PendingArcSince >= ArcDwellTime)
		{
			Arc = Candidate;
		}
	}
	if (Arc != Previous)
	{
		UE_LOG(LogTemp, Log, TEXT("[Throw] arc %d -> %d  aim=%.0f  far=%.0f +/-%.0f  action=%s"),
			static_cast<int32>(Previous), static_cast<int32>(Arc), AimDistance,
			Definition->FarArcDistance, Definition->FarArcHysteresis, *ThrowActionId.ToString());
	}

	// ---- 2. One solve, one prediction, for the arc just chosen ------------------------------------
	const FAZ_ThrowLaunchSolution Solution =
		UAZ_ThrowLaunchSolver::BuildSolution(Hero, Hero->GetMesh(), Definition, Profile, Arc,
			AcceptedAimDistance, AcceptedAim, false);
	// Logged on the EDGE only: at 20Hz a per-tick message would bury everything else in the log.
	if (Solution.Status != LastPreviewStatus)
	{
		LastPreviewStatus = Solution.Status;
		UE_LOG(LogTemp, Warning, TEXT("[Throw] preview status -> %d (0=Valid 1=NoDefinition 2=BlockedAtHand) arc=%d"),
			static_cast<int32>(Solution.Status), static_cast<int32>(Arc));
	}
	if (!Solution.IsValid())
	{
		// A shoulder camera routinely sees over cover the hand cannot clear. Showing an arc from a hand that
		// is behind geometry would promise a throw the release is about to refuse, so draw nothing.
		HidePreview();
		return;
	}
	const FAZ_ThrowPreviewResult Result =
		UAZ_ThrowLaunchSolver::Predict(this, Solution, Hero, PreviewHorizon, PreviewFrequency);

	// The ability owns the solution; the component only draws what it is given. One producer of launch
	// parameters, so the picture and the server's launch cannot drift apart.
	UAZ_ThrowPreviewComponent* Component = GetPreview();
	if (!Component)
	{
		return;
	}
	Component->ShowSolution(Result);
}

UAZ_ThrowPreviewComponent* UAZ_GA_Throw::GetPreview()
{
	// PreviewStyle is EditDefaultsOnly art configuration. Read the class defaults
	// explicitly: a live Blueprint ability can retain constructor-valued members
	// from its cached post-construction property list after nested CDO edits, even
	// while its material references update. That produced the new sunlight material
	// with the old 3px/.31 calibration. The preview must use one coherent style.
	const UAZ_GA_Throw* ClassDefaults = GetClass()->GetDefaultObject<UAZ_GA_Throw>();
	const FAZ_ThrowPreviewStyle& ConfiguredStyle = ClassDefaults ? ClassDefaults->PreviewStyle : PreviewStyle;
	if (Preview)
	{
		if (!FAZ_ThrowPreviewStyle::StaticStruct()->CompareScriptStruct(&Preview->Style, &ConfiguredStyle, 0))
		{
			Preview->ConfigureStyle(ConfiguredStyle);
			UE_LOG(LogTemp, Log, TEXT("[Throw] preview art refreshed width=%.2f filament=%.2f pulse=%.2f"),
				ConfiguredStyle.PulseWidthPixels, ConfiguredStyle.FilamentAlpha, ConfiguredStyle.PulseAlpha);
		}
		return Preview;
	}
	// OWNER ONLY. A simulated proxy or a dedicated server has no business drawing an aim arc, and creating
	// the component there would put pooled render state on every remote copy of every thrower.
	AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !Hero->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] no preview: hero=%s locallyControlled=%d"),
			*GetNameSafe(Hero), Hero ? Hero->IsLocallyControlled() : 0);
		return nullptr;
	}
	// Created once and kept for the lifetime of the pawn: the ability is InstancedPerActor, so this survives
	// between throws and nothing is spawned or destroyed per solve.
	Preview = NewObject<UAZ_ThrowPreviewComponent>(Hero);
	Preview->ConfigureStyle(ConfiguredStyle);
	Preview->SetupAttachment(Hero->GetRootComponent());
	Preview->RegisterComponent();
	UE_LOG(LogTemp, Warning, TEXT("[Throw] preview component created on %s (arcMesh=%s arcMat=%s width=%.2f filament=%.2f pulse=%.2f)"),
		*GetNameSafe(Hero), *GetNameSafe(ConfiguredStyle.ArcMesh), *GetNameSafe(ConfiguredStyle.ArcMaterial),
		ConfiguredStyle.PulseWidthPixels, ConfiguredStyle.FilamentAlpha, ConfiguredStyle.PulseAlpha);
	return Preview;
}

void UAZ_GA_Throw::HidePreview()
{
	if (Preview)
	{
		Preview->HidePreview();
	}
}

void UAZ_GA_Throw::SuppressHandProp(const bool bSuppressed) const
{
	if (auto* Hand = FindHandComponent())
	{
		Hand->SetSuppressed(bSuppressed);
	}
}

void UAZ_GA_Throw::SetThrowCommittedTag(const bool bCommitted) const
{
	if (UAbilitySystemComponent* Asc = GetAbilitySystemComponentFromActorInfo())
	{
		// Absolute count, never Add/Remove: EndAbility can be reached from several paths and an unbalanced
		// pair would leave the pawn planted with no throw in sight.
		Asc->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().Ability_State_Throwing, bCommitted ? 1 : 0);
	}
}

void UAZ_GA_Throw::SetHandActionOwnership(const bool bOwned) const
{
	if (auto* Hand = FindHandComponent())
	{
		Hand->SetActionOwnsBody(bOwned);
	}
}

UAZ_ThrowableHandComponent* UAZ_GA_Throw::FindHandComponent() const
{
	const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	const AController* Controller = Hero ? Hero->GetController() : nullptr;
	return Controller ? Controller->FindComponentByClass<UAZ_ThrowableHandComponent>() : nullptr;
}

void UAZ_GA_Throw::RequestThrow()
{
	// * A cancel already consumed this action; a press arriving after it must not throw the item the player
	// just put away.
	if (bCancelRequested || Phase == EAZ_ThrowPhase::Cancelling || Phase == EAZ_ThrowPhase::None)
	{
		return;
	}
	switch (Phase)
	{
	case EAZ_ThrowPhase::Preparing:
		// Start is still playing. Latch exactly ONE intent, consumed at the ready seam. A press during the
		// draw therefore still throws rather than being refused; it simply throws once Start reaches the seam.
		bPendingCommit = true;
		break;
	case EAZ_ThrowPhase::Aiming:
		EnterWindup();
		break;
	default:
		// Windup, Released, Recovering: the throw is already under way and a second press means nothing.
		break;
	}
}

void UAZ_GA_Throw::OnReadySeam()
{
	if (Phase != EAZ_ThrowPhase::Preparing)
	{
		return;
	}
	if (bPendingCommit)
	{
		// Branch straight from Start's end into the release. The seam is exact, so no Loop cycle is played
		// and no delay is invented to "wait for a good frame".
		EnterWindup();
		return;
	}
	EnterAiming();
}

void UAZ_GA_Throw::DetachPresentation()
{
	if (!PresentationTask)
	{
		return;
	}
	// * EndTask leaves the montage's blend-out delegate bound to the task, and the task's broadcast gate
	// only asks whether the ABILITY is active - which it very much is mid-transition. The next montage
	// interrupts the previous one, so without clearing these the Start -> Loop seam would arrive as
	// OnInterrupted and cancel the throw the player is still holding.
	PresentationTask->OnCompleted.Clear();
	PresentationTask->OnBlendOut.Clear();
	PresentationTask->OnInterrupted.Clear();
	PresentationTask->OnCancelled.Clear();
	PresentationTask->EventReceived.Clear();
}

void UAZ_GA_Throw::ReleasePresentation()
{
	DetachPresentation();
	if (PresentationTask)
	{
		// EndTask (AbilityEnded=false) deliberately does NOT stop the montage: the outgoing clip keeps
		// playing until the incoming one blends it out, which is what makes an authored seam look like one
		// motion instead of a cut.
		PresentationTask->EndTask();
		PresentationTask = nullptr;
	}
}

void UAZ_GA_Throw::StopPreparationMontages(const float BlendOutTime) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!Anim || !Profile) return;
	for (UAnimMontage* Montage : {Profile->StartMontage.Get(), Profile->LoopMontage.Get()})
	{
		if (Montage && Anim->Montage_IsPlaying(Montage) && !Anim->Montage_GetIsStopped(Montage))
		{
			// Preparation uses the Throwable group; release uses FullBody/DefaultGroup.
			// Montage_Play stops only the incoming group, so an ended preparation task
			// otherwise leaves its loop running under the release and visible afterwards.
			const float Duration = BlendOutTime >= 0.f ? BlendOutTime : Montage->BlendOut.GetBlendTime();
			Anim->Montage_Stop(Duration, Montage);
			UE_LOG(LogTemp, Log, TEXT("[Throw] preparation stopped montage=%s blend=%.3f"),
				*GetNameSafe(Montage), Duration);
		}
	}
}

void UAZ_GA_Throw::EnterAiming()
{
	Phase = EAZ_ThrowPhase::Aiming;
	ReleasePresentation();
	// Loop repeats for as long as the button is held. bStopWhenAbilityEnds is true: nothing else stops a
	// LOOPING montage, so an action that ended without it would leave the character winding up forever.
	PresentationTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("ThrowLoop"), Profile->LoopMontage, FGameplayTagContainer(), 1.f, NAME_None,
		/*bStopWhenAbilityEnds*/ true);
	PresentationTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->OnCancelled.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->ReadyForActivation();
}

void UAZ_GA_Throw::EnterWindup()
{
	if (Phase != EAZ_ThrowPhase::Preparing && Phase != EAZ_ThrowPhase::Aiming)
	{
		return;
	}
	// RESERVE HERE, not on activation. This is the first moment the player has actually committed a throw;
	// before it the grenade is merely readied and must stay invisible to the inventory however long it is
	// carried. A refused reservation aborts the throw rather than winding up for a unit that is not there.
	{
		const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
		const AAZ_PlayerController* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
		UAZ_Inv_CommonUI_InventoryComponent* Inventory =
			Controller ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
		if (Hero && Hero->HasAuthority() && (!Inventory || !Inventory->TryBeginThrow(this, SourceItemId, ThrowActionId)))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Throw] windup refused: no reservation for item=%s"), *SourceItemId.ToString());
			FinishAndRelease(true);
			return;
		}
	}

	Phase = EAZ_ThrowPhase::Windup;
	bPendingCommit = false;
	// ★ The body is planted from HERE, not from activation. Aiming is now upper-body only and the player keeps
	// walking in the combat stance (user call 2026-09-18); it is the committed release — a full-body clip that
	// swings the pelvis and resets the feet — that cannot survive moving legs underneath it.
	// Ability.State.Throwing already meant exactly "committed release + recovery" and had no other consumer.
	// A loose explicit pair rather than ActivationOwnedTags, which would raise it for the whole ability.
	SetThrowCommittedTag(true);

	// * FREEZE. From here the displayed candidate and the accepted aim are the throw: the arc is not
	// re-selected and the aim does not follow the camera, so the object goes where the player committed
	// rather than where the camera drifted during the wind-up.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreviewTimer);
	}
	// The player has committed; the arc has done its job and the wind-up should not be watched through a
	// line that no longer updates.
	HidePreview();

	UAnimMontage* ReleaseMontage = Profile->GetReleaseMontage(Arc);
	if (!ReleaseMontage)
	{
		FinishAndRelease(true);
		return;
	}
	ReleasePresentation();
	// Detach callbacks first, then fade preparation with the incoming release blend.
	// Ending the task alone deliberately leaves its montage playing.
	StopPreparationMontages(ReleaseMontage->BlendIn.GetBlendTime());
	// The release cue is a gameplay event authored on the project-owned montage at the measured time. A
	// timer is deliberately not used as the trigger: if the cue never arrives the action cancels, and no
	// watchdog is allowed to invent a throw.
	FGameplayTagContainer CueTags;
	if (ReleaseEventTag.IsValid())
	{
		CueTags.AddTag(ReleaseEventTag);
	}
	PresentationTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("ThrowRelease"), ReleaseMontage, CueTags, 1.f, NAME_None, /*bStopWhenAbilityEnds*/ true);
	PresentationTask->EventReceived.AddDynamic(this, &UAZ_GA_Throw::OnReleaseCue);
	PresentationTask->OnCompleted.AddDynamic(this, &UAZ_GA_Throw::OnPresentationEnded);
	PresentationTask->OnBlendOut.AddDynamic(this, &UAZ_GA_Throw::OnPresentationEnded);
	PresentationTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->OnCancelled.AddDynamic(this, &UAZ_GA_Throw::OnPresentationInterrupted);
	PresentationTask->ReadyForActivation();
}

void UAZ_GA_Throw::OnReleaseCue(FGameplayTag /*EventTag*/, FGameplayEventData /*EventData*/)
{
	// The generic notify carries neither the originating montage nor an action token, so the phase check
	// here is what stops a stray or late event from any other source launching something.
	if (Phase != EAZ_ThrowPhase::Windup)
	{
		return;
	}
	PerformRelease();
}

void UAZ_GA_Throw::PerformRelease()
{
	AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	const AAZ_PlayerController* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
	UAZ_Inv_CommonUI_InventoryComponent* Inventory =
		Controller ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!Hero || !Inventory || !World || !Hero->HasAuthority())
	{
		// Remote copies never spawn a damaging projectile; the server's replicates to them.
		Phase = EAZ_ThrowPhase::Released;
		return;
	}
	if (Inventory->IsThrowCommitted(ThrowActionId))
	{
		return;   // duplicate cue: the receipt already stands, spend and spawn exactly once
	}

	// Now the hand is where it will actually be, so the live grip is authoritative and the corridor is
	// rechecked. A camera can see past cover the hand cannot clear; that aborts here, before consumption.
	const FAZ_ThrowLaunchSolution Solution =
		UAZ_ThrowLaunchSolver::BuildSolution(Hero, Hero->GetMesh(), Definition, Profile, Arc,
			AcceptedAimDistance, AcceptedAim, true);
	if (!Solution.IsValid() || !Definition->ProjectileClass || !HasIgnitionTool(Inventory, Definition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] release refused status=%d action=%s"),
			static_cast<int32>(Solution.Status), *ThrowActionId.ToString());
		FinishAndRelease(true);
		return;
	}

	// Prepare INERT first. Spawning runs construction callbacks, so nothing may be spent until the actor
	// exists and has been revalidated; activation is the point of no return, not spawning.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Hero;
	SpawnParams.Instigator = Hero;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAZ_ThrowableProjectile* Projectile = World->SpawnActor<AAZ_ThrowableProjectile>(
		Definition->ProjectileClass, Solution.Origin, Solution.Velocity.Rotation(), SpawnParams);
	if (!IsValid(Projectile))
	{
		FinishAndRelease(true);
		return;
	}

	FAZ_InventoryPickupRecord Payload;
	if (!Inventory->TryCommitThrowRelease(ThrowActionId, Payload))
	{
		// Failed before commit: destroy the staged actor and leave the inventory exactly as it was.
		Projectile->Destroy();
		FinishAndRelease(true);
		return;
	}

	// Committed. Only now does the object become real, and the record inventory just produced travels with
	// it - that is what lets a settled object become the same item again instead of a new one.
	// The hand empties on the SAME frame the projectile is armed, so it is never in two places.
	// The hand empties on the SAME frame the projectile is armed, so the object is never in two places.
	Phase = EAZ_ThrowPhase::Released;
	// ★ ONLY IF THIS ABILITY IS STILL ALIVE. TryCommitThrowRelease above publishes the inventory change
	// SYNCHRONOUSLY, and its listeners can end this ability before it returns: the last grenade leaves the
	// stack, the quick bar clears, the hand component's Refresh calls LeaveThrowAction, and EndAbility runs
	// — including the SuppressHandProp(false) that is supposed to pair with this line. Execution then comes
	// back here and re-suppresses a hand nobody will ever un-suppress, so every later grenade is readied
	// with no carry montage and no hold pose.
	//
	// Measured 2026-09-19: "[Throw] end" printed BEFORE "[Throw] release", which is the fingerprint of the
	// ability having died inside the commit. EndAbility invalidates ThrowActionId, so that is the cheapest
	// honest test of whether we are still the live action.
	if (ThrowActionId.IsValid())
	{
		SuppressHandProp(true);
	}
	// The three yaws that have to agree, plus the origin in ACTOR space. If the body yaw does not track the
	// aim yaw, the character throws one way while the object goes another - which is exactly how it looked
	// before the throw started driving the pawn's facing.
	{
		const FTransform ActorToWorld = Hero->GetActorTransform();
		const FVector LocalOrigin = ActorToWorld.InverseTransformPosition(Solution.Origin);
		UE_LOG(LogTemp, Warning,
			TEXT("[Throw] release bodyYaw=%.1f aimYaw=%.1f velYaw=%.1f (body-aim=%.1f) originLocal=(%.0f,%.0f,%.0f) arc=%d aimDist=%.0f blend=%.2f"),
			ActorToWorld.Rotator().Yaw, AcceptedAim.Yaw, Solution.Velocity.Rotation().Yaw,
			FMath::FindDeltaAngleDegrees(AcceptedAim.Yaw, ActorToWorld.Rotator().Yaw),
			LocalOrigin.X, LocalOrigin.Y, LocalOrigin.Z, static_cast<int32>(Arc),
			AcceptedAimDistance, Definition->GetAimBlend(AcceptedAimDistance));
	}
	Projectile->SetRecoveryPayload(Payload);
	Projectile->Activate(Solution, Definition, Hero);
}

void UAZ_GA_Throw::OnPresentationEnded(FGameplayTag /*EventTag*/, FGameplayEventData /*EventData*/)
{
	if (Phase == EAZ_ThrowPhase::Released)
	{
		// The authored tail is handing the body back to locomotion. The item left long ago; nothing here
		// can still be refused or refunded, so this is a clean end and not a cancellation.
		Phase = EAZ_ThrowPhase::Recovering;
		FinishAndRelease(false);
		return;
	}
	// Completion is NOT a throw. If the release clip ran out without its cue, nothing was spent and the
	// action simply ends - a missing cue cancels rather than being retroactively treated as a launch.
	FinishAndRelease(true);
}

void UAZ_GA_Throw::OnPresentationInterrupted(FGameplayTag /*EventTag*/, FGameplayEventData /*EventData*/)
{
	// Interruption after release cleans up presentation only: the projectile is the world's now and is never
	// recalled, refunded or destroyed because the thrower's recovery was cut short.
	FinishAndRelease(Phase != EAZ_ThrowPhase::Released);
}

bool UAZ_GA_Throw::HasCommittedRelease() const
{
	if (Phase == EAZ_ThrowPhase::Released || Phase == EAZ_ThrowPhase::Recovering) return true;
	const auto* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo());
	const auto* Controller = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
	const auto* Inventory = Controller ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	return Inventory && Inventory->IsThrowCommitted(ThrowActionId);
}

void UAZ_GA_Throw::RequestCancel()
{
	if (bEndingThrow || Phase == EAZ_ThrowPhase::None || Phase == EAZ_ThrowPhase::Cancelling)
	{
		return;
	}
	// After the physical release there is nothing to cancel — only the remaining recovery presentation.
	// The inventory broadcasts last-unit removal before PerformRelease can advance Phase.
	// Its committed receipt already owns that outcome; do not play a cancel over the release.
	if (HasCommittedRelease())
	{
		return;
	}
	// Disarm FIRST, before anything else can synthesise a release from the still-held button.
	bCancelRequested = true;
	bPendingCommit = false;
	Phase = EAZ_ThrowPhase::Cancelling;


	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreviewTimer);
		World->GetTimerManager().ClearTimer(ReadySeamTimer);
	}
	HidePreview();
	if (Profile && Profile->CancelMontage)
	{
		ReleasePresentation();
		// ★ bStopWhenAbilityEnds = FALSE, unlike every other presentation here, and no completion delegates.
		// The cancel clip is COSMETIC: gameplay ownership must be released on this frame so the body returns
		// to combat-ready walking immediately (user call 2026-09-18, "мгновенно"). Waiting out the clip kept
		// Ability.State.ThrowPreparing raised, and that tag is what zeroes movement intent — the player stood
		// frozen through an animation that had already given up. The clip is a one-shot and ends itself; a new
		// wind-up simply blends it out.
		PresentationTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
			this, FName("ThrowCancel"), Profile->CancelMontage, FGameplayTagContainer(), 1.f, NAME_None,
			/*bStopWhenAbilityEnds*/ false);
		PresentationTask->ReadyForActivation();
		// ★ THE PUT-AWAY IS A PHASE, AND SOMETHING HAS TO OWN ITS LENGTH. This clip plays in the throwable
		// slot, whose mask owns everything above spine_01, and it outlives this ability on purpose — so a
		// weapon draw issued the moment the ability ends spends its entire length invisible underneath it
		// and the weapon just appears in the hand (measured 2026-09-20). Told here, at the one point that
		// knows the clip actually started, so every cancel route is covered.
		if (auto* Hand = FindHandComponent())
		{
			Hand->BeginPutAway(Profile->CancelMontage);
		}
	}
	FinishAndRelease(true);
}

void UAZ_GA_Throw::FinishAndRelease(const bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UAZ_GA_Throw::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (bEndingThrow) return;
	TGuardValue<bool> EndingGuard(bEndingThrow, true);
	const TWeakObjectPtr<UAZ_ThrowableHandComponent> Hand = FindHandComponent();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreviewTimer);
		World->GetTimerManager().ClearTimer(ReadySeamTimer);
	}
	// Did this activation get past the entry guards at all? Phase is still None when one of them refused,
	// and a refusal must not be mistaken for a finished action. Captured before the reset at the bottom.
	const bool bActionRan = (Phase != EAZ_ThrowPhase::None);
	// Silence preparation callbacks before stopping its clips. This also cleans up an
	// interrupted aim without stopping another ability's montage or the cosmetic cancel.
	DetachPresentation();
	if (bActionRan) StopPreparationMontages();
	// Hidden, not destroyed: the pooled segments and materials are wanted again on the next throw.
	HidePreview();
	// Keep presentation suppressed until the old selection is cleared and GAS has
	// ended the action. Unmasking here can latch the next selection onto this ability
	// or play carry over the cancel animation before readiness has been cleared.
	SetThrowCommittedTag(false);
	// Keep the detached task: ability teardown still reaches it with AbilityEnded=true
	// and stops the action montage. The separate preparation group was faded above.
	// Release the reservation last. An uncommitted action spent nothing so there is nothing to refund; a
	// committed one is never rolled back, which is what keeps a reentrant "last unit vanished" callback from
	// destroying the projectile it just paid for.
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(Hero->GetController()))
		{
			if (auto* Inventory = Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>())
			{
				Inventory->EndThrow(ThrowActionId);
			}
			// UN-READY, on every exit of an action that actually RAN — thrown, cancelled or interrupted
			// alike. The action always ends back in Explore (user call 2026-09-18), and readiness is what
			// would otherwise re-enter it: the hand component treats a readied throwable as the entry
			// condition, so leaving the item readied would re-arm the loop and the grenade could never be
			// put away. Re-entrant by design and safe: the refresh this triggers finds no definition.
			//
			// ★ NOT on a REFUSED activation. Phase is still None when a guard above rejected the entry, and
			// clearing there destroys the player's selection for something they never got to do: measured
			// 2026-09-18, readying a grenade set the state and un-set it in the same millisecond, so neither
			// the combat stance nor the ready pose ever appeared.
			//
			// ★ And only while the readied item is still the one this action was holding. Swapping the quick
			// bar to a potion mid-throw ends this ability too, and clearing there would un-ready the item the
			// player just chose.
			if (auto* QuickBar = Controller->FindComponentByClass<UAZ_QuickBarComponent>())
			{
				if (bActionRan && QuickBar->GetReadyItemId() == SourceItemId)
				{
					QuickBar->ClearReadyItem();
				}
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Throw] end action=%s phase=%d cancelled=%d cancelRequested=%d pendingCommit=%d arc=%d"),
		*ThrowActionId.ToString(), static_cast<int32>(Phase), bWasCancelled ? 1 : 0,
		bCancelRequested ? 1 : 0, bPendingCommit ? 1 : 0, static_cast<int32>(Arc));
	Phase = EAZ_ThrowPhase::None;
	bPendingCommit = false;
	bCancelRequested = false;
	LastPreviewStatus = EAZ_ThrowSolutionStatus::Valid;
	PendingArc = Arc;
	ThrowActionId.Invalidate();
	SourceItemId.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (bActionRan && Hand.IsValid()) Hand->FinishThrowAction();
}
