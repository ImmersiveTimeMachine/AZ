// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "AZ_Inv_EquipActor.generated.h"

UCLASS()
class AZ_API AAZ_Inv_EquipActor : public AActor
{
	GENERATED_BODY()

public:
	AAZ_Inv_EquipActor();

	FGameplayTag GetEquipmentType() const { return EquipmentType; }
	void SetEquipmentType(FGameplayTag Type) { EquipmentType = Type; }

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag EquipmentType;
};
