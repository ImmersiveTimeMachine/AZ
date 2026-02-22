// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_CommonUI_GameInventoryMenu.generated.h"

class UAZ_Inv_CommonUI_ItemComponent;
/**
 * 
 */
UCLASS(Abstract)
class AZ_API UAZ_Inv_CommonUI_GameInventoryMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	
	virtual FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const;
};
