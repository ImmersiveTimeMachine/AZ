#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_GridSlot.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/Items/Fragments/AZ_Inv_ItemFragment.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_InventoryGrid.generated.h"

class UAZ_Inv_ItemPopUp;
class UAZ_Inv_HoverItem;
class UAZ_Inv_SlottedItem;
struct FAZ_Inv_ItemManifest;
class UAZ_Inv_ItemComponent;
class UAZ_Inv_InventoryItem;
class UAZ_Inv_InventoryComponent;
class UCanvasPanel;
class UAZ_Inv_GridSlot;

/**
 * Widget class that represents a grid-based inventory system
 */
UCLASS()
class AZ_API UAZ_Inv_InventoryGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	
	EInv_ItemCategory GetItemCategory() const { return ItemCategory; }

	UFUNCTION()
	void AddItem(UAZ_Inv_InventoryItem* Item);

	FAZ_Inv_SlotAvailabilityResult HasRoomForItem(const UAZ_Inv_ItemComponent* ItemComponent);
	
	void AssignHoverItem(UAZ_Inv_InventoryItem* InventoryItem);
	void SetOwningCanvas(UCanvasPanel* OwningCanvas);
	
	void ShowCursor();
	void HideCursor();
	void OnHide();
	void DropItem();
	void ClearHoverItem();
	bool HasHoverItem() const;
	UAZ_Inv_HoverItem* GetHoverItem() const;
	float GetTileSize() const { return TileSize; }

protected:
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void NativeOnInitialized() override;

private:
		
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 Rows;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 Columns;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float TileSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AZ|Inventory")
	EInv_ItemCategory ItemCategory;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	UPROPERTY(EditDefaultsOnly, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_GridSlot> GridSlotClass;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_SlottedItem> SlottedItemClass;

	UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_GridSlot>> GridSlots;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_HoverItem> HoverItemClass;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UUserWidget> VisibleCursorWidgetClass;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UUserWidget> HiddenCursorWidgetClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_HoverItem> HoverItem;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> VisibleCursorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> HiddenCursorWidget;
	
	UPROPERTY()
	TMap<int32,TObjectPtr<UAZ_Inv_SlottedItem>> SlottedItems;
	
	UPROPERTY()
	TObjectPtr<UAZ_Inv_ItemPopUp> ItemPopUp;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_ItemPopUp> ItemPopUpClass;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FVector2D ItemPopUpOffset;
	
	UFUNCTION()
	void AddStacks(const FAZ_Inv_SlotAvailabilityResult& Result);
	
	UFUNCTION()
	void OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent);

	UFUNCTION()
	void OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent);
	
	UFUNCTION()
	void OnPopUpMenuSplit(int32 SplitAmount, int32 Index);
	
	UFUNCTION()
	void OnPopUpMenuConsume(int32 Index);
	
	UFUNCTION()
	void OnPopUpMenuDrop(int32 Index);
	
	void PutDownOnIndex(int32 Index);
	UUserWidget* GetVisibleCursorWidget();
	UUserWidget* GetHiddenCursorWidget();

	UFUNCTION()
	void OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent);

	UFUNCTION()
	void OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent);
	
	bool CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location);
	
	bool bMouseWithinCanvas;
	bool bLastMouseWithinCanvas;
	int32 LastHighlightedIndex;
	FIntPoint LastHighlightedDimensions;

	TWeakObjectPtr<UAZ_Inv_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<UCanvasPanel> OwningCanvasPanel;

	void ConstructGrid();

	void PutHoverItemBack();
	
	FAZ_Inv_SlotAvailabilityResult HasRoomForItem(const UAZ_Inv_InventoryItem* Item, const int32 StackAmountOverride = -1);
	FAZ_Inv_SlotAvailabilityResult HasRoomForItem(const FAZ_Inv_ItemManifest& Manifest, const int32 StackAmountOverride = -1);
	void AddItemToGridSlots(const FAZ_Inv_SlotAvailabilityResult& SlotAvailabilityResult, UAZ_Inv_InventoryItem* NewItem);
	bool MatchesCategory(const UAZ_Inv_InventoryItem* Item) const;
	FVector2D GetDrawSize(const FAZ_Inv_GridFragment* GridFragment) const;
	void SetSlottedItemImage(const FAZ_Inv_GridFragment* GridFragment, const FAZ_Inv_ImageFragment* ImageFragment, const UAZ_Inv_SlottedItem* SlottedItem);
	UAZ_Inv_SlottedItem* CreateSlottedItem(UAZ_Inv_InventoryItem* NewItem, const FAZ_Inv_GridFragment* GridFragment, const FAZ_Inv_ImageFragment* ImageFragment, int32 Index,
	                                       bool bStackable, int32 StackAmount);
	void AddItemAtIndex(UAZ_Inv_InventoryItem* NewItem, int32 Index, bool bStackable, int32 StackAmount);
	void AddSlottedItemToCanvas(const int32 Index, const FAZ_Inv_GridFragment* GridFragment, UAZ_Inv_SlottedItem* SlottedItem ) const;
	void UpdateGridSlots(UAZ_Inv_InventoryItem* NewItem, int32 Index, bool bStackableItem, int32 StackAmount);
	bool IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const;
	bool IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const;
	bool IsUpperLeftSlot(const UAZ_Inv_GridSlot* GridSlot, const UAZ_Inv_GridSlot* SubGridSlot) const;
	bool DoesItemTypeMatch(const UAZ_Inv_InventoryItem* SubItem, const FGameplayTag& ItemType) const;
	int32 DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill, const UAZ_Inv_GridSlot* GridSlot) const;
	int32 GetStackAmount(const UAZ_Inv_GridSlot* GridSlot) const;
	bool IsRightClick(const FPointerEvent& MouseEvent) const;
	bool IsLeftClick(const FPointerEvent& MouseEvent) const;
	void PickUp(UAZ_Inv_InventoryItem* ClickedInventoryItem, const int32 GridIndex);
	void AssignHoverItem(UAZ_Inv_InventoryItem* InventoryItem, const int32 GridIndex, const int32 PreviousGridIndex);
	void RemoveItemFromGrid(UAZ_Inv_InventoryItem* InventoryItem, const int32 GridIndex);
	void CreateItemPopUp(const int32 GridIndex);
	void HighlightSlots(const int32 Index, const FIntPoint& Dimensions);
	void UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions);
	void UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition);
	void OnTileParametersUpdated(const FAZ_Inv_TileParameters& Parameters);
	FIntPoint CalculateHoveredCoordinates(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	EAZ_Inv_TileQuadrant CalculateTileQuadrant(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	FIntPoint CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EAZ_Inv_TileQuadrant Quadrant) const;
	FAZ_Inv_SpaceQueryResult CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions);
	void ChangeHoverType(const int32 Index, const FIntPoint& Dimensions, EInv_GridSlotState GridSlotState);
	bool IsSameStackable(const UAZ_Inv_InventoryItem* ClickedInventoryItem) const;
	bool ShouldSwapStackCounts(int32 RoomInClickedSlot, int32 HoveredStackCount, int32 MaxStackSize) const;
	void SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldConsumeHoverItemStacks(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const;
	void ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldFillInStack(int32 RoomInClickedSlot, int32 HoveredStackCount) const;
	void FillInStack(int32 FillAmount, int32 Remainder, int32 Index);
	void SwapWithHoverItem(UAZ_Inv_InventoryItem* ClickedInventoryItem, int32 GridIndex);
	
	FIntPoint GetItemDimensions(const FAZ_Inv_ItemManifest& Manifest) const;
	bool HasValidItem(const UAZ_Inv_GridSlot* GridSlot) const;
	
	FAZ_Inv_TileParameters TileParameters;
	FAZ_Inv_TileParameters LastTileParameters;
	
	int32 ItemDropIndex{INDEX_NONE};
	FAZ_Inv_SpaceQueryResult CurrentQueryResult;

	bool HasRoomAtIndex(const UAZ_Inv_GridSlot* GridSlot,
	                    const FIntPoint& Dimensions,
	                    const TSet<int32>& CheckedIndices,
	                    TSet<int32>& OutTentativelyClaimed,
	                    const FGameplayTag& ItemTypeTag,
	                    const int32 MaxStackSize);

	bool CheckSlotConstraints(const UAZ_Inv_GridSlot* GridSlot,
	                          const UAZ_Inv_GridSlot* SubGridSlot,
	                          const TSet<int32>& CheckedIndices,
	                          TSet<int32>& OutTentativelyClaimed,
	                          const FGameplayTag& ItemType,
	                          const int32 MaxStackSize) const;
};