#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "AZ_Item.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UAZ_InventoryComponent;
class UAZ_InventoryItemDefinition;

UCLASS()
class AAZ_Item : public AActor
{
	GENERATED_BODY()

public:
	AAZ_Item();

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

	// Optional: asset/data describing this item (used by inventory)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Item")
	TObjectPtr<UAZ_InventoryItemDefinition> ItemDefinition;

	// Optional: asset/data describing this item (used by inventory)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Item")
	TSubclassOf<UAZ_InventoryItemDefinition> ItemDefinitionClass;

	// Radius for overlap volume (editable)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Item", meta=(ClampMin="10.0"))
	float PickupRadius = 100.f;

	// -------------------------------
	// Accessors
	// -------------------------------

	UFUNCTION(BlueprintPure, Category="AZ|Inventory")
	int32 GetActorId() const { return ActorId; }

	UFUNCTION(BlueprintPure, Category="AZ|Inventory")
	FGuid GetItemGuid() const { return ItemGuid; }

	// -------------------------------
	// Collision
	// -------------------------------

	UFUNCTION(Category="AZ|Item")
	void SetAllComponentsCollision(bool bIsColliding) const;
	
	// Multicast to enable/disable collision on all clients
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetCollisionEnabled(bool NewState);

protected:
	// -------------------------------
	// Actor lifecycle
	// -------------------------------

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------------------------------
	// Replication
	// -------------------------------

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_ActorId, Category="AZ|Item")
	uint32 ActorId = 0;

	UFUNCTION()
	void OnRep_ActorId();

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

	// -------------------------------
	// Unique item identifier
	// -------------------------------

	/** A stable, unique identifier per instance. Generated on the server. */
	UPROPERTY(VisibleAnywhere, Category = "AZ|Item")
	FGuid ItemGuid;

private:
	// Compute a deterministic GUID for this actor
	void ComputeGuid();
};
