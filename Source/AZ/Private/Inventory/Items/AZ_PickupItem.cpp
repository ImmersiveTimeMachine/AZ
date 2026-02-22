// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Items/AZ_PickupItem.h"

#include "Character/AZ_HeroCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"
#include "Inventory/Widgets/HUD/AZ_InventoryHudWidget.h"
#include "Player/AZ_PlayerController.h"


// Sets default values
AAZ_PickupItem::AAZ_PickupItem()
{
	PrimaryActorTick.bCanEverTick = false; // This actor does not need to tick every frame
	
	// -------------------------------
	// Root: Sphere collision for overlaps
	// -------------------------------
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);

	PickupSphere->InitSphereRadius(PickupRadius); // Set initial radius
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Only overlap queries
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore); // Ignore all channels by default
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Only overlap pawns
	PickupSphere->SetGenerateOverlapEvents(true); // Enable overlap events
	PickupSphere->SetIsReplicated(true); // Replicate to clients
	PickupSphere->bHiddenInGame = true; // Invisible in game; only used for overlap detection

	// -------------------------------
	// Visual mesh (static)
	// -------------------------------
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(PickupSphere); // Attach to root collision
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // No collisions
	MeshComponent->SetGenerateOverlapEvents(false); // Mesh does not trigger overlaps
	MeshComponent->SetCanEverAffectNavigation(false); // Do not affect navmesh

	// -------------------------------
	// Visual mesh (skeletal)
	// -------------------------------
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComponent->SetupAttachment(PickupSphere); // Attach to root collision
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // No collisions
	SkeletalMeshComponent->SetGenerateOverlapEvents(false); // Mesh does not trigger overlaps
	SkeletalMeshComponent->SetCanEverAffectNavigation(false); // Do not affect navmesh
}

void AAZ_PickupItem::BeginPlay()
{
	Super::BeginPlay();

	//const auto* InventoryComponent = FindComponentByClass<UAZ_Inv_ItemComponent>();
	
	const auto* InventoryComponent = FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	ensure(IsValid(InventoryComponent));

	if (PickupSphere)
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AAZ_PickupItem::HandleBeginOverlap);
		PickupSphere->OnComponentEndOverlap.AddDynamic(this, &AAZ_PickupItem::HandleEndOverlap);
	}
}



void AAZ_PickupItem::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
                                            AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp,
                                            int32 OtherBodyIndex,
                                            bool bFromSweep,
                                            const FHitResult& SweepResult)
{
	// Basic sanity
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	if (const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		// Locally controlled hero: show UI prompt
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->ShowPickupMessage(FString("Press 'E' to add item."));
				
				PlayerCtrl->SetActivePickUpActor(this);
			}

			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString("Press 'E' to add item."));
		}
		else
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString("Press 'E' to add item."));
		}
	}
}

void AAZ_PickupItem::HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex)
{
	if (const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		// Locally controlled hero: hide UI prompt
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->HidePickupMessage();

				PlayerCtrl->SetActivePickUpActor(nullptr);
			}
		}
	}
}