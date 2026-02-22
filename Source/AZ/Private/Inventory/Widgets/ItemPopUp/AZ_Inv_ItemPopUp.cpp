// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/ItemPopUp/AZ_Inv_ItemPopUp.h"

#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

void UAZ_Inv_ItemPopUp::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	ensure(Button_Split);
	ensure(Button_Drop);
	ensure(Button_Consume);
	ensure(Slider_Split);

	Button_Split->OnClicked.AddDynamic(this, &ThisClass::SplitButtonClicked);
	Button_Drop->OnClicked.AddDynamic(this, &ThisClass::DropButtonClicked);
	Button_Consume->OnClicked.AddDynamic(this, &ThisClass::ConsumeButtonClicked);
	Slider_Split->OnValueChanged.AddDynamic(this, &ThisClass::SliderValueChanged);
}

void UAZ_Inv_ItemPopUp::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	RemoveFromParent();
}

int32 UAZ_Inv_ItemPopUp::GetSplitAmount() const
{
	return FMath::RoundToInt32(Slider_Split->GetValue());
}

void UAZ_Inv_ItemPopUp::CollapseSplitButton() const
{
	if (Button_Split)
	{
		Button_Split->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Slider_Split)
	{
		Slider_Split->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_SplitAmount)
	{
		Text_SplitAmount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_ItemPopUp::CollapseConsumeButton() const
{
	if (Button_Consume)
	{
		Button_Consume->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_ItemPopUp::SetSliderParams(const float Max, const float Value) const
{
	if (Slider_Split)
	{
		Slider_Split->SetMaxValue(Max);
		Slider_Split->SetMinValue(1.0f);
		Slider_Split->SetValue(Value);
	}
	if (Text_SplitAmount)
	{
		Text_SplitAmount->SetText(FText::AsNumber(FMath::RoundToInt32(Value)));
	}
}

FVector2D UAZ_Inv_ItemPopUp::GetBoxSize() const
{
	return FVector2D(SizeBox_Root->GetWidthOverride(), SizeBox_Root->GetHeightOverride());
}

void UAZ_Inv_ItemPopUp::SplitButtonClicked()
{
	if (OnSplit.ExecuteIfBound(GetSplitAmount(), GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_ItemPopUp::DropButtonClicked()
{
	if (OnDrop.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_ItemPopUp::ConsumeButtonClicked()
{
	if (OnConsume.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_ItemPopUp::SliderValueChanged(float Value)
{
	if (Text_SplitAmount)
	{
		Text_SplitAmount->SetText(FText::AsNumber(FMath::RoundToInt32(Value)));
	}
}
