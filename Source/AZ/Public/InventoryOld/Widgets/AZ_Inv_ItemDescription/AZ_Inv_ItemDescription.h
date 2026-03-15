// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SizeBox.h"
#include "InventoryOld/Widgets/Composite/AZ_Inv_Composite.h"
#include "AZ_Inv_ItemDescription.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_ItemDescription : public UAZ_Inv_Composite
{
	GENERATED_BODY()
	
public:

	FVector2D GetBoxSize() const;
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox;
};
