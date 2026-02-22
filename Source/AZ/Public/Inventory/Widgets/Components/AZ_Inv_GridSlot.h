// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "AZ_Inv_GridSlot.generated.h"

class UAZ_Inv_ItemPopUp;
class UAZ_Inv_InventoryItem;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_GridSlotEvent, int32, GridIndex, const FPointerEvent&, MouseEvent);

UENUM(BlueprintType)
enum class EInv_GridSlotState : uint8
{
	Unoccupied,
	Occupied,
	Selected,
	GrayedOut
};

UCLASS()
class AZ_API UAZ_Inv_GridSlot : public UUserWidget
{
	GENERATED_BODY()
	
public:

	void SetState(EInv_GridSlotState NewState)
	{
		CurrentState = NewState;
		switch (CurrentState)
		{
		case EInv_GridSlotState::Unoccupied:
			Image_GridSlot->SetBrush(UnoccupiedBrush);
			break;
		case EInv_GridSlotState::Occupied:
			Image_GridSlot->SetBrush(OccupiedBrush);
			break;
		case EInv_GridSlotState::Selected:
			Image_GridSlot->SetBrush(SelectedBrush);
			break;
		case EInv_GridSlotState::GrayedOut:
			Image_GridSlot->SetBrush(GrayedOutBrush);
			break;
		}
	}

	EInv_GridSlotState GetState() const
	{
		return CurrentState;
	}

	TWeakObjectPtr<UAZ_Inv_InventoryItem> GetInventoryItem() const;
	void SetInventoryItem(TWeakObjectPtr<UAZ_Inv_InventoryItem> InventoryItem);
	
	virtual void NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	void SetOccupiedTexture();
	void SetUnoccupiedTexture();
	void SetSelectedTexture();
	void SetGrayedOutTexture();
	
	FAZ_GridSlotEvent GridSlotClicked;
	FAZ_GridSlotEvent GridSlotHovered;
	FAZ_GridSlotEvent GridSlotUnhovered;

	int32 GetIndex() const { return TileIndex; }
	void SetIndex(const int32 InTileIndex) { TileIndex = InTileIndex; }
	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(const int32 InStackCount) { StackCount = InStackCount; }
	int32 GetUpperLeftIndex() const { return UpperLeftIndex; }
	void SetUpperLeftIndex(const int32 InUpperLeftIndex) { UpperLeftIndex = InUpperLeftIndex; }
	bool IsAvailable() const { return bAvailable; }
	void SetAvailable(const bool bInAvailable) { bAvailable = bInAvailable; }
	
	UAZ_Inv_ItemPopUp* GetItemPopUp() const;
	void SetItemPopUp(UAZ_Inv_ItemPopUp* PopUp);
	void OnItemPopUpDestruct(UUserWidget* Menu);

private:

	int32 TileIndex;
	int32 StackCount;
	int32 UpperLeftIndex{INDEX_NONE};
	TWeakObjectPtr<UAZ_Inv_InventoryItem> InventoryItem;
	TWeakObjectPtr<UAZ_Inv_ItemPopUp> ItemPopUp;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GridSlot;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush UnoccupiedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush OccupiedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush SelectedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush GrayedOutBrush;
	
	EInv_GridSlotState GridSlotState;

private:
	EInv_GridSlotState CurrentState = EInv_GridSlotState::Unoccupied;

	bool bAvailable{true};
};
