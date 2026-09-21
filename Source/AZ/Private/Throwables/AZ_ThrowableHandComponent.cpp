// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableHandComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_Throw.h"
#include "AZ_GameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "Player/AZ_PlayerController.h"
#include "Throwables/AZ_ThrowPresentationProfile.h"
#include "Throwables/AZ_ThrowableDefinition.h"

UAZ_ThrowableHandComponent::UAZ_ThrowableHandComponent()
{
	// Event driven. Nothing here needs polling: readiness, inventory contents and possession all announce
	// themselves, and a tick would only re-resolve the same answer sixty times a second.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAZ_ThrowableHandComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const APlayerController* Owner = Cast<APlayerController>(GetOwner()))
	{
		if (auto* QuickBar = Owner->FindComponentByClass<UAZ_QuickBarComponent>())
		{
			QuickBar->OnReadyItemChanged.AddUniqueDynamic(this, &UAZ_ThrowableHandComponent::HandleReadyItemChanged);
		}
		if (auto* Inventory = Owner->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>())
		{
			// Throwing the last one empties the hand, and picking one up fills it, without readiness itself
			// changing — so contents have to be watched as well as selection.
			Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UAZ_ThrowableHandComponent::HandleInventoryChanged);
		}
		// ★ THE FRAME THE HOLSTER COMMITS, not up to a tenth of a second later. Readying a throwable puts the
		// weapon away first, and the grenade can only come out once the hands are free — which used to be
		// noticed by a slow poll. Measured 2026-09-20: six frames passed between the holster committing and
		// the grenade being taken out, and for those six frames the upper body showed the bare unarmed idle.
		// That gap is the jerk between the two animations; the poll stays only as a safety net.
		//
		// HandleInventoryChanged is the shared "something changed, re-resolve everything" handler — the same
		// one the quick bar and the inventory use. Refresh is idempotent, so a spare broadcast costs nothing.
		if (auto* Equipment = Owner->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>())
		{
			Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UAZ_ThrowableHandComponent::HandleInventoryChanged);
		}
	}
	if (APlayerController* Owner = Cast<APlayerController>(GetOwner()))
	{
		Owner->OnPossessedPawnChanged.AddUniqueDynamic(this, &UAZ_ThrowableHandComponent::HandlePawnChanged);
	}
	Refresh();
}

void UAZ_ThrowableHandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* Owner = Cast<APlayerController>(GetOwner()))
	{
		if (auto* QuickBar = Owner->FindComponentByClass<UAZ_QuickBarComponent>())
		{
			QuickBar->OnReadyItemChanged.RemoveDynamic(this, &UAZ_ThrowableHandComponent::HandleReadyItemChanged);
		}
		if (auto* Inventory = Owner->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>())
		{
			Inventory->OnInventoryChanged.RemoveDynamic(this, &UAZ_ThrowableHandComponent::HandleInventoryChanged);
		}
		if (auto* Equipment = Owner->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>())
		{
			Equipment->OnEquipmentChanged.RemoveDynamic(this, &UAZ_ThrowableHandComponent::HandleInventoryChanged);
		}
		Owner->OnPossessedPawnChanged.RemoveDynamic(this, &UAZ_ThrowableHandComponent::HandlePawnChanged);
	}
	// The tag lives on the pawn's ASC, which outlives this component on a controller teardown. Left set, the
	// body would stay locked to combat-ready walking with no grenade in sight.
	PublishReadyTag(false);
	// Same reasoning, worse consequences: a stuck Stowing tag makes every future weapon switch defer for
	// good, because equipment reads it as a committed action.
	EndPutAway();
	Super::EndPlay(EndPlayReason);
}

void UAZ_ThrowableHandComponent::HandleReadyItemChanged() { Refresh(); }
void UAZ_ThrowableHandComponent::HandleInventoryChanged() { Refresh(); }

void UAZ_ThrowableHandComponent::HandlePawnChanged(APawn* OldPawn, APawn* /*NewPawn*/)
{
	// Refresh below republishes onto the NEW pawn's ASC, which would leave the old body walking in a combat
	// stance for a grenade it no longer has.
	if (const auto* Previous = Cast<AAZ_PawnMoverHeroCharacter>(OldPawn))
	{
		if (UAbilitySystemComponent* PreviousAsc = Previous->GetAbilitySystemComponent())
		{
			PreviousAsc->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Throwable_Ready, 0);
		}
	}
	// The props hang off the OLD pawn's mesh. Drop them rather than re-parent: the new body may not even be
	// the same skeleton, and Refresh rebuilds them on the mesh that is actually being driven now.
	if (StaticProp) { StaticProp->DestroyComponent(); StaticProp = nullptr; }
	if (SkeletalProp) { SkeletalProp->DestroyComponent(); SkeletalProp = nullptr; }
	AttachedTo.Reset();
	Refresh();
}

AAZ_PawnMoverHeroCharacter* UAZ_ThrowableHandComponent::GetHero() const
{
	const APlayerController* Owner = Cast<APlayerController>(GetOwner());
	return Owner ? Cast<AAZ_PawnMoverHeroCharacter>(Owner->GetPawn()) : nullptr;
}

const UAZ_ThrowableDefinition* UAZ_ThrowableHandComponent::ResolveReadyThrowable(
	const UAZ_ThrowPresentationProfile*& OutProfile) const
{
	OutProfile = nullptr;
	const APlayerController* Owner = Cast<APlayerController>(GetOwner());
	const auto* QuickBar = Owner ? Owner->FindComponentByClass<UAZ_QuickBarComponent>() : nullptr;
	const UAZ_Inv_CommonUI_InventoryItem* Item = QuickBar ? QuickBar->GetReadyItem() : nullptr;
	if (!Item || !Item->IsInitialized())
	{
		return nullptr;
	}
	// Capability, not category — the same test the ability uses to resolve a throw.
	const auto* Fragment = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_ThrowableFragment>();
	const UAZ_ThrowableDefinition* Definition = Fragment ? Fragment->ThrowableDefinition.Get() : nullptr;
	if (!Definition)
	{
		return nullptr;
	}
	OutProfile = Definition->DefaultProfile.Get();
	return OutProfile ? Definition : nullptr;
}

void UAZ_ThrowableHandComponent::SetSuppressed(const bool bInSuppressed)
{
	if (bSuppressed == bInSuppressed)
	{
		return;
	}
	bSuppressed = bInSuppressed;
	Refresh();
}

void UAZ_ThrowableHandComponent::SetActionOwnsBody(const bool bInOwned)
{
	if (bActionOwnsBody == bInOwned)
	{
		return;
	}
	bActionOwnsBody = bInOwned;
	Refresh();
}

void UAZ_ThrowableHandComponent::PublishReadyTag(const bool bReadied) const
{
	AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	UAbilitySystemComponent* Asc = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	if (!Asc && !bReadied)
	{
		return;   // clearing with no pawn left is just teardown, not a failure worth reporting
	}
	if (!Asc)
	{
		// The ASC lives on the PlayerState, so an early Refresh can legitimately find none. Only worth saying
		// when we were trying to SET the state — a clear with no pawn left is just PIE tearing down.
		UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] no ASC (hero=%s) — State.Throwable.Ready not published"),
			*GetNameSafe(Hero));
		return;
	}
	// Absolute count, never Add/Remove: Refresh runs on every inventory and readiness event, and a counted
	// tag driven by unbalanced pairs drifts until the carry state never clears. Same reason GA_HitReact owns
	// State.Combat.Staggered as an explicit pair.
	const FGameplayTag& ReadyTag = FAZ_GameplayTags::Get().State_Throwable_Ready;
	if (Asc->HasMatchingGameplayTag(ReadyTag) == bReadied)
	{
		return;   // already right: Refresh runs on every inventory event, and this must not spam the log
	}
	Asc->SetLooseGameplayTagCount(ReadyTag, bReadied ? 1 : 0);
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] State.Throwable.Ready -> %d on %s"),
		bReadied ? 1 : 0, *GetNameSafe(Asc->GetOwner()));
}

void UAZ_ThrowableHandComponent::UpdateCarryMontage(USkeletalMeshComponent* Mesh,
	const UAZ_ThrowPresentationProfile* Profile)
{
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* Wanted = Profile ? Profile->CarryMontage.Get() : nullptr;

	// ENTRY TRACE. Three of the paths below return without playing anything and without a word in the log,
	// so "no [ThrowCarry] play line" could mean a null profile, a suppressed carry, or an early-out that
	// silently re-played — indistinguishable after the fact. That ambiguity is what made "the pistol pose
	// stays on the upper body after re-taking a grenade" (2026-09-19) unreadable from a log alone.
	UE_LOG(LogTemp, Warning,
		TEXT("[ThrowCarry] update profile=%s wanted=%s active=%s suppressed=%d actionOwnsBody=%d anim=%d playing=%d"),
		*GetNameSafe(Profile), *GetNameSafe(Wanted), *GetNameSafe(ActiveCarryMontage),
		bSuppressed ? 1 : 0, bActionOwnsBody ? 1 : 0, Anim ? 1 : 0,
		(Anim && Wanted) ? (Anim->Montage_IsPlaying(Wanted) ? 1 : 0) : -1);

	if (!Anim)
	{
		ActiveCarryMontage = nullptr;
		return;
	}
	if (ActiveCarryMontage == Wanted)
	{
		// Already showing the right thing. Re-playing every refresh would restart the loop on every
		// inventory event and make the idle stutter.
		//
		// ★ Montage_IsPlaying stays TRUE for the whole of a montage's blend-OUT, so "playing" is not the
		// same as "will still be contributing next frame". A carry left mid-fade would be treated as present
		// and never restarted, and the upper-body mask — which is driven by the montage's weight — would
		// close onto the locomotion pose. Treat a stopping montage as absent.
		if (Wanted && (!Anim->Montage_IsPlaying(Wanted) || Anim->Montage_GetIsStopped(Wanted)))
		{
			const float Replayed = Anim->Montage_Play(Wanted, 1.f);
			UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] re-play %s -> %.3f%s"),
				*Wanted->GetName(), Replayed, Replayed > 0.f ? TEXT("") : TEXT("  <== REFUSED"));
		}
		return;
	}
	if (ActiveCarryMontage && Anim->Montage_IsPlaying(ActiveCarryMontage))
	{
		Anim->Montage_Stop(0.2f, ActiveCarryMontage);
	}
	ActiveCarryMontage = Wanted;
	if (Wanted)
	{
		// Montage_Play returns the play length, or 0 when it refused — a montage whose slot the graph does not
		// contain fails exactly this way and is otherwise completely silent. Logged with the slot so a wiring
		// problem in the AnimBP is told apart from a data problem in the profile.
		const float Played = Anim->Montage_Play(Wanted, 1.f);
		const FName Slot = Wanted->SlotAnimTracks.Num() > 0 ? Wanted->SlotAnimTracks[0].SlotName : NAME_None;
		UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] play %s slot=%s -> %.3f%s"),
			*Wanted->GetName(), *Slot.ToString(), Played,
			Played > 0.f ? TEXT("") : TEXT("  <== REFUSED"));
	}
}

bool UAZ_ThrowableHandComponent::EnsureHandsFreeForThrow()
{
	AAZ_PlayerController* Owner = Cast<AAZ_PlayerController>(GetOwner());
	auto* Equipment = Owner ? Owner->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
	if (!Equipment)
	{
		return true;   // nothing can be held, so nothing is in the way
	}

	// Mid-switch: something is already moving. Wait for it rather than issuing a second request into a
	// transition that owns the hands.
	if (Equipment->IsSwitchingWeapon())
	{
		ArmStowWait();
		return false;
	}

	UAZ_Inv_CommonUI_InventoryItem* Active = Equipment->GetActiveItem();
	if (!Active)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(StowWaitTimer);
		}
		return true;   // hands already free
	}

	// Remember BEFORE asking, so the draw-back still knows what to restore even if the unequip completes
	// synchronously and re-enters this component.
	StowedForThrow = Active;
	if (!Equipment->RequestUnequipItem(Active))
	{
		// Refused — the weapon is busy (firing, reloading, its own switch). Do not strand the throw: forget
		// the claim and try again shortly, rather than holding a grenade that never arms.
		StowedForThrow = nullptr;
		ArmStowWait();
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] stowing %s to free the hands for a throwable"),
		*GetNameSafe(Active));
	ArmStowWait();
	return false;
}

void UAZ_ThrowableHandComponent::ArmStowWait()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(StowWaitTimer))
	{
		return;
	}
	// Slow on purpose. This only has to notice that an animation finished; polling faster would buy nothing
	// and run Refresh dozens of times through a holster.
	World->GetTimerManager().SetTimer(StowWaitTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			Refresh();
		}), 0.1f, true);
}

void UAZ_ThrowableHandComponent::AbandonStowedWeapon()
{
	if (!StowedForThrow.IsValid())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] dropping the claim on %s - the player chose a weapon themselves"),
		*GetNameSafe(StowedForThrow.Get()));
	StowedForThrow = nullptr;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StowWaitTimer);
	}
}

void UAZ_ThrowableHandComponent::RestoreStowedWeapon()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StowWaitTimer);
	}
	UAZ_Inv_CommonUI_InventoryItem* Restore = StowedForThrow.Get();
	StowedForThrow = nullptr;   // cleared FIRST: the request can re-enter this component synchronously
	if (!Restore)
	{
		return;
	}
	AAZ_PlayerController* Owner = Cast<AAZ_PlayerController>(GetOwner());
	auto* Equipment = Owner ? Owner->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
	if (!Equipment)
	{
		return;
	}
	// ★ A REFUSAL HERE MUST NOT LOSE THE WEAPON. The draw-back often arrives while the holster that put it
	// away is still committing, and the equipment component rightly refuses a request during its own
	// transition. Dropping it there left the player permanently empty-handed after a grenade (measured
	// 2026-09-19: "request refused" immediately followed by "committed Weapon.None"). Keep the claim and
	// come back for it.
	if (!Equipment->RequestEquipItem(Restore))
	{
		StowedForThrow = Restore;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(StowWaitTimer,
				FTimerDelegate::CreateWeakLambda(this, [this]() { RestoreStowedWeapon(); }), 0.15f, false);
		}
		UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] draw-back of %s refused, retrying"), *GetNameSafe(Restore));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] drawing %s back after the throwable"), *GetNameSafe(Restore));
}

bool UAZ_ThrowableHandComponent::IsPuttingAway() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(PutAwayTimer);
}

void UAZ_ThrowableHandComponent::BeginPutAway(const UAnimMontage* PutAwayClip)
{
	UWorld* World = GetWorld();
	AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	UAbilitySystemComponent* Asc = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	// To the start of the clip's blend-out, not to its end: the draw should begin as the hand comes down.
	const float Seconds = PutAwayClip
		? FMath::Max(0.f, PutAwayClip->GetPlayLength() - PutAwayClip->GetDefaultBlendOutTime())
		: 0.f;
	if (!World || !Asc || Seconds <= 0.f)
	{
		// No clip, no window. Deferring a switch behind an animation that does not exist would only stall the
		// draw, which is worse than the overlap this is here to prevent.
		return;
	}
	Asc->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Throwable_Stowing, 1);
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] putting %s away - holding the weapon switch for %.2fs"),
		*GetNameSafe(PutAwayClip), Seconds);
	World->GetTimerManager().SetTimer(PutAwayTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			EndPutAway();
			// The refresh is what takes the prop out of the hand and lets the deferred draw through on the
			// same beat. Called here rather than from EndPutAway, which also runs during teardown.
			Refresh();
		}), Seconds, false);
}

void UAZ_ThrowableHandComponent::EndPutAway()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PutAwayTimer);
	}
	AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	UAbilitySystemComponent* Asc = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag& StowingTag = FAZ_GameplayTags::Get().State_Throwable_Stowing;
	if (!Asc || !Asc->HasMatchingGameplayTag(StowingTag))
	{
		return;
	}
	// Clearing this is what releases the deferred weapon selection: equipment watches the tag and retries.
	Asc->SetLooseGameplayTagCount(StowingTag, 0);
	UE_LOG(LogTemp, Warning, TEXT("[ThrowCarry] the throwable is away - the weapon may be drawn now"));
}

void UAZ_ThrowableHandComponent::EnterThrowAction() const
{
	const AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	auto* Asc = Hero ? Cast<UAZ_AbilitySystemComponent>(Hero->GetAbilitySystemComponent()) : nullptr;
	if (!Asc)
	{
		return;
	}
	// The same pressed edge the controller sends, so the ability sees one consistent entry path whether the
	// player readied the grenade from the quick bar or the action was re-entered after a throw.
	Asc->SendAbilityInputEdge(UAZ_GA_Throw::StaticClass(), true);
}

void UAZ_ThrowableHandComponent::LeaveThrowAction() const
{
	const AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	auto* Asc = Hero ? Cast<UAZ_AbilitySystemComponent>(Hero->GetAbilitySystemComponent()) : nullptr;
	if (!Asc)
	{
		return;
	}
	// ★ The RELEASE half of the pair. SendAbilityInputEdge refuses a press whose spec is already marked
	// InputPressed — that is what makes one physical click one press — so an entry that only ever sends
	// the pressed edge latches the flag forever and every later entry is swallowed silently, with no
	// activation and no refusal to show for it (measured 2026-09-18: the grenade armed once, then never
	// again after a cancel). Readying is the press; no longer being readied is the release.
	Asc->SendAbilityInputEdge(UAZ_GA_Throw::StaticClass(), false);
}

void UAZ_ThrowableHandComponent::HideProps()
{
	if (StaticProp) { StaticProp->SetVisibility(false, true); }
	if (SkeletalProp) { SkeletalProp->SetVisibility(false, true); }
}

void UAZ_ThrowableHandComponent::Refresh()
{
	AAZ_PawnMoverHeroCharacter* Hero = GetHero();
	USkeletalMeshComponent* Mesh = Hero ? Hero->GetMesh() : nullptr;
	const UAZ_ThrowPresentationProfile* Profile = nullptr;
	const UAZ_ThrowableDefinition* Definition = ResolveReadyThrowable(Profile);

	// ★ SUPPRESSION CANNOT OUTLIVE THE ACTION THAT ASKED FOR IT. Both suppression flags exist only for the
	// duration of a throw: one hides the prop while the projectile carries it, the other yields the slot to
	// the wind-up. With no throw running they mean nothing, and a stale one is invisible — it does not
	// disable the feature loudly, it just silently withholds the carry montage from every grenade after it.
	//
	// That is not hypothetical: the throw commit ends the ability re-entrantly, and the release path that
	// follows used to re-suppress afterwards (see UAZ_GA_Throw::PerformRelease). That leak is fixed at its
	// source; this makes the whole class of it self-correcting, because the cost is one pointer compare and
	// the failure mode is a feature that quietly stops working.
	if (bSuppressed || bActionOwnsBody)
	{
		AAZ_PlayerController* SuppressionOwner = Cast<AAZ_PlayerController>(GetOwner());
		if (!SuppressionOwner || !SuppressionOwner->FindActiveThrow())
		{
			bSuppressed = false;
			bActionOwnsBody = false;
		}
	}

	// The carry STATE, published before anything cosmetic: it is what puts the body into combat-ready facing
	// at a walk, and the masked hold below is only legible on top of that stance. Published from the readied
	// item alone, so it survives the wind-up (where ThrowPreparing takes over the body) and clears itself the
	// moment the last unit is thrown and readiness resolves to nothing.
	PublishReadyTag(Definition != nullptr);

	// ★ HANDS FIRST — AND THAT INCLUDES THE POSE, NOT JUST THE ACTION. Resolved once here because it has
	// side effects (it issues the holster and arms the wait), and because the carry montage and the throw
	// action must agree about when the hands became free.
	//
	// The carry montage used to start on the same frame the holster was requested, and its mask owns
	// everything from spine_01 up — so the holster animation played underneath it, completely hidden. From
	// outside that is indistinguishable from "the switch animation does not work", which is exactly how it
	// was reported (2026-09-19), while rifle-to-pistol looked perfect because no grenade pose covers it.
	const bool bHandsFree = (Definition && !bSuppressed) ? EnsureHandsFreeForThrow() : true;

	if (Definition)
	{
		// Something throwable is in hand again, so whatever was being put away is over. Left running, its tag
		// would defer the player's next weapon switch behind an animation nobody is watching — and the prop
		// hold below would fight the one that has just been taken out.
		EndPutAway();
	}

	// Readying a throwable IS entering the throw action: Start into Loop, held until the player throws or
	// cancels (user call 2026-09-18). Activation is safe to attempt on every refresh — the ability blocks
	// itself with Ability.State.ThrowPreparing, so an already-running one is not restarted.
	//
	// ★ BEFORE THE CARRY IDLE, NOT AFTER. Entering takes the body: it re-enters this function with
	// bActionOwnsBody raised, and the carry idle is then never started at all. The other way round, the idle
	// was played and stopped inside the same frame — its mask rose off zero and fell straight back while the
	// wind-up was blending in, which is the twitch on taking a grenade out (reported 2026-09-20).
	if (Definition && !bSuppressed)
	{
		// ★ HANDS FIRST, GRENADE SECOND. Holstering is animated and takes time, so the throw is not started
		// on this pass when a weapon is still out — the wait below brings us back here when the hands are
		// free. Entering anyway is what produced a grenade thrown with a rifle still in hand.
		if (bHandsFree)
		{
			EnterThrowAction();
		}
	}
	else if (!Definition)
	{
		// Readiness moved off the throwable — swapped to another quick-slot item, or the last unit is gone.
		// The action does not watch the quick bar, so without this it would keep holding the ready pose for
		// a grenade the player no longer has.
		if (auto* Owner = Cast<AAZ_PlayerController>(GetOwner()))
		{
			if (UAZ_GA_Throw* Active = Owner->FindActiveThrow())
			{
				// Starting the cancel clip is what opens the put-away window — the ability calls back into
				// BeginPutAway from there, so every cancel route gets it, not just this one.
				Active->RequestCancel();
			}
		}
		LeaveThrowAction();
		// The throwable is gone from the hand — thrown, cancelled or swapped away. Whatever was holstered to
		// make room comes back out, or the player is left empty-handed after every grenade.
		RestoreStowedWeapon();
	}

	// The prop and the carry IDLE are suppressed independently: the grenade stays in the hand while aiming,
	// but the idle must yield the upper-body slot to the wind-up. Read AFTER the action block, so its
	// decision is made against what the action just did rather than against the state before it.
	UpdateCarryMontage(Mesh,
		Definition && !bSuppressed && !bActionOwnsBody && bHandsFree ? Profile : nullptr);

	// ★ THE HAND CANNOT BE EMPTY DURING A PUT-AWAY. The clip shows the character lowering a grenade and
	// stowing it; deleting the prop on the frame the clip starts plays that whole two seconds on an empty
	// hand. Readiness is already false by now — that is what started the clip — so the prop is held by the
	// window alone, and the refresh at the end of it takes it away.
	//
	// Not while SUPPRESSED: there the projectile IS the grenade, and a second copy in the hand would put one
	// object in two places.
	if (!Definition && !bSuppressed && Mesh && IsPuttingAway())
	{
		return;
	}
	if (!Mesh || !Definition || bSuppressed)
	{
		HideProps();
		return;
	}

	// A pawn change between refreshes leaves the props on a mesh that is no longer ours.
	if (AttachedTo.Get() != Mesh)
	{
		if (StaticProp) { StaticProp->DestroyComponent(); StaticProp = nullptr; }
		if (SkeletalProp) { SkeletalProp->DestroyComponent(); SkeletalProp = nullptr; }
		AttachedTo = Mesh;
	}

	// Skeletal in-hand art wins when the definition supplies it (a grenade is carried with its pin and spoon
	// and thrown armed); anything else falls back to the thrown mesh.
	UMeshComponent* Prop = nullptr;
	FVector Scale = FVector::OneVector;
	if (USkeletalMesh* InHand = Definition->HeldSkeletalMesh)
	{
		if (!SkeletalProp)
		{
			SkeletalProp = NewObject<USkeletalMeshComponent>(Hero);
			SkeletalProp->SetMobility(EComponentMobility::Movable);
			SkeletalProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SkeletalProp->SetGenerateOverlapEvents(false);
			// Nothing animates it, so skip the per-frame skeletal work a ticking component costs.
			SkeletalProp->PrimaryComponentTick.bCanEverTick = false;
			SkeletalProp->SetupAttachment(Mesh);
			SkeletalProp->RegisterComponent();
		}
		SkeletalProp->SetSkeletalMeshAsset(InHand);
		if (StaticProp) { StaticProp->SetVisibility(false, true); }
		Prop = SkeletalProp;
		Scale = Definition->GetHeldSkeletalMeshScale();
	}
	else if (Definition->HeldMesh)
	{
		if (!StaticProp)
		{
			StaticProp = NewObject<UStaticMeshComponent>(Hero);
			StaticProp->SetMobility(EComponentMobility::Movable);
			StaticProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StaticProp->SetGenerateOverlapEvents(false);
			StaticProp->SetupAttachment(Mesh);
			StaticProp->RegisterComponent();
		}
		StaticProp->SetStaticMesh(Definition->HeldMesh);
		if (SkeletalProp) { SkeletalProp->SetVisibility(false, true); }
		Prop = StaticProp;
		Scale = Definition->GetHeldMeshScale();
	}
	if (!Prop)
	{
		HideProps();
		return;
	}

	// Attached to the grip socket with the item's own offset — the same GripOffset the solver composes onto
	// the release transform, so the object leaves from where it was visibly held.
	Prop->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Profile->GripBone);
	Prop->SetRelativeTransform(Profile->GripOffset);
	// Scale LAST: SetRelativeTransform writes GripOffset's own scale of 1 over anything set before it.
	Prop->SetRelativeScale3D(Scale);
	Prop->SetVisibility(true, true);
}
