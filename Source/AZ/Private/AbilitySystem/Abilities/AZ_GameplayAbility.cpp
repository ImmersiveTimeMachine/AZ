#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_AttributeSet.h"
#include "AbilitySystem/AttributeSets/AZ_HeroAttributeSet.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_HeroCharacter.h"
#include "Character/AZ_HeroPawn.h"
#include "Weapon/AZ_Weapon.h"

UAZ_GameplayAbility::UAZ_GameplayAbility(const FObjectInitializer& ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	ActivationPolicy = EAZAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EAZAbilityActivationGroup::Independent;
}

bool UAZ_GameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (bCannotActivateWhileInteracting)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC->GetTagCount(InteractingTag) > ASC->GetTagCount(InteractingRemovalTag))
		{
			return false;
		}
	}

	if (bSourceObjectMustEqualCurrentWeaponToActivate)
	{
		auto* Hero = Cast<AAZ_HeroCharacter>(ActorInfo->AvatarActor);
		if (Hero && Hero->GetCurrentWeapon() && reinterpret_cast<UObject*>(Hero->GetCurrentWeapon()) == GetSourceObject(Handle, ActorInfo))
		{
			return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
		}
		else
		{
			return false;
		}
	}
	
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

FString UAZ_GameplayAbility::GetDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>%s, </><Level>%d</>"), L"Default Ability Name - LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum LoremIpsum", Level);
}

FString UAZ_GameplayAbility::GetNextLevelDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>Next Level: </><Level>%d</> \n<Default>Causes much more damage. </>"), Level);
}

FString UAZ_GameplayAbility::GetLockedDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>Spell Locked Until Level: %d</>"), Level);
}

AAZ_HeroCharacter* UAZ_GameplayAbility::GetHeroCharacterFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<AAZ_HeroCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

AAZ_HeroPawn* UAZ_GameplayAbility::GetHeroPawnFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<AAZ_HeroPawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

AAZ_Weapon* UAZ_GameplayAbility::GetPrimaryWeapon() const
{
	if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(GetCurrentSourceObject()))
	{
		return Weapon;
	}

	if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		const FName PrimaryTag = FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName();
		TArray<AActor*> AttachedActors;
		AvatarActor->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (Attached->ActorHasTag(PrimaryTag))
			{
				return Cast<AAZ_Weapon>(Attached);
			}
		}
	}

	return nullptr;
}

UAnimMontage* UAZ_GameplayAbility::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	FAbilityMeshMontage AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		return AbilityMeshMontage.Montage;
	}

	return nullptr;
}

void UAZ_GameplayAbility::SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, class UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());

	FAbilityMeshMontage AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		AbilityMeshMontage.Montage = InCurrentMontage;
	}
	else
	{
		CurrentAbilityMeshMontages.Add(FAbilityMeshMontage(InMesh, InCurrentMontage));
	}
}

UObject* UAZ_GameplayAbility::K2_GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	return GetSourceObject(Handle, &ActorInfo);
}

bool UAZ_GameplayAbility::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately)
{
	if (auto* AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		return AZ_AbilitySystemComponent->BatchRPCTryActivateAbility(InAbilityHandle, EndAbilityImmediately);
	}

	return false;
}

void UAZ_GameplayAbility::ExternalEndAbility()
{
	check(CurrentActorInfo);

	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UAZ_GameplayAbility::GetManaCost(float InLevel) const
{
	float ManaCost = 0.f;
	if (const UGameplayEffect* CostEffect = GetCostGameplayEffect())
	{
		for (FGameplayModifierInfo Mod : CostEffect->Modifiers)
		{
			if (Mod.Attribute == UAZ_HeroAttributeSet::GetManaAttribute())
			{
				Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, ManaCost);
				break;
			}
		}
	}
	return ManaCost;
}

float UAZ_GameplayAbility::GetCooldown(float InLevel) const
{
	float Cooldown = 0.f;
	if (const UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect())
	{
		CooldownEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, Cooldown);
	}
	return Cooldown;
}

bool UAZ_GameplayAbility::FindAbillityMeshMontage(USkeletalMeshComponent* InMesh, FAbilityMeshMontage& InAbilityMeshMontage)
{
	for (FAbilityMeshMontage& MeshMontage : CurrentAbilityMeshMontages)
	{
		if (MeshMontage.Mesh == InMesh)
		{
			InAbilityMeshMontage = MeshMontage;
			return true;
		}
	}

	return false;
}
