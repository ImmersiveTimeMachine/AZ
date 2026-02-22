// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AZ_Inv_ProxyMesh.generated.h"

class UAZ_Inv_EquipmentComponent;

UCLASS()
class AZ_API AAZ_Inv_ProxyMesh : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAZ_Inv_ProxyMesh();
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
private:
	
	// This is the mesh on the player-controlled Character.
	TWeakObjectPtr<USkeletalMeshComponent> SourceMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAZ_Inv_EquipmentComponent> EquipmentComponent;

	// This is the proxy mesh we will see in the Inventory Menu.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	FTimerHandle TimerForNextTick;
	void DelayedInitializeOwner();
	void DelayedInitialization();
};
