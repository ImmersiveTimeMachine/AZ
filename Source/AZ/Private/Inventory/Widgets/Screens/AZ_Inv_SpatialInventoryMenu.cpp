#include "Inventory/Widgets/Screens/AZ_Inv_SpatialInventoryMenu.h"

#include "AZ/AZ.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Inventory/Items/AZ_Inv_InventoryItem.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"
#include "Inventory/Widgets/AZ_Inv_ItemDescription/AZ_Inv_ItemDescription.h"
#include "Inventory/Widgets/Components/AZ_Inv_EquippedGridSlot.h"
#include "Inventory/Widgets/Components/AZ_Inv_InventoryGrid.h"
#include "Inventory/Widgets/SlottedItems/AZ_Inv_EquippedSlottedItem.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_SpatialInventoryMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bind button click events
	if (Button_Equippable) Button_Equippable->OnClicked.AddDynamic(this, &UAZ_Inv_SpatialInventoryMenu::ShowEquippables);
	if (Button_Consumable) Button_Consumable->OnClicked.AddDynamic(this, &UAZ_Inv_SpatialInventoryMenu::ShowConsumables);
	if (Button_Craftable) Button_Craftable->OnClicked.AddDynamic(this, &UAZ_Inv_SpatialInventoryMenu::ShowCraftables);
	
	Grid_Equippables->SetOwningCanvas(CanvasPanel);
	Grid_Consumables->SetOwningCanvas(CanvasPanel);
	Grid_Craftables->SetOwningCanvas(CanvasPanel);

	// Set initial active grid
	ShowEquippables();
	
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
 		UAZ_Inv_EquippedGridSlot* EquippedGridSlot = Cast<UAZ_Inv_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			EquippedGridSlots.Add(EquippedGridSlot);
			EquippedGridSlot->EquippedGridSlotClicked.AddDynamic(this, &ThisClass::EquippedGridSlotClicked);
		}
	});
}

FReply UAZ_Inv_SpatialInventoryMenu::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//ActiveGrid->DropItem();
	return FReply::Handled();
}

void UAZ_Inv_SpatialInventoryMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!IsValid(ItemDescription)) return;
	SetItemDescriptionSizeAndPosition(ItemDescription, CanvasPanel);
	SetEquippedItemDescriptionSizeAndPosition(ItemDescription, EquippedItemDescription, CanvasPanel);
}

void UAZ_Inv_SpatialInventoryMenu::OnItemHovered(UAZ_Inv_InventoryItem* Item)
{	
	const auto& Manifest = Item->GetItemManifest();
	UAZ_Inv_ItemDescription* DescriptionWidget = GetItemDescription();
	DescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);

	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);

	FTimerDelegate DescriptionTimerDelegate;
	DescriptionTimerDelegate.BindLambda([this, Item, &Manifest, DescriptionWidget]()
	{
		GetItemDescription()->SetVisibility(ESlateVisibility::HitTestInvisible);
		Manifest.AssimilateInventoryFragments(DescriptionWidget);
		
		// For the second item description, showing the equipped item of this type.
		FTimerDelegate EquippedDescriptionTimerDelegate;
		EquippedDescriptionTimerDelegate.BindUObject(this, &ThisClass::ShowEquippedItemDescription, Item);
		GetOwningPlayer()->GetWorldTimerManager().SetTimer(EquippedDescriptionTimer, EquippedDescriptionTimerDelegate, EquippedDescriptionTimerDelay, false);
	});

	GetOwningPlayer()->GetWorldTimerManager().SetTimer(DescriptionTimer, DescriptionTimerDelegate, DescriptionTimerDelay, false);
}

void UAZ_Inv_SpatialInventoryMenu::OnItemUnHovered()
{
	GetItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	GetEquippedItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);
}

bool UAZ_Inv_SpatialInventoryMenu::HasHoverItem() const
{
	if (Grid_Equippables->HasHoverItem()) return true;
	if (Grid_Consumables->HasHoverItem()) return true;
	if (Grid_Craftables->HasHoverItem()) return true;
	return false;
}

UAZ_Inv_HoverItem* UAZ_Inv_SpatialInventoryMenu::GetHoverItem() const
{
	if (!ActiveGrid.IsValid()) return nullptr;
	return ActiveGrid->GetHoverItem();
}

float UAZ_Inv_SpatialInventoryMenu::GetTileSize() const
{
	return Grid_Equippables->GetTileSize();
}

FAZ_Inv_SlotAvailabilityResult UAZ_Inv_SpatialInventoryMenu::HasRoomForItem(UAZ_Inv_ItemComponent* ItemComponent) const
{
	switch (UAZ_Inv_InventoryStatics::GetItemCategoryFromItemComp(ItemComponent))
	{
		case EInv_ItemCategory::Equippable:
			return Grid_Equippables->HasRoomForItem(ItemComponent);
		
		case EInv_ItemCategory::Consumable:
			return Grid_Consumables->HasRoomForItem(ItemComponent);
		
		case EInv_ItemCategory::Craftable:
			return Grid_Craftables->HasRoomForItem(ItemComponent);
		
		default:
			UE_LOG(Log_AZ, Warning, TEXT("HasRoomForItem encountered invalid item category for item: %s"), *GetNameSafe(ItemComponent));
			return FAZ_Inv_SlotAvailabilityResult();
	}
}

void UAZ_Inv_SpatialInventoryMenu::ShowEquippables()
{
	SetActiveGrid(Grid_Equippables, Button_Equippable);
}

void UAZ_Inv_SpatialInventoryMenu::ShowConsumables()
{
	SetActiveGrid(Grid_Consumables, Button_Consumable);
}

void UAZ_Inv_SpatialInventoryMenu::ShowCraftables()
{
	SetActiveGrid(Grid_Craftables, Button_Craftable);
}

void UAZ_Inv_SpatialInventoryMenu::DisableButton(UButton* Button)
{
	Button_Equippable->SetIsEnabled(true);
	Button_Consumable->SetIsEnabled(true);
	Button_Craftable->SetIsEnabled(true);
	Button->SetIsEnabled(false);
}

void UAZ_Inv_SpatialInventoryMenu::ShowEquippedItemDescription(UAZ_Inv_InventoryItem* Item)
{
	const auto& Manifest = Item->GetItemManifest();
	const FAZ_Inv_EquipmentFragment* EquipmentFragment = Manifest.GetFragmentOfType<FAZ_Inv_EquipmentFragment>();
	if (!EquipmentFragment) return;

	const FGameplayTag HoveredEquipmentType = EquipmentFragment->GetEquipmentType();
	
	auto EquippedGridSlot = EquippedGridSlots.FindByPredicate([Item](const UAZ_Inv_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == Item;
	});
	if (EquippedGridSlot != nullptr) return; // The hovered item is already equipped, we're already showing its Item Description

	// It's not equipped, so find the equipped item with the same equipment type
	auto FoundEquippedSlot = EquippedGridSlots.FindByPredicate([HoveredEquipmentType](const UAZ_Inv_EquippedGridSlot* GridSlot)
	{
		UAZ_Inv_InventoryItem* InventoryItem = GridSlot->GetInventoryItem().Get();
		return IsValid(InventoryItem) ? InventoryItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_EquipmentFragment>()->GetEquipmentType() == HoveredEquipmentType : false ;
	});
	UAZ_Inv_EquippedGridSlot* EquippedSlot = FoundEquippedSlot ? *FoundEquippedSlot : nullptr;
	if (!IsValid(EquippedSlot)) return; // No equipped item with the same equipment type

	UAZ_Inv_InventoryItem* EquippedItem = EquippedSlot->GetInventoryItem().Get();
	if (!IsValid(EquippedItem)) return;

	const auto& EquippedItemManifest = EquippedItem->GetItemManifest();
	UAZ_Inv_ItemDescription* DescriptionWidget = GetEquippedItemDescription();

	auto EquippedDescriptionWidget = GetEquippedItemDescription();
	
	EquippedDescriptionWidget->Collapse();
	DescriptionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);	
	EquippedItemManifest.AssimilateInventoryFragments(EquippedDescriptionWidget);
}

UAZ_Inv_ItemDescription* UAZ_Inv_SpatialInventoryMenu::GetItemDescription()
{
	if (!IsValid(ItemDescription))
	{
		ItemDescription = CreateWidget<UAZ_Inv_ItemDescription>(GetOwningPlayer(), ItemDescriptionClass);
		CanvasPanel->AddChild(ItemDescription);
	}
	return ItemDescription;
}

UAZ_Inv_ItemDescription* UAZ_Inv_SpatialInventoryMenu::GetEquippedItemDescription()
{
	if (!IsValid(EquippedItemDescription))
	{
		EquippedItemDescription = CreateWidget<UAZ_Inv_ItemDescription>(GetOwningPlayer(), EquippedItemDescriptionClass);
		CanvasPanel->AddChild(EquippedItemDescription);
	}
	return EquippedItemDescription;
}

void UAZ_Inv_SpatialInventoryMenu::EquippedGridSlotClicked(UAZ_Inv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag)
{
	// Check to see if we can equip the Hover Item
	if (!CanEquipHoverItem(EquippedGridSlot, EquipmentTypeTag)) return;

	UAZ_Inv_HoverItem* HoverItem = GetHoverItem();

	// Create an Equipped Slotted Item and add it to the Equipped Grid Slot (call EquippedGridSlot->OnItemEquipped())
	const float TileSize = UAZ_Inv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem = EquippedGridSlot->OnItemEquipped(
		HoverItem->GetInventoryItem(),
		EquipmentTypeTag,
		TileSize
	);
	EquippedSlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::EquippedSlottedItemClicked);

	// Inform the server that we've equipped an item (potentially unequipping an item as well)
	UAZ_Inv_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent)); 

	InventoryComponent->Server_EquipSlotClicked(HoverItem->GetInventoryItem(), nullptr);
	
	// Clear the Hover Item
	Grid_Equippables->ClearHoverItem();
}

void UAZ_Inv_SpatialInventoryMenu::EquippedSlottedItemClicked(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem)
{
	// Remove the Item Description
	UAZ_Inv_InventoryStatics::ItemUnhovered(GetOwningPlayer());

	if (IsValid(GetHoverItem()) && GetHoverItem()->IsStackable()) return;

	// Get Item to Equip
	UAZ_Inv_InventoryItem* ItemToEquip = IsValid(GetHoverItem()) ? GetHoverItem()->GetInventoryItem() : nullptr;

	// Get Item to Unequip
	UAZ_Inv_InventoryItem* ItemToUnequip = EquippedSlottedItem->GetInventoryItem();

	// Get the Equipped Grid Slot holding this item
	UAZ_Inv_EquippedGridSlot* EquippedGridSlot = FindSlotWithEquippedItem(ItemToUnequip);
	
	// Clear the equipped grid slot of this item (set its inventory item to nullptr)
	ClearSlotOfItem(EquippedGridSlot);

	// Assign previously equipped item as the hover item
	Grid_Equippables->AssignHoverItem(ItemToUnequip);
	
	// Remove of the equipped slotted item from the equipped grid slot
	RemoveEquippedSlottedItem(EquippedSlottedItem);
	
	// Make a new equipped slotted item (for the item we held in HoverItem)
	MakeEquippedSlottedItem(EquippedSlottedItem, EquippedGridSlot, ItemToEquip);
	
	// Broadcast delegates for OnItemEquipped/OnItemUnequipped (from the IC)
	BroadcastSlotClickedDelegates(ItemToEquip, ItemToUnequip);
}

void UAZ_Inv_SpatialInventoryMenu::SetItemDescriptionSizeAndPosition(UAZ_Inv_ItemDescription* Description, UCanvasPanel* Canvas) const
{
	UCanvasPanelSlot* ItemDescriptionCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(Description);
	if (!IsValid(ItemDescriptionCPS)) return;

	const FVector2D ItemDescriptionSize = Description->GetBoxSize();
	ItemDescriptionCPS->SetSize(ItemDescriptionSize);

	FVector2D ClampedPosition = UAZ_Inv_WidgetUtils::GetClampedWidgetPosition(
		UAZ_Inv_WidgetUtils::GetWidgetSize(Canvas),
		ItemDescriptionSize,
		UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer()));

	ItemDescriptionCPS->SetPosition(ClampedPosition);
}

void UAZ_Inv_SpatialInventoryMenu::SetEquippedItemDescriptionSizeAndPosition(UAZ_Inv_ItemDescription* Description,
	UAZ_Inv_ItemDescription* EquippedDescription, UCanvasPanel* Canvas) const
{
	UCanvasPanelSlot* ItemDescriptionCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(Description);
	UCanvasPanelSlot* EquippedItemDescriptionCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(EquippedDescription);
	if (!IsValid(ItemDescriptionCPS) || !IsValid(EquippedItemDescriptionCPS)) return;

	const FVector2D ItemDescriptionSize = Description->GetBoxSize();
	const FVector2D EquippedItemDescriptionSize = EquippedDescription->GetBoxSize();

	FVector2D ClampedPosition = UAZ_Inv_WidgetUtils::GetClampedWidgetPosition(
		UAZ_Inv_WidgetUtils::GetWidgetSize(Canvas),
		ItemDescriptionSize,
		UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer()));
	ClampedPosition.X -= EquippedItemDescriptionSize.X;

	EquippedItemDescriptionCPS->SetSize(EquippedItemDescriptionSize);
	EquippedItemDescriptionCPS->SetPosition(ClampedPosition);
}

bool UAZ_Inv_SpatialInventoryMenu::CanEquipHoverItem(UAZ_Inv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag) const
{
	if (!IsValid(EquippedGridSlot) || EquippedGridSlot->GetInventoryItem().IsValid()) return false;

	UAZ_Inv_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem)) return false;

	UAZ_Inv_InventoryItem* HeldItem = HoverItem->GetInventoryItem();


	const bool bHasHoverItem = HasHoverItem();
	const bool bHeldItemValid = IsValid(HeldItem);
	const bool bNotStackable = !HoverItem->IsStackable();
	const bool bIsEquippable = HeldItem && HeldItem->GetItemManifest().GetItemCategory() == EInv_ItemCategory::Equippable;
	const bool bMatchesType = HeldItem && HeldItem->GetItemManifest().GetItemTypeTag().MatchesTag(EquipmentTypeTag);

	return bHasHoverItem &&
		bHeldItemValid &&
		bNotStackable &&
		bIsEquippable &&
		bMatchesType;
}

UAZ_Inv_EquippedGridSlot* UAZ_Inv_SpatialInventoryMenu::FindSlotWithEquippedItem(UAZ_Inv_InventoryItem* EquippedItem) const
{
	auto* FoundEquippedGridSlot = EquippedGridSlots.FindByPredicate([EquippedItem](const UAZ_Inv_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == EquippedItem;
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

void UAZ_Inv_SpatialInventoryMenu::ClearSlotOfItem(UAZ_Inv_EquippedGridSlot* EquippedGridSlot)
{
	if (IsValid(EquippedGridSlot))
	{
		EquippedGridSlot->SetEquippedSlottedItem(nullptr);
		EquippedGridSlot->SetInventoryItem(nullptr);
	}
}

void UAZ_Inv_SpatialInventoryMenu::RemoveEquippedSlottedItem(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem)
{
	if (!IsValid(EquippedSlottedItem)) return;

	if (EquippedSlottedItem->OnEquippedSlottedItemClicked.IsAlreadyBound(this, &ThisClass::EquippedSlottedItemClicked))
	{
		EquippedSlottedItem->OnEquippedSlottedItemClicked.RemoveDynamic(this, &ThisClass::EquippedSlottedItemClicked);
	}
	EquippedSlottedItem->RemoveFromParent();
}

void UAZ_Inv_SpatialInventoryMenu::MakeEquippedSlottedItem(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem,
	UAZ_Inv_EquippedGridSlot* EquippedGridSlot, UAZ_Inv_InventoryItem* ItemToEquip)
{
	if (!IsValid(EquippedGridSlot)) return;

	UAZ_Inv_EquippedSlottedItem* SlottedItem = EquippedGridSlot->OnItemEquipped(
		ItemToEquip,
		EquippedSlottedItem->GetEquipmentTypeTag(),
		UAZ_Inv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize());
	if (IsValid(SlottedItem)) SlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::EquippedSlottedItemClicked);

	EquippedGridSlot->SetEquippedSlottedItem(SlottedItem);
}

void UAZ_Inv_SpatialInventoryMenu::BroadcastSlotClickedDelegates(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip) const
{
	UAZ_Inv_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent));
	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UAZ_Inv_SpatialInventoryMenu::SetActiveGrid(UAZ_Inv_InventoryGrid* Grid, UButton* Button)
{
	if (ActiveGrid.IsValid())
	{
		ActiveGrid->HideCursor();
		ActiveGrid->OnHide();
	}
	ActiveGrid = Grid;
	if (ActiveGrid.IsValid()) ActiveGrid->ShowCursor();
	DisableButton(Button);
	Switcher->SetActiveWidget(Grid);
}

TMap<UAZ_Inv_InventoryGrid*, UButton*> UAZ_Inv_SpatialInventoryMenu::GetGridButtonMap() const
{
	TMap<UAZ_Inv_InventoryGrid*, UButton*> Map;

	if (Grid_Equippables && Button_Equippable) Map.Add(Grid_Equippables, Button_Equippable);
	if (Grid_Consumables && Button_Consumable) Map.Add(Grid_Consumables, Button_Consumable);
	if (Grid_Craftables && Button_Craftable) Map.Add(Grid_Craftables, Button_Craftable);

	return Map;
}
