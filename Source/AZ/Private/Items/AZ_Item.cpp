// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/AZ_Item.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Character/AZ_HeroCharacter.h"
#include "Inventory/AZ_InventoryItemDefinition.h"
#include "InventoryOld/Widgets/HUD/AZ_InventoryHudWidget.h"
#include "Player/AZ_PlayerController.h"

namespace
{
	// Simple per-process counter (server-only). Replace with a GameState allocator if you need cross-level persistence.
	static uint32 GNextActorId = 1;
}


AAZ_Item::AAZ_Item()
{
	PrimaryActorTick.bCanEverTick = false; // This actor does not need to tick every frame

	// -------------------------------
	// Replication settings
	// -------------------------------
	bReplicates = true; // Enable replication for this actor
	bNetUseOwnerRelevancy = true; // Actor relevancy is tied to its owner
	SetNetUpdateFrequency(100.f); // High frequency updates for smooth replication

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

void AAZ_Item::ComputeGuid()
{
	// Generate a deterministic GUID based on class path and ActorId
	const FString ObjectPath = GetClass()->GetPathName(); // stable across runs.
	ItemGuid = FGuid::NewDeterministicGuid(ObjectPath, ActorId);
}

void AAZ_Item::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Ensure the pickup sphere radius is valid
	if (PickupSphere)
	{
		PickupSphere->SetSphereRadius(FMath::Max(10.f, PickupRadius), true);
	}
}

void AAZ_Item::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate ActorId on initial spawn and always notify clients
	DOREPLIFETIME_CONDITION_NOTIFY(AAZ_Item, ActorId, COND_InitialOnly, REPNOTIFY_Always);
}

void AAZ_Item::SetAllComponentsCollision(const bool bIsColliding) const
{
	if (PickupSphere)
	{
		PickupSphere->SetCollisionEnabled(bIsColliding ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(bIsColliding ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetCollisionEnabled(bIsColliding ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void AAZ_Item::Multicast_SetCollisionEnabled_Implementation(bool NewState)
{
	SetActorEnableCollision(false);

	// Update the pickup sphere collision state
	if (PickupSphere)
	{
		PickupSphere->SetCollisionEnabled(NewState ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AAZ_Item::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap events
	if (PickupSphere)
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AAZ_Item::HandleBeginOverlap);
		PickupSphere->OnComponentEndOverlap.AddDynamic(this, &AAZ_Item::HandleEndOverlap);
	}

	// Server-only: assign unique ActorId and compute GUID
	if (HasAuthority())
	{
		if (ActorId == 0)
		{
			ActorId = GNextActorId++; // assign once on server
		}
		ComputeGuid();
	}
}

void AAZ_Item::OnRep_ActorId()
{
	// Recompute deterministic GUID when ActorId is replicated
	ComputeGuid();
}

void AAZ_Item::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
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

	const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor);
	if (!Hero)
	{
		return;
	}

	if (const auto EquipmentManagerComponent = Hero->GetEquipmentManagerComponent())
	{
		// Server: track item by GUID
		if (HasAuthority())
		{
			EquipmentManagerComponent->AddOverlappedItemId(ItemGuid);
		}

		// Add item to local overlapped list
		EquipmentManagerComponent->AddOverlappedItem(this);

		// Locally controlled hero: show UI prompt
		if (Hero->IsLocallyControlled())
		{
			if (const AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->ShowPickupMessage(FString("Press 'E' to add item."));
			}

			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString("Press 'E' to add item."));
		}
		else
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString("Press 'E' to add item."));
		}
	}
}

void AAZ_Item::HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex)
{
	const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor);
	if (!Hero)
	{
		return;
	}

	if (const auto EquipmentManagerComponent = Hero->GetEquipmentManagerComponent())
	{
		// Server: remove item from GUID list
		if (HasAuthority())
		{
			EquipmentManagerComponent->RemoveOverlappedItemId(ItemGuid);
		}

		// Remove item from local overlapped list
		EquipmentManagerComponent->RemoveOverlappedItem(this);

		// Locally controlled hero: hide UI prompt
		if (Hero->IsLocallyControlled())
		{
			if (const AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->HidePickupMessage();
			}
		}
	}
}
