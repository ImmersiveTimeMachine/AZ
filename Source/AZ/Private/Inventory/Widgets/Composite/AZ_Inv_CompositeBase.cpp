// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/Composite/AZ_Inv_CompositeBase.h"

void UAZ_Inv_CompositeBase::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAZ_Inv_CompositeBase::Expand()
{
	SetVisibility(ESlateVisibility::Visible);
}
