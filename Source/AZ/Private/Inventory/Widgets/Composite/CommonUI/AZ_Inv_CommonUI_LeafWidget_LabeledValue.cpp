// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_LeafWidget_LabeledValue.h"

#include "CommonTextBlock.h"

void UAZ_Inv_CommonUI_LeafWidget_LabeledValue::SetText_Label(const FText& Text, bool bCollapse) const
{
	if (bCollapse)
	{
		Text_Label->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text_Label->SetText(Text);
}

void UAZ_Inv_CommonUI_LeafWidget_LabeledValue::SetText_Value(const FText& Text, bool bCollapse) const
{
	if (bCollapse)
	{
		Text_Value->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text_Value->SetText(Text);
}

void UAZ_Inv_CommonUI_LeafWidget_LabeledValue::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!Text_Label || !Text_Value)
	{
		return;
	}

	if (LabelStyle)
	{
		Text_Label->SetStyle(LabelStyle);
	}

	if (ValueStyle)
	{
		Text_Value->SetStyle(ValueStyle);
	}

	if (!PreviewLabel.IsEmpty())
	{
		Text_Label->SetText(PreviewLabel);
	}

	if (!PreviewValue.IsEmpty())
	{
		Text_Value->SetText(PreviewValue);
	}
}
