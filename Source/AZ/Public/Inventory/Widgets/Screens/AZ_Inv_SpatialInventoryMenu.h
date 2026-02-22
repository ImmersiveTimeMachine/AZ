#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_InventoryMenuBase.h"
#include "Components/WidgetSwitcher.h"
#include "AZ_Inv_SpatialInventoryMenu.generated.h"

class UAZ_Inv_EquippedSlottedItem;
class UAZ_Inv_EquippedGridSlot;
class UAZ_Inv_ItemDescription;
class UCanvasPanel;
class UAZ_Inv_ItemComponent;
class UButton;
class UAZ_Inv_InventoryGrid;

/**
 * Spatial inventory widget handling multiple inventory grids (Equippable, Consumable, Craftable).
 */
UCLASS()
class AZ_API UAZ_Inv_SpatialInventoryMenu : public UAZ_Inv_InventoryMenuBase
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	virtual FAZ_Inv_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_ItemComponent* ItemComponent) const override;
	virtual void OnItemHovered(UAZ_Inv_InventoryItem* Item) override;
	virtual void OnItemUnHovered() override;
	virtual bool HasHoverItem() const override;
	virtual UAZ_Inv_HoverItem* GetHoverItem() const override;
	virtual float GetTileSize() const override;

private:
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	/** Switcher to toggle between inventory grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher;

	/** Inventory grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_InventoryGrid> Grid_Equippables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_InventoryGrid> Grid_Consumables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_InventoryGrid> Grid_Craftables;

	/** Buttons to switch between grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Equippable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Consumable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Craftable;

	/** Show the equippable items grid */
	UFUNCTION()
	void ShowEquippables();

    /** Show the consumable items grid */
    UFUNCTION()
    void ShowConsumables();

    /** Show the craftable items grid */
    UFUNCTION()
    void ShowCraftables();
	
	void DisableButton(UButton* Button);
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_ItemDescription> ItemDescriptionClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_ItemDescription> ItemDescription;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_ItemDescription> EquippedItemDescriptionClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_ItemDescription> EquippedItemDescription;

	FTimerHandle DescriptionTimer;
	FTimerHandle EquippedDescriptionTimer;

	UFUNCTION()
	void ShowEquippedItemDescription(UAZ_Inv_InventoryItem* Item);

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DescriptionTimerDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float EquippedDescriptionTimerDelay = 0.5f;
	
	UAZ_Inv_ItemDescription* GetItemDescription();
	UAZ_Inv_ItemDescription* GetEquippedItemDescription();
	

	UFUNCTION()
	void EquippedGridSlotClicked(UAZ_Inv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag);

	UFUNCTION()
	void EquippedSlottedItemClicked(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem);
	
	void SetItemDescriptionSizeAndPosition(UAZ_Inv_ItemDescription* Description, UCanvasPanel* Canvas) const;
	void SetEquippedItemDescriptionSizeAndPosition(UAZ_Inv_ItemDescription* Description, UAZ_Inv_ItemDescription* EquippedDescription, UCanvasPanel* Canvas) const;
	bool CanEquipHoverItem(UAZ_Inv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag) const;
	UAZ_Inv_EquippedGridSlot* FindSlotWithEquippedItem(UAZ_Inv_InventoryItem* EquippedItem) const;
	void ClearSlotOfItem(UAZ_Inv_EquippedGridSlot* EquippedGridSlot);
	void RemoveEquippedSlottedItem(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem);
	void MakeEquippedSlottedItem(UAZ_Inv_EquippedSlottedItem* EquippedSlottedItem, UAZ_Inv_EquippedGridSlot* EquippedGridSlot, UAZ_Inv_InventoryItem* ItemToEquip);
	void BroadcastSlotClickedDelegates(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip) const;

	UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_EquippedGridSlot>> EquippedGridSlots;
	
	/**
	 * Set the active grid and update the corresponding button states.
	 * @param Grid The inventory grid to show.
	 * @param Button The button corresponding to the grid.
	 */
	void SetActiveGrid(UAZ_Inv_InventoryGrid* Grid, UButton* Button);

	/** Helper to get all grids mapped to their buttons for iteration */
	TMap<UAZ_Inv_InventoryGrid*, UButton*> GetGridButtonMap() const;
	
	TWeakObjectPtr<UAZ_Inv_InventoryGrid> ActiveGrid;
};
