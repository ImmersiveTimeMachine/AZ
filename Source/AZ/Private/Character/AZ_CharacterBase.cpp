#include <AZ/Public/Character/AZ_CharacterBase.h>

#include "AZ_GameplayTags.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_AttributeSet.h"
#include "AZ/AZ.h"
#include "Components/CapsuleComponent.h"
#include "Player/AZ_PlayerState.h"


// Sets default values
AAZ_CharacterBase::AAZ_CharacterBase()
{
	DefaultAttributeSetClass = UAZ_AttributeSet::StaticClass();
	
	PrimaryActorTick.bCanEverTick = true;
	const FAZ_GameplayTags& GameplayTags = FAZ_GameplayTags::Get();
	//CurrentMovementStateTag = GameplayTags.Movement_Idle;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), RightHandSocketName);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AAZ_CharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitDefaultAttributes();
	InitInputAbilities(CharacterInputAbilities);
	SetWeaponAndTag(EWeaponType::WT_None);
}

void AAZ_CharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitDefaultAttributes();
	InitInputAbilities(CharacterInputAbilities);
	SetWeaponAndTag(EWeaponType::WT_None);
}

void AAZ_CharacterBase::InitDefaultAttributes()
{
	AAZ_PlayerState* AzPlayerState = GetPlayerState<AAZ_PlayerState>();
	if (!AzPlayerState) return;

	UAZ_AbilitySystemComponent* AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(AzPlayerState->GetAbilitySystemComponent());
	if (!AbilitySystemComponent || AbilitySystemComponent->GetAvatarActor() == this) return;

	if (DefaultAttributeSetClass)
	{
		UAttributeSet* NewAttributeSet = const_cast<UAttributeSet*>(AbilitySystemComponent->InitStats(DefaultAttributeSetClass, nullptr));
		AzPlayerState->SetAttributeSet(NewAttributeSet);
	}

	AbilitySystemComponent->InitAbilityActorInfo(AzPlayerState, this);
	//AddInitialCharacterAbilities(); // Grant abilities after initialization
}

void AAZ_CharacterBase::InitDefaultAbilities() const
{
	if (!HasAuthority()) return;
	
	UAZ_AbilitySystemComponent* AzASC = Cast<UAZ_AbilitySystemComponent>(GetASC());
	if (!AzASC)
	{
		UE_LOG(Log_AZ, Warning, TEXT("AddInitialCharacterAbilities: Failed to get AZ Ability System Component"));
		return;
	}

	
	AzASC->GrantAbilitiesWithInputTag(StartupAbilities);
	//AzASC->OnMovementStateChanged(DetermineMovementStateFromSpeed(0));
	TArray<FGameplayAbilitySpecHandle> OutAbilityHandles;
	AzASC->GetAllAbilities(OutAbilityHandles);
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : OutAbilityHandles)
	{
		if (AbilitySpecHandle.IsValid())
		{
			AzASC->TryActivateAbility(AbilitySpecHandle);
		}
	}
	
	//AzASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
}

void AAZ_CharacterBase::InitInputAbilities(const TArray<TSubclassOf<UAZ_GameplayAbility>>& InInputAbilities) const
{
	if (!HasAuthority() && !InInputAbilities.IsValidIndex(0))
	{
		return;
	}

	if (const auto EchoAbilitySystemComp = Cast<UAZ_AbilitySystemComponent>(GetASC()))
	{
		EchoAbilitySystemComp->GrantAbilitiesWithInputTag(InInputAbilities);
	}
}

UAbilitySystemComponent* AAZ_CharacterBase::GetASC() const
{
	const AAZ_PlayerState* AzPlayerState = GetPlayerState<AAZ_PlayerState>();
	return AzPlayerState ? AzPlayerState->GetAbilitySystemComponent() : nullptr;
}

UAttributeSet* AAZ_CharacterBase::GetAttributeSet() const
{
	const AAZ_PlayerState* AzPlayerState = GetPlayerState<AAZ_PlayerState>();
	return AzPlayerState ? AzPlayerState->GetAttributeSet() : nullptr;
}

FOnASCRegistered& AAZ_CharacterBase::GetOnASCRegisteredDelegate()
{
	return OnAscRegistered;
}

UAbilitySystemComponent* AAZ_CharacterBase::GetAbilitySystemComponent() const
{
	return GetASC();
}

const TMap<FGameplayTag, FName>& AAZ_CharacterBase::GetEquipmentSocketMap() const
{
	return EquipmentSocketMap;
}

// Called when the game starts or when spawned
void AAZ_CharacterBase::BeginPlay()
{
	Super::BeginPlay();	
}

void AAZ_CharacterBase::ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, const float Level) const
{
	check(IsValid(GetASC()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle = GetASC()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = GetASC()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	GetASC()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetASC());
}

// Called every frame
void AAZ_CharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Determine the new state based on speed
	const FGameplayTag NewMovementTag = DetermineMovementStateFromSpeed(GetVelocity().Size());

	// Notify the ASC of the new state. The ASC will handle the rest.
	/*if (auto* ASC = Cast<UAZ_AbilitySystemComponent>(GetASC()))
	{
		ASC->OnMovementStateChanged(NewMovementTag);
	}*/
}

FGameplayTag AAZ_CharacterBase::DetermineMovementStateFromSpeed(float Speed) const
{
	const FAZ_GameplayTags& MovementTags = FAZ_GameplayTags::Get();
	
	if (Speed > 900.0f)
	{
		return MovementTags.Movement_Sprinting;
	}
	else if (Speed > 210.0f)
	{
		return MovementTags.Movement_Running;
	}
	else if (Speed > 10.0f)
	{
		return MovementTags.Movement_Walking;
	}
	else
	{
		return MovementTags.Movement_Idle;
	}
}

void AAZ_CharacterBase::SetWeaponAndTag(const EWeaponType WeaponType)
{
	ActiveWeaponType = WeaponType;

	const FAZ_GameplayTags& GameplayTags = FAZ_GameplayTags::Get();
	auto* ASC = Cast<UAZ_AbilitySystemComponent>(GetASC());
	if (!ASC) return;

	// Remove old weapon tag if it exists
	if (CurrentWeaponStateTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(CurrentWeaponStateTag);
	}

	// Set new weapon tag based on weapon type
	switch (WeaponType)
	{
	case EWeaponType::WT_Pistol:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_Pistol);
		break;
	case EWeaponType::WT_Rifle:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_Rifle);
		break;
	case EWeaponType::WT_RocketLauncher:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_RocketLauncher);
		break;
	case EWeaponType::WT_Shotgun:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_Shotgun);
		break;
	case EWeaponType::WT_SMG:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_SMG);
		break;
	case EWeaponType::WT_Sniper:
		//CurrentWeaponStateTag = GameplayTags.Weapon_Sniper;
		break;
	case EWeaponType::WT_Melee:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_Melee);
		break;
	case EWeaponType::WT_None:
	default:
		ASC->OnWeaponEquipped(GameplayTags.Weapon_None);
		break;
	}
}

void AAZ_CharacterBase::SwitchToWeaponInSlot(int32 SlotIndex)
{
	if (!HasAuthority()) return; // Switching weapons is a server-authoritative action

	
}
