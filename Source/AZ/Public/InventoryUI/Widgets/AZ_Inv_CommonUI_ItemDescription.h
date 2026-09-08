// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SizeBox.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeWidget.h"
#include "AZ_Inv_CommonUI_ItemDescription.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_InventoryComponent;

/**
 * CommonUI version of item description widget.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_ItemDescription : public UAZ_Inv_CommonUI_CompositeWidget
{
	GENERATED_BODY()

public:

	FVector2D GetBoxSize() const;
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	void ShowItem(UAZ_Inv_CommonUI_InventoryItem* Item, const UAZ_Inv_CommonUI_InventoryComponent* Inventory);

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox;
};
