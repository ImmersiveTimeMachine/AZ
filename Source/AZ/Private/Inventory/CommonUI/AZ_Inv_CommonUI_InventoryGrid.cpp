// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryGrid.h"

#include "AZ_GameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBoxSlot.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_GridSlot.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_CommonUI_InventoryGrid::NativeConstruct()
{
	Super::NativeConstruct();

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
	ConstructGrid();

	CommonUI_InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
	ensure(CommonUI_InventoryComponent.IsValid());

	CommonUI_InventoryComponent->OnItemAdded.AddDynamic(this, &ThisClass::AddItem);
	/*InventoryComponent->OnStackChange.AddDynamic(this, &ThisClass::AddStacks);*/
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
}

bool UAZ_Inv_CommonUI_InventoryGrid::MatchesCategory(const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	return Item->GetItemManifest().GetItemCategory() == ItemCategory;
}


// Public
FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventoryGrid::HasRoomForItem(const UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
	return HasRoomForItem(ItemComponent->GetItemManifest());
}

void UAZ_Inv_CommonUI_InventoryGrid::AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem, const int32 GridIndex,
                                                     const int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);

	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpdateStackCount(InventoryItem->IsStackable()
		? GridSlots[GridIndex]->GetStackCount()
		: 0);
}

void UAZ_Inv_CommonUI_InventoryGrid::AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem)
{
	if (!IsValid(HoverItem))
	{
		HoverItem = CreateWidget<UAZ_Inv_CommonUI_HoverItem>(GetOwningPlayer(), HoverItemClass);
	}

	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(InventoryItem, Tags.Item_Fragment_Grid);
	const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(InventoryItem, Tags.Item_Fragment_Icon);

	if (!GridFragment || !ImageFragment) return;

	const FVector2D DrawSize = GetDrawSize(GridFragment);

	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(ImageFragment->GetIcon());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.ImageSize = DrawSize;

	HoverItem->SetImageBrush(IconBrush);
	HoverItem->SetGridDimensions(GridFragment->GetGridSize());
	HoverItem->SetInventoryItem(InventoryItem);
	HoverItem->SetIsStackable(InventoryItem->IsStackable());

	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, HoverItem);
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventoryGrid::HasRoomForItem(const UAZ_Inv_CommonUI_InventoryItem* Item,
                                                                                       const int32 StackAmountOverride)
{
	return HasRoomForItem(Item->GetItemManifest(), StackAmountOverride);
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventoryGrid::HasRoomForItem(const FAZ_Inv_CommonUI_ItemManifest& Manifest,
                                                                                       const int32 StackAmountOverride)
{
	FAZ_Inv_CommonUI_SlotAvailabilityResult Result;

	// Determine if the item is stackable.
	const auto* StackableFragment = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
	Result.bIsStackable = StackableFragment != nullptr;

	// Determine how many stacks to add.
	const int32 MaxStackSize = StackableFragment
		? StackableFragment->GetMaxStackSize()
		: 1;
	int32 AmountToFill = StackableFragment
		? StackableFragment->GetStackCount()
		: 1;
	if (StackAmountOverride != -1 && Result.bIsStackable)
	{
		AmountToFill = StackAmountOverride;
	}

	TSet<int32> CheckedIndices;

	for (const auto GridSlot : GetAllGridSlots())
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

		// Update the amount left to fill
		Result.TotalRoomToFill += AmountToFillInSlot;
		Result.AvailableSlots.Emplace(
			FInv_SlotAvailability{
				GridSlot->GetInventoryItem().IsValid()
				? GridSlot->GetUpperLeftIndex()
				: GridSlot->GetIndex(),
				Result.bIsStackable
				? AmountToFillInSlot
				: 0,
				GridSlot->GetInventoryItem().IsValid()
			}
		);

		AmountToFill -= AmountToFillInSlot;

		// How much is the Remainder?
		Result.RemainingRooms = AmountToFill;

		if (AmountToFill == 0) return Result;
	}

	return Result;
}

bool UAZ_Inv_CommonUI_InventoryGrid::HasRoomAtIndex(const UAZ_Inv_CommonUI_GridSlot* GridSlot,
                                                    const FIntPoint& Dimensions,
                                                    const TSet<int32>& CheckedIndices,
                                                    TSet<int32>& OutTentativelyClaimed,
                                                    const FGameplayTag& ItemTypeTag,
                                                    const int32 MaxStackSize) const
{
	if (!GridSlot) return false;

	const int32 ColumnCount = FMath::TruncToInt(GridSize.X);
	const int32 RowCount = FMath::TruncToInt(GridSize.Y);

	bool bHasRoomAtIndex = true;

	for (int32 Y = 0; Y < Dimensions.Y && bHasRoomAtIndex; ++Y)
	{
		for (int32 X = 0; X < Dimensions.X && bHasRoomAtIndex; ++X)
		{
			const int32 Index = GridSlot->GetIndex() + X + Y * ColumnCount;

			// Bounds check
			if (Index < 0 || Index >= ColumnCount * RowCount)
			{
				bHasRoomAtIndex = false;
				break;
			}

			// Already claimed elsewhere?
			if (CheckedIndices.Contains(Index))
			{
				bHasRoomAtIndex = false;
				break;
			}

			const UAZ_Inv_CommonUI_GridSlot* CandidateSlot = SlotsByIndex.IsValidIndex(Index)
				? SlotsByIndex[Index]
				: nullptr;
			if (!CandidateSlot)
			{
				bHasRoomAtIndex = false;
				break;
			}

			// Empty slot – tentatively claim it
			if (!CandidateSlot->GetInventoryItem().IsValid())
			{
				OutTentativelyClaimed.Add(CandidateSlot->GetIndex());
				continue;
			}

			// Must belong to the same upper-left anchor
			if (CandidateSlot->GetUpperLeftIndex() != GridSlot->GetIndex())
			{
				bHasRoomAtIndex = false;
				break;
			}

			const UAZ_Inv_CommonUI_InventoryItem* SubItem = CandidateSlot->GetInventoryItem().Get();
			if (!SubItem || !SubItem->IsStackable())
			{
				bHasRoomAtIndex = false;
				break;
			}

			// Type must match the incoming item
			if (!SubItem->GetItemManifest().GetItemTypeTag().MatchesTagExact(ItemTypeTag))
			{
				bHasRoomAtIndex = false;
				break;
			}

			// Must have stack room
			if (CandidateSlot->GetStackCount() >= MaxStackSize)
			{
				bHasRoomAtIndex = false;
				break;
			}

			OutTentativelyClaimed.Add(Index);
		}
	}

	return bHasRoomAtIndex;
}

FVector2D UAZ_Inv_CommonUI_InventoryGrid::GetDrawSize(const FAZ_Inv_CommonUI_GridFragment* GridFragment) const
{
	FVector2D BaseTileSize(TileSize, TileSize);

	// Try to fetch the actual size from the first slot if available and layout has occurred
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

		SlottedItem->UpdateStackCount(StackAmount);
	}

	return SlottedItem;
}


bool UAZ_Inv_CommonUI_InventoryGrid::IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const
{
	return CheckedIndices.Contains(Index);
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

int32 UAZ_Inv_CommonUI_InventoryGrid::DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill,
                                                                 const UAZ_Inv_CommonUI_GridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackSize - GetStackAmount(GridSlot);
	return bStackable
		? FMath::Min(AmountToFill, RoomInSlot)
		: 1;
}

void UAZ_Inv_CommonUI_InventoryGrid::AddItemToGridSlots(const FAZ_Inv_CommonUI_SlotAvailabilityResult& SlotAvailabilityResult,
                                                        UAZ_Inv_CommonUI_InventoryItem* NewItem)
{
	for (const auto& AvailableItem : SlotAvailabilityResult.AvailableSlots)
	{
		AddItemAtIndex(NewItem, AvailableItem.Index, SlotAvailabilityResult.bIsStackable, AvailableItem.AmountToFill);
		UpdateGridSlots(NewItem, AvailableItem.Index, SlotAvailabilityResult.bIsStackable, AvailableItem.AmountToFill);
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::AddItemAtIndex(UAZ_Inv_CommonUI_InventoryItem* NewItem, int32 Index, bool bStackable, int32 StackAmount)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(NewItem, Tags.Item_Fragment_Grid);
	const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(NewItem, Tags.Item_Fragment_Icon);
	if (!GridFragment || !ImageFragment) return;

	UAZ_Inv_CommonUI_SlottedItem* SlottedItem = CreateSlottedItem(NewItem, GridFragment, ImageFragment, Index, bStackable, StackAmount);
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
			GridSlot->SetOccupiedTexture();
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

TArray<UAZ_Inv_CommonUI_GridSlot*> UAZ_Inv_CommonUI_InventoryGrid::GetAllGridSlots() const
{
	TArray<UAZ_Inv_CommonUI_GridSlot*> Tmp;
	for (UWidget* Child : InventoryGridPanel->GetAllChildren())
	{
		if (UAZ_Inv_CommonUI_GridSlot* GridSlot = Cast<UAZ_Inv_CommonUI_GridSlot>(Child))
		{
			Tmp.Add(GridSlot);
		}
	}
	return Tmp;
}

void UAZ_Inv_CommonUI_InventoryGrid::AddItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!MatchesCategory(Item)) return;

	FAZ_Inv_CommonUI_SlotAvailabilityResult Result = HasRoomForItem(Item);

	AddItemToGridSlots(Result, Item);
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

				// The Blueprint connects the Outer Loop (Col) to 'InColumn' 
				// and the Inner Loop (Row) to 'InRow'.

				if (UGridSlot* NewSlotVisual = InventoryGridPanel->AddChildToGrid(GridSlot, Row, Col))
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
}

void UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotHovered(UCommonButtonBase* Button)
{
	if (UAZ_Inv_CommonUI_GridSlot* GridSlot = Cast<UAZ_Inv_CommonUI_GridSlot>(Button); GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Occupied);
	}
}

void UAZ_Inv_CommonUI_InventoryGrid::OnGridSlotUnhovered(UCommonButtonBase* Button)
{
	if (UAZ_Inv_CommonUI_GridSlot* GridSlot = Cast<UAZ_Inv_CommonUI_GridSlot>(Button); GridSlot->IsAvailable())
	{
		GridSlot->SetState(EInv_CommonUI_GridSlotState::Unoccupied);
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
