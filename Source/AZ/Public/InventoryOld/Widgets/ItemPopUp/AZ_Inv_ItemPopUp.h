// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AZ_Inv_ItemPopUp.generated.h"

class USizeBox;
class UTextBlock;
class USlider;
class UButton;

DECLARE_DYNAMIC_DELEGATE_TwoParams(FAZ_PopUpMenuSplit, int32, SplitAmount, int32, Index);
DECLARE_DYNAMIC_DELEGATE_OneParam(FAZ_PopUpMenuDrop, int32, Index);
DECLARE_DYNAMIC_DELEGATE_OneParam(FAZ_PopUpMenuConsume, int32, Index);


/**
	On right click
 */
UCLASS()
class AZ_API UAZ_Inv_ItemPopUp : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	
	FAZ_PopUpMenuSplit OnSplit;
	FAZ_PopUpMenuDrop OnDrop;
	FAZ_PopUpMenuConsume OnConsume;

	int32 GetSplitAmount() const;
	void CollapseSplitButton() const;
	void CollapseConsumeButton() const;
	void SetSliderParams(const float Max, const float Value) const;
	FVector2D GetBoxSize() const;
	void SetGridIndex(const int32 Index) { GridIndex = Index; }
	int32 GetGridIndex() const { return GridIndex; }
	
private:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Split;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Drop;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Consume;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> Slider_Split;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SplitAmount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_Root;
	
	int32 GridIndex{INDEX_NONE};
	
	UFUNCTION()
	void SplitButtonClicked();

	UFUNCTION()
	void DropButtonClicked();

	UFUNCTION()
	void ConsumeButtonClicked();

	UFUNCTION()
	void SliderValueChanged(float Value);
};
