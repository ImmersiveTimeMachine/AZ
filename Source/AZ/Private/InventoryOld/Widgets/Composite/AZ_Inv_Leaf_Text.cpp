// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/Composite/AZ_Inv_Leaf_Text.h"

#include "Components/TextBlock.h"

void UAZ_Inv_Leaf_Text::SetText(const FText& Text) const
{
	Text_LeafText->SetText(Text);
}

void UAZ_Inv_Leaf_Text::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	FSlateFontInfo FontInfo = Text_LeafText->GetFont();
	FontInfo.Size = FontSize;
	
	Text_LeafText->SetFont(FontInfo);
}
