// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "StructUtils/InstancedStruct.h"
#include "Weapon/AZ_WeaponTypes.h"

#include "AZ_Inv_CommonUI_ItemFragment.generated.h"

/**
 * 
 */

class UAZ_Inv_CommonUI_CompositeBaseWidget;
class UAZ_Inv_CompositeBase;
class UAZ_GameplayAbility;
class UAZ_WeaponAnimationProfile;
class UAZ_HUDReticleDefinition;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

	FAZ_Inv_CommonUI_ItemFragment() = default;
	FAZ_Inv_CommonUI_ItemFragment(const FAZ_Inv_CommonUI_ItemFragment& Other) = default;
	FAZ_Inv_CommonUI_ItemFragment(FAZ_Inv_CommonUI_ItemFragment&& Other) noexcept = default;
	FAZ_Inv_CommonUI_ItemFragment& operator=(const FAZ_Inv_CommonUI_ItemFragment& Other) = default;
	FAZ_Inv_CommonUI_ItemFragment& operator=(FAZ_Inv_CommonUI_ItemFragment&& Other) noexcept = default;
	
	virtual ~FAZ_Inv_CommonUI_ItemFragment() {}

private:

	UPROPERTY(EditAnywhere,Category = "AZ|Inventory")
	FGameplayTag FragmentTag;
	
public:
	
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(const FGameplayTag& InFragmentTag) { FragmentTag = InFragmentTag; }
	
	virtual void Manifest() {}
};	

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_InventoryItem_Fragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const;
protected:
	bool MatchesWidgetTag(const UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_GridFragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

private:
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TObjectPtr<UTexture2D> Icon{nullptr};

	UPROPERTY(EditAnywhere,Category = "AZ|Inventory")
	FIntPoint GridSize{1,1};
	
	UPROPERTY(EditAnywhere,Category = "AZ|Inventory")
	float GridPadding{.0f};

public:
	FIntPoint GetGridSize() const { return GridSize; }
	void SetGridSize(const FIntPoint& InGridSize) { GridSize = InGridSize; }
	
	float GetGridPadding() const { return GridPadding; }
	void SetGridPadding(const float InGridPadding) { GridPadding = InGridPadding; }

};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ImageFragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()
	
	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;
	
	TObjectPtr<UTexture2D> GetIcon() const { return Icon; }
	void SetIcon(const TObjectPtr<UTexture2D>& InIcon) { Icon = InIcon; }

	FVector2D GetIconDimensions() const { return IconDimensions; }
	void SetIconDimensions(const FVector2D& InIconDimensions) { IconDimensions = InIconDimensions; }
	
private:
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FVector2D IconDimensions{100.0f, 100.0f};
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_Text_Fragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()

	FText GetText() const { return FragmentText; }
	void SetText(const FText& Text) { FragmentText = Text; }
	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FText FragmentText;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_Stackable_Fragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

private:

	// How many items can be stacked in this item menu grid?
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 MaxStackSize{1};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 StackCount{1};

public:
	int32 GetMaxStackSize() const { return MaxStackSize; }
	void SetMaxStackSize(const int32 InMaxStackSize) { MaxStackSize = InMaxStackSize; }

	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(const int32 InStackCount) { StackCount = InStackCount; }
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_LabeledNumberFragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()

	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;

	float GetValue() const { return Value; }
	void SetValue(float InValue) { Value = InValue; }

	FText GetLabel() const { return Text_Label; }
	void SetLabel(const FText& InLabel) { Text_Label = InLabel; }

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FText Text_Label{};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float Value{0.f};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	bool bCollapseLabel{false};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	bool bCollapseValue{false};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 MinFractionalDigits{1};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 MaxFractionalDigits{1};
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_RandomizedNumberFragment : public FAZ_Inv_CommonUI_LabeledNumberFragment
{
	GENERATED_BODY()

	virtual void Manifest() override;

	bool bRandomizeOnManifest{true};

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float Min{0};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float Max{0};
};

// Consume Fragments

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ConsumeModifier : public FAZ_Inv_CommonUI_RandomizedNumberFragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ConsumableFragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC);
	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;
	virtual void Manifest() override;
private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FAZ_Inv_CommonUI_ConsumeModifier>> ConsumeModifiers;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_HealthPotionFragment : public FAZ_Inv_CommonUI_ConsumeModifier
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ManaPotionFragment : public FAZ_Inv_CommonUI_ConsumeModifier
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) override;
};

// Equipment

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_EquipModifier : public FAZ_Inv_CommonUI_RandomizedNumberFragment
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) {}
	virtual void OnUnequip(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_StrengthModifier : public FAZ_Inv_CommonUI_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_ArmorModifier : public FAZ_Inv_CommonUI_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_DamageModifier : public FAZ_Inv_CommonUI_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

// Weapon State Fragment - persistence for ammo/fire mode between equip cycles.
// On equip: values → player ASC WeaponAttributeSet
// On unequip: player ASC attributes → saved back to this fragment
// Also holds the weapon actor class — the weapon to spawn when equipped.

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_WeaponStateFragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

	/** New firearms use real magazine inventory items, never the legacy ASC ammo snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon")
	bool bUsesDetachableMagazines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon")
	FName MagazineFamily;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon", meta=(DisplayName="Weapon Actor Class (spawned on equip)"))
	TSubclassOf<AActor> WeaponActorClass = nullptr;

	/** Character animation vocabulary while this inventory weapon is the committed selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon")
	TObjectPtr<UAZ_WeaponAnimationProfile> AnimationProfile = nullptr;

	/** Optional local HUD presentation. No definition means this weapon has no aiming reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|UI")
	TObjectPtr<UAZ_HUDReticleDefinition> ReticleDefinition = nullptr;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	int32 CurrentClipAmmo{30};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	int32 MaxClipAmmo{30};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	int32 CurrentReserveAmmo{90};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	int32 MaxReserveAmmo{90};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	float FireRate{10.f};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	float BaseDamage{20.f};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	float ReloadSpeed{2.f};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	float SpreadBase{1.f};

	/** Baseline FULL cone angle. Accepted-shot recoil adds twice its additional radius to this angle. */
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	float SpreadAim{0.5f};

	/** Shot-driven spread and owner camera kick; ammunition remains on canonical magazine items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(EditCondition="bUsesDetachableMagazines"))
	FAZ_FirearmRecoilSettings Recoil;

	/** Static firearm tuning; magazine inventory items remain the only source of current rounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="1", ForceUnits="cm"))
	float MaxRange = 10000.f;

	/** A real socket on the active weapon mesh is required; there is no component-origin fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Requires a raised posture (timed Ready or explicit RMB precision aim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	bool bRequiresAimToFire = true;

	/** Keep the firing posture this long after an accepted shot; precision aim has its own input lifetime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="0.1", ClampMax="60", ForceUnits="s"))
	float ReadyDurationSeconds = 3.f;

	/** Initial relaxed-to-raised preparation. Already raised weapons use only their normal fire cadence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="0", ClampMax="2", ForceUnits="s"))
	float FirearmRaiseDelaySeconds = 0.12f;

	/** Definition defaults only; an owned firearm's selected mode lives in its item state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(EditCondition="bUsesDetachableMagazines"))
	EAZ_FirearmFireMode DefaultFireMode = EAZ_FirearmFireMode::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(EditCondition="bUsesDetachableMagazines"))
	TArray<EAZ_FirearmFireMode> SupportedFireModes = {EAZ_FirearmFireMode::Single, EAZ_FirearmFireMode::Automatic};

	bool IsFireModeSupported(EAZ_FirearmFireMode Mode) const
	{
		return bUsesDetachableMagazines
			&& (Mode == EAZ_FirearmFireMode::Single || Mode == EAZ_FirearmFireMode::Automatic)
			&& SupportedFireModes.Contains(Mode);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	TObjectPtr<UNiagaraSystem> MuzzleFlash = nullptr;

	/** Optional Cascade muzzle flash for authored weapon packs; Niagara takes priority if both are set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	TObjectPtr<UParticleSystem> CascadeMuzzleFlash = nullptr;

	/** Optional one-shot scenery impact; author the effect to emit outward along local +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire")
	TObjectPtr<UParticleSystem> WorldImpactEffect = nullptr;

	/** Uniform cosmetic scale. Zero disables the scenery impact without affecting the shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="0"))
	float WorldImpactScale = 1.f;

	/** Hearing is emitted by the authoritative accepted-shot operation, independently of audio playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="0"))
	float ShotNoiseLoudness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory|Weapon|Fire", meta=(ClampMin="0", ForceUnits="cm"))
	float ShotNoiseMaxRange = 3000.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	FGameplayTag FireMode;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Weapon")
	bool bInfiniteAmmo{false};

	bool IsWeaponItem() const { return WeaponActorClass != nullptr; }

	void SaveFromASC(class UAbilitySystemComponent* ASC);
	void ApplyToASC(class UAbilitySystemComponent* ASC) const;
};

/** Definition/defaults only. Current rounds live on the unique inventory item state. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_Inv_CommonUI_MagazineFragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AZ|Inventory|Magazine")
	FName MagazineFamily;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AZ|Inventory|Magazine", meta=(ClampMin="1"))
	int32 Capacity = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AZ|Inventory|Magazine", meta=(ClampMin="0"))
	int32 InitialRounds = 30;
};

UENUM(BlueprintType)
enum class EEquipmentState : uint8
{
	None,
	Carried,	// On back / holstered
	Equipped,	// In hand / active
};

class AAZ_Inv_EquipActor;

// EquipmentFragment — pure state + modifiers.
// The EquipmentComponent orchestrates spawning. Actors own their sockets.

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_EquipmentFragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()

	void OnPickup(APlayerController* PC);
	void OnEquip(APlayerController* PC);
	void OnUnequip(APlayerController* PC);
	void OnDrop();

	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;
	virtual void Manifest() override;

	void ReattachActor(FName NewSocket) const;
	void DestroyAttachedActor();
	void SetEquippedActor(AActor* InActor);
	void ResetRuntimeState() { State = EEquipmentState::None; EquippedActor.Reset(); }

	FGameplayTag GetEquipmentType() const { return EquipmentType; }
	EEquipmentState GetState() const { return State; }
	AActor* GetEquippedActor() const { return EquippedActor.Get(); }
	TSubclassOf<AAZ_Inv_EquipActor> GetEquipActorClass() const { return EquipActorClass; }

private:

	EEquipmentState State{EEquipmentState::None};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TArray<TInstancedStruct<FAZ_Inv_CommonUI_EquipModifier>> EquipModifiers;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta=(DisplayName="Prop/Equipment Actor Class"))
	TSubclassOf<AAZ_Inv_EquipActor> EquipActorClass = nullptr;

	TWeakObjectPtr<AActor> EquippedActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag EquipmentType = FGameplayTag::EmptyTag;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_AbilityGrantFragment : public FAZ_Inv_CommonUI_ItemFragment
{
	GENERATED_BODY()

	void OnEquip(APlayerController* PC, AActor* SourceObject = nullptr);
	void OnUnequip(APlayerController* PC);

	const TArray<TSubclassOf<UAZ_GameplayAbility>>& GetAbilitiesToGrant() const { return AbilitiesToGrant; }
	void ResetRuntimeState() { GrantedAbilityHandles.Reset(); }

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory|Abilities")
	TArray<TSubclassOf<UAZ_GameplayAbility>> AbilitiesToGrant;

	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};
