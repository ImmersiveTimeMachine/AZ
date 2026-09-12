// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "AZ_PlayerUITypes.generated.h"

class UTexture2D;
class UAZ_HUDReticleDefinition;

/** A local readout of the PlayerState's combat vitals, never a second attribute store. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_PlayerVitalsView
{
	GENERATED_BODY()

	/** False means there is no supported reading; numeric fields must not be displayed. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Vitals")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Vitals")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Vitals")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Vitals")
	float Fraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Vitals")
	bool bCritical = false;
};

/** The committed equipment selection and its derived physical-magazine readout. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_PlayerWeaponView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	bool bHasWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	bool bUsesMagazines = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	bool bHasFireMode = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	EAZ_FirearmFireMode SelectedFireMode = EAZ_FirearmFireMode::Single;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	FVector2D IconDimensions = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	FGameplayTag Profile;

	/** Preserve Unavailable, NoMagazine, Empty and Loaded; spares count nonempty backpack magazines. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Weapon")
	FAZ_WeaponAmmoSnapshot Ammo;
};

/** Local presentation of the committed weapon's aiming reference; never a firing permission. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_PlayerReticleView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	bool bAiming = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	FGameplayTag WeaponProfile;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	FGuid WeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	TObjectPtr<UAZ_HUDReticleDefinition> Definition = nullptr;

	/** Full cone angle used by the current firearm shot path; zero for unsupported ballistics. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Reticle")
	float SpreadAngleDegrees = 0.f;
};
