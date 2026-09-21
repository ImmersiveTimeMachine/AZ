
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_LeafWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget_LabeledValue.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;

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
	TObjectPtr<UCommonTextBlock> Text_Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Value;

	UPROPERTY(EditInstanceOnly, Category = "AZ|Inventory")
	FText PreviewLabel;

	UPROPERTY(EditInstanceOnly, Category = "AZ|Inventory")
	FText PreviewValue;

	// Reusable styles must also be authorable on WidgetTree templates.
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UCommonTextStyle> LabelStyle;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UCommonTextStyle> ValueStyle;
};
