#pragma once

#include <CoreMinimal.h>
#include <UObject/ObjectMacros.h>

DECLARE_LOG_CATEGORY_EXTERN(Log_AZ, Log, All);

#define ACTOR_ROLE_FSTRING *(FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ENetRole"), true)->GetNameStringByValue(GetLocalRole()))
#define GET_ACTOR_ROLE_FSTRING(Actor) *(FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ENetRole"), true)->GetNameStringByValue(Actor->GetLocalRole()))

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define ECC_SkeletalMesh ECollisionChannel::ECC_GameTraceChannel1
#define ECC_HitBox ECollisionChannel::ECC_GameTraceChannel2

#define CUSTOM_DEPTH_RED 250
#define CUSTOM_DEPTH_BLUE 251
#define CUSTOM_DEPTH_TAN 252

#define ECC_Projectile ECollisionChannel::ECC_GameTraceChannel1
#define ECC_Target ECollisionChannel::ECC_GameTraceChannel2
#define ECC_ExcludePlayers ECollisionChannel::ECC_GameTraceChannel3

#define COLLISION_ABILITY						ECollisionChannel::ECC_GameTraceChannel1
#define COLLISION_PROJECTILE					ECollisionChannel::ECC_GameTraceChannel2
#define COLLISION_ABILITYOVERLAPPROJECTILE		ECollisionChannel::ECC_GameTraceChannel3
#define COLLISION_PICKUP						ECollisionChannel::ECC_GameTraceChannel4
#define COLLISION_INTERACTABLE					ECollisionChannel::ECC_GameTraceChannel5

UENUM(BlueprintType)
enum class EAZ_AbilityInputID : uint8
{
	// 0 None
	None				UMETA(DisplayName = "None"),
	// 1 Confirm
	Confirm				UMETA(DisplayName = "Confirm"),
	// 2 Cancel
	Cancel				UMETA(DisplayName = "Cancel"),
	// 3 Sprint
	Sprint				UMETA(DisplayName = "Sprint"),
	// 4 Jump
	Jump				UMETA(DisplayName = "Jump"),
	// 5 PrimaryFire
	PrimaryFire			UMETA(DisplayName = "Primary Fire"),
	// 6 SecondaryFire
	SecondaryFire		UMETA(DisplayName = "Secondary Fire"),
	// 7 Alternate Fire
	AlternateFire		UMETA(DisplayName = "Alternate Fire"),
	// 8 Reload
	Reload				UMETA(DisplayName = "Reload"),
	// 9 NextWeapon
	NextWeapon			UMETA(DisplayName = "Next Weapon"), 
	// 10 PrevWeapon
	PrevWeapon			UMETA(DisplayName = "Previous Weapon"),
	// 11 Interact
	Interact			UMETA(DisplayName = "Interact")
};