// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Items/Fragments/AZ_Inv_CommonUI_Item_Fragment.h"
#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_CompositeBaseWidget.h"

void FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	if (!MatchesWidgetTag(Composite)) return;
	Composite->Expand();
}

bool FAZ_Inv_CommonUI_InventoryItem_Fragment::MatchesWidgetTag(const UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	return Composite->GetFragmentTag().MatchesTagExact(GetFragmentTag());
}

void FAZ_Inv_CommonUI_Image_Fragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
}

void FAZ_Inv_CommonUI_Text_Fragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
}
