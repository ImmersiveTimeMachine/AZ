// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_InventoryItem.h"
#include "GameFramework/Actor.h"
#include "AZ_PickupItem.generated.h"

class USphereComponent;

UCLASS()
class AZ_API AAZ_PickupItem : public AActor
{
	GENERATED_BODY()

public:

	AAZ_PickupItem();

	// -------------------------------
	// Components
	// -------------------------------

	// Visual mesh (no collision)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Components")
	UStaticMeshComponent* MeshComponent;

	// Overlap interaction volume (query-only)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Components")
	USphereComponent* PickupSphere;

	// Skeletal mesh (no collision)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Components")
	USkeletalMeshComponent* SkeletalMeshComponent;

	// -------------------------------
	// Item data
	// -------------------------------

	// Radius for overlap volume (editable)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Item", meta=(ClampMin="10.0"))
	float PickupRadius = 100.f;

protected:
	
	virtual void BeginPlay() override;

	// -------------------------------
	// Overlap events
	// -------------------------------

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
	                        AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex,
	                        bool bFromSweep,
	                        const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
	                      AActor* OtherActor,
	                      UPrimitiveComponent* OtherComp,
	                      int32 OtherBodyIndex);
	
};
