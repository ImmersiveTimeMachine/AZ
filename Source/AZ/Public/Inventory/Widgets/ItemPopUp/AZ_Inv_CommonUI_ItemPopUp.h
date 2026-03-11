// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AZ_Inv_CommonUI_ItemPopUp.generated.h"

class UAZ_Inv_CommonUI_Button;
class UCommonButtonBase;
class USizeBox;
class UTextBlock;
class USlider;

DECLARE_DYNAMIC_DELEGATE(FAZ_CommonUI_PopUpMenuDismissed);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FAZ_CommonUI_PopUpMenuSplit, int32, SplitAmount, int32, Index);
DECLARE_DYNAMIC_DELEGATE_OneParam(FAZ_CommonUI_PopUpMenuDrop, int32, Index);
DECLARE_DYNAMIC_DELEGATE_OneParam(FAZ_CommonUI_PopUpMenuConsume, int32, Index);

/**
 * CommonUI popup menu for item actions (split, drop, consume).
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_ItemPopUp : public UCommonUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	FAZ_CommonUI_PopUpMenuDismissed OnDismissed;
	FAZ_CommonUI_PopUpMenuSplit OnSplit;
	FAZ_CommonUI_PopUpMenuDrop OnDrop;
	FAZ_CommonUI_PopUpMenuConsume OnConsume;

	int32 GetSplitAmount() const;
	void CollapseSplitButton() const;
	void CollapseConsumeButton() const;
	void SetSliderParams(const float Max, const float Value) const;
	FVector2D GetBoxSize() const;
	void SetGridIndex(const int32 Index) { GridIndex = Index; }
	int32 GetGridIndex() const { return GridIndex; }

private:

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_Button> Button_Split;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_Button> Button_Drop;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_Button> Button_Consume;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> Slider_Split;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SplitAmount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_Root;

	int32 GridIndex{INDEX_NONE};

	UFUNCTION()
	void SplitButtonClicked(UCommonButtonBase* Button);

	UFUNCTION()
	void DropButtonClicked(UCommonButtonBase* Button);

	UFUNCTION()
	void ConsumeButtonClicked(UCommonButtonBase* Button);

	UFUNCTION()
	void SliderValueChanged(float Value);
};
