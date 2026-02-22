#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/AZ_InventoryItemDefinition.h"
#include "AZ_InventoryItemData.generated.h"

USTRUCT(BlueprintType)
struct FAZ_InventoryItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UAZ_InventoryItemDefinition> ItemDefinition;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntPoint TopLeftInGrid;
    
	// ... other mutable state like float Durability ...
};