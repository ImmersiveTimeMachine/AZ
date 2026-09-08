// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryUI/AZ_Inv_CommonUI_InventoryGrid.h"

#include "AZ_GameplayTags.h"
#include "InventoryUI/Widgets/ItemPopUp/AZ_Inv_CommonUI_ItemPopUp.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/ScrollBox.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SizeBoxSlot.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_GridSlot.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"
#include "InventoryOld/Widgets/Utils/AZ_Inv_WidgetUtils.h"

void UAZ_Inv_CommonUI_InventoryGrid::NativeConstruct()
{
	Super::NativeConstruct();

	if (ItemsScrollBox)
	{
		ItemsScrollBox->SetAllowRightClickDragScrolling(false);
	}

	FTimerHandle DelayHandle;
	GetWorld()->GetTimerManager().SetTimer(
		DelayHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				SetScrollbarStyle();
			}),
		0.2f,
		false
	);

	SetupGridContainer();
	CommonUI_InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
	if (CommonUI_InventoryComponent.IsValid())
	{
		GridSize = FVector2D(CommonUI_InventoryComponent->GetGridDimensions(ItemCategory));
		CommonUI_InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromInventory);
	}
	ConstructGrid();
	RefreshFromInventory();
}

void UAZ_Inv_CommonUI_InventoryGrid::NativeDestruct()
{
	OnHide();
	if (CommonUI_InventoryComponent.IsValid())
	{
		CommonUI_InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshFromInventory);
	}
	CommonUI_InventoryComponent.Reset();
	Super::NativeDestruct();
}

void UAZ_Inv_CommonUI_InventoryGrid::RefreshFromInventory()
{
	if (!CommonUI_InventoryComponent.IsValid() || GridSlots.IsEmpty()) return;

	// Dragging is a presentation preview; committed inventory changes cancel stale previews.
	DestroyItemPopUp();
	ClearHoverItem();
	LastHoveredGridIndex = INDEX_NONE;
	ItemDropIndex = INDEX_NONE;
	CurrentQueryResult = FAZ_Inv_CommonUI_SpaceQueryResult();
	for (const auto& Pair : SlottedItems)
	{
		if (IsValid(Pair.Value)) Pair.Value->RemoveFromParent();
	}
	SlottedItems.Reset();
	for (UAZ_Inv_CommonUI_GridSlot* GridSlot : GridSlots)
	{
		if (!IsValid(GridSlot)) continue;
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Unoccupied);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
	}
	for (const auto& Placement : CommonUI_InventoryComponent->GetPlacements())
	{
		UAZ_Inv_CommonUI_InventoryItem* Item = CommonUI_InventoryComponent->FindItemById(Placement.ItemId);
		if (!MatchesCategory(Item) || !IsInGridBounds(Placement.GridIndex, GetItemDimensions(Item->GetItemManifest()))) continue;
		AddItemAtIndex(Item, Placement.GridIndex, Item->IsStackable(), Item->IsStackable() ? Placement.StackCount : 0);
		UpdateGridSlots(Item, Placement.GridIndex, Item->IsStackable(), Placement.StackCount);
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(ContentSizeBox))
	{
		ContentSizeBox->SetHeightOverride(ContentHeight);
	}

	if (ItemsSizeBox)
	{
		// 1. Set Height Override
		ItemsSizeBox->SetHeightOverride(GridContainerHeight);

		// 2. Set Padding for the ScrollBox within the SizeBox slot
		if (ItemsScrollBox)
		{
			if (USizeBoxSlot* ScrollSlot = Cast<USizeBoxSlot>(ItemsScrollBox->Slot))
			{
				FMargin NewPadding(GridPadding.X, GridPadding.Y, GridPadding.Z, GridPadding.W);
				ScrollSlot->SetPadding(NewPadding);
			}
		}
	}

	SetupPreviewSlots(IsDesignTime());
}

void UAZ_Inv_CommonUI_InventoryGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Detect cell size changes (handles window resize / fullscreen toggle)
	if (GridSlots.IsValidIndex(0) && GridSlots[0])
	{
		const FVector2D CurrentCellSize = GridSlots[0]->GetCachedGeometry().GetLocalSize();
		if (CurrentCellSize.X > 0.f && CurrentCellSize.Y > 0.f && !CurrentCellSize.Equals(LastCellSize, 1.f))
		{
			LastCellSize = CurrentCellSize;
			RefreshSlottedItemImages();
		}
	}

	if (!IsValid(HoverItem)) return;

	const FVector2D GridPosition = UAZ_Inv_WidgetUtils::GetWidgetPosition(InventoryGridPanel);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	if (CursorExitedGrid(GridPosition, UAZ_Inv_WidgetUtils::GetWidgetSize(InventoryGridPanel), MousePosition))
	{
		return;
	}

	UpdateTileParameters(GridPosition, MousePosition);
}

bool UAZ_Inv_CommonUI_InventoryGrid::MatchesCategory(const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	return IsValid(Item) && Item->GetItemManifest().GetItemCategory() == ItemCategory;
}

// Public
FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventoryGrid::HasRoomForItem(const UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
	return IsValid(ItemComponent) && CommonUI_InventoryComponent.IsValid()
		? CommonUI_InventoryComponent->GetRoomForItem(ItemComponent->GetItemManifest())
		: FAZ_Inv_CommonUI_SlotAvailabilityResult();
}

void UAZ_Inv_CommonUI_InventoryGrid::AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem, const int32 GridIndex,
                                                     const int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);
	if (!IsValid(HoverItem) || !GridSlots.IsValidIndex(GridIndex)) return;

	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpdateStackCount(InventoryItem->IsStackable()
		? GridSlots[GridIndex]->GetStackCount()
		: 0);
}

void UAZ_Inv_CommonUI_InventoryGrid::AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem)
{
	if (!IsValid(InventoryItem) || !HoverItemClass || !IsValid(GetOwningPlayer())) return;
	if (!IsValid(HoverItem))
	{
		HoverItem = CreateWidget<UAZ_Inv_CommonUI_HoverItem>(GetOwningPlayer(), HoverItemClass);
	}
	if (!IsValid(HoverItem)) return;

	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(InventoryItem, Tags.Item_Fragment_Grid);
	const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(InventoryItem, Tags.Item_Fragment_Icon);

	if (!GridFragment || !ImageFragment)
	{
		ClearHoverItem();
		return;
	}

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

FVector2D UAZ_Inv_CommonUI_InventoryGrid::GetDrawSize(const FAZ_Inv_CommonUI_GridFragment* GridFragment) const
{
	FVector2D BaseTileSize(TileSize, TileSize);

	// Use actual grid cell size when geometry is available (after layout)
	if (GridSlots.IsValidIndex(0) && GridSlots[0])
	{
		const FVector2D CachedSize = GridSlots[0]->GetCachedGeometry().GetLocalSize();
		if (CachedSize.X > 0.f && CachedSize.Y > 0.f)
		{
			BaseTileSize = CachedSize;
		}
	}

	if (!GridFragment)
	{
		return BaseTileSize;
	}

	const FVector2D GridDimensions = FVector2D(GridFragment->GetGridSize());
	const float GridFragmentGridPadding = GridFragment->GetGridPadding() * 2.f;

	return FVector2D(
		(BaseTileSize.X - GridFragmentGridPadding) * GridDimensions.X,
		(BaseTileSize.Y - GridFragmentGridPadding) * GridDimensions.Y
	);
}
void UAZ_Inv_CommonUI_InventoryGrid::SetSlottedItemImage(const FAZ_Inv_CommonUI_GridFragment* GridFragment,
                                                         const FAZ_Inv_CommonUI_ImageFragment* ImageFragment,
                                                         UAZ_Inv_CommonUI_SlottedItem* SlottedItem) const
{
	if (!SlottedItem || !ImageFragment)
	{
		return;
	}

	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = GetDrawSize(GridFragment);

	SlottedItem->SetImageBrush(Brush);
}

void UAZ_Inv_CommonUI_InventoryGrid::RefreshSlottedItemImages()
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	for (auto& [Index, SlottedItem] : SlottedItems)
	{
		if (!IsValid(SlottedItem)) continue;

		const TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InvItem = SlottedItem->GetInventoryItem();
		if (!InvItem.IsValid()) continue;

		const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(InvItem.Get(), Tags.Item_Fragment_Grid);
		const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(InvItem.Get(), Tags.Item_Fragment_Icon);

		SetSlottedItemImage(GridFragment, ImageFragment, SlottedItem);
	}
}

UAZ_Inv_CommonUI_SlottedItem* UAZ_Inv_CommonUI_InventoryGrid::CreateSlottedItem(UAZ_Inv_CommonUI_InventoryItem* NewItem,
                                                                                const FAZ_Inv_CommonUI_GridFragment* GridFragment,
                                                                                const FAZ_Inv_CommonUI_ImageFragment* ImageFragment,
                                                                                const int32 Index,
                                                                                bool bStackable,
                                                                                const int32 StackAmount) const
{
	if (!NewItem || !SlottedItemClass)
	{
		return nullptr;
	}

	UAZ_Inv_CommonUI_SlottedItem* SlottedItem = CreateWidget<UAZ_Inv_CommonUI_SlottedItem>(GetOwningPlayer(), SlottedItemClass);
	if (SlottedItem)
	{
		SlottedItem->SetInventoryItem(NewItem);

		SetSlottedItemImage(GridFragment, ImageFragment, SlottedItem);

		SlottedItem->SetGridIndex(Index);
		if (GridFragment)
		{
			SlottedItem->SetGridDimensions(GridFragment->GetGridSize());
		}

		SlottedItem->SetIsStackable(bStackable);
		const int32 StackUpdateAmount = StackAmount > 0 ? StackAmount : 0;
		SlottedItem->UpdateStackCount(StackUpdateAmount);
		SlottedItem->OnItemClicked().AddDynamic(this, &ThisClass::OnSlottedItemClicked);
		SlottedItem->OnItemHovered().AddDynamic(this, &ThisClass::OnSlottedItemHovered);
		SlottedItem->OnItemUnhovered().AddDynamic(this, &ThisClass::OnSlottedItemUnhovered);
		SlottedItem->OnItemMouseButtonDown().AddDynamic(this, &ThisClass::OnSlottedItemMouseButtonDown);
	}

	return SlottedItem;
}

bool UAZ_Inv_CommonUI_InventoryGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	// Derive grid dimensions from GridSize (same values used when constructing the grid)
	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	const int32 RowCount = FMath::TruncToInt(GridSize.Y);

	// Fast reject for invalid start index
	if (StartIndex < 0 || StartIndex >= ColumnCount * RowCount)
	{
		return false;
	}

	// Compute starting cell (row/col) and ending extents
	const int32 StartRow = StartIndex / ColumnCount;
	const int32 StartCol = StartIndex % ColumnCount;

	const int32 EndCol = StartCol + ItemDimensions.X;
	const int32 EndRow = StartRow + ItemDimensions.Y;

	// Must start inside grid and fit fully within bounds
	const bool bStartsInside = StartCol >= 0 && StartRow >= 0 && StartCol < ColumnCount && StartRow < RowCount;
	const bool bFitsInside = EndCol <= ColumnCount && EndRow <= RowCount;

	return bStartsInside && bFitsInside;
}

FIntPoint UAZ_Inv_CommonUI_InventoryGrid::GetItemDimensions(const FAZ_Inv_CommonUI_ItemManifest& Manifest) const
{
	const auto* GridFragment = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_GridFragment>();
	return GridFragment
		? GridFragment->GetGridSize()
		: FIntPoint(1, 1);
}

int32 UAZ_Inv_CommonUI_InventoryGrid::GetStackAmount(const UAZ_Inv_CommonUI_GridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	// If we are at a slot that doesn't hold the stack count. we must get the actual stack count.
	if (const int32 UpperLeftIndex = GridSlot->GetUpperLeftIndex(); UpperLeftIndex != INDEX_NONE)
	{
		const UAZ_Inv_CommonUI_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftGridSlot->GetStackCount();
	}
	return CurrentSlotStackCount;
}

void UAZ_Inv_CommonUI_InventoryGrid::AddItemAtIndex(UAZ_Inv_CommonUI_InventoryItem* NewItem, int32 Index, bool bStackable, int32 StackAmount)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(NewItem, Tags.Item_Fragment_Grid);
	const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(NewItem, Tags.Item_Fragment_Icon);
	if (!GridFragment || !ImageFragment) return;

	UAZ_Inv_CommonUI_SlottedItem* SlottedItem = CreateSlottedItem(NewItem, GridFragment, ImageFragment, Index, bStackable, StackAmount);
	if (!IsValid(SlottedItem)) return;
	AddSlottedItemToPanel(Index, GridFragment, SlottedItem);

	SlottedItems.Add(Index, SlottedItem);
}

void UAZ_Inv_CommonUI_InventoryGrid::UpdateGridSlots(UAZ_Inv_CommonUI_InventoryItem* NewItem, int32 Index, bool bStackableItem, int32 StackAmount)
{
	check(GridSlots.IsValidIndex(Index));

	if (bStackableItem)
	{
		GridSlots[Index]->SetStackCount(StackAmount);
	}

	const FAZ_GameplayTags Tags = FAZ_GameplayTags::Get();
	const auto GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(NewItem, Tags.Item_Fragment_Grid);
	const FIntPoint Dimensions = GridFragment
		? GridFragment->GetGridSize()
		: FIntPoint(1, 1);

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);

	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, ColumnCount, [&](UAZ_Inv_CommonUI_GridSlot* GridSlot)
		{
			GridSlot->SetInventoryItem(NewItem);
			GridSlot->SetUpperLeftIndex(Index);
			GridSlot->SetState(EInv_CommonUI_GridSlotState::Occupied);
			GridSlot->SetUnoccupiedTexture();
			GridSlot->SetAvailable(false);
		});
}

void UAZ_Inv_CommonUI_InventoryGrid::AddSlottedItemToPanel(const int32 Index, const FAZ_Inv_CommonUI_GridFragment* GridFragment,
                                                           UAZ_Inv_CommonUI_SlottedItem* SlottedItem) const
{
	if (!SlottedItem || !GridFragment || !InventoryGridPanel) return;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	if (ColumnCount <= 0) return;

	const int32 Row = Index / ColumnCount;
	const int32 Col = Index % ColumnCount;

	const FIntPoint ItemDimensions = GridFragment->GetGridSize();

	if (UGridSlot* GridSlot = InventoryGridPanel->AddChildToGrid(SlottedItem, Row, Col))
	{
		GridSlot->SetRowSpan(ItemDimensions.Y);
		GridSlot->SetColumnSpan(ItemDimensions.X);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
		GridSlot->SetPadding(FMargin(1.0f));
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::AddItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	RefreshFromInventory();
}

void UAZ_Inv_CommonUI_InventoryGrid::SetupGridContainer()
{
	if (!bShowBackground)
	{
		// Safety check to prevent crashing if the widget binding failed
		if (GridContainerBorder)
		{
			// [BP Node: SetBrushColor]
			// The BP default value was (R=0, G=0, B=0, A=0), which is FLinearColor::Transparent.
			GridContainerBorder->SetBrushColor(FLinearColor::Transparent);
		}
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::ConstructGrid()
{
	// Safety Check: Ensure the Grid reference and the Widget Class are valid
	if (!InventoryGridPanel || !GridSlotWidgetClass)
	{
		return;
	}

	// Calculate TileSize dynamically based on ContentHeight and RowCount
	/*if (GridSize.Y > 0)
	{
		TileSize = ContentHeight / GridSize.Y;
	}*/

	InventoryGridPanel->ClearChildren();
	GridSlots.Reset();
	SlotsByIndex.Reset();
	SlottedItems.Reset();
	// We convert the float size (e.g., 5.0, 4.0) into integers for the loop.
	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	const int32 RowCount = FMath::TruncToInt(GridSize.Y);

	SlotsByIndex.SetNum(ColumnCount * RowCount);

	// Note: In C++, "i < Count" is the same as BP's "0 to Count-1"
	for (int32 Col = 0; Col < RowCount; Col++)
	{
		for (int32 Row = 0; Row < ColumnCount; Row++)
		{
			if (UAZ_Inv_CommonUI_GridSlot* GridSlot = CreateWidget<UAZ_Inv_CommonUI_GridSlot>(this, GridSlotWidgetClass))
			{
				GridSlots.Add(GridSlot);
				GridSlot->OnSlotHovered().AddDynamic(this, &UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotHovered);
				GridSlot->OnSlotUnhovered().AddDynamic(this, &UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotUnhovered);
				GridSlot->OnSlotClicked().AddDynamic(this, &UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotClicked);

				const FIntPoint TilePosition(Row, Col);
				const int32 LinearIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(TilePosition, ColumnCount);
				GridSlot->SetIndex(LinearIndex);
				if (SlotsByIndex.IsValidIndex(LinearIndex))
				{
					SlotsByIndex[LinearIndex] = GridSlot;
				}

				// Standard: outer loop (Col = row index) → InRow, inner loop (Row = column index) → InColumn
				if (UGridSlot* NewSlotVisual = InventoryGridPanel->AddChildToGrid(GridSlot, Col, Row))
				{
					NewSlotVisual->SetHorizontalAlignment(HAlign_Fill);

					NewSlotVisual->SetVerticalAlignment(VAlign_Fill);

					NewSlotVisual->SetPadding(FMargin(1.0f));

					NewSlotVisual->SetRowSpan(1); // Takes up 1 Rows height
					NewSlotVisual->SetColumnSpan(1); // Takes up 1 Columns width
				}
			}
		}
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotClicked(UCommonButtonBase* Button)
{
	if (!IsValid(HoverItem)) return;
	if (!GridSlots.IsValidIndex(ItemDropIndex)) return;

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		if (SlottedItems.Contains(CurrentQueryResult.UpperLeftIndex))
		{
			UAZ_Inv_CommonUI_SlottedItem* TargetSlottedItem = SlottedItems.FindChecked(CurrentQueryResult.UpperLeftIndex);
			OnSlottedItemClicked(TargetSlottedItem);
		}
		return;
	}

	if (!IsInGridBounds(ItemDropIndex, HoverItem->GetGridDimensions())) return;
	if (!CurrentQueryResult.bHasSpace) return;
	auto GridSlot = GridSlots[ItemDropIndex];
	if (!GridSlot->GetInventoryItem().IsValid())
	{
		PutDownOnIndex(ItemDropIndex);
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotHovered(UCommonButtonBase* Button)
{
	if (IsValid(HoverItem)) return;

	UAZ_Inv_CommonUI_GridSlot* GridSlot = Cast<UAZ_Inv_CommonUI_GridSlot>(Button);
	if (GridSlot && GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Occupied);
		GridSlot->SetOccupiedTexture();
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotUnhovered(UCommonButtonBase* Button)
{
	if (IsValid(HoverItem)) return;

	UAZ_Inv_CommonUI_GridSlot* GridSlot = Cast<UAZ_Inv_CommonUI_GridSlot>(Button);
	if (GridSlot && GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Unoccupied);
		GridSlot->SetUnoccupiedTexture();
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::SetupPreviewSlots(bool bIsDesignTime)
{
	// If we are not in design time (or forced preview), exit early.
	if (!bIsDesignTime)
	{
		return;
	}

	// Safety Check: Ensure the Grid reference and the Widget Class are valid
	if (!InventoryGridPanel || !GridSlotWidgetClass)
	{
		return;
	}
}

bool UAZ_Inv_CommonUI_InventoryGrid::HasHoverItem() const
{
	return IsValid(HoverItem);
}

UAZ_Inv_CommonUI_HoverItem* UAZ_Inv_CommonUI_InventoryGrid::GetHoverItem() const
{
	return HoverItem;
}

float UAZ_Inv_CommonUI_InventoryGrid::GetTileSize() const
{
	return TileSize;
}

void UAZ_Inv_CommonUI_InventoryGrid::ClearHoverItem()
{
	if (!IsValid(HoverItem)) return;

	// Force-deselect highlighted slots regardless of availability —
	// UnHighlightSlots would re-select occupied slots, so deselect directly
	if (LastHighlightedIndex != INDEX_NONE)
	{
		const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
		UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, LastHighlightedIndex, LastHighlightedDimensions, ColumnCount,
			[](UAZ_Inv_CommonUI_GridSlot* GridSlot)
			{
				GridSlot->SetUnoccupiedTexture();
			});
	}
	LastHighlightedIndex = INDEX_NONE;
	LastHighlightedDimensions = FIntPoint::ZeroValue;

	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetIsStackable(false);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->UpdateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());

	HoverItem->RemoveFromParent();
	HoverItem = nullptr;

	ShowCursor();
}

void UAZ_Inv_CommonUI_InventoryGrid::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	OwningCanvasPanel = OwningCanvas;
}

void UAZ_Inv_CommonUI_InventoryGrid::HideCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetHiddenCursorWidget());
}

void UAZ_Inv_CommonUI_InventoryGrid::ShowCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetVisibleCursorWidget());
}

void UAZ_Inv_CommonUI_InventoryGrid::OnHide()
{
	DestroyItemPopUp();
	PutHoverItemBack();
	UAZ_Inv_InventoryStatics::CommonUI_ItemUnhovered(GetOwningPlayer());
}

bool UAZ_Inv_CommonUI_InventoryGrid::CancelInteraction()
{
	if (IsValid(ItemPopUp))
	{
		DestroyItemPopUp();
		return true;
	}
	if (IsValid(HoverItem))
	{
		PutHoverItemBack();
		return true;
	}
	return false;
}

void UAZ_Inv_CommonUI_InventoryGrid::SetScrollbarStyle()
{
	if (!ItemsScrollBox)
	{
		return;
	}

	// 1. Force Layout Prepass to ensure scroll offsets are up to date
	ItemsScrollBox->ForceLayoutPrepass();

	// 2. Check if we are at the end of the scroll offset
	// Equivalent to your EqualEqual (Double) node logic
	const bool bIsAtEnd = FMath::IsNearlyEqual(ItemsScrollBox->GetScrollOffset(), ItemsScrollBox->GetScrollOffsetOfEnd());

	// 3. Get the current style to modify it
	FScrollBarStyle NewStyle = ItemsScrollBox->GetWidgetBarStyle();

	// Determine target colors based on the offset condition
	FLinearColor TargetNormal = bIsAtEnd
		? BarTransparentColor
		: BarNormalColor;
	FLinearColor TargetHover = bIsAtEnd
		? BarTransparentColor
		: BarHighlightedColor;
	FLinearColor TargetDrag = bIsAtEnd
		? BarTransparentColor
		: BarDragColor;

	// 4. Update the Thumb images (Normal, Hovered, Dragged)
	auto UpdateThumb = [&](FSlateBrush& Brush, const FLinearColor& Color)
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			// Equivalent to your MakeVector4 node for Corner Radii
			Brush.OutlineSettings.CornerRadii = FVector4(BarCornerRadius, BarCornerRadius, BarCornerRadius, BarCornerRadius);
		};

	UpdateThumb(NewStyle.NormalThumbImage, TargetNormal);
	UpdateThumb(NewStyle.HoveredThumbImage, TargetHover);
	UpdateThumb(NewStyle.DraggedThumbImage, TargetDrag);

	// Apply the modified style back to the widget
	ItemsScrollBox->SetWidgetBarStyle(NewStyle);

	// 5. Update Scrollbar Padding
	// If at end: (0,0,0,0) | If not at end: (Left: 5, Top: 2, Right: 2, Bottom: 2)
	FMargin NewPadding = bIsAtEnd
		? FMargin(0.f)
		: FMargin(5.f, 2.f, 2.f, 2.f);
	ItemsScrollBox->SetScrollbarPadding(NewPadding);
}

// =============================================================================
// Slotted Item Click Handler
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::OnSlottedItemClicked(UCommonButtonBase* Button)
{
	UAZ_Inv_CommonUI_SlottedItem* SlottedItem = Cast<UAZ_Inv_CommonUI_SlottedItem>(Button);
	if (!SlottedItem) return;
	const int32 GridIndex = SlottedItem->GetGridIndex();
	if (!GridSlots.IsValidIndex(GridIndex)) return;
	UAZ_Inv_InventoryStatics::CommonUI_ItemUnhovered(GetOwningPlayer());
	UAZ_Inv_CommonUI_InventoryItem* ClickedItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(ClickedItem)) return;
	if (!IsValid(HoverItem))
	{
		PickUp(ClickedItem, GridIndex);
		return;
	}
	// Merge, move and swap are validated atomically by the inventory owner.
	PutDownOnIndex(GridIndex);
}

// =============================================================================
// Slotted Item Hover Handlers
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::OnSlottedItemHovered(UCommonButtonBase* Button)
{
	UAZ_Inv_CommonUI_SlottedItem* SlottedItem = Cast<UAZ_Inv_CommonUI_SlottedItem>(Button);
	if (!SlottedItem) return;

	LastHoveredGridIndex = SlottedItem->GetGridIndex();

	UAZ_Inv_CommonUI_InventoryItem* Item = SlottedItem->GetInventoryItem().Get();
	if (!IsValid(Item)) return;

	UAZ_Inv_InventoryStatics::CommonUI_ItemHovered(GetOwningPlayer(), Item);
}

void UAZ_Inv_CommonUI_InventoryGrid::OnSlottedItemUnhovered(UCommonButtonBase* Button)
{
	LastHoveredGridIndex = INDEX_NONE;
	UAZ_Inv_InventoryStatics::CommonUI_ItemUnhovered(GetOwningPlayer());
}

void UAZ_Inv_CommonUI_InventoryGrid::OnSlottedItemMouseButtonDown(UCommonButtonBase* Button, FKey PressedKey)
{
	// Check if the pressed key is mapped to ContextMenuAction in the active IMC.
	if (ContextMenuAction)
	{
		if (const auto* PC = GetOwningPlayer())
		{
			if (const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				TArray<FKey> MappedKeys = Subsystem->QueryKeysMappedToAction(ContextMenuAction);
				if (MappedKeys.Contains(PressedKey))
				{
					TryShowContextMenu();
					return;
				}
			}
		}
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::TryShowContextMenu()
{
	if (IsValid(HoverItem)) return;
	if (!GridSlots.IsValidIndex(LastHoveredGridIndex)) return;

	CreateItemPopUp(LastHoveredGridIndex);
}

// =============================================================================
// Stack Change Handler
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::AddStacks(const FAZ_Inv_CommonUI_SlotAvailabilityResult& Result)
{
	RefreshFromInventory();
}

// =============================================================================
// PopUp Menu Handlers
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::OnPopUpMenuSplit(int32 SplitAmount, int32 Index)
{
	DestroyItemPopUp();
	if (!GridSlots.IsValidIndex(Index)) return;
	UAZ_Inv_CommonUI_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (!RightClickedItem->IsStackable()) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UAZ_Inv_CommonUI_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = UpperLeftGridSlot->GetStackCount();
	if (SplitAmount <= 0 || SplitAmount >= StackCount) return;
	const int32 NewStackCount = StackCount - SplitAmount;
	AssignHoverItem(RightClickedItem, UpperLeftIndex, UpperLeftIndex);
	if (!IsValid(HoverItem)) return;
	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);
	HoverItem->UpdateStackCount(SplitAmount);
}

void UAZ_Inv_CommonUI_InventoryGrid::OnPopUpMenuConsume(int32 Index)
{
	DestroyItemPopUp();
	if (!GridSlots.IsValidIndex(Index) || !CommonUI_InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	CommonUI_InventoryComponent->Server_ConsumeItem(RightClickedItem);
}

void UAZ_Inv_CommonUI_InventoryGrid::OnPopUpMenuDismissed()
{
	DestroyItemPopUp();
}

void UAZ_Inv_CommonUI_InventoryGrid::OnPopUpMenuDrop(int32 Index)
{
	DestroyItemPopUp();
	if (!GridSlots.IsValidIndex(Index) || !CommonUI_InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	CommonUI_InventoryComponent->Server_DropItem(RightClickedItem, RightClickedItem->IsStackable() ? GetStackAmount(GridSlots[Index]) : 1);
}

void UAZ_Inv_CommonUI_InventoryGrid::OnPopUpMenuEquip(int32 Index)
{
	DestroyItemPopUp();
	if (!GridSlots.IsValidIndex(Index) || !CommonUI_InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	CommonUI_InventoryComponent->Server_EquipSlotClicked(RightClickedItem, nullptr);
}

// =============================================================================
// Item Placement
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::PutDownOnIndex(const int32 Index)
{
	if (!IsValid(HoverItem) || !CommonUI_InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* Item = HoverItem->GetInventoryItem();
	const int32 SourceIndex = HoverItem->GetPreviousGridIndex();
	const int32 StackCount = HoverItem->IsStackable() ? HoverItem->GetStackCount() : 1;
	// Restore the model before making the request. A refused request leaves it visible.
	ClearHoverItem();
	RefreshFromInventory();
	CommonUI_InventoryComponent->Server_MoveItem(Item, SourceIndex, Index, StackCount);
}

void UAZ_Inv_CommonUI_InventoryGrid::PutHoverItemBack()
{
	if (!IsValid(HoverItem)) return;

	ClearHoverItem();
	RefreshFromInventory();
}

void UAZ_Inv_CommonUI_InventoryGrid::RemoveItemFromGrid(UAZ_Inv_CommonUI_InventoryItem* InventoryItem, const int32 GridIndex)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(InventoryItem, Tags.Item_Fragment_Grid);
	if (!GridFragment) return;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);

	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, GridIndex, GridFragment->GetGridSize(), ColumnCount, [&](UAZ_Inv_CommonUI_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Unoccupied);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
	});

	if (SlottedItems.Contains(GridIndex))
	{
		TObjectPtr<UAZ_Inv_CommonUI_SlottedItem> FoundSlottedItem;
		SlottedItems.RemoveAndCopyValue(GridIndex, FoundSlottedItem);
		FoundSlottedItem->RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::PickUp(UAZ_Inv_CommonUI_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	if (!IsValid(ClickedInventoryItem) || !GridSlots.IsValidIndex(GridIndex)) return;
	AssignHoverItem(ClickedInventoryItem, GridIndex, GridIndex);
	if (IsValid(HoverItem)) RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
}

void UAZ_Inv_CommonUI_InventoryGrid::DropItem()
{
	if (!IsValid(HoverItem)) return;
	if (!IsValid(HoverItem->GetInventoryItem())) return;

	if (!CommonUI_InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* Item = HoverItem->GetInventoryItem();
	const int32 StackCount = HoverItem->IsStackable() ? HoverItem->GetStackCount() : 1;
	ClearHoverItem();
	RefreshFromInventory();
	CommonUI_InventoryComponent->Server_DropItem(Item, StackCount);
}

bool UAZ_Inv_CommonUI_InventoryGrid::HasActivePopUp() const
{
	return IsValid(ItemPopUp);
}

void UAZ_Inv_CommonUI_InventoryGrid::DestroyItemPopUp()
{
	if (IsValid(ItemPopUp))
	{
		UAZ_Inv_CommonUI_ItemPopUp* PopUp = ItemPopUp;
		ItemPopUp = nullptr;
		const int32 PrevIndex = PopUp->GetGridIndex();
		if (GridSlots.IsValidIndex(PrevIndex))
		{
			GridSlots[PrevIndex]->SetItemPopUp(nullptr);
		}
		PopUp->OnDismissed.Unbind();
		PopUp->RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::CreateItemPopUp(const int32 GridIndex)
{
	if (!GridSlots.IsValidIndex(GridIndex)) return;
	UAZ_Inv_CommonUI_InventoryItem* RightClickedItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (!ItemPopUpClass) return;

	DestroyItemPopUp();

	ItemPopUp = CreateWidget<UAZ_Inv_CommonUI_ItemPopUp>(this, ItemPopUpClass);
	ItemPopUp->SetGridIndex(GridIndex);
	GridSlots[GridIndex]->SetItemPopUp(ItemPopUp);

	if (OwningCanvasPanel.IsValid())
	{
		OwningCanvasPanel->AddChild(ItemPopUp);
		UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ItemPopUp);
		const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
		CanvasSlot->SetPosition(MousePosition - ItemPopUpOffset);
	}
	else
	{
		ItemPopUp->AddToViewport(100);
		const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
		ItemPopUp->SetPositionInViewport(MousePosition - ItemPopUpOffset);
	}

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

	ItemPopUp->OnDismissed.BindDynamic(this, &ThisClass::OnPopUpMenuDismissed);
	ItemPopUp->OnDrop.BindDynamic(this, &ThisClass::OnPopUpMenuDrop);

	if (RightClickedItem->IsConsumable())
	{
		ItemPopUp->OnConsume.BindDynamic(this, &ThisClass::OnPopUpMenuConsume);
	}
	else
	{
		ItemPopUp->CollapseConsumeButton();
	}

	if (RightClickedItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>())
	{
		ItemPopUp->OnEquip.BindDynamic(this, &ThisClass::OnPopUpMenuEquip);
	}
	else
	{
		ItemPopUp->CollapseEquipButton();
	}
}

// =============================================================================
// Highlight System
// =============================================================================

void UAZ_Inv_CommonUI_InventoryGrid::HighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	if (!bMouseWithinGrid) return;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, ColumnCount, [&](UAZ_Inv_CommonUI_GridSlot* GridSlot)
	{
		GridSlot->SetOccupiedTexture();
	});
	LastHighlightedDimensions = Dimensions;
	LastHighlightedIndex = Index;
}

void UAZ_Inv_CommonUI_InventoryGrid::UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, ColumnCount, [&](UAZ_Inv_CommonUI_GridSlot* GridSlot)
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

void UAZ_Inv_CommonUI_InventoryGrid::ChangeHoverType(const int32 Index, const FIntPoint& Dimensions, EInv_CommonUI_GridSlotState GridSlotState)
{
	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, ColumnCount, [State = GridSlotState](UAZ_Inv_CommonUI_GridSlot* GridSlot)
	{
		switch (State)
		{
		case EInv_CommonUI_GridSlotState::Occupied:
			GridSlot->SetState(EInv_CommonUI_GridSlotState::Occupied);
			GridSlot->SetOccupiedTexture();
			break;
		case EInv_CommonUI_GridSlotState::Unoccupied:
			GridSlot->SetState(EInv_CommonUI_GridSlotState::Unoccupied);
			GridSlot->SetUnoccupiedTexture();
			break;
		case EInv_CommonUI_GridSlotState::GrayedOut:
			GridSlot->SetState(EInv_CommonUI_GridSlotState::GrayedOut);
			GridSlot->SetGrayedOutTexture();
			break;
		case EInv_CommonUI_GridSlotState::Selected:
			GridSlot->SetState(EInv_CommonUI_GridSlotState::Selected);
			GridSlot->SetSelectedTexture();
			break;
		}
	});

	LastHighlightedIndex = Index;
	LastHighlightedDimensions = Dimensions;
}

// =============================================================================
// Tile Tracking
// =============================================================================

bool UAZ_Inv_CommonUI_InventoryGrid::CursorExitedGrid(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location)
{
	bLastMouseWithinGrid = bMouseWithinGrid;
	bMouseWithinGrid = UAZ_Inv_WidgetUtils::IsWithinBounds(BoundaryPos, BoundarySize, Location);
	if (!bMouseWithinGrid && bLastMouseWithinGrid)
	{
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
		return true;
	}
	return false;
}

void UAZ_Inv_CommonUI_InventoryGrid::UpdateTileParameters(const FVector2D& GridPosition, const FVector2D& MousePosition)
{
	if (!bMouseWithinGrid) return;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	const FIntPoint HoveredTileCoordinates = CalculateHoveredCoordinates(GridPosition, MousePosition);

	LastTileParameters = TileParameters;
	TileParameters.TileCoordinats = HoveredTileCoordinates;
	TileParameters.TileIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(HoveredTileCoordinates, ColumnCount);
	TileParameters.TileQuadrant = CalculateTileQuadrant(GridPosition, MousePosition);

	OnTileParametersUpdated(TileParameters);
}

void UAZ_Inv_CommonUI_InventoryGrid::OnTileParametersUpdated(const FAZ_Inv_TileParameters& Parameters)
{
	if (!IsValid(HoverItem)) return;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	const FIntPoint Dimensions = HoverItem->GetGridDimensions();

	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinats, Dimensions, Parameters.TileQuadrant);
	ItemDropIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(StartingCoordinate, ColumnCount);

	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);

	if (CurrentQueryResult.bHasSpace)
	{
		HighlightSlots(ItemDropIndex, Dimensions);
		return;
	}
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(
			CurrentQueryResult.ValidItem.Get(), FAZ_GameplayTags::Get().Item_Fragment_Grid);
		if (!GridFragment) return;

		ChangeHoverType(CurrentQueryResult.UpperLeftIndex, GridFragment->GetGridSize(), EInv_CommonUI_GridSlotState::GrayedOut);
	}
}

FIntPoint UAZ_Inv_CommonUI_InventoryGrid::CalculateHoveredCoordinates(const FVector2D& GridPosition, const FVector2D& MousePosition) const
{
	// Derive actual tile size from grid panel geometry
	const FVector2D GridPanelSize = UAZ_Inv_WidgetUtils::GetWidgetSize(InventoryGridPanel);
	const float ActualTileW = GridSize.X > 0 ? GridPanelSize.X / GridSize.X : TileSize;
	const float ActualTileH = GridSize.Y > 0 ? GridPanelSize.Y / GridSize.Y : TileSize;

	return FIntPoint(
		FMath::FloorToInt32((MousePosition.X - GridPosition.X) / ActualTileW),
		FMath::FloorToInt32((MousePosition.Y - GridPosition.Y) / ActualTileH)
	);
}

EAZ_Inv_TileQuadrant UAZ_Inv_CommonUI_InventoryGrid::CalculateTileQuadrant(const FVector2D& GridPosition, const FVector2D& MousePosition) const
{
	const FVector2D GridPanelSize = UAZ_Inv_WidgetUtils::GetWidgetSize(InventoryGridPanel);
	const float ActualTileW = GridSize.X > 0 ? GridPanelSize.X / GridSize.X : TileSize;
	const float ActualTileH = GridSize.Y > 0 ? GridPanelSize.Y / GridSize.Y : TileSize;

	const float TileLocalX = FMath::Fmod(MousePosition.X - GridPosition.X, ActualTileW);
	const float TileLocalY = FMath::Fmod(MousePosition.Y - GridPosition.Y, ActualTileH);

	const bool bIsTop = TileLocalY < ActualTileH / 2.f;
	const bool bIsLeft = TileLocalX < ActualTileW / 2.f;

	if (bIsTop && bIsLeft) return EAZ_Inv_TileQuadrant::TopLeft;
	if (bIsTop && !bIsLeft) return EAZ_Inv_TileQuadrant::TopRight;
	if (!bIsTop && bIsLeft) return EAZ_Inv_TileQuadrant::BottomLeft;
	return EAZ_Inv_TileQuadrant::BottomRight;
}

FIntPoint UAZ_Inv_CommonUI_InventoryGrid::CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EAZ_Inv_TileQuadrant Quadrant) const
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
		return FIntPoint(-1, -1);
	}
	return StartingCoord;
}

FAZ_Inv_CommonUI_SpaceQueryResult UAZ_Inv_CommonUI_InventoryGrid::CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions)
{
	FAZ_Inv_CommonUI_SpaceQueryResult Result;
	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	if (Position.X < 0 || Position.Y < 0 || Position.X + Dimensions.X > ColumnCount || Position.Y + Dimensions.Y > FMath::TruncToInt(GridSize.Y)) return Result;
	if (!IsInGridBounds(UAZ_Inv_WidgetUtils::GetIndexFromPosition(Position, ColumnCount), Dimensions)) return Result;

	Result.bHasSpace = true;

	TSet<int32> OccupiedUpperLeftIndices;
	UAZ_Inv_InventoryStatics::ForEach2D(GridSlots, UAZ_Inv_WidgetUtils::GetIndexFromPosition(Position, ColumnCount), Dimensions, ColumnCount,
		[&](const UAZ_Inv_CommonUI_GridSlot* GridSlot)
	{
		if (GridSlot->GetInventoryItem().IsValid())
		{
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			Result.bHasSpace = false;
		}
	});

	if (OccupiedUpperLeftIndices.Num() == 1)
	{
		const int32 Index = *OccupiedUpperLeftIndices.CreateConstIterator();
		Result.ValidItem = GridSlots[Index]->GetInventoryItem();
		Result.UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	}

	return Result;
}

// =============================================================================
// Cursor Widget Helpers
// =============================================================================

UUserWidget* UAZ_Inv_CommonUI_InventoryGrid::GetVisibleCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(VisibleCursorWidget) && VisibleCursorWidgetClass)
	{
		VisibleCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), VisibleCursorWidgetClass);
	}
	return VisibleCursorWidget;
}

UUserWidget* UAZ_Inv_CommonUI_InventoryGrid::GetHiddenCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(HiddenCursorWidget) && HiddenCursorWidgetClass)
	{
		HiddenCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), HiddenCursorWidgetClass);
	}
	return HiddenCursorWidget;
}
