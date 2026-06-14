// Fill out your copyright notice in the Description page of Project Settings.


#include "Equipment/ProxyMesh/AZ_Inv_ProxyMesh.h"

#include "GameFramework/Character.h"
#include "Equipment/Components/AZ_Inv_EquipmentComponent.h"


// Sets default values
AAZ_Inv_ProxyMesh::AAZ_Inv_ProxyMesh()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	Mesh->SetupAttachment(RootComponent);

	EquipmentComponent = CreateDefaultSubobject<UAZ_Inv_EquipmentComponent>("Equipment");
	EquipmentComponent->SetOwningSkeletalMesh(Mesh);
	EquipmentComponent->SetIsProxy(true);
}

// Called when the game starts or when spawned
void AAZ_Inv_ProxyMesh::BeginPlay()
{
	Super::BeginPlay();
	DelayedInitializeOwner();
}

void AAZ_Inv_ProxyMesh::DelayedInitializeOwner()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		DelayedInitialization();
		return;
	}

	// Resolve the OWNING player, not player 0: in 2-player listen-server PIE every proxy mesh resolved to
	// player 0's character regardless of whose inventory it mirrored (audit P1-14). Spawners should set
	// Owner (controller or pawn) or Instigator; GetFirstPlayerController stays as the SP-only fallback.
	APlayerController* PC = nullptr;
	if (AController* OwnerController = Cast<AController>(GetOwner()))
	{
		PC = Cast<APlayerController>(OwnerController);
	}
	else if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		PC = Cast<APlayerController>(OwnerPawn->GetController());
	}
	else if (const APawn* InstigatorPawn = GetInstigator())
	{
		PC = Cast<APlayerController>(InstigatorPawn->GetController());
	}
	if (!IsValid(PC))
	{
		PC = World->GetFirstPlayerController();   // SP/legacy fallback — wrong player in MP, set Owner instead
	}
	if (!IsValid(PC))
	{
		DelayedInitialization();
		return;
	}

	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	if (!IsValid(Character))
	{
		DelayedInitialization();
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!IsValid(CharacterMesh))
	{
		DelayedInitialization();
		return;
	}

	SourceMesh = CharacterMesh;
	Mesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
	Mesh->SetAnimInstanceClass(SourceMesh->GetAnimInstance()->GetClass());

	EquipmentComponent->InitializeOwner(PC);
}

void AAZ_Inv_ProxyMesh::DelayedInitialization()
{
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ThisClass::DelayedInitializeOwner);
	GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDelegate);
}
