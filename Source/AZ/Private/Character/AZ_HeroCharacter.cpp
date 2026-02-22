#include "Character/AZ_HeroCharacter.h"

#include "AbilitySystem/AZ_AbilityFunctionLibrary.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_HeroAttributeSet.h"
#include "AbilitySystem/AttributeSets/AZ_WeaponAttributeSet.h"
#include "AZ/AZ.h"
#include "Camera/CameraComponent.h"
#include "Inventory/AZ_InventoryComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Inventory/AZ_EquipmentManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/AZ_PlayerController.h"
#include "Player/AZ_PlayerState.h"
#include "Sound/SoundCue.h"
#include "Weapon/AZ_Weapon.h"


AAZ_HeroCharacter::AAZ_HeroCharacter()
{
    DefaultAttributeSetClass = UAZ_HeroAttributeSet::StaticClass();
    
    // Set size for collision capsule
    GetCapsuleComponent()->InitCapsuleSize(25.f, 90.0f);
		
    // Don't rotate when the controller rotates. Let that just affect the camera.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    bReplicates = true;

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

    // Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
    // instead of recompiling to adjust them
    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

    // Create a camera boom (pulls in towards the player if there is a collision)
    ThirdPersonCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonCameraBoom"));
    ThirdPersonCameraBoom->SetupAttachment(RootComponent);
    ThirdPersonCameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
    ThirdPersonCameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

    // Create a follow camera
    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(ThirdPersonCameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
    ThirdPersonCamera->bUsePawnControlRotation = false;

    EquipmentManagerComponent = CreateDefaultSubobject<UAZ_EquipmentManagerComponent>(TEXT("EquipmentManagerComponent"));
}

void AAZ_HeroCharacter::OnRep_EquippedWeapon()
{
    // This function runs on the CLIENT after the server has changed the EquippedWeapon.

    // If there was an old weapon, detach it first.
    if (PreviousWeapon)
    {
        // ... logic to detach or hide the previous weapon ...
    }

    // If the new weapon is valid, attach it.
    if (EquippedWeapon)
    {
        EquippedWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponHandSocket"));
    }
    
    // Update the previous weapon for the next time this runs
    PreviousWeapon = EquippedWeapon;
}

void AAZ_HeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AAZ_HeroCharacter, OldImplInventory);
    // Only replicate CurrentWeapon to simulated clients and manually sync CurrentWeeapon with Owner when we're ready.
    // This allows us to predict weapon changing.
    DOREPLIFETIME_CONDITION(AAZ_HeroCharacter, CurrentWeapon, COND_SimulatedOnly);
    // Register the variable for replication.
    DOREPLIFETIME(AAZ_HeroCharacter, EquippedWeapon);
    
}

// SERVER-SIDE INITIALIZATION
void AAZ_HeroCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // --- Link the Components ---
    LinkInventoryAndEquipment();
        
    ApplyEffectToDefaultAttributes();
    //InitDefaultAbilities();
    InitializeHUD();
    //CheckAndPrintAttributes();
}

// CLIENT-SIDE INITIALIZATION
void AAZ_HeroCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // --- Link the Components ---
    LinkInventoryAndEquipment();
        
    ApplyEffectToDefaultAttributes();
    //InitDefaultAbilities();
    InitializeHUD();
    //CheckAndPrintAttributes();
}

void AAZ_HeroCharacter::LinkInventoryAndEquipment() const
{
    if (const AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
    {
        // Get the Inventory Component from the PlayerState
        UAZ_InventoryComponent* InvComp = PS->GetInventoryComponent();

        // Get our own Equipment Manager Component
        UAZ_EquipmentManagerComponent* EquipComp = FindComponentByClass<UAZ_EquipmentManagerComponent>();
        
        if (InvComp && EquipComp)
        {
            // Give the Equipment Manager a reference to the Inventory.
            EquipComp->Initialize(InvComp);
        }
    }
}

void AAZ_HeroCharacter::SetCurrentWeapon(AAZ_Weapon* NewWeapon, AAZ_Weapon* LastWeapon)
{
    if (NewWeapon == LastWeapon)
    {
        return;
    }

    // Cancel active weapon abilities
    if (AZ_AbilitySystemComponent)
    {
        FGameplayTagContainer AbilityTagsToCancel = FGameplayTagContainer(WeaponAbilityTag);
        AZ_AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel);
    }

    if (NewWeapon)
    {

        if (AZ_AbilitySystemComponent)
        {
            // Clear out potential NoWeaponTag
            AZ_AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
        }

        // Weapons coming from OnRep_CurrentWeapon won't have the owner set
        CurrentWeapon = NewWeapon;
        //CurrentWeapon->SetOwningCharacter(this);
        //CurrentWeapon->Equip();
        CurrentWeaponTag = CurrentWeapon->WeaponTag;

        if (AZ_AbilitySystemComponent)
        {
            AZ_AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
        }

        /*NewWeapon->OnPrimaryClipAmmoChanged.AddDynamic(this, &AAZ_HeroCharacter::CurrentWeaponPrimaryClipAmmoChanged);
        NewWeapon->OnSecondaryClipAmmoChanged.AddDynamic(this, &AAZ_HeroCharacter::CurrentWeaponSecondaryClipAmmoChanged);

        if (AZ_AbilitySystemComponent)
        {
            PrimaryReserveAmmoChangedDelegateHandle = AZ_AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UEchoAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType)).AddUObject(this, &AAZ_HeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged);
            SecondaryReserveAmmoChangedDelegateHandle = AZ_AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UEchoAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType)).AddUObject(this, &AAZ_HeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged);
        }*/
    }
}

void AAZ_HeroCharacter::LoadProgress()
{
    //InitializeDefaultAttributes();
    //AddInitialCharacterAbilities();
}

void AAZ_HeroCharacter::ApplyEffectToDefaultAttributes() const
{
    ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
    //ApplyEffectToSelf(DefaultTagsToActivate, 1.f);
    //ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
    //ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

UAZ_HeroAttributeSet* AAZ_HeroCharacter::GetHeroAttributeSet() const
{
    // Get the attribute set from our PlayerState and cast it.
    const AAZ_PlayerState* AzPlayerState = GetPlayerState<AAZ_PlayerState>();
    return AzPlayerState ? Cast<UAZ_HeroAttributeSet>(AzPlayerState->GetAttributeSet()) : nullptr;
}

USkeletalMeshComponent* AAZ_HeroCharacter::GetThirdPersonMesh() const
{
    return GetMesh();
}

bool AAZ_HeroCharacter::IsHeroAlive() const
{
    /*if (IsValid(GetHeroAttributeSet()))
    {
        return GetHeroAttributeSet()->GetHealth() > 0.0f;
    }

    return false;*/
    return true;
}

bool AAZ_HeroCharacter::DoesWeaponExistInInventory(AAZ_Weapon* InWeapon)
{
    for (const AAZ_Weapon* weaponInInventory : OldImplInventory.Weapons)
    {
        if (weaponInInventory && InWeapon && weaponInInventory->GetClass() == InWeapon->GetClass())
        {
            return true;
        }
    }
    
    return false;
}

void AAZ_HeroCharacter::EquipWeapon(AAZ_Weapon* NewWeapon)
{
    if (GetLocalRole() < ROLE_Authority)
    {
        ServerEquipWeapon(NewWeapon);
        SetCurrentWeapon(NewWeapon, CurrentWeapon);
        bChangedWeaponLocally = true;
    }
    else
    {
        SetCurrentWeapon(NewWeapon, CurrentWeapon);
    }
}

void AAZ_HeroCharacter::ServerEquipWeapon_Implementation(AAZ_Weapon* NewWeapon)
{
    EquipWeapon(NewWeapon);
}

bool AAZ_HeroCharacter::ServerEquipWeapon_Validate(AAZ_Weapon* NewWeapon)
{
    return true;
}

bool AAZ_HeroCharacter::AddWeaponToInventory(AAZ_Weapon* NewWeapon)
 {
     // Validate input parameters
     if (!NewWeapon)
     {
         UE_LOG(LogTemp, Warning, TEXT("AAZ_HeroCharacter::AddWeaponToInventory: NewWeapon is null"));
         return false;
     }
 
     // Check if PlayerState exists and is of correct type
     AAZ_PlayerState* AzPlayerState = Cast<AAZ_PlayerState>(GetPlayerState());
     if (!AzPlayerState)
     {
         UE_LOG(LogTemp, Warning, TEXT("AAZ_HeroCharacter::AddWeaponToInventory: PlayerState is null or not of type AAZ_PlayerState"));
         return false;
     }
 
     // Check if weapon is already owned by another character
     if (NewWeapon->GetOwner() && NewWeapon->GetOwner() != this)
     {
         UE_LOG(LogTemp, Warning, TEXT("AAZ_HeroCharacter::AddWeaponToInventory: Weapon is already owned by another actor"));
         return false;
     }
 
     // Check if we have authority for server-side operations
     if (!HasAuthority())
     {
         UE_LOG(LogTemp, Warning, TEXT("AAZ_HeroCharacter::AddWeaponToInventory: No authority to add weapon to inventory"));
         return false;
     }
 
    // Add weapon to inventory
    
    
     /*if (DoesWeaponExistInInventory(NewWeapon))
     {
         USoundCue* PickupSound = NewWeapon->GetPickupSound();
 
         if (PickupSound && IsLocallyControlled())
         {
             UGameplayStatics::SpawnSoundAttached(PickupSound, GetRootComponent());
         }
 
         if (GetLocalRole() < ROLE_Authority)
         {
             return false;
         }
 
         // Create a dynamic instant Gameplay Effect to give the primary and secondary ammo
         UGameplayEffect* GEAmmo = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("Ammo")));
         GEAmmo->DurationPolicy = EGameplayEffectDurationType::Instant;
 
         if (NewWeapon->PrimaryAmmoType != WeaponAmmoTypeNoneTag)
         {
             int32 Idx = GEAmmo->Modifiers.Num();
             GEAmmo->Modifiers.SetNum(Idx + 1);
 
             FGameplayModifierInfo& InfoPrimaryAmmo = GEAmmo->Modifiers[Idx];
             InfoPrimaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetPrimaryClipAmmo());
             InfoPrimaryAmmo.ModifierOp = EGameplayModOp::Additive;
             if (const UAZ_WeaponAttributeSet* WeaponAttributeSet = GetAbilitySystemComponent()->GetSet<UAZ_WeaponAttributeSet>())
             {
                 InfoPrimaryAmmo.Attribute = WeaponAttributeSet->GetReserveAmmoAttributeFromTag(NewWeapon->PrimaryAmmoType);
             }
         }
 
         if (NewWeapon->SecondaryAmmoType != WeaponAmmoTypeNoneTag)
         {
             int32 Idx = GEAmmo->Modifiers.Num();
             GEAmmo->Modifiers.SetNum(Idx + 1);
 
             FGameplayModifierInfo& InfoSecondaryAmmo = GEAmmo->Modifiers[Idx];
             InfoSecondaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetSecondaryClipAmmo());
             InfoSecondaryAmmo.ModifierOp = EGameplayModOp::Additive;
             if (const UAZ_WeaponAttributeSet* WeaponAttributeSet = GetAbilitySystemComponent()->GetSet<UAZ_WeaponAttributeSet>())
             {
                 InfoSecondaryAmmo.Attribute = WeaponAttributeSet->GetReserveAmmoAttributeFromTag(NewWeapon->SecondaryAmmoType);
             }
         }
 
         if (GEAmmo->Modifiers.Num() > 0)
         {
             AZ_AbilitySystemComponent->ApplyGameplayEffectToSelf(GEAmmo, 1.0f, AZ_AbilitySystemComponent->MakeEffectContext());
         }
 
         NewWeapon->Destroy();
 
         return false;
     }
 
     if (GetLocalRole() < ROLE_Authority)
     {
         return false;
     }
 
     Inventory.Weapons.Add(NewWeapon);
     NewWeapon->SetOwningCharacter(this);
     NewWeapon->AddAbilities();
 
     if (bEquipWeapon)
     {
         EquipWeapon(NewWeapon);
         ClientSyncCurrentWeapon(CurrentWeapon);
     }
     */
     return true;
 }

bool AAZ_HeroCharacter::EquipWeaponFromSlot(int InSlot)
{
    //auto Weapon = GetInventory()->GetItemFromSlot(InSlot);

    return true;
}

void AAZ_HeroCharacter::AttachItem(AAZ_Item* Item)
{
    ServerAttachItem(Item);
}

void AAZ_HeroCharacter::ServerAttachItem_Implementation(AAZ_Item* Item)
{
    if (!Item) return;
    
    FName SocketName = EquipmentManagerComponent->TagToSocket.FindRef(Item->ItemDefinition->AllowedSlotTag);

    if (Item)
    {
        Item->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
    }
    
    // Disable collisions
    Item->SetActorEnableCollision(false);
    if (Item->PickupSphere)
    {
        Item->PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Force network update so all clients see the attached state
    Item->ForceNetUpdate();
}

void AAZ_HeroCharacter::ServerSyncCurrentWeapon_Implementation()
{
    
}

bool AAZ_HeroCharacter::ServerSyncCurrentWeapon_Validate()
{
    return true;
}

void AAZ_HeroCharacter::ClientSyncCurrentWeapon_Implementation(AAZ_Weapon* InWeapon)
{
    AAZ_Weapon* LastWeapon = CurrentWeapon;
    CurrentWeapon = InWeapon;
    OnRep_CurrentWeapon(LastWeapon);
}

bool AAZ_HeroCharacter::ClientSyncCurrentWeapon_Validate(AAZ_Weapon* InWeapon)
{   
    return true;
}

void AAZ_HeroCharacter::InitializeHUD()
{
    const auto* AZ_PlayerState = GetPlayerState<AAZ_PlayerState>();
    check(AZ_PlayerState); // Ensure PlayerState itself is valid

    auto AZ_ASC = Cast<UAZ_AbilitySystemComponent>(GetASC());
    check(AZ_ASC); // Ensure the retrieved AbilitySystemComponent is valid

    AttributeSet = AZ_PlayerState->GetAttributeSet();
    if (!AttributeSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to acquire Attribute Set"));
    }

    OnAscRegistered.Broadcast(GetASC());
    
    // AbilitySystemComponent->RegisterGameplayTagEvent(FAuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAuraCharacter::StunTagChanged);

    if (AAZ_PlayerController* AZ_PlayerController = Cast<AAZ_PlayerController>(GetController()))
    {
        
        //AuraHUD->InitOverlay(FHUDInitializationData(AZ_PlayerController, AZ_PlayerState, AZ_ASC, AttributeSet));
    }
}

void AAZ_HeroCharacter::CheckAndPrintAttributes() const
{
    if (const UAZ_HeroAttributeSet* HeroAttributes = Cast<UAZ_HeroAttributeSet>(GetAttributeSet()))
    {
        const FString StrengthString = FString::Printf(TEXT("Current Strength: %f"), HeroAttributes->GetStrength());
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, StrengthString);
    }
    else
    {
        // Handle the case where AttributeSet is not of the expected type
        UE_LOG(LogTemp, Warning, TEXT("AAZ_HeroCharacter: AttributeSet is not of type UAZ_HeroAttributeSet."));
    }
}

void AAZ_HeroCharacter::OnRep_CurrentWeapon(AAZ_Weapon* LastWeapon)
{
    bChangedWeaponLocally = false;
    SetCurrentWeapon(CurrentWeapon, LastWeapon);
}

void AAZ_HeroCharacter::OnRep_Inventory()
{
    if (GetLocalRole() == ROLE_AutonomousProxy && OldImplInventory.Weapons.Num() > 0 && !CurrentWeapon)
    {
        // Since we don't replicate the CurrentWeapon to the owning client, this is a way to ask the Server to sync
        // the CurrentWeapon after it's been spawned via replication from the Server.
        // The weapon spawning is replicated but the variable CurrentWeapon is not on the owning client.
        ServerSyncCurrentWeapon();
    }
}