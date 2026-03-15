// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeWidget.h"

#include "Blueprint/WidgetTree.h"

void UAZ_Inv_CommonUI_CompositeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UAZ_Inv_CommonUI_CompositeBaseWidget* Composite = Cast<UAZ_Inv_CommonUI_CompositeBaseWidget>(Widget); IsValid(Composite))
		{
			Children.Add(Composite);
			Composite->Collapse();
		}
	});
}

void UAZ_Inv_CommonUI_CompositeWidget::ApplyFunction(FuncType Function)
{
	for (const auto& Child : Children)
	{
		Child->ApplyFunction(Function);
	}
}

void UAZ_Inv_CommonUI_CompositeWidget::Collapse()
{
	for (const auto& Child : Children)
	{
		Child->Collapse();
	}
}