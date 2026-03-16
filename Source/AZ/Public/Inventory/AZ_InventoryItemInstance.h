// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AZ_InventoryItemInstance.generated.h"

class UAZ_InventoryItemDefinition;
/**
 * 
 */
UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class AZ_API UAZ_InventoryItemInstance : public UObject
{
	GENERATED_BODY()
	
public:

	// Returns a unique, persistent identifier for this item instance.
	UFUNCTION(BlueprintPure, Category="AZ|Inventory")
	FGuid GetInstanceId() const { return InstanceId; }

	
	// The definition asset that describes this item
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AZ|Item")
	TObjectPtr<UAZ_InventoryItemDefinition> ItemDefinition;

	// ... you could add mutable state here, like Durability, Level, etc. ...

	// Required for replication
	virtual bool IsSupportedForNetworking() const override
	{
		return true;
	}

	/** The coordinate of the top-left slot this item occupies in the inventory grid. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AZ|Inventory")
	FIntPoint TopLeftInGrid;

protected:

	// Ensure the GUID exists after construction/duplication/load
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	// A stable, unique identifier per instance. Generated on first creation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="AZ|Inventory", meta=(AllowPrivateAccess="true"))
	FGuid InstanceId;

	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	
};
