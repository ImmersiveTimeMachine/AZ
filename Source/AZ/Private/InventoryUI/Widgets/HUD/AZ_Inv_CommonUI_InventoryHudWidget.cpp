// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"

#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InfoMessage.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"


void UAZ_Inv_CommonUI_InventoryHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer()))
	{
		InventoryComponent->OnNoRoomInInventory.AddDynamic(this, &ThisClass::OnNoRoom);
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::OnNoRoom()
{
	if (!IsValid(InfoMessage)) return;

	InfoMessage->SetMessage(FText::FromString("No Room In Inventory."));
}
