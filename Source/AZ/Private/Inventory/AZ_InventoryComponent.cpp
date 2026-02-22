// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/AZ_InventoryComponent.h"

#include "Engine/ActorChannel.h"

#include "Inventory/AZ_InventoryItemDefinition.h"
#include "Inventory/AZ_InventoryItemInstance.h"
#include "Inventory/AZ_InventorySlot.h"
#include "Net/UnrealNetwork.h"


UAZ_InventoryComponent::UAZ_InventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	GridWidth = 10;
	GridHeight = 10;
}

void UAZ_InventoryComponent::AddItemToInventory(UAZ_InventoryItemDefinition* ItemDef)
{
	ServerAddItemToInventory(ItemDef);
}

void UAZ_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_InventoryComponent, GridWidth,  COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_InventoryComponent, GridHeight,  COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_InventoryComponent, InventoryItemInstances, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_InventoryComponent, CurrentInventoryItemInstanceID,  COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_InventoryComponent, InventoryItems,  COND_OwnerOnly, REPNOTIFY_Always);
	
}

void UAZ_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	// Initialize the grid with current size. It may be resized again by OnRep_GridSize when replicated.
	GridSlots.Init(nullptr, GridWidth * GridHeight);

	// If we are a client, we might have already received the replicated data.
	if (InventoryItemInstances.Num() > 0)
	{
		RebuildGrid();
	}
}

void UAZ_InventoryComponent::OnRep_AllItems()
{
	RefreshLocalStateAfterItemsChanged();
}

void UAZ_InventoryComponent::OnRep_GridSize()
{
	// Resize grid and rebuild with new dimensions
	GridSlots.Init(nullptr, GridWidth * GridHeight);
	RebuildGrid();
	OnInventoryUpdated.Broadcast();
}

void UAZ_InventoryComponent::RefreshLocalStateAfterItemsChanged()
{
	RebuildGrid();
	OnInventoryUpdated.Broadcast();
}

void UAZ_InventoryComponent::OnRep_CurrentInventoryItemInstanceID()
{
	OnItemAddedToInventory.Broadcast(CurrentInventoryItemInstanceID);
}

void UAZ_InventoryComponent::ServerAddItemToInventory_Implementation(UAZ_InventoryItemDefinition* ItemDef)
{
	if (FIntPoint FoundTopLeft{}; FindEmptySpaceForItem(ItemDef, FoundTopLeft))
	{
		// 1. Create a new struct instance on the stack.
		FAZ_InventoryItemData NewItemData;
		NewItemData.ItemDefinition = ItemDef;
		NewItemData.InstanceId = FGuid::NewGuid(); // Server generates the ID
		NewItemData.TopLeftInGrid = FoundTopLeft;

		// 2. Add the struct to the replicated array.
		InventoryItems.Add(NewItemData);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}

		// 3. Manually call the OnRep for the server's UI to update.
		OnRep_InventoryItems();
	}

	

	
	
	
	/*FIntPoint FoundTopLeft;
	if (FindEmptySpaceForItem(ItemDef, FoundTopLeft))
	{
		UAZ_InventoryItemInstance* NewInstance = NewObject<UAZ_InventoryItemInstance>(GetOwner());
		NewInstance->ItemDefinition = ItemDef;
		NewInstance->TopLeftInGrid = FoundTopLeft;

		// Register it for replication. This uses the engine's registered subobject list on the component.
		// NOTE: This requires the owning actor/component to replicate.
		AddReplicatedSubObject(NewInstance, COND_OwnerOnly);


		InventoryItemInstances.Add(NewInstance);

		//CurrentInventoryItemInstanceID = NewInstance->GetInstanceId();

		// Fire server-side too, so server listeners receive the event.
		OnItemAddedToInventory.Broadcast(CurrentInventoryItemInstanceID);

		// Nudge replication so clients get the subobject + pointer quickly
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}

		// Update local state on server (clients will react via OnRep_AllItems)
		//RefreshLocalStateAfterItemsChanged();
	}*/
}

bool UAZ_InventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool Wrote = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	for (UAZ_InventoryItemInstance* Item : InventoryItemInstances)
	{
		if (Item)
		{
			Wrote |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
		}
	}
	return Wrote;
}

void UAZ_InventoryComponent::RemoveItem(UAZ_InventoryItemInstance* ItemInstance)
{
	if (GetOwner()->HasAuthority())
	{
		if (!ItemInstance) return;

		InventoryItemInstances.Remove(ItemInstance);
		ClearItemFromGrid(ItemInstance);

		// Update local state on server (clients react via OnRep_AllItems)
		RefreshLocalStateAfterItemsChanged();
	}
	else
	{
		Server_RemoveItem(ItemInstance);
	}
}

void UAZ_InventoryComponent::Server_RemoveItem_Implementation(UAZ_InventoryItemInstance* ItemInstance)
{
	// Optional: restrict to owning client
	if (!CanModifyInventory())
	{
		return;
	}
	RemoveItem(ItemInstance);
}

bool UAZ_InventoryComponent::CanModifyInventory() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	// Allow server (authority) and the owning client to invoke server RPCs
	return (GetOwner() && GetOwner()->HasAuthority()) || (PawnOwner && PawnOwner->IsLocallyControlled());
}

int32 UAZ_InventoryComponent::GetIndexFromCoords(const int32 X, const int32 Y) const
{
	if (X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
	{
		return INDEX_NONE;
	}
	return Y * GridWidth + X;
}

bool UAZ_InventoryComponent::CanPlaceItemAt(const UAZ_InventoryItemDefinition* ItemDef, const FIntPoint TopLeft) const
{
	if (!ItemDef)
	{
		return false;
	}

	const FIntPoint Dimensions = ItemDef->Dimensions;

	// Loop through the entire footprint of the item
	for (int32 y = 0; y < Dimensions.Y; ++y)
	{
		for (int32 x = 0; x < Dimensions.X; ++x)
		{
			// Calculate the coordinates of the current cell we are checking
			const int32 TargetX = TopLeft.X + x;
			const int32 TargetY = TopLeft.Y + y;

			// Get the 1D array index for that coordinate
			const int32 Index = GetIndexFromCoords(TargetX, TargetY);

			// Check if this cell is out of bounds OR already occupied
			if (Index == INDEX_NONE || !GridSlots.IsValidIndex(Index) || GridSlots[Index] != nullptr)
			{
				// If any cell is invalid or occupied, we cannot place the item here.
				return false;
			}
		}
	}

	// If we looped through all the cells and none were occupied, the placement is valid.
	return true;
}

void UAZ_InventoryComponent::OnRep_InventoryItems()
{
	RefreshLocalStateAfterItemsChanged();
}

bool UAZ_InventoryComponent::FindEmptySpaceForItem(const UAZ_InventoryItemDefinition* ItemDef, FIntPoint& OutTopLeft) const
{
	// Ensure grid is sized
	if (GridSlots.Num() != GridWidth * GridHeight)
	{
		// Cannot modify GridSlots in const function; treat as no space and let caller rebuild/init beforehand.
		return false;
	}

	// Treat the item as a 1x1 for now; extend with ItemDef dimensions when available.
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint Candidate{X, Y};
			if (CanPlaceItemAt(ItemDef, Candidate))
			{
				OutTopLeft = Candidate;
				return true;
			}
		}
	}
	return false;
}

void UAZ_InventoryComponent::PlaceItemOnGrid(UAZ_InventoryItemInstance* ItemInstance)
{
	if (!IsValid(ItemInstance))
	{
		return;
	}

	if (GridSlots.Num() != GridWidth * GridHeight)
	{
		GridSlots.Init(nullptr, GridWidth * GridHeight);
	}

	const int32 Idx = GetIndexFromCoords(ItemInstance->TopLeftInGrid.X, ItemInstance->TopLeftInGrid.Y);
	if (Idx != INDEX_NONE && GridSlots.IsValidIndex(Idx))
	{
		GridSlots[Idx] = ItemInstance;
	}
}

void UAZ_InventoryComponent::ClearItemFromGrid(UAZ_InventoryItemInstance* ItemInstance)
{
	if (!IsValid(ItemInstance) || GridSlots.Num() == 0)
	{
		return;
	}

	for (UAZ_InventoryItemInstance*& Slot : GridSlots)
	{
		if (Slot == ItemInstance)
		{
			Slot = nullptr;
		}
	}
}

void UAZ_InventoryComponent::RebuildGrid()
{
	GridSlots.Init(nullptr, GridWidth * GridHeight);

	for (UAZ_InventoryItemInstance* Item : InventoryItemInstances)
	{
		PlaceItemOnGrid(Item);
	}
}