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
		Owner->OnPossessedPawnChanged.RemoveDynamic(this, &UAZ_ThrowableHandComponent::HandlePawnChanged);
	}
	// The tag lives on the pawn's ASC, which outlives this component on a controller teardown. Left set, the
	// body would stay locked to combat-ready walking with no grenade in sight.
	PublishReadyTag(false);
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
	if (!Anim)
	{
		ActiveCarryMontage = nullptr;
		return;
	}
	if (ActiveCarryMontage == Wanted)
	{
		// Already showing the right thing. Re-playing every refresh would restart the loop on every
		// inventory event and make the idle stutter.
		if (Wanted && !Anim->Montage_IsPlaying(Wanted))
		{
			Anim->Montage_Play(Wanted, 1.f);   // something else stopped it; bring it back
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

	// The carry STATE, published before anything cosmetic: it is what puts the body into combat-ready facing
	// at a walk, and the masked hold below is only legible on top of that stance. Published from the readied
	// item alone, so it survives the wind-up (where ThrowPreparing takes over the body) and clears itself the
	// moment the last unit is thrown and readiness resolves to nothing.
	PublishReadyTag(Definition != nullptr);

	// The prop and the carry IDLE are suppressed independently: the grenade stays in the hand while aiming,
	// but the idle must yield the upper-body slot to the wind-up.
	UpdateCarryMontage(Mesh, Definition && !bSuppressed && !bActionOwnsBody ? Profile : nullptr);

	// Readying a throwable IS entering the throw action: Start into Loop, held until the player throws or
	// cancels (user call 2026-09-18). Activation is safe to attempt on every refresh — the ability blocks
	// itself with Ability.State.ThrowPreparing, so an already-running one is not restarted.
	if (Definition && !bSuppressed)
	{
		EnterThrowAction();
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
				Active->RequestCancel();
			}
		}
		LeaveThrowAction();
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
