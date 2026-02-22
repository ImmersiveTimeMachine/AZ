
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_LeafWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget_LabeledValue.generated.h"

class UTextBlock;

/**
 * CommonUI version of labeled value leaf widget.
 * Displays a label-value pair with independent font size controls.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_LeafWidget_LabeledValue : public UAZ_Inv_CommonUI_LeafWidget
{
	GENERATED_BODY()
	
public:

	void SetText_Label(const FText& Text, bool bCollapse) const;
	void SetText_Value(const FText& Text, bool bCollapse) const;
	virtual void NativePreConstruct() override;
	
private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Value;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 FontSize_Label{12};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 FontSize_Value{18};
};
