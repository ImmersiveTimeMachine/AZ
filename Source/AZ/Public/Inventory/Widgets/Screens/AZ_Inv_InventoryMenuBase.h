// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_InventoryMenuBase.generated.h"

class UAZ_Inv_HoverItem;
class UAZ_Inv_ItemComponent;
/**
 * Base class for all inventory screen widgets.
 * Provides common interface for inventory UI implementations.
 */
UCLASS(Abstract)
class AZ_API UAZ_Inv_InventoryMenuBase : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual FAZ_Inv_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_ItemComponent* ItemComponent) const
    {
        return FAZ_Inv_SlotAvailabilityResult();
    }
    
    virtual void OnItemHovered(UAZ_Inv_InventoryItem* Item) {}
    virtual void OnItemUnHovered() {}
    virtual bool HasHoverItem() const { return false; }
    virtual UAZ_Inv_HoverItem* GetHoverItem() const { return nullptr; }
    virtual float GetTileSize() const { return 0.f; }
};
