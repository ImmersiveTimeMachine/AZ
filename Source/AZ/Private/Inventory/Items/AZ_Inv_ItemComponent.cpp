// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Items/AZ_Inv_ItemComponent.h"

#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UAZ_Inv_ItemComponent::UAZ_Inv_ItemComponent()
{
	
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// ...
}

void UAZ_Inv_ItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PickupItemManifest);
}

void UAZ_Inv_ItemComponent::DestroyItem()
{
		if (auto Owner = GetOwner())
			Owner->Destroy();
}

void UAZ_Inv_ItemComponent::PickedUp()
{
	OnPickedUp();
	GetOwner()->Destroy();
}

void UAZ_Inv_ItemComponent::InitItemManifest(FAZ_Inv_ItemManifest CopyOfManifest)
{
	PickupItemManifest = CopyOfManifest;
}