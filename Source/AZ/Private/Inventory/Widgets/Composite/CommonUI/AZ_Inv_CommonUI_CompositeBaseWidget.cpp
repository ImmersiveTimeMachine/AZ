// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/Composite/CommonUI/AZ_Inv_CommonUI_CompositeBaseWidget.h"

void UAZ_Inv_CommonUI_CompositeBaseWidget::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAZ_Inv_CommonUI_CompositeBaseWidget::Expand()
{
	SetVisibility(ESlateVisibility::Visible);
}