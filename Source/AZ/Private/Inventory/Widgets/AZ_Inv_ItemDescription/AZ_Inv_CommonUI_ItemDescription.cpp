// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/AZ_Inv_ItemDescription/AZ_Inv_CommonUI_ItemDescription.h"

FVector2D UAZ_Inv_CommonUI_ItemDescription::GetBoxSize() const
{
	return SizeBox->GetDesiredSize();
}

void UAZ_Inv_CommonUI_ItemDescription::SetVisibility(ESlateVisibility InVisibility)
{
	for (auto Child : GetChildren())
	{
		Child->Collapse();
	}
	Super::SetVisibility(InVisibility);
}
