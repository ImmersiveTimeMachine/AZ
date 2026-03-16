// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AZ_Weapon.h"

#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/TargetActors/AZ_GATA_LineTrace.h"
#include "AbilitySystem/TargetActors/AZ_GATA_SphereTrace.h"
#include "Character/AZ_HeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AAZ_Weapon::AAZ_Weapon()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	NetUpdateFrequency = 100.0f; // Set this to a value that's appropriate for your game
	bSpawnWithCollision = true;

	/*ObjectLiftingZone = CreateDefaultSubobject<UCapsuleComponent>(FName("ObjectLiftingZone"));
	ObjectLiftingZone->InitCapsuleSize(40.0f, 50.0f);
	ObjectLiftingZone->SetCollisionObjectType(COLLISION_PICKUP);
	ObjectLiftingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Manually enable when in pickup mode
	ObjectLiftingZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	ObjectLiftingZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = ObjectLiftingZone;*/
	
	CollisionComp = CreateDefaultSubobject<UCapsuleComponent>(FName("CollisionComponent"));
	CollisionComp->InitCapsuleSize(40.0f, 50.0f);
	CollisionComp->SetCollisionObjectType(COLLISION_PICKUP);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Manually enable when in pickup mode
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = CollisionComp;

	WeaponMesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh1P"));
	WeaponMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh1P->CastShadow = false;
	WeaponMesh1P->SetVisibility(false, true);
	WeaponMesh1P->SetupAttachment(CollisionComp);
	WeaponMesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	WeaponMesh3PickupRelativeLocation = FVector(0.0f, -25.0f, 0.0f);

	WeaponMesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh3P"));
	WeaponMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh3P->SetupAttachment(CollisionComp);
	WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
	WeaponMesh3P->CastShadow = true;
	WeaponMesh3P->SetVisibility(true, true);
	WeaponMesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	
	// Create the weapon's own Ability System Component.
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Create the weapon's own Attribute Set, which will be managed by its ASC.
	AttributeSet = CreateDefaultSubobject<UAZ_WeaponAttributeSet>("AttributeSet");

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	const auto& GameplayTags = FAZ_GameplayTags::Get();
	PrimaryAmmoType = GameplayTags.Abilities_Type_None;
	SecondaryAmmoType = GameplayTags.Abilities_Type_None;
}

USkeletalMeshComponent* AAZ_Weapon::GetWeaponMesh1P() const
{
	return WeaponMesh1P;
}

USkeletalMeshComponent* AAZ_Weapon::GetWeaponMesh3P() const
{
	return WeaponMesh3P;
}

void AAZ_Weapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AAZ_Weapon, OwningCharacter, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, PrimaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, MaxPrimaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, SecondaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, MaxSecondaryClipAmmo, COND_OwnerOnly);
}

void AAZ_Weapon::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AAZ_Weapon, PrimaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
	DOREPLIFETIME_ACTIVE_OVERRIDE(AAZ_Weapon, SecondaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
}

void AAZ_Weapon::SetOwningCharacter(AAZ_HeroCharacter* InOwningCharacter)
{
	OwningCharacter = InOwningCharacter;
	if (OwningCharacter)
	{
		// Called when added to inventory
		//AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());
		SetOwner(InOwningCharacter);
		// Attach the weapon
		//ObjectLiftingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
		auto res = AttachToComponent(OwningCharacter->GetThirdPersonMesh(), AttachmentRules, CarrySocketName);
		UE_LOG(LogTemp, Warning, TEXT("Attached To Component:  %s"), res ? TEXT("Success") : TEXT("Failed")
);
		/*AttachToComponent(OwningCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);*/

		/*if (OwningCharacter->GetCurregentWeapon() != this)
		{
			WeaponMesh3P->CastShadow = false;
			WeaponMesh3P->SetVisibility(true, true);
			WeaponMesh3P->SetVisibility(false, true);
		}*/
	}
	else
	{
		//AZ_AbilitySystemComponent = nullptr;
		SetOwner(nullptr);
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AAZ_Weapon::NotifyActorBeginOverlap(class AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	/*if (const auto ASC = Cast<AAZ_HeroCharacter>(OtherActor)->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(WeaponTag);	
	}
	
	if (IsValid(this) && !OwningCharacter)
    {
    	PickUpOnTouch(Cast<AAZ_HeroCharacter>(OtherActor));
    }*/
}

void AAZ_Weapon::NotifyActorEndOverlap(class AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	/*if (const auto ASC = Cast<AAZ_HeroCharacter>(OtherActor)->GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(WeaponTag);	
	}*/
}

void AAZ_Weapon::MakeCosmetic()
{
	bIsCosmeticOnly = true;
	// Disable collision and any other gameplay-relevant logic.
	SetActorEnableCollision(false);
	// You might also disable tick if it does anything important.
	SetActorTickEnabled(false); 
}

/*void AAZ_Weapon::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& OverlapInfo)
{
	if (const auto HeroCharacter = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (const auto ASC = HeroCharacter->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(EligibleToCollectTag);
			HeroCharacter->AddOverlappingWeapon(this);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString("Press 'E' to add item."));
		}
	}
}

void AAZ_Weapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const auto HeroCharacter = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (const auto ASC = HeroCharacter->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(EligibleToCollectTag);
			HeroCharacter->RemoveOverlappingWeapon(this);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString("Item Not Overlapping, Move closer."));
		}
	}
}*/

void AAZ_Weapon::Equip()
{
	if (!OwningCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s %s OwningCharacter is nullptr"), *FString(__FUNCTION__), *GetName());
		return;
	}

	if (WeaponMesh3P)
	{
		WeaponMesh3P->AttachToComponent(OwningCharacter->GetThirdPersonMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, RelaxedSocketName);
		WeaponMesh3P->SetRelativeLocation(WeaponMesh3PEquippedRelativeLocation);
		WeaponMesh3P->SetRelativeRotation(FRotator(0, 0, -90.0f));
		WeaponMesh3P->CastShadow = true;
		WeaponMesh3P->bCastHiddenShadow = true;

		if (OwningCharacter->IsInFirstPersonPerspective())
		{
			WeaponMesh3P->SetVisibility(true, true); // Without this, the weapon's 3p shadow doesn't show
			WeaponMesh3P->SetVisibility(false, true);
		}
		else
		{
			WeaponMesh3P->SetVisibility(true, true);
		}
	}
}

void AAZ_Weapon::UnEquip()
{
	if (OwningCharacter == nullptr)
	{
		return;
	}

	WeaponMesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	WeaponMesh3P->CastShadow = false;
	WeaponMesh3P->bCastHiddenShadow = false;
	WeaponMesh3P->SetVisibility(true, true); // Without this, the unequipped weapon's 3p shadow hangs around
	WeaponMesh3P->SetVisibility(false, true);
}

void AAZ_Weapon::AddAbilities()
{
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}

	auto* ASC = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());

	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("%s %s Role: %s ASC is null"), *FString(__FUNCTION__), *GetName(), GET_ACTOR_ROLE_FSTRING(OwningCharacter));
		return;
	}

	// Grant abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	for (TSubclassOf<UAZ_GameplayAbility>& GPAbility : Abilities)
	{
		const auto Ability = Cast<UAZ_GameplayAbility>(GPAbility.GetDefaultObject());
		auto AbilitySpec = FGameplayAbilitySpec(Ability, GetAbilityLevel(Ability->EchoAbilityID), static_cast<int32>(Ability->EchoAbilityInputID), this);
		AbilitySpec.DynamicAbilityTags.AddTag(Ability->InputTag);
		AbilitySpecHandles.Add(ASC->GiveAbility(AbilitySpec));
	}
	
	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(OwningCharacter->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}
	}
}

void AAZ_Weapon::RemoveAbilities()
{
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}

	auto* ASC = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());

	if (!ASC)
	{
		return;
	}

	// Remove abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	for (FGameplayAbilitySpecHandle& SpecHandle : AbilitySpecHandles)
	{
		ASC->ClearAbility(SpecHandle);
	}
}

void AAZ_Weapon::ResetWeapon()
{
	FireMode = DefaultFireMode;
	StatusText = DefaultStatusText;
}

void AAZ_Weapon::OnDropped_Implementation(FVector NewLocation)
{
	SetOwningCharacter(nullptr);
	ResetWeapon();

	SetActorLocation(NewLocation);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (WeaponMesh1P)
	{
		WeaponMesh1P->AttachToComponent(CollisionComp, FAttachmentTransformRules::SnapToTargetIncludingScale);
		WeaponMesh1P->SetVisibility(false, true);
	}

	if (WeaponMesh3P)
	{
		WeaponMesh3P->AttachToComponent(CollisionComp, FAttachmentTransformRules::SnapToTargetIncludingScale);
		WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
		WeaponMesh3P->CastShadow = true;
		WeaponMesh3P->SetVisibility(true, true);
	}
}

bool AAZ_Weapon::OnDropped_Validate(FVector NewLocation)
{
	return true;
}

int32 AAZ_Weapon::GetPrimaryClipAmmo() const
{
	return PrimaryClipAmmo;
}

int32 AAZ_Weapon::GetMaxPrimaryClipAmmo() const
{
	return MaxPrimaryClipAmmo;
}

int32 AAZ_Weapon::GetSecondaryClipAmmo() const
{
	return SecondaryClipAmmo;
}

int32 AAZ_Weapon::GetMaxSecondaryClipAmmo() const
{
	return MaxSecondaryClipAmmo;
}

void AAZ_Weapon::SetPrimaryClipAmmo(int32 NewPrimaryClipAmmo)
{
	int32 OldPrimaryClipAmmo = PrimaryClipAmmo;
	PrimaryClipAmmo = NewPrimaryClipAmmo;
	OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AAZ_Weapon::SetMaxPrimaryClipAmmo(int32 NewMaxPrimaryClipAmmo)
{
	int32 OldMaxPrimaryClipAmmo = MaxPrimaryClipAmmo;
	MaxPrimaryClipAmmo = NewMaxPrimaryClipAmmo;
	OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AAZ_Weapon::SetSecondaryClipAmmo(int32 NewSecondaryClipAmmo)
{
	int32 OldSecondaryClipAmmo = SecondaryClipAmmo;
	SecondaryClipAmmo = NewSecondaryClipAmmo;
	OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AAZ_Weapon::SetMaxSecondaryClipAmmo(int32 NewMaxSecondaryClipAmmo)
{
	int32 OldMaxSecondaryClipAmmo = MaxSecondaryClipAmmo;
	MaxSecondaryClipAmmo = NewMaxSecondaryClipAmmo;
	OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

bool AAZ_Weapon::HasInfiniteAmmo() const
{
	return bInfiniteAmmo;
}

UAnimMontage* AAZ_Weapon::GetEquip1PMontage() const
{
	return Equip1PMontage;
}

UAnimMontage* AAZ_Weapon::GetEquip3PMontage() const
{
	return Equip3PMontage;	
}

class USoundCue* AAZ_Weapon::GetPickupSound() const
{
	return PickupSound;
}

FText AAZ_Weapon::GetDefaultStatusText() const
{
	return DefaultStatusText;
}

AAZ_GATA_LineTrace* AAZ_Weapon::GetLineTraceTargetActor()
{
	if (LineTraceTargetActor)
	{
		return LineTraceTargetActor;
	}

	LineTraceTargetActor = GetWorld()->SpawnActor<AAZ_GATA_LineTrace>();
	LineTraceTargetActor->SetOwner(this);
	return LineTraceTargetActor;
}

AAZ_GATA_SphereTrace* AAZ_Weapon::GetSphereTraceTargetActor()
{
	if (SphereTraceTargetActor)
	{
		return SphereTraceTargetActor;
	}

	SphereTraceTargetActor = GetWorld()->SpawnActor<AAZ_GATA_SphereTrace>();
	SphereTraceTargetActor->SetOwner(this);
	return SphereTraceTargetActor;
}

void AAZ_Weapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (LineTraceTargetActor)
	{
		LineTraceTargetActor->Destroy();
	}

	if (SphereTraceTargetActor)
	{
		SphereTraceTargetActor->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void AAZ_Weapon::PickUpOnTouch(AAZ_HeroCharacter* InCharacter)
{
	if (!InCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter is null."));
		return;
	}

	if (!InCharacter->IsHeroAlive())
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter is not alive."));
		return;
	}

	if (!InCharacter->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter does not have an Ability System Component."));
		return;
	}

	if (InCharacter->GetAbilitySystemComponent()->HasAnyMatchingGameplayTags(RestrictedPickupTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter has restricted pickup tags."));
		return;
	}

	InCharacter->AddWeaponToInventory(this);
	
	/*if (InCharacter->AddWeaponToInventory(this, true) && OwningCharacter->IsInFirstPersonPerspective())
	{
		WeaponMesh3P->CastShadow = false;
		WeaponMesh3P->SetVisibility(true, true);
		WeaponMesh3P->SetVisibility(false, true);
	}*/
}

void AAZ_Weapon::OnRep_PrimaryClipAmmo(int32 OldPrimaryClipAmmo)
{
	OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AAZ_Weapon::OnRep_MaxPrimaryClipAmmo(int32 OldMaxPrimaryClipAmmo)
{
	OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AAZ_Weapon::OnRep_SecondaryClipAmmo(int32 OldSecondaryClipAmmo)
{
	OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AAZ_Weapon::OnRep_MaxSecondaryClipAmmo(int32 OldMaxSecondaryClipAmmo)
{
	OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

class UAbilitySystemComponent* AAZ_Weapon::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// Called when the game starts or when spawned
void AAZ_Weapon::BeginPlay()
{
	ResetWeapon();

	if (!OwningCharacter && bSpawnWithCollision)
	{
		// Spawned into the world without an owner, enable collision as we are in pickup mode
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	
	Super::BeginPlay();

	//CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAZ_Weapon::OnSphereBeginOverlap);
	//CollisionComp->OnComponentEndOverlap.AddDynamic(this, &AAZ_Weapon::OnSphereEndOverlap);
}

// Called every frame
void AAZ_Weapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

