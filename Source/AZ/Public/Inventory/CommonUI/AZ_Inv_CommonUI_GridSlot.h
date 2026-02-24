// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_Button.h"
#include "Components/Image.h"
#include "AZ_Inv_CommonUI_GridSlot.generated.h"

class UAZ_Inv_CommonUI_ItemPopUp;
class UAZ_Inv_CommonUI_InventoryItem;

UENUM(BlueprintType)
enum class EInv_CommonUI_GridSlotState : uint8
{
	Unoccupied,
	Occupied,
	Selected,
	GrayedOut
};


UCLASS()
class AZ_API UAZ_Inv_CommonUI_GridSlot : public UAZ_Inv_CommonUI_Button
{
	GENERATED_BODY()
	
	EInv_CommonUI_GridSlotState CurrentState = EInv_CommonUI_GridSlotState::Unoccupied;

	bool bAvailable{true};
	
	int32 TileIndex;
	int32 StackCount;
	int32 UpperLeftIndex{INDEX_NONE};
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InventoryItem;
	TWeakObjectPtr<UAZ_Inv_CommonUI_ItemPopUp> ItemPopUp;

	/*UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GridSlot;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush UnoccupiedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush OccupiedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush SelectedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	FSlateBrush GrayedOutBrush;*/
	
	EInv_CommonUI_GridSlotState GridSlotState;


protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnClicked() override;

public:

	void SetOccupiedTexture();
	void SetUnoccupiedTexture();
	void SetSelectedTexture();
	void SetGrayedOutTexture();
	
	int32 GetIndex() const { return TileIndex; }
	void SetIndex(const int32 InTileIndex) { TileIndex = InTileIndex; }
	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(const int32 InStackCount) { StackCount = InStackCount; }
	int32 GetUpperLeftIndex() const { return UpperLeftIndex; }
	void SetUpperLeftIndex(const int32 InUpperLeftIndex) { UpperLeftIndex = InUpperLeftIndex; }
	bool IsAvailable() const { return bAvailable; }
	void SetAvailable(const bool bInAvailable) { bAvailable = bInAvailable; }
	
	UAZ_Inv_CommonUI_ItemPopUp* GetItemPopUp() const;
	void SetItemPopUp(UAZ_Inv_CommonUI_ItemPopUp* PopUp);
	void OnItemPopUpDestruct(UCommonUserWidget* Menu);

	void SetState(const EInv_CommonUI_GridSlotState NewState)
	{
		CurrentState = NewState;
		switch (CurrentState)
		{
		case EInv_CommonUI_GridSlotState::Unoccupied:
			//Image_GridSlot->SetBrush(UnoccupiedBrush);
			break;
		case EInv_CommonUI_GridSlotState::Occupied:
			//Image_GridSlot->SetBrush(OccupiedBrush);
			break;
		case EInv_CommonUI_GridSlotState::Selected:
			//Image_GridSlot->SetBrush(SelectedBrush);
			break;
		case EInv_CommonUI_GridSlotState::GrayedOut:
			//Image_GridSlot->SetBrush(GrayedOutBrush);
			break;
		}
	}

	EInv_CommonUI_GridSlotState GetState() const
	{
		return CurrentState;
	}
	
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> GetInventoryItem() const;
	void SetInventoryItem(TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InInventoryItem);
	
	/** Expose the protected base-class delegates */
	FCommonButtonBaseClicked& OnSlotHovered()   { return OnButtonBaseHovered; }
	FCommonButtonBaseClicked& OnSlotUnhovered() { return OnButtonBaseUnhovered; }
	FCommonButtonBaseClicked& OnSlotClicked()   { return OnButtonBaseClicked; }
};
