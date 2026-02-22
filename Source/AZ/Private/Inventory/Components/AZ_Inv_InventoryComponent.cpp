#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Widgets/Screens/AZ_Inv_InventoryMenuBase.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/Items/AZ_Inv_InventoryItem.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"
#include "Inventory/Items/Fragments/AZ_Inv_ItemFragment.h"
#include "Inventory/Items/Manifest/AZ_Inv_ItemManifest.h"
#include "Net/UnrealNetwork.h"

UAZ_Inv_InventoryComponent::UAZ_Inv_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}

void UAZ_Inv_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

void UAZ_Inv_InventoryComponent::TryAddItem(UAZ_Inv_ItemComponent* ItemComponent)
{
	auto Result = InventoryMenu->HasRoomForItem(ItemComponent);
	UAZ_Inv_InventoryItem* FoundItem = InventoryList.FindFirstItemByTypeTag(ItemComponent->GetItemManifest().GetItemTypeTag());
	Result.Item = FoundItem;

	if (Result.TotalRoomToFill == 0)
	{ 
		OnNoRoomInInventory.Broadcast();
		return;
	}

	if (Result.Item.IsValid() && Result.bIsStackable)
	{
		// Add stacks to an item that already exists in the inventory. We only want to update the stack count,
		// not create a new item of this type.
		OnStackChange.Broadcast(Result);
		Server_AddStacksToItem(ItemComponent, Result.TotalRoomToFill, Result.RemainingRooms);
	}
	else if (Result.TotalRoomToFill > 0)
	{
		Server_AddNewItem(ItemComponent, Result.bIsStackable ? Result.TotalRoomToFill : 0, Result.RemainingRooms);
	}
}

void UAZ_Inv_InventoryComponent::Server_AddNewItem_Implementation(UAZ_Inv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	UAZ_Inv_InventoryItem* NewItem = InventoryList.CreateInventoryEntryFromItemComponent(ItemComponent);
	NewItem->SetTotalStackCount(StackCount);

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}
	
	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FAZ_Inv_StackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}
	
void UAZ_Inv_InventoryComponent::Server_AddStacksToItem_Implementation(UAZ_Inv_ItemComponent* ItemComponent, int32 StackCount, int32 RemainingRooms)
{
	const FGameplayTag& ItemType = IsValid(ItemComponent) ? ItemComponent->GetItemManifest().GetItemTypeTag() : FGameplayTag::EmptyTag;
	UAZ_Inv_InventoryItem* Item = InventoryList.FindFirstItemByTypeTag(ItemType);
	if (!IsValid(Item)) return;

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	if (RemainingRooms == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FAZ_Inv_StackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_StackableFragment>())
	{
		StackableFragment->SetStackCount(RemainingRooms);
	}
}

void UAZ_Inv_InventoryComponent::Server_DropItem_Implementation(UAZ_Inv_InventoryItem* Item, int32 StackCount)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - StackCount;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveInventoryItem(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	SpawnDroppedItem(Item, StackCount);
}

void UAZ_Inv_InventoryComponent::Server_ConsumeItem_Implementation(UAZ_Inv_InventoryItem* Item)
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

	if (FAZ_Inv_ConsumableFragment* ConsumableFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_ConsumableFragment>())
	{
		ConsumableFragment->OnConsume(OwningController.Get());
	}
}

void UAZ_Inv_InventoryComponent::Server_EquipSlotClicked_Implementation(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip)
{
	Multicast_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UAZ_Inv_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip)
{
	// Equipment Component will listen to these delegates
	OnItemEquipped.Broadcast(ItemToEquip);
	OnItemUnequipped.Broadcast(ItemToUnequip);
}

void UAZ_Inv_InventoryComponent::SpawnDroppedItem(UAZ_Inv_InventoryItem* Item, int32 StackCount)
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotatedForward = OwningPawn->GetActorForwardVector();
	RotatedForward = RotatedForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotatedForward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
	SpawnLocation.Z -= RelativeSpawnElevation;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FAZ_Inv_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	if (FAZ_Inv_StackableFragment* StackableFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_StackableFragment>())
	{
		StackableFragment->SetStackCount(StackCount);
	}
	ItemManifest.SpawnPickupActor(this, SpawnLocation, SpawnRotation);
}

void UAZ_Inv_InventoryComponent::ToggleInventoryMenu()
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

void UAZ_Inv_InventoryComponent::AddRepSubObjects(UObject* SubObjects)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObjects))
	{
		AddReplicatedSubObject(SubObjects);
	}
}

void UAZ_Inv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
	
}

// Initializes the inventory component by setting up a reference to the owning PlayerController.
// Ensures that the component is attached to a valid PlayerController at construction time.
void UAZ_Inv_InventoryComponent::ConstructInventory()
{
	OwningController = Cast<APlayerController>(GetOwner());
	checkf(OwningController.IsValid(), TEXT("UAZ_Inv_InventoryComponent::ConstructInventory - OwningController is invalid. The component must be owned by a valid PlayerController."));

	if (!OwningController->IsLocalController())
		return;

	InventoryMenu = CreateWidget<UAZ_Inv_InventoryMenuBase>(OwningController.Get(), InventoryScreenClass);
	InventoryMenu->AddToViewport();
	CloseInventoryMenu();
}

void UAZ_Inv_InventoryComponent::OpenInventoryMenu()
{
	if (!IsValid(InventoryMenu) && OwningController.IsValid())
		return;

	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bInventoryMenuOpen = true;

	FInputModeGameAndUI InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);
}

void UAZ_Inv_InventoryComponent::CloseInventoryMenu()
{
	if (!IsValid(InventoryMenu) && OwningController.IsValid())
		return;

	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	bInventoryMenuOpen = false;

	FInputModeGameOnly InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(false);
}
