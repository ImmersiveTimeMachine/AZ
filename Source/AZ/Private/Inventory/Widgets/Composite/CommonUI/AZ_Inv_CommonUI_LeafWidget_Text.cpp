// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_LeafWidget_Text.h"

#include "CommonTextBlock.h"

void UAZ_Inv_CommonUI_LeafWidget_Text::SetText(const FText& Text) const
{
	Text_LeafText->SetText(Text);
}

void UAZ_Inv_CommonUI_LeafWidget_Text::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!Text_LeafText)
	{
		return;
	}

	if (TextStyle)
	{
		Text_LeafText->SetStyle(TextStyle);
	}

	if (!PreviewText.IsEmpty())
	{
		Text_LeafText->SetText(PreviewText);
	}
}
