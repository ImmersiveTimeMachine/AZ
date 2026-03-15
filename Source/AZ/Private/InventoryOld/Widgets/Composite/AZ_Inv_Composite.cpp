// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/Composite/AZ_Inv_Composite.h"
#include "Blueprint/WidgetTree.h"

void UAZ_Inv_Composite::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UAZ_Inv_CompositeBase* Composite = Cast<UAZ_Inv_CompositeBase>(Widget); IsValid(Composite))
		{
			Children.Add(Composite);
			Composite->Collapse();
		}
	});
}

void UAZ_Inv_Composite::ApplyFunction(FuncType Function)
{
	for (auto& Child : Children)
	{
		Child->ApplyFunction(Function);
	}
}

void UAZ_Inv_Composite::Collapse()
{
	for (auto& Child : Children)
	{
		Child->Collapse();
	}
}