#pragma once

#include "CoreMinimal.h"
#include "Items/AZ_Item.h"
#include "AZ_PickupItem.generated.h"

/**
 * World pickup actor for the CommonUI inventory system.
 * Inherits mesh, pickup sphere, GUID, and replication from AAZ_Item.
 * Overrides overlap behavior to work with CommonUI ItemComponent and PlayerController.
 */
UCLASS()
class AZ_API AAZ_PickupItem : public AAZ_Item
{
	GENERATED_BODY()

public:
	AAZ_PickupItem();

protected:
	virtual void BeginPlay() override;

	virtual void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
	                        AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex,
	                        bool bFromSweep,
	                        const FHitResult& SweepResult) override;

	virtual void HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
	                      AActor* OtherActor,
	                      UPrimitiveComponent* OtherComp,
	                      int32 OtherBodyIndex) override;
};
