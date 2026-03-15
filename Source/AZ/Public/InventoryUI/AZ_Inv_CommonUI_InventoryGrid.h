// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_GridSlot.h"
#include "AZ_Inv_CommonUI_InventoryComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_SlottedItem.h"
#include "AZ_Inv_CommonUI_InventoryGrid.generated.h"

class UAZ_Inv_CommonUI_ItemPopUp;
class UCanvasPanel;
class UCommonButtonBase;
class UGridPanel;
class UInputAction;
class UScrollBox;
class UBorder;
class USizeBox;
class UAZ_Inv_CommonUI_GridSlot;

UCLASS()
class AZ_API UAZ_Inv_CommonUI_InventoryGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------
	// FUNCTIONS
	// ---------------------------------------------------

	// Optional: Override NativeConstruct to apply your colors/logic immediately in C++
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//protected:
	//virtual FGeometry GetTickSpaceGeometry(const FGeometry& MyGeometry) const override;
	/** * Configures the grid container visual style.
	 * Logic: Hides the background border if bShowBackground is false.
	 */
	
	UFUNCTION()
	void AddItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(const UAZ_Inv_CommonUI_ItemComponent* ItemComponent);
	void AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem);
	bool HasHoverItem() const;
	UAZ_Inv_CommonUI_HoverItem* GetHoverItem() const;
	float GetTileSize() const;
	void ClearHoverItem();
	void SetOwningCanvas(UCanvasPanel* OwningCanvas);
	void HideCursor();
	void ShowCursor();
	void OnHide();
	void DropItem();
	bool HasActivePopUp() const;
	void TryShowContextMenu();
	void SetContextMenuAction(UInputAction* InAction) { ContextMenuAction = InAction; }

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	void SetupGridContainer();

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	virtual void SetupPreviewSlots(bool bIsDesignTime);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	void SetScrollbarStyle();

	// ---------------------------------------------------
	// CONFIGURATION (Matches your "Grid Container" Category)
	// ---------------------------------------------------

	/** The class of the slot widget to spawn (e.g., WBP_InventorySlot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_GridSlot> GridSlotWidgetClass;

	/*UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_CommonUI_Slot>> GridSlots;*/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	bool bShowBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	FVector2D GridSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	float ContentHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	float TileSize{100.0f};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	float BarCornerRadius;

	// ---------------------------------------------------
	// STYLING
	// ---------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	FLinearColor BarNormalColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	FLinearColor BarTransparentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	FLinearColor BarHighlightedColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	FLinearColor BarDragColor;

	// ---------------------------------------------------
	// BINDINGS (WIDGETS)
	// ---------------------------------------------------

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "AZ|Inventory")
	TObjectPtr<UBorder> GridContainerBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "AZ|Inventory")
	TObjectPtr<USizeBox> ItemsSizeBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "AZ|Inventory")
	TObjectPtr<UScrollBox> ItemsScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "AZ|Inventory")
	TObjectPtr<UGridPanel> InventoryGridPanel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "AZ|Inventory")
	TObjectPtr<USizeBox> ContentSizeBox;

	// ---------------------------------------------------
	// LAYOUT CONFIG (merged from Horror)
	// ---------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory", meta = (AllowPrivateAccess = "true"))
	float GridContainerHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory", meta = (AllowPrivateAccess = "true"))
	FVector4 GridPadding; // X=Left, Y=Top, Z=Right, W=Bottom

private:
	bool MatchesCategory(const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(const UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackAmountOverride = -1);

	bool HasRoomAtIndex(const UAZ_Inv_CommonUI_GridSlot* GridSlot, const FIntPoint& Dimensions, const TSet<int32>& CheckedIndices,
	                    TSet<int32>& OutTentativelyClaimed,
	                    const FGameplayTag& ItemTypeTag, const int32 MaxStackSize) const;

	UAZ_Inv_CommonUI_SlottedItem* CreateSlottedItem(UAZ_Inv_CommonUI_InventoryItem* NewItem, const FAZ_Inv_CommonUI_GridFragment* GridFragment,
	                                                const FAZ_Inv_CommonUI_ImageFragment* ImageFragment, int32 Index,
	                                                bool bStackable, int32 StackAmount) const;

	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(const FAZ_Inv_CommonUI_ItemManifest& Manifest, const int32 StackAmountOverride = -1);

	FVector2D GetDrawSize(const FAZ_Inv_CommonUI_GridFragment* GridFragment) const;
	
	void SetSlottedItemImage(const FAZ_Inv_CommonUI_GridFragment* GridFragment, const FAZ_Inv_CommonUI_ImageFragment* ImageFragment,
	                         UAZ_Inv_CommonUI_SlottedItem* SlottedItem) const;

	bool IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const;
	FIntPoint GetItemDimensions(const FAZ_Inv_CommonUI_ItemManifest& Manifest) const;

	int32 GetStackAmount(const UAZ_Inv_CommonUI_GridSlot* GridSlot) const;
	int32 DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill,
	                                 const UAZ_Inv_CommonUI_GridSlot* GridSlot) const;
	
	void AddItemToGridSlots(const FAZ_Inv_CommonUI_SlotAvailabilityResult& SlotAvailabilityResult, UAZ_Inv_CommonUI_InventoryItem* NewItem);
	void AddItemAtIndex(UAZ_Inv_CommonUI_InventoryItem* NewItem, int32 Index, bool bStackable, int32 StackAmount);
	void UpdateGridSlots(UAZ_Inv_CommonUI_InventoryItem* NewItem, int32 Index, bool bStackableItem, int32 StackAmount);
	void AddSlottedItemToPanel(const int32 Index, const FAZ_Inv_CommonUI_GridFragment* GridFragment, UAZ_Inv_CommonUI_SlottedItem* SlottedItem ) const;
	void AssignHoverItem(UAZ_Inv_CommonUI_InventoryItem* InventoryItem, const int32 GridIndex, const int32 PreviousGridIndex);
	
	bool IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AZ|Inventory")
	EInv_ItemCategory ItemCategory;

	UPROPERTY(EditDefaultsOnly, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_GridSlot> GridSlotClass;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_SlottedItem> SlottedItemClass;

	UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_CommonUI_GridSlot>> GridSlots;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_HoverItem> HoverItemClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_HoverItem> HoverItem;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_ItemPopUp> ItemPopUp;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_ItemPopUp> ItemPopUpClass;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FVector2D ItemPopUpOffset;

	TObjectPtr<UInputAction> ContextMenuAction;

	UPROPERTY()
	TMap<int32, TObjectPtr<UAZ_Inv_CommonUI_SlottedItem>> SlottedItems;


	void ConstructGrid();

	UFUNCTION()
	void OnGridSlotClicked(UCommonButtonBase* Button);
	UFUNCTION()
	void OnGridSlotHovered(UCommonButtonBase* Button);
	UFUNCTION()
	void OnGridSlotUnhovered(UCommonButtonBase* Button);
	UFUNCTION()
	void OnSlottedItemClicked(UCommonButtonBase* Button);
	UFUNCTION()
	void OnSlottedItemHovered(UCommonButtonBase* Button);
	UFUNCTION()
	void OnSlottedItemUnhovered(UCommonButtonBase* Button);
	UFUNCTION()
	void OnSlottedItemMouseButtonDown(UCommonButtonBase* Button, FKey PressedKey);

	UFUNCTION()
	void AddStacks(const FAZ_Inv_CommonUI_SlotAvailabilityResult& Result);

	UFUNCTION()
	void OnPopUpMenuDismissed();
	UFUNCTION()
	void OnPopUpMenuSplit(int32 SplitAmount, int32 Index);
	UFUNCTION()
	void OnPopUpMenuConsume(int32 Index);
	UFUNCTION()
	void OnPopUpMenuDrop(int32 Index);
	UFUNCTION()
	void OnPopUpMenuEquip(int32 Index);

	void PutDownOnIndex(int32 Index);
	void PutHoverItemBack();
	void RemoveItemFromGrid(UAZ_Inv_CommonUI_InventoryItem* InventoryItem, int32 GridIndex);
	void PickUp(UAZ_Inv_CommonUI_InventoryItem* ClickedInventoryItem, int32 GridIndex);
	void CreateItemPopUp(int32 GridIndex);
	void DestroyItemPopUp();

	// Stack interaction helpers
	bool IsSameStackable(const UAZ_Inv_CommonUI_InventoryItem* ClickedInventoryItem) const;
	bool ShouldSwapStackCounts(int32 RoomInClickedSlot, int32 HoveredStackCount, int32 MaxStackSize) const;
	void SwapStackCounts(int32 ClickedStackCount, int32 HoveredStackCount, int32 Index);
	bool ShouldConsumeHoverItemStacks(int32 HoveredStackCount, int32 RoomInClickedSlot) const;
	void ConsumeHoverItemStacks(int32 ClickedStackCount, int32 HoveredStackCount, int32 Index);
	bool ShouldFillInStack(int32 RoomInClickedSlot, int32 HoveredStackCount) const;
	void FillInStack(int32 FillAmount, int32 Remainder, int32 Index);
	void SwapWithHoverItem(UAZ_Inv_CommonUI_InventoryItem* ClickedInventoryItem, int32 GridIndex);

	// Highlight system
	void HighlightSlots(int32 Index, const FIntPoint& Dimensions);
	void UnHighlightSlots(int32 Index, const FIntPoint& Dimensions);
	void ChangeHoverType(int32 Index, const FIntPoint& Dimensions, EInv_CommonUI_GridSlotState GridSlotState);

	// Tile tracking
	void UpdateTileParameters(const FVector2D& GridPosition, const FVector2D& MousePosition);
	void OnTileParametersUpdated(const FAZ_Inv_TileParameters& Parameters);
	FIntPoint CalculateHoveredCoordinates(const FVector2D& GridPosition, const FVector2D& MousePosition) const;
	EAZ_Inv_TileQuadrant CalculateTileQuadrant(const FVector2D& GridPosition, const FVector2D& MousePosition) const;
	FIntPoint CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, EAZ_Inv_TileQuadrant Quadrant) const;
	FAZ_Inv_CommonUI_SpaceQueryResult CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions);
	bool CursorExitedGrid(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location);

	// Cursor widget helpers
	UUserWidget* GetVisibleCursorWidget();
	UUserWidget* GetHiddenCursorWidget();

	// Data helpers
	bool HasValidItem(const UAZ_Inv_CommonUI_GridSlot* GridSlot) const;
	bool IsUpperLeftSlot(const UAZ_Inv_CommonUI_GridSlot* GridSlot, const UAZ_Inv_CommonUI_GridSlot* SubGridSlot) const;
	bool DoesItemTypeMatch(const UAZ_Inv_CommonUI_InventoryItem* SubItem, const FGameplayTag& ItemType) const;
	bool CheckSlotConstraints(const UAZ_Inv_CommonUI_GridSlot* GridSlot, const UAZ_Inv_CommonUI_GridSlot* SubGridSlot,
	                          const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
	                          const FGameplayTag& ItemType, int32 MaxStackSize) const;

	// ---------------------------------------------------
	// LOGIC & DATA
	// ---------------------------------------------------

	TWeakObjectPtr<UCanvasPanel> OwningCanvasPanel;

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> CommonUI_InventoryComponent;

	TArray<UAZ_Inv_CommonUI_GridSlot*> GetAllGridSlots() const;

	UPROPERTY()
	TArray<UAZ_Inv_CommonUI_GridSlot*> SlotsByIndex;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UUserWidget> VisibleCursorWidgetClass;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UUserWidget> HiddenCursorWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> VisibleCursorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> HiddenCursorWidget;

	// Tile tracking state
	bool bMouseWithinGrid{false};
	bool bLastMouseWithinGrid{false};
	int32 LastHighlightedIndex{INDEX_NONE};
	FIntPoint LastHighlightedDimensions{FIntPoint::ZeroValue};

	// Cell size tracking for resize handling
	FVector2D LastCellSize{FVector2D::ZeroVector};
	void RefreshSlottedItemImages();

	FAZ_Inv_TileParameters TileParameters;
	FAZ_Inv_TileParameters LastTileParameters;

	int32 ItemDropIndex{INDEX_NONE};
	int32 LastHoveredGridIndex{INDEX_NONE};
	FAZ_Inv_CommonUI_SpaceQueryResult CurrentQueryResult;
};
