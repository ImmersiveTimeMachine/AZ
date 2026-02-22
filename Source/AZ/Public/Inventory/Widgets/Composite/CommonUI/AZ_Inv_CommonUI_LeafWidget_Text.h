// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_LeafWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget_Text.generated.h"

class UTextBlock;

/**
 * CommonUI version of text leaf widget.
 * Provides functionality for displaying text with configurable font size.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_LeafWidget_Text : public UAZ_Inv_CommonUI_LeafWidget
{
	GENERATED_BODY()
	
public:
	void SetText(const FText& Text) const;
	virtual void NativePreConstruct() override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_LeafText;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 FontSize{12};
};
