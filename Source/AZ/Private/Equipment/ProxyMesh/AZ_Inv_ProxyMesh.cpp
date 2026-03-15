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

	APlayerController* PC = World->GetFirstPlayerController();
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
