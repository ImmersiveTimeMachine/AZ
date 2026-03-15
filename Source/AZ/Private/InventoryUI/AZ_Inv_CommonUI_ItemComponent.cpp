// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"

#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UAZ_Inv_CommonUI_ItemComponent::UAZ_Inv_CommonUI_ItemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UAZ_Inv_CommonUI_ItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PickupItemManifest);
}

void UAZ_Inv_CommonUI_ItemComponent::DestroyItem() const
{
	if (auto Owner = GetOwner())
		Owner->Destroy();
}

void UAZ_Inv_CommonUI_ItemComponent::PickedUp()
{
	OnPickedUp();
	GetOwner()->Destroy();
}

void UAZ_Inv_CommonUI_ItemComponent::InitItemManifest(FAZ_Inv_CommonUI_ItemManifest CopyOfManifest)
{
	PickupItemManifest = CopyOfManifest;
}
