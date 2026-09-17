// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableHandComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
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
	Super::EndPlay(EndPlayReason);
}

void UAZ_ThrowableHandComponent::HandleReadyItemChanged() { Refresh(); }
void UAZ_ThrowableHandComponent::HandleInventoryChanged() { Refresh(); }

void UAZ_ThrowableHandComponent::HandlePawnChanged(APawn* /*OldPawn*/, APawn* /*NewPawn*/)
{
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

void UAZ_ThrowableHandComponent::UpdateCarryMontage(USkeletalMeshComponent* Mesh,
	const UAZ_ThrowPresentationProfile* Profile)
{
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	// The legacy pose lane is still fed for any ABP that consumes it; the MHC hero's does not, which is why
	// the montage below is what actually produces its held idle.
	if (auto* Mover = Cast<UAZ_MoverAnimInstance>(Anim))
	{
		Mover->SetThrowableCarryPose(Profile ? Profile->CarryPose.Get() : nullptr);
	}
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
		Anim->Montage_Play(Wanted, 1.f);
	}
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
	// The carry pose rides the anim instance's upper-body lane, so it is pushed even when the prop itself is
	// suppressed mid-throw: the FullBody throw montage is overriding that lane anyway, and clearing it here
	// would make the arms drop for a frame the moment the action ended.
	// The prop and the carry IDLE are suppressed independently: the grenade stays in the hand while aiming,
	// but the idle must yield the upper-body slot to the wind-up.
	UpdateCarryMontage(Mesh, Definition && !bSuppressed && !bActionOwnsBody ? Profile : nullptr);

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
