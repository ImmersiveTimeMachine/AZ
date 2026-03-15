// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_LeafWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget_Text.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;

UCLASS()
class AZ_API UAZ_Inv_CommonUI_LeafWidget_Text : public UAZ_Inv_CommonUI_LeafWidget
{
	GENERATED_BODY()

public:
	void SetText(const FText& Text) const;
	virtual void NativePreConstruct() override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_LeafText;

	UPROPERTY(EditInstanceOnly, Category = "AZ|Inventory")
	FText PreviewText;

	UPROPERTY(EditInstanceOnly, Category = "AZ|Inventory")
	TSubclassOf<UCommonTextStyle> TextStyle;
};
