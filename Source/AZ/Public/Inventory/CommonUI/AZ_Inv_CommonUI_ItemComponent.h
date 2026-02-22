// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "AZ_Inv_CommonUI_ItemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, editinlinenew)
class AZ_API UAZ_Inv_CommonUI_ItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAZ_Inv_CommonUI_ItemComponent();
	
	float GetPickupRadius() const { return PickupRadius; }
	FString GetPickupMessage() const { return PickupMessage; }
	FAZ_Inv_CommonUI_ItemManifest GetItemManifest() const { return PickupItemManifest; }
	FAZ_Inv_CommonUI_ItemManifest& GetItemManifestMutable() { return PickupItemManifest; }

	UFUNCTION()
	void DestroyItem() const;
	
	void PickedUp();
	void InitItemManifest(FAZ_Inv_CommonUI_ItemManifest CopyOfManifest);

protected:

	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void OnPickedUp();
	
	UPROPERTY(Replicated, EditAnywhere, Category = "AZ|Inventory")
	FAZ_Inv_CommonUI_ItemManifest PickupItemManifest;
	
	// Radius for overlap volume (editable)
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta=(ClampMin="10.0"))
	float PickupRadius = 100.f;

	/*UPROPERTY(Replicated, EditAnywhere, Category = "AZ|Inventory")
	FAZ_Inv_ItemManifest ItemManifest;*/
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FString PickupMessage;
};
