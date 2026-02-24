// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SizeBox.h"
#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_CompositeWidget.h"
#include "AZ_Inv_CommonUI_ItemDescription.generated.h"

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

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox;
};
