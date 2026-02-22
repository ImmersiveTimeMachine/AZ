// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/Composite/AZ_Inv_Leaf.h"

void UAZ_Inv_Leaf::ApplyFunction(FuncType Function)
{
	Function(this);
}
