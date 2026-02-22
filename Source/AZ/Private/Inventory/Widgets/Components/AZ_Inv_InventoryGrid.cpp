#include "Inventory/Widgets/Components/AZ_Inv_InventoryGrid.h"

#include "AZ_GameplayTags.h"
#include "AZ/AZ.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Items/AZ_Inv_InventoryItem.h"
#include "Inventory/Items/Fragments/AZ_Inv_ItemFragment.h"
#include "Inventory/Widgets/Components/AZ_Inv_GridSlot.h"
#include "Inventory/Widgets/ItemPopUp/AZ_Inv_ItemPopUp.h"
#include "Inventory/Widgets/SlottedItems/AZ_Inv_SlottedItem.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"
#include "Inventory/Widgets/Utils/AZ_Inv_WidgetUtils.h"

void UAZ_Inv_InventoryGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConstructGrid();

	InventoryComponent = UAZ_Inv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	ensure(InventoryComponent.IsValid());

	InventoryComponent->OnItemAdded.AddDynamic(this, &ThisClass::AddItem);
	InventoryComponent->OnStackChange.AddDynamic(this, &ThisClass::AddStacks);
}

void UAZ_Inv_InventoryGrid::AddStacks(const FAZ_Inv_SlotAvailabilityResult& Result)
{
	if (!MatchesCategory(Result.Item.Get())) return;

	for (const auto& Availability : Result.AvailableSlots)
	{
		if (Availability.bItemAtIndex)
		{
			const auto& GridSlot = GridSlots[Availability.Index];
			const auto& SlottedItem = SlottedItems.FindChecked(Availability.Index);
			SlottedItem->UpdateStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
			GridSlot->SetStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
		}
		else
		{
			AddItemAtIndex(Result.Item.Get(), Availability.Index, Result.bIsStackable, Availability.AmountToFill);
			UpdateGridSlots(Result.Item.Get(), Availability.Index, Result.bIsStackable, Availability.AmountToFill);
		}
	}
}

void UAZ_Inv_InventoryGrid::OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	UAZ_Inv_InventoryStatics::ItemUnhovered(GetOwningPlayer());	
	check(GridSlots.IsValidIndex(GridIndex));
	
	auto* ClickedInventoryItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	
	if (!IsValid(HoverItem) && IsLeftClick(MouseEvent))
	{
		PickUp(ClickedInventoryItem, GridIndex);
		return;
	}

	if (IsRightClick(MouseEvent))
	{
		CreateItemPopUp(GridIndex);
		return;
	}
	
	// Do the hovered item and the clicked inventory item share a type, and are they stackable?
	if (IsSameStackable(ClickedInventoryItem))
	{
		const int32 ClickedStackCount = GridSlots[GridIndex]->GetStackCount();
		const FAZ_Inv_StackableFragment* StackableFragment = ClickedInventoryItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_StackableFragment>();
		const int32 MaxStackSize = StackableFragment->GetMaxStackSize();
		const int32 RoomInClickedSlot = MaxStackSize - ClickedStackCount;
		const int32 HoveredStackCount = HoverItem->GetStackCount();
		
		// Should we swap their stack counts? (Room in the clicked slot == 0 && HoveredStackCount < MaxStackSize)
		if (ShouldSwapStackCounts(RoomInClickedSlot, HoveredStackCount, MaxStackSize))
		{
			SwapStackCounts(ClickedStackCount, HoveredStackCount, GridIndex);
			return;
		}
		
		// Should we consume the hover item's stacks? (Room in the clicked slot >= HoveredStackCount)
		if (ShouldConsumeHoverItemStacks(HoveredStackCount, RoomInClickedSlot))
		{
			ConsumeHoverItemStacks(ClickedStackCount, HoveredStackCount, GridIndex);
			return;
		}
		
		// Should we fill in the stacks of the clicked item? (and not consume the hover item)
		if (ShouldFillInStack(RoomInClickedSlot, HoveredStackCount))
		{
			FillInStack(RoomInClickedSlot, HoveredStackCount - RoomInClickedSlot, GridIndex);
			return;
		}
		
		// Clicked slot is already full - do nothing (maybe play a sound?)
		if (RoomInClickedSlot == 0)
		{
			return;
		}
	}
	
	// Make sure wee can swap with a valid item 
	if (CurrentQueryResult.ValidItem.IsValid())
	{
		// Swap with the hover item.
		SwapWithHoverItem(ClickedInventoryItem, GridIndex);
	}
}

void UAZ_Inv_InventoryGrid::SwapWithHoverItem(UAZ_Inv_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	if (!IsValid(HoverItem)) return;

	UAZ_Inv_InventoryItem* TempInventoryItem = HoverItem->GetInventoryItem();
	const int32 TempStackCount = HoverItem->GetStackCount();
	const bool bTempIsStackable = HoverItem->IsStackable();

	// Keep the same previous grid index
	AssignHoverItem(ClickedInventoryItem, GridIndex, HoverItem->GetPreviousGridIndex());
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
	AddItemAtIndex(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
	UpdateGridSlots(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
}

void UAZ_Inv_InventoryGrid::FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index)
{
	UAZ_Inv_GridSlot* GridSlot = GridSlots[Index];
	const int32 NewStackCount = GridSlot->GetStackCount() + FillAmount;

	GridSlot->SetStackCount(NewStackCount);

	UAZ_Inv_SlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpdateStackCount(NewStackCount);

	HoverItem->UpdateStackCount(Remainder);
}

bool UAZ_Inv_InventoryGrid::ShouldFillInStack(const int32 RoomInClickedSlot, const int32 HoveredStackCount) const
{
	return RoomInClickedSlot < HoveredStackCount;
}

void UAZ_Inv_InventoryGrid::ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index)
{
	const int32 AmountToTransfer = HoveredStackCount;
	const int32 NewClickedStackCount = ClickedStackCount + AmountToTransfer;

	GridSlots[Index]->SetStackCount(NewClickedStackCount);
	SlottedItems.FindChecked(Index)->UpdateStackCount(NewClickedStackCount);
	ClearHoverItem();
	ShowCursor();

	const FAZ_Inv_GridFragment* GridFragment = GridSlots[Index]->GetInventoryItem()->GetItemManifest().GetFragmentOfType<FAZ_Inv_GridFragment>();
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
	HighlightSlots(Index, Dimensions);
}

bool UAZ_Inv_InventoryGrid::ShouldConsumeHoverItemStacks(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const
{
	return RoomInClickedSlot >= HoveredStackCount;
}

void UAZ_Inv_InventoryGrid::SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index)
{
	UAZ_Inv_GridSlot* GridSlot = GridSlots[Index];
	GridSlot->SetStackCount(HoveredStackCount);

	const auto ClickedItem = SlottedItems.FindChecked(Index);
	UAZ_Inv_SlottedItem* ClickedSlottedItem = ClickedItem;
	ClickedSlottedItem->UpdateStackCount(HoveredStackCount);

	HoverItem->UpdateStackCount(ClickedStackCount);
}

bool UAZ_Inv_InventoryGrid::ShouldSwapStackCounts(const int32 RoomInClickedSlot, const int32 HoveredStackCount, const int32 MaxStackSize) const
{
	return RoomInClickedSlot == 0 && HoveredStackCount < MaxStackSize;
}

bool UAZ_Inv_InventoryGrid::IsSameStackable(const UAZ_Inv_InventoryItem* ClickedInventoryItem) const
{
	const bool bIsSameItem = ClickedInventoryItem == HoverItem->GetInventoryItem();
	const bool bIsStackable = ClickedInventoryItem->IsStackable();
	return bIsSameItem && bIsStackable && HoverItem->GetItemType().MatchesTagExact(ClickedInventoryItem->GetItemManifest().GetItemTypeTag());
}

void UAZ_Inv_InventoryGrid::OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (!IsValid(HoverItem)) return;
	if (!GridSlots.IsValidIndex(ItemDropIndex)) return;

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		OnSlottedItemClicked(CurrentQueryResult.UpperLeftIndex, MouseEvent);
		return;
	}

	if (!IsInGridBounds(ItemDropIndex, HoverItem->GetGridDimensions())) return;
	auto GridSlot = GridSlots[ItemDropIndex];
	if (!GridSlot->GetInventoryItem().IsValid())
	{
		PutDownOnIndex(ItemDropIndex);
	}
}

void UAZ_Inv_InventoryGrid::OnPopUpMenuSplit(int32 SplitAmount, int32 Index)
{
	UAZ_Inv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (!RightClickedItem->IsStackable()) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UAZ_Inv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = UpperLeftGridSlot->GetStackCount();
	const int32 NewStackCount = StackCount - SplitAmount;

	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	AssignHoverItem(RightClickedItem, UpperLeftIndex, UpperLeftIndex);
	HoverItem->UpdateStackCount(SplitAmount);
}

void UAZ_Inv_InventoryGrid::OnPopUpMenuConsume(int32 Index)
{
	UAZ_Inv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UAZ_Inv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 NewStackCount = UpperLeftGridSlot->GetStackCount() - 1;

	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	InventoryComponent->Server_ConsumeItem(RightClickedItem);

	if (NewStackCount <= 0)
	{
		RemoveItemFromGrid(RightClickedItem, Index);
	}
}

void UAZ_Inv_InventoryGrid::OnPopUpMenuDrop(int32 Index)
{
	UAZ_Inv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	PickUp(RightClickedItem, Index);
	DropItem();
}

void UAZ_Inv_InventoryGrid::PutDownOnIndex(const int32 Index)
{
	AddItemAtIndex(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	UpdateGridSlots(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	ClearHoverItem();
}

void UAZ_Inv_InventoryGrid::ClearHoverItem()
{
	if (!IsValid(HoverItem)) return;

	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetIsStackable(false);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->UpdateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());

	HoverItem->RemoveFromParent();
	HoverItem = nullptr;

	ShowCursor();
}

bool UAZ_Inv_InventoryGrid::HasHoverItem() const
{
	return IsValid(HoverItem);
}

UAZ_Inv_HoverItem* UAZ_Inv_InventoryGrid::GetHoverItem() const
{
	return HoverItem;
}

void UAZ_Inv_InventoryGrid::ShowCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetVisibleCursorWidget());
}

void UAZ_Inv_InventoryGrid::HideCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetHiddenCursorWidget());
}

void UAZ_Inv_InventoryGrid::OnHide()
{
	PutHoverItemBack();
}

void UAZ_Inv_InventoryGrid::DropItem()
{
	if (!IsValid(HoverItem)) return;
	if (!IsValid(HoverItem->GetInventoryItem())) return;

	InventoryComponent->Server_DropItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());

	ClearHoverItem();
	ShowCursor();
}

void UAZ_Inv_InventoryGrid::PutHoverItemBack()
{
	if (!IsValid(HoverItem)) return;

	FAZ_Inv_SlotAvailabilityResult Result = HasRoomForItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());
	Result.Item = HoverItem->GetInventoryItem();

	AddStacks(Result);
	ClearHoverItem();
}

UUserWidget* UAZ_Inv_InventoryGrid::GetVisibleCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(VisibleCursorWidget))
	{
		VisibleCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), VisibleCursorWidgetClass);
	}
	return VisibleCursorWidget;
}

UUserWidget* UAZ_Inv_InventoryGrid::GetHiddenCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(HiddenCursorWidget))
	{
		HiddenCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), HiddenCursorWidgetClass);
	}
	return HiddenCursorWidget;
}

void UAZ_Inv_InventoryGrid::OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UAZ_Inv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_GridSlotState::Occupied);
		GridSlot->SetOccupiedTexture();
	}
}

void UAZ_Inv_InventoryGrid::OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UAZ_Inv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_GridSlotState::Unoccupied);
		GridSlot->SetUnoccupiedTexture();
	}
}

bool UAZ_Inv_InventoryGrid::CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location)
{
	bLastMouseWithinCanvas = bMouseWithinCanvas;
	bMouseWithinCanvas = UAZ_Inv_WidgetUtils::IsWithinBounds(BoundaryPos, BoundarySize, Location);
	if (!bMouseWithinCanvas && bLastMouseWithinCanvas)
	{
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
		return true;
	}
	return false;
}

void UAZ_Inv_InventoryGrid::ConstructGrid()
{
	GridSlots.Reserve(Rows * Columns);

	for (int32 j = 0; j < Rows; ++j)
	{
		for (int32 i = 0; i < Columns; ++i)
		{
			UAZ_Inv_GridSlot* GridSlot = CreateWidget<UAZ_Inv_GridSlot>(this, GridSlotClass);
			CanvasPanel->AddChild(GridSlot);

			const FIntPoint TilePosition = FIntPoint(i, j);
			GridSlot->SetIndex(UAZ_Inv_WidgetUtils::GetIndexFromPosition(TilePosition, Columns));

			UCanvasPanelSlot* GridCanvasPanelSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(GridSlot);
			GridCanvasPanelSlot->SetSize(FVector2D(TileSize));
			GridCanvasPanelSlot->SetPosition(FIntPoint(TilePosition * TileSize));
			GridSlots.Add(GridSlot);
			
			GridSlot->GridSlotClicked.AddDynamic(this, &ThisClass::OnGridSlotClicked);
			GridSlot->GridSlotHovered.AddDynamic(this, &ThisClass::OnGridSlotHovered);
			GridSlot->GridSlotUnhovered.AddDynamic(this, &ThisClass::OnGridSlotUnhovered);
		}
	}
}

void UAZ_Inv_InventoryGrid::AddItem(UAZ_Inv_InventoryItem* Item)
{
	if (!MatchesCategory(Item)) return;

	FAZ_Inv_SlotAvailabilityResult Result = HasRoomForItem(Item);
	AddItemToGridSlots(Result, Item);
}

FVector2D UAZ_Inv_InventoryGrid::GetDrawSize(const FAZ_Inv_GridFragment* GridFragment) const
{
	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	return GridFragment->GetGridSize() * IconTileWidth;
}

void UAZ_Inv_InventoryGrid::SetSlottedItemImage(const FAZ_Inv_GridFragment* GridFragment, const FAZ_Inv_ImageFragment* ImageFragment, const UAZ_Inv_SlottedItem* SlottedItem)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Type::Image;
	Brush.ImageSize = GetDrawSize(GridFragment);
	SlottedItem->SetImageBrush(Brush);
}

UAZ_Inv_SlottedItem* UAZ_Inv_InventoryGrid::CreateSlottedItem(UAZ_Inv_InventoryItem* NewItem, const FAZ_Inv_GridFragment* GridFragment, const FAZ_Inv_ImageFragment* ImageFragment,
                                                              const int32 Index, const bool bStackable, const int32
                                                              StackAmount)
{
	UAZ_Inv_SlottedItem* SlottedItem = CreateWidget<UAZ_Inv_SlottedItem>(GetOwningPlayer(), SlottedItemClass);
	SlottedItem->SetInventoryItem(NewItem);
	SetSlottedItemImage(GridFragment, ImageFragment, SlottedItem);
	SlottedItem->SetGridIndex(Index);
	SlottedItem->SetIsStackable(bStackable);
	const int32 StackUpdateAmount = StackAmount > 0 ? StackAmount : 0;
	SlottedItem->UpdateStackCount(StackUpdateAmount);
	SlottedItem->OnSlottedItemClickedEvent.AddDynamic(this, &ThisClass::OnSlottedItemClicked);

	return SlottedItem;
}

void UAZ_Inv_InventoryGrid::AddItemAtIndex(UAZ_Inv_InventoryItem* NewItem, const int32 Index, const bool bStackable, const int32 StackAmount)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_GridFragment* GridFragment = GetFragment<FAZ_Inv_GridFragment>(NewItem, Tags.Item_Fragment_Grid);
	const FAZ_Inv_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_ImageFragment>(NewItem, Tags.Item_Fragment_Icon);
	if (!GridFragment || !ImageFragment) return;

	UAZ_Inv_SlottedItem* SlottedItem = CreateSlottedItem(NewItem, GridFragment, ImageFragment, Index, bStackable, StackAmount);
	AddSlottedItemToCanvas(Index, GridFragment, SlottedItem);

	SlottedItems.Add(Index, SlottedItem);
}

void UAZ_Inv_InventoryGrid::AddSlottedItemToCanvas(const int32 Index, const FAZ_Inv_GridFragment* GridFragment, UAZ_Inv_SlottedItem* SlottedItem) const
{
	CanvasPanel->AddChild(SlottedItem);
	UCanvasPanelSlot* CanvasPanelSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(SlottedItem);
	CanvasPanelSlot->SetSize(GetDrawSize(GridFragment));
	const FVector2D DrowPos = UAZ_Inv_WidgetUtils::GetPositionFromIndex(Index, Columns) * TileSize;
	const FVector2D DrowPosWithPAdding = DrowPos + FVector2D(GridFragment->GetGridPadding());
	CanvasPanelSlot->SetPosition(DrowPosWithPAdding);
}

void UAZ_Inv_InventoryGrid::UpdateGridSlots(UAZ_Inv_InventoryItem* NewItem, const int32 Index, bool bStackableItem, const int32 StackAmount)
{
	check(GridSlots.IsValidIndex(Index));

	if (bStackableItem)
	{
		GridSlots[Index]->SetStackCount(StackAmount);
	}

	const FAZ_GameplayTags Tags = FAZ_GameplayTags::Get();
	const auto GridFragment = GetFragment<FAZ_Inv_GridFragment>(NewItem, Tags.Item_Fragment_Grid);
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);

	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UAZ_Inv_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(NewItem);
		GridSlot->SetUpperLeftIndex(Index);
		GridSlot->SetState(EInv_GridSlotState::Occupied);
		GridSlot->SetOccupiedTexture();
		GridSlot->SetAvailable(false);
	});
}

bool UAZ_Inv_InventoryGrid::IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const
{
	return CheckedIndices.Contains(Index);
}

bool UAZ_Inv_InventoryGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num()) return false;
	const int32 EndColumn = (StartIndex % Columns) + ItemDimensions.X;
	const int32 EndRow = (StartIndex / Columns) + ItemDimensions.Y;
	return EndColumn <= Columns && EndRow <= Rows;
}

bool UAZ_Inv_InventoryGrid::IsUpperLeftSlot(const UAZ_Inv_GridSlot* GridSlot, const UAZ_Inv_GridSlot* SubGridSlot) const
{
	return SubGridSlot->GetUpperLeftIndex() == GridSlot->GetIndex();
}

bool UAZ_Inv_InventoryGrid::DoesItemTypeMatch(const UAZ_Inv_InventoryItem* SubItem, const FGameplayTag& ItemType) const
{
	return SubItem->GetItemManifest().GetItemTypeTag().MatchesTagExact(ItemType);
}

int32 UAZ_Inv_InventoryGrid::DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill, const UAZ_Inv_GridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackSize - GetStackAmount(GridSlot);
	return bStackable ? FMath::Min(AmountToFill, RoomInSlot) : 1;
}

int32 UAZ_Inv_InventoryGrid::GetStackAmount(const UAZ_Inv_GridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	// If we are at a slot that doesn't hold the stack count. we must get the actual stack count.
	if (const int32 UpperLeftIndex = GridSlot->GetUpperLeftIndex(); UpperLeftIndex != INDEX_NONE)
	{
		const UAZ_Inv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftGridSlot->GetStackCount();
	}
	return CurrentSlotStackCount;
}

bool UAZ_Inv_InventoryGrid::IsRightClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
}

bool UAZ_Inv_InventoryGrid::IsLeftClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
}

void UAZ_Inv_InventoryGrid::PickUp(UAZ_Inv_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	AssignHoverItem(ClickedInventoryItem, GridIndex, GridIndex);
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
}

void UAZ_Inv_InventoryGrid::AssignHoverItem(UAZ_Inv_InventoryItem* InventoryItem, const int32 GridIndex, const int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);

	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpdateStackCount(InventoryItem->IsStackable() ? GridSlots[GridIndex]->GetStackCount() : 0);
}

void UAZ_Inv_InventoryGrid::AssignHoverItem(UAZ_Inv_InventoryItem* InventoryItem)
{
	if (!IsValid(HoverItem))
	{
		HoverItem = CreateWidget<UAZ_Inv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	}

	const FAZ_Inv_GridFragment* GridFragment = GetFragment<FAZ_Inv_GridFragment>(InventoryItem,  FAZ_GameplayTags::Get().Item_Fragment_Grid);
	const FAZ_Inv_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_ImageFragment>(InventoryItem,  FAZ_GameplayTags::Get().Item_Fragment_Icon);
	if (!GridFragment || !ImageFragment) return;

	const FVector2D DrawSize = GetDrawSize(GridFragment);

	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(ImageFragment->GetIcon());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.ImageSize = DrawSize * UWidgetLayoutLibrary::GetViewportScale(this);

	HoverItem->SetImageBrush(IconBrush);
	HoverItem->SetGridDimensions(GridFragment->GetGridSize());
	HoverItem->SetInventoryItem(InventoryItem);
	HoverItem->SetIsStackable(InventoryItem->IsStackable());

	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, HoverItem);
}

void UAZ_Inv_InventoryGrid::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	OwningCanvasPanel = OwningCanvas;
}

void UAZ_Inv_InventoryGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	const FVector2D CanvasPosition = UAZ_Inv_WidgetUtils::GetWidgetPosition(CanvasPanel);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	
	if (CursorExitedCanvas(CanvasPosition, UAZ_Inv_WidgetUtils::GetWidgetSize(CanvasPanel), MousePosition))
	{
		return;
	}

	UpdateTileParameters(CanvasPosition, MousePosition);
}

void UAZ_Inv_InventoryGrid::UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition)
{
	if (!bMouseWithinCanvas) return;
	
	// Calculate the tile quadrant, tile index, and coordinates
	const FIntPoint HoveredTileCoordinates = CalculateHoveredCoordinates(CanvasPosition, MousePosition);
	
	LastTileParameters = TileParameters;
	TileParameters.TileCoordinats = HoveredTileCoordinates;
	TileParameters.TileIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(HoveredTileCoordinates, Columns);
	TileParameters.TileQuadrant = CalculateTileQuadrant(CanvasPosition, MousePosition);
	
	OnTileParametersUpdated(TileParameters);
}

void UAZ_Inv_InventoryGrid::OnTileParametersUpdated(const FAZ_Inv_TileParameters& Parameters)
{
	if (!IsValid(HoverItem)) return;

	// Get Hover Item's dimensions
	const FIntPoint Dimensions = HoverItem->GetGridDimensions();
	
	// calculate the starting coordinate for highlighting
	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinats, Dimensions, Parameters.TileQuadrant);
	ItemDropIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(StartingCoordinate, Columns);
	
	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);

	if (CurrentQueryResult.bHasSpace)
	{
		HighlightSlots(ItemDropIndex, Dimensions);
		return;
	}
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		const FAZ_Inv_GridFragment* GridFragment = GetFragment<FAZ_Inv_GridFragment>(CurrentQueryResult.ValidItem.Get(), FAZ_GameplayTags::Get().Item_Fragment_Grid);
		if (!GridFragment) return;

		ChangeHoverType(CurrentQueryResult.UpperLeftIndex, GridFragment->GetGridSize(), EInv_GridSlotState::GrayedOut);
	}
}

void UAZ_Inv_InventoryGrid::RemoveItemFromGrid(UAZ_Inv_InventoryItem* InventoryItem, const int32 GridIndex)
{
	const FAZ_Inv_GridFragment* GridFragment = GetFragment<FAZ_Inv_GridFragment>(InventoryItem, FAZ_GameplayTags::Get().Item_Fragment_Grid);
	if (!GridFragment) return;

	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, GridIndex, GridFragment->GetGridSize(), Columns, [&](UAZ_Inv_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetState(EInv_GridSlotState::Unoccupied);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
	});

	if (SlottedItems.Contains(GridIndex))
	{
		TObjectPtr<UAZ_Inv_SlottedItem> FoundSlottedItem;
		SlottedItems.RemoveAndCopyValue(GridIndex, FoundSlottedItem);
		FoundSlottedItem->RemoveFromParent();
	}
}

void UAZ_Inv_InventoryGrid::CreateItemPopUp(const int32 GridIndex)
{
	UAZ_Inv_InventoryItem* RightClickedItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (IsValid(GridSlots[GridIndex]->GetItemPopUp())) return;

	ItemPopUp = CreateWidget<UAZ_Inv_ItemPopUp>(this, ItemPopUpClass);
	GridSlots[GridIndex]->SetItemPopUp(ItemPopUp);

	OwningCanvasPanel->AddChild(ItemPopUp);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ItemPopUp);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	CanvasSlot->SetPosition(MousePosition - ItemPopUpOffset);
	CanvasSlot->SetSize(ItemPopUp->GetBoxSize());

	const int32 SliderMax = GridSlots[GridIndex]->GetStackCount() - 1;
	if (RightClickedItem->IsStackable() && SliderMax > 0)
	{
		ItemPopUp->OnSplit.BindDynamic(this, &ThisClass::OnPopUpMenuSplit);
		ItemPopUp->SetSliderParams(SliderMax, FMath::Max(1, GridSlots[GridIndex]->GetStackCount() / 2));
	}
	else
	{
		ItemPopUp->CollapseSplitButton();
	}

	ItemPopUp->OnDrop.BindDynamic(this, &ThisClass::OnPopUpMenuDrop);

	if (RightClickedItem->IsConsumable())
	{
		ItemPopUp->OnConsume.BindDynamic(this, &ThisClass::OnPopUpMenuConsume);
	}
	else
	{
		ItemPopUp->CollapseConsumeButton();
	}
}

void UAZ_Inv_InventoryGrid::HighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	if (!bMouseWithinCanvas) return;
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UAZ_Inv_GridSlot* GridSlot)
	{
		GridSlot->SetOccupiedTexture();
	});
	LastHighlightedDimensions = Dimensions;
	LastHighlightedIndex = Index;
}

void UAZ_Inv_InventoryGrid::UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UAZ_Inv_GridSlot* GridSlot)
	{
		if (GridSlot->IsAvailable())
		{
			GridSlot->SetUnoccupiedTexture();
		}
		else
		{
			GridSlot->SetOccupiedTexture();
		}
	});
}

FIntPoint UAZ_Inv_InventoryGrid::CalculateHoveredCoordinates(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const
{
	return FIntPoint{
		static_cast<int32>(FMath::FloorToInt((MousePosition.X - CanvasPosition.X) / TileSize)),
		static_cast<int32>(FMath::FloorToInt((MousePosition.Y - CanvasPosition.Y) / TileSize))
	};
}

EAZ_Inv_TileQuadrant UAZ_Inv_InventoryGrid::CalculateTileQuadrant(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const
{
	// Calculate relative position within the current tile
	const float TileLocalX = FMath::Fmod(MousePosition.X - CanvasPosition.X, TileSize);
	const float TileLocalY = FMath::Fmod(MousePosition.Y - CanvasPosition.Y, TileSize);

	// Determine which quadrant the mouse is in
	const bool bIsTop = TileLocalY < TileSize / 2.f; // Top if Y is in the upper half
	const bool bIsLeft = TileLocalX < TileSize / 2.f; // Left if X is in the left half

	EAZ_Inv_TileQuadrant HoveredTileQuadrant{EAZ_Inv_TileQuadrant::None};
	if (bIsTop && bIsLeft) HoveredTileQuadrant = EAZ_Inv_TileQuadrant::TopLeft;
	else if (bIsTop && !bIsLeft) HoveredTileQuadrant = EAZ_Inv_TileQuadrant::TopRight;
	else if (!bIsTop && bIsLeft) HoveredTileQuadrant = EAZ_Inv_TileQuadrant::BottomLeft;
	else if (!bIsTop && !bIsLeft) HoveredTileQuadrant = EAZ_Inv_TileQuadrant::BottomRight;

	return HoveredTileQuadrant;
}

FIntPoint UAZ_Inv_InventoryGrid::CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EAZ_Inv_TileQuadrant Quadrant) const
{
	const int32 HasEvenWidth = Dimensions.X % 2 == 0 ? 1 : 0;
	const int32 HasEvenHeight = Dimensions.Y % 2 == 0 ? 1 : 0;

	FIntPoint StartingCoord;
	switch (Quadrant)
	{
	case EAZ_Inv_TileQuadrant::TopLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EAZ_Inv_TileQuadrant::TopRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EAZ_Inv_TileQuadrant::BottomLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	case EAZ_Inv_TileQuadrant::BottomRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	default:
		UE_LOG(Log_AZ, Error, TEXT("Invalid Quadrant."))
		return FIntPoint(-1, -1);
	}
	return StartingCoord;
}

FAZ_Inv_SpaceQueryResult UAZ_Inv_InventoryGrid::CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions)
{
	FAZ_Inv_SpaceQueryResult Result;
	
	// in the grid bounds?
	if (!IsInGridBounds(UAZ_Inv_WidgetUtils::GetIndexFromPosition(Position, Columns), Dimensions)) return Result;

	Result.bHasSpace = true;
	
	// If more than one of the indices is occupied with the same item, we need to see if they all have the same upper left index.
	TSet<int32> OccupiedUpperLeftIndices;
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, UAZ_Inv_WidgetUtils::GetIndexFromPosition(Position, Columns), Dimensions, Columns, [&](const UAZ_Inv_GridSlot* GridSlot)
	{
		if (GridSlot->GetInventoryItem().IsValid())
		{
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			Result.bHasSpace = false;
		}
	});
	
	// if so, is there only one item in the way? (can we swap?)
	if (OccupiedUpperLeftIndices.Num() == 1) // single item at position - it's valid for swapping/combining
	{
		const int32 Index = *OccupiedUpperLeftIndices.CreateConstIterator();
		Result.ValidItem = GridSlots[Index]->GetInventoryItem();
		Result.UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	}

	return Result;
}

void UAZ_Inv_InventoryGrid::ChangeHoverType(const int32 Index, const FIntPoint& Dimensions, EInv_GridSlotState GridSlotState)
{
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [State = GridSlotState](UAZ_Inv_GridSlot* GridSlot)
	{
		switch (State)
		{
		case EInv_GridSlotState::Occupied:
			GridSlot->SetState(EInv_GridSlotState::Occupied);
			GridSlot->SetOccupiedTexture();
			break;
		case EInv_GridSlotState::Unoccupied:
			GridSlot->SetState(EInv_GridSlotState::Unoccupied);
			GridSlot->SetUnoccupiedTexture();
			break;
		case EInv_GridSlotState::GrayedOut:
			GridSlot->SetState(EInv_GridSlotState::GrayedOut);
			GridSlot->SetGrayedOutTexture();
			break;
		case EInv_GridSlotState::Selected:
			GridSlot->SetState(EInv_GridSlotState::Selected);
			GridSlot->SetSelectedTexture();
			break;
		}
	});

	LastHighlightedIndex = Index;
	LastHighlightedDimensions = Dimensions;
}

FIntPoint UAZ_Inv_InventoryGrid::GetItemDimensions(const FAZ_Inv_ItemManifest& Manifest) const
{
	const auto* GridFragment = Manifest.GetFragmentOfType<FAZ_Inv_GridFragment>();
	return GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
}

bool UAZ_Inv_InventoryGrid::HasValidItem(const UAZ_Inv_GridSlot* GridSlot) const
{
	return GridSlot->GetInventoryItem().IsValid();
}

bool UAZ_Inv_InventoryGrid::HasRoomAtIndex(const UAZ_Inv_GridSlot* GridSlot, const FIntPoint& Dimensions, const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
                                           const FGameplayTag& ItemTypeTag, const int32 MaxStackSize)
{
	// Is there room at this index? (i.e. are there other items in the way?)
	bool bHasRoomAtIndex = true;
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, GridSlot->GetIndex(), Dimensions, Columns, [&](const UAZ_Inv_GridSlot* SubGridSlot)
	{
		if (CheckSlotConstraints(GridSlot, SubGridSlot, CheckedIndices, OutTentativelyClaimed, ItemTypeTag, MaxStackSize))
		{
			OutTentativelyClaimed.Add(SubGridSlot->GetIndex());
		}
		else
		{
			bHasRoomAtIndex = false;
		}
	});

	return bHasRoomAtIndex;
}

bool UAZ_Inv_InventoryGrid::CheckSlotConstraints(const UAZ_Inv_GridSlot* GridSlot, const UAZ_Inv_GridSlot* SubGridSlot, const TSet<int32>& CheckedIndices,
	TSet<int32>& OutTentativelyClaimed, const FGameplayTag& ItemType, const int32 MaxStackSize) const
{
	// Index claimed?
	if (IsIndexClaimed(CheckedIndices, SubGridSlot->GetIndex())) return false;
	
	// Has valid item?
	if (!HasValidItem(SubGridSlot))
	{
		OutTentativelyClaimed.Add(SubGridSlot->GetIndex());
		return true;
	}

	// Is this Grid Slot an upper left slot?
	if (!IsUpperLeftSlot(GridSlot, SubGridSlot)) return false;
	
	// If so, is this a stackable item?
	const UAZ_Inv_InventoryItem* SubItem = SubGridSlot->GetInventoryItem().Get();
	if (!SubItem->IsStackable()) return false;
	
	// Is this item the same type as the item we're trying to add?
	if (!DoesItemTypeMatch(SubItem, ItemType)) return false;
	
	// If stackable, is this slot at the max stack size already?
	if (GridSlot->GetStackCount() >= MaxStackSize) return false;
	
	return true;
}

void UAZ_Inv_InventoryGrid::AddItemToGridSlots(const FAZ_Inv_SlotAvailabilityResult& SlotAvailabilityResult, UAZ_Inv_InventoryItem* NewItem)
{
	for (const auto& AvailableItem : SlotAvailabilityResult.AvailableSlots)
	{
		AddItemAtIndex(NewItem, AvailableItem.Index, SlotAvailabilityResult.bIsStackable, AvailableItem.AmountToFill);
		UpdateGridSlots(NewItem, AvailableItem.Index, SlotAvailabilityResult.bIsStackable, AvailableItem.AmountToFill);
	}
}

FAZ_Inv_SlotAvailabilityResult UAZ_Inv_InventoryGrid::HasRoomForItem(const UAZ_Inv_ItemComponent* ItemComponent)
{
	return HasRoomForItem(ItemComponent->GetItemManifest());
}

FAZ_Inv_SlotAvailabilityResult UAZ_Inv_InventoryGrid::HasRoomForItem(const UAZ_Inv_InventoryItem* Item, const int32 StackAmountOverride)
{
	return HasRoomForItem(Item->GetItemManifest(), StackAmountOverride);
}

FAZ_Inv_SlotAvailabilityResult UAZ_Inv_InventoryGrid::HasRoomForItem(const FAZ_Inv_ItemManifest& Manifest, const int32 StackAmountOverride)
{
	FAZ_Inv_SlotAvailabilityResult Result;

	// Determine if the item is stackable.
	const FAZ_Inv_StackableFragment* StackableFragment = Manifest.GetFragmentOfType<FAZ_Inv_StackableFragment>();
	Result.bIsStackable = StackableFragment != nullptr;

	// Determine how many stacks to add.
	const int32 MaxStackSize = StackableFragment ? StackableFragment->GetMaxStackSize() : 1;
	int32 AmountToFill = StackableFragment ? StackableFragment->GetStackCount() : 1;
	if (StackAmountOverride != -1 && Result.bIsStackable)
	{
		AmountToFill = StackAmountOverride;
	}

	TSet<int32> CheckedIndices;
	// For each Grid Slot:
	for (const auto& GridSlot : GridSlots)
	{
		// If we don't have anymore to fill, break out of the loop early.
		if (AmountToFill == 0) break;
		
		// Is this index claimed yet?
		if (IsIndexClaimed(CheckedIndices, GridSlot->GetIndex())) continue;

		// Is the item in grid bounds?
		if (!IsInGridBounds(GridSlot->GetIndex(), GetItemDimensions(Manifest))) continue;
		
		// Can the item fit here? (i.e. is it out of grid bounds?)
		TSet<int32> TentativelyClaimed;
		if (!HasRoomAtIndex(GridSlot, GetItemDimensions(Manifest), CheckedIndices, TentativelyClaimed, Manifest.GetItemTypeTag(), MaxStackSize))
		{
			continue;
		}
		
		// How much to fill?
		const int32 AmountToFillInSlot = DetermineFillAmountForSlot(Result.bIsStackable, MaxStackSize, AmountToFill, GridSlot);
		if (AmountToFillInSlot == 0) continue;

		CheckedIndices.Append(TentativelyClaimed);
		
		// Update the amount left to fill !!?? Check this Error
		Result.TotalRoomToFill += AmountToFillInSlot;
		Result.AvailableSlots.Emplace(
			FInv_SlotAvailability{
				HasValidItem(GridSlot) ? GridSlot->GetUpperLeftIndex() : GridSlot->GetIndex(),
				Result.bIsStackable ? AmountToFillInSlot : 0,
				HasValidItem(GridSlot)
			}
		);

		AmountToFill -= AmountToFillInSlot;

		// How much is the Remainder?
		Result.RemainingRooms = AmountToFill;
		
		if (AmountToFill == 0) return Result;
	}
	
	return Result;
}

bool UAZ_Inv_InventoryGrid::MatchesCategory(const UAZ_Inv_InventoryItem* Item) const
{
	return Item->GetItemManifest().GetItemCategory() == ItemCategory;
}