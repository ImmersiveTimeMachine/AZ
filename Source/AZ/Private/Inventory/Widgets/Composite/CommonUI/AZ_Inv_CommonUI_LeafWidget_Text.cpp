// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_LeafWidget_Text.h"

#include "Components/TextBlock.h"

void UAZ_Inv_CommonUI_LeafWidget_Text::SetText(const FText& Text) const
{
	Text_LeafText->SetText(Text);
}

void UAZ_Inv_CommonUI_LeafWidget_Text::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	FSlateFontInfo FontInfo = Text_LeafText->GetFont();
	FontInfo.Size = FontSize;
	
	Text_LeafText->SetFont(FontInfo);
}
