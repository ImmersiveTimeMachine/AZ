// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget.h"

void UAZ_Inv_CommonUI_LeafWidget::ApplyFunction(FuncType Function)
{
	Function(this);
}
