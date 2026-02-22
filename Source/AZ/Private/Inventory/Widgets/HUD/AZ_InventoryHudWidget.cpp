// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/HUD/AZ_InventoryHudWidget.h"

#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"


void UAZ_InventoryHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UAZ_Inv_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::GetInventoryComponent(GetOwningPlayer()))
	{
		InventoryComponent->OnNoRoomInInventory.AddDynamic(this, &ThisClass::OnNoRoom);
	}
}

void UAZ_InventoryHudWidget::OnNoRoom()
{
	if (!IsValid(InfoMessage)) return;

	InfoMessage->SetMessage(FText::FromString("No Room In Inventory."));
}
