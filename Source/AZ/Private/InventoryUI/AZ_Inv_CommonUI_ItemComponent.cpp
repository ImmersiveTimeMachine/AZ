// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "AZ_GameplayTags.h"
#include "GameFramework/Actor.h"

#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UAZ_Inv_CommonUI_ItemComponent::UAZ_Inv_CommonUI_ItemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_Inv_CommonUI_ItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PickupItemManifest);
	DOREPLIFETIME(ThisClass, PickupState);
	DOREPLIFETIME(ThisClass, ContainedItems);
	DOREPLIFETIME(ThisClass, PickupStackCount);
}

void UAZ_Inv_CommonUI_ItemComponent::DestroyItem() const
{
	if (auto Owner = GetOwner())
		Owner->Destroy();
}

FString UAZ_Inv_CommonUI_ItemComponent::GetPickupMessage() const
{
	const auto* Magazine = PickupItemManifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
	if (!Magazine) return PickupMessage;

	FText ItemName = NSLOCTEXT("AZInventoryPickup", "MagazineName", "Magazine");
	const auto& Tags = FAZ_GameplayTags::Get();
	for (const FGameplayTag& Tag : {Tags.Item_Fragment_Name_StaticText, Tags.Item_Fragment_Name, Tags.Item_Fragment_Ammo_Primary_Name})
	{
		if (const auto* Name = PickupItemManifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tag);
			Name && !Name->GetText().IsEmpty())
		{
			ItemName = Name->GetText();
			break;
		}
	}
	// Zero is a real empty magazine. A payload still arriving over replication
	// stays unknown instead of presenting the class template's InitialRounds.
	const bool bRoundsKnown = PickupState.InstanceId.IsValid() && Magazine->Capacity > 0
		&& PickupState.CurrentRounds >= 0 && PickupState.CurrentRounds <= Magazine->Capacity;
	const FText Unknown = NSLOCTEXT("AZInventoryPickup", "UnknownRounds", "--");
	const FText Rounds = bRoundsKnown ? FText::AsNumber(PickupState.CurrentRounds) : Unknown;
	const FText Capacity = Magazine->Capacity > 0 ? FText::AsNumber(Magazine->Capacity) : Unknown;
	return FText::Format(NSLOCTEXT("AZInventoryPickup", "MagazinePrompt", "Press E to pick up {0} ({1}/{2})"),
		ItemName, Rounds, Capacity).ToString();
}

void UAZ_Inv_CommonUI_ItemComponent::PickedUp()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bPickupCommitted) return;
	bPickupCommitted = true;
	OnPickedUp();
	GetOwner()->Destroy();
}

void UAZ_Inv_CommonUI_ItemComponent::InitItemManifest(FAZ_Inv_CommonUI_ItemManifest CopyOfManifest)
{
	PickupItemManifest = CopyOfManifest;
	PickupState = FAZ_InventoryItemState();
	ContainedItems.Reset();
	InitializePickupPayload();
}

void UAZ_Inv_CommonUI_ItemComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority()) InitializePickupPayload();
}

bool UAZ_Inv_CommonUI_ItemComponent::InitializePickupPayload()
{
	// Never put an instance identity on a class default/template or an editor-only placed actor.
	if (IsTemplate() || !GetOwner() || GetOwner()->IsTemplate() || !GetOwner()->HasAuthority() || bPickupCommitted ||
		(!GetOwner()->HasActorBegunPlay() && !GetOwner()->IsActorBeginningPlay())) return false;
	if (PickupState.InstanceId.IsValid()) return true;
	PickupState.InstanceId = FGuid::NewGuid();
	PickupState.Location = EAZ_InventoryItemLocation::World;
	if (const auto* Weapon = PickupItemManifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		Weapon && Weapon->bUsesDetachableMagazines)
	{
		PickupState.SelectedFireMode = Weapon->DefaultFireMode;
	}
	if (const auto* Stack = PickupItemManifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>(); PickupItemManifest.IsStackable() && Stack)
	{
		PickupStackCount = FMath::Max(1, Stack->GetStackCount());
	}
	if (const auto* Magazine = PickupItemManifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>())
	{
		PickupState.CurrentRounds = FMath::Clamp(Magazine->InitialRounds, 0, Magazine->Capacity);
	}
	for (const auto& Manifest : InitialContainedItemManifests)
	{
		FAZ_InventoryPickupRecord& Record = ContainedItems.AddDefaulted_GetRef();
		Record.Manifest = Manifest;
		Record.State.InstanceId = FGuid::NewGuid();
		Record.State.Location = EAZ_InventoryItemLocation::WeaponMagazine;
		Record.State.ParentItemId = PickupState.InstanceId;
		if (const auto* Magazine = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>())
		{
			Record.State.CurrentRounds = FMath::Clamp(Magazine->InitialRounds, 0, Magazine->Capacity);
		}
		PickupState.InsertedMagazineId = Record.State.InstanceId;
	}
	return true;
}

FAZ_InventoryPickupRecord UAZ_Inv_CommonUI_ItemComponent::GetRootRecord() const
{
	FAZ_InventoryPickupRecord Result;
	Result.Manifest = PickupItemManifest;
	Result.State = PickupState;
	Result.StackCount = PickupStackCount;
	return Result;
}

void UAZ_Inv_CommonUI_ItemComponent::SetPickupPayload(const FAZ_InventoryPickupRecord& RootRecord, const TArray<FAZ_InventoryPickupRecord>& Children)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	PickupItemManifest = RootRecord.Manifest;
	PickupState = RootRecord.State;
	PickupState.Location = EAZ_InventoryItemLocation::World;
	PickupState.ParentItemId.Invalidate();
	PickupStackCount = RootRecord.StackCount;
	ContainedItems = Children;
	bPickupCommitted = false;
	GetOwner()->ForceNetUpdate();
}

void UAZ_Inv_CommonUI_ItemComponent::SetRemainingStackCount(int32 Count)
{
	PickupStackCount = FMath::Max(0, Count);
	if (auto* Stack = PickupItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>()) Stack->SetStackCount(PickupStackCount);
	GetOwner()->ForceNetUpdate();
}
