// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Throwables/AZ_ThrowableTypes.h"
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
/**
 * The throwable currently READIED on the quick bar, for the HUD.
 *
 * Deliberately separate from the weapon view: a readied throwable is not equipped and does not replace the
 * weapon in the player's hands, so the HUD shows both at once rather than one standing in for the other.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_PlayerThrowableView
{
	GENERATED_BODY()

	/** False means nothing throwable is readied; the HUD hides the whole element rather than showing zero. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	bool bHasThrowable = false;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Units remaining in the readied stack. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	int32 Count = 0;

	/** Where the action is. The HUD shows a different hint while carrying than while aiming, and must not
	 *  offer to cancel a projectile that has already left the hand. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	EAZ_ThrowPhase Phase = EAZ_ThrowPhase::None;

	/** True while the launch corridor is refused, so the HUD can say OBSTRUCTED instead of showing a range
	 *  for a throw that will not happen. A near wall hit is NOT this: that is a perfectly good short throw. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	bool bLaunchBlocked = false;

	/** True when the predicted flight reaches something inside the horizon. False means open sky, and the
	 *  HUD hides both the marker range and the label rather than inventing a landing spot. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	bool bHasContact = false;

	/** Straight-line metres from the accepted release origin to predicted FIRST contact. Real measured data,
	 *  never a blast radius. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	float ContactRangeMetres = 0.f;

	/** Context hint, with the ACTUAL bound keys resolved from Enhanced Input rather than the word "RMB"
	 *  baked into a string that a rebind would make a lie. */
	UPROPERTY(BlueprintReadOnly, Category="AZ|UI|Throwable")
	FText Hint;
};

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
