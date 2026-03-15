// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"

#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UAZ_Inv_CommonUI_InventoryComponent::UAZ_Inv_CommonUI_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}


// Called when the game starts
void UAZ_Inv_CommonUI_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
	
}


// Called every frame
void UAZ_Inv_CommonUI_InventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UAZ_Inv_CommonUI_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

void UAZ_Inv_CommonUI_InventoryComponent::ToggleInventoryMenu()
{
	if (bInventoryMenuOpen)
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::TryAddItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
	auto Result = InventoryMenu->HasRoomForItem(ItemComponent);

	if (Result.TotalRoomToFill == 0)
	{
		OnNoRoomInInventory.Broadcast();
		return;
	}

	// Only look for an existing item when the incoming item is stackable.
	if (Result.bIsStackable)
	{
		Result.Item = InventoryList.FindFirstItemByTypeTag(ItemComponent->GetItemManifest().GetItemTypeTag());
	}

	if (Result.Item.IsValid() && Result.bIsStackable)
	{
		// Add stacks to an item that already exists in the inventory. We only want to update the stack count,
		// not create a new item of this type.
		OnStackChange.Broadcast(Result);
		Server_AddStacksToItem(ItemComponent, Result.TotalRoomToFill, Result.RemainingRooms);
	}
	else
	{
		Server_AddNewItem(ItemComponent, Result.bIsStackable ? Result.TotalRoomToFill : 0, Result.RemainingRooms);
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_AddNewItem_Implementation(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount,
	int32 Remainder)
{
	UAZ_Inv_CommonUI_InventoryItem* NewItem = InventoryList.CreateInventoryEntryFromItemComponent(ItemComponent);
	NewItem->SetTotalStackCount(StackCount);

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FAZ_Inv_CommonUI_Stackable_Fragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}

}

void UAZ_Inv_CommonUI_InventoryComponent::Server_AddStacksToItem_Implementation(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount,
	int32 RemainingRooms)
{
	const FGameplayTag& ItemType = IsValid(ItemComponent) ? ItemComponent->GetItemManifest().GetItemTypeTag() : FGameplayTag::EmptyTag;
	UAZ_Inv_CommonUI_InventoryItem* Item = InventoryList.FindFirstItemByTypeTag(ItemType);
	if (!IsValid(Item)) return;

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	if (RemainingRooms == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FAZ_Inv_CommonUI_Stackable_Fragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>())
	{
		StackableFragment->SetStackCount(RemainingRooms);
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_DropItem_Implementation(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - StackCount;

	// Notify equipment system before removing (so it can clean up attached actors)
	if (NewStackCount <= 0)
	{
		OnItemDropped.Broadcast(Item);
		InventoryList.RemoveInventoryItem(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	SpawnDroppedItem(Item, StackCount);
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_ConsumeItem_Implementation(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - 1;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveInventoryItem(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	if (FAZ_Inv_CommonUI_ConsumableFragment* ConsumableFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_ConsumableFragment>())
	{
		ConsumableFragment->OnConsume(OwningController.Get());
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::SpawnDroppedItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount)
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotatedForward = OwningPawn->GetActorForwardVector();
	RotatedForward = RotatedForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotatedForward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
	SpawnLocation.Z -= RelativeSpawnElevation;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	if (FAZ_Inv_CommonUI_Stackable_Fragment* StackableFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>())
	{
		StackableFragment->SetStackCount(StackCount);
	}
	ItemManifest.SpawnPickupActor(this, SpawnLocation, SpawnRotation);
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_EquipSlotClicked_Implementation(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip,
	UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip)
{
	Multicast_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UAZ_Inv_CommonUI_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip,
	UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip)
{
	if (IsValid(ItemToEquip))
	{
		OnItemEquipped.Broadcast(ItemToEquip);
	}
	if (IsValid(ItemToUnequip))
	{
		OnItemUnequipped.Broadcast(ItemToUnequip);
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::AddRepSubObjects(UObject* SubObjects)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObjects))
	{
		AddReplicatedSubObject(SubObjects);
	}
}

void UAZ_Inv_CommonUI_InventoryComponent::ConstructInventory()
{
	OwningController = Cast<APlayerController>(GetOwner());
	checkf(OwningController.IsValid(), TEXT("UAZ_Inv_InventoryComponent::ConstructInventory - OwningController is invalid. The component must be owned by a valid PlayerController."));

	if (!OwningController->IsLocalController())
		return;

	InventoryMenu = CreateWidget<UAZ_Inv_CommonUI_GameInventoryMenu>(OwningController.Get(), InventoryScreenClass);
	InventoryMenu->OnBackAction.BindUObject(this, &ThisClass::CloseInventoryMenu);
	InventoryMenu->AddToViewport();
	CloseInventoryMenu();
}

void UAZ_Inv_CommonUI_InventoryComponent::OpenInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid())
		return;

	// Set input mode BEFORE activating so CommonUI can register bindings properly.
	FInputModeGameAndUI InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);

	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	InventoryMenu->ActivateWidget();
	bInventoryMenuOpen = true;
}

void UAZ_Inv_CommonUI_InventoryComponent::CloseInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid())
		return;

	if (bInventoryMenuOpen)
	{
		InventoryMenu->DeactivateWidget();
	}

	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	bInventoryMenuOpen = false;

	FInputModeGameOnly InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(false);
}

