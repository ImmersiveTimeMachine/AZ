// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/ItemPopUp/AZ_Inv_CommonUI_ItemPopUp.h"

#include "CommonButtonBase.h"
#include "InventoryUI/AZ_Inv_CommonUI_Button.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

void UAZ_Inv_CommonUI_ItemPopUp::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ensure(Button_Split);
	ensure(Button_Drop);
	ensure(Button_Consume);
	ensure(Button_Equip);
	ensure(Slider_Split);

	Button_Split->OnButtonClicked().AddDynamic(this, &ThisClass::SplitButtonClicked);
	Button_Drop->OnButtonClicked().AddDynamic(this, &ThisClass::DropButtonClicked);
	Button_Consume->OnButtonClicked().AddDynamic(this, &ThisClass::ConsumeButtonClicked);
	Button_Equip->OnButtonClicked().AddDynamic(this, &ThisClass::EquipButtonClicked);
	Slider_Split->OnValueChanged.AddDynamic(this, &ThisClass::SliderValueChanged);
}

void UAZ_Inv_CommonUI_ItemPopUp::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnDismissed.ExecuteIfBound();
}

int32 UAZ_Inv_CommonUI_ItemPopUp::GetSplitAmount() const
{
	return FMath::RoundToInt32(Slider_Split->GetValue());
}

void UAZ_Inv_CommonUI_ItemPopUp::CollapseSplitButton() const
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

void UAZ_Inv_CommonUI_ItemPopUp::CollapseConsumeButton() const
{
	if (Button_Consume)
	{
		Button_Consume->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::CollapseEquipButton() const
{
	if (Button_Equip)
	{
		Button_Equip->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::SetSliderParams(const float Max, const float Value) const
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

FVector2D UAZ_Inv_CommonUI_ItemPopUp::GetBoxSize() const
{
	return FVector2D(SizeBox_Root->GetWidthOverride(), SizeBox_Root->GetHeightOverride());
}

void UAZ_Inv_CommonUI_ItemPopUp::SplitButtonClicked(UCommonButtonBase* Button)
{
	if (OnSplit.ExecuteIfBound(GetSplitAmount(), GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::DropButtonClicked(UCommonButtonBase* Button)
{
	if (OnDrop.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::ConsumeButtonClicked(UCommonButtonBase* Button)
{
	if (OnConsume.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::EquipButtonClicked(UCommonButtonBase* Button)
{
	if (OnEquip.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UAZ_Inv_CommonUI_ItemPopUp::SliderValueChanged(float Value)
{
	if (Text_SplitAmount)
	{
		Text_SplitAmount->SetText(FText::AsNumber(FMath::RoundToInt32(Value)));
	}
}
