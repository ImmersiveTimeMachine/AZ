// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "AZ_Inv_ItemFragment.generated.h"

class UAZ_Inv_CompositeBase;

USTRUCT(BlueprintType)
struct FAZ_Inv_ItemFragment
{
	GENERATED_BODY()

	FAZ_Inv_ItemFragment() = default;
	FAZ_Inv_ItemFragment(const FAZ_Inv_ItemFragment& Other) = default;
	FAZ_Inv_ItemFragment(FAZ_Inv_ItemFragment&& Other) noexcept = default;
	FAZ_Inv_ItemFragment& operator=(const FAZ_Inv_ItemFragment& Other) = default;
	FAZ_Inv_ItemFragment& operator=(FAZ_Inv_ItemFragment&& Other) noexcept = default;
	
	virtual ~FAZ_Inv_ItemFragment() {}

private:

	UPROPERTY(EditAnywhere,Category = "AZ|Inventory")
	FGameplayTag FragmentTag;
	
public:
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(const FGameplayTag& InFragmentTag) { FragmentTag = InFragmentTag; }
	virtual void Manifest() {}
};	

/*
 * Item fragment specifically for assimilation into a widget.
 */

USTRUCT(BlueprintType)
struct FAZ_Inv_InventoryItemFragment : public FAZ_Inv_ItemFragment
{
	GENERATED_BODY()

	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const;
protected:
	bool MatchesWidgetTag(const UAZ_Inv_CompositeBase* Composite) const;
};

/*USTRUCT(BlueprintType)
struct FInv_GridFragment : public FAZ_Inv_ItemFragment
{
	GENERATED_BODY()

	FIntPoint GetGridSize() const { return GridSize; }
	void SetGridSize(const FIntPoint& Size) { GridSize = Size; }
	float GetGridPadding() const { return GridPadding; }
	void SetGridPadding(float Padding) { GridPadding = Padding; }

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FIntPoint GridSize{1, 1};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float GridPadding{0.f};
	
};*/

USTRUCT(BlueprintType)
struct FAZ_Inv_GridFragment : public FAZ_Inv_ItemFragment
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
struct FAZ_Inv_ImageFragment : public FAZ_Inv_InventoryItemFragment
{
	GENERATED_BODY()
	
	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const override;
	
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
struct FAZ_Inv_TextFragment : public FAZ_Inv_InventoryItemFragment
{
	GENERATED_BODY()

	FText GetText() const { return FragmentText; }
	void SetText(const FText& Text) { FragmentText = Text; }
	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const override;

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FText FragmentText;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_StackableFragment : public FAZ_Inv_ItemFragment
{
	GENERATED_BODY()

private:

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
struct FAZ_Inv_LabeledNumberFragment : public FAZ_Inv_InventoryItemFragment
{
	GENERATED_BODY()

	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const override;
	virtual void Manifest() override;
	float GetValue() const { return Value; }

	// When manifesting for the first time, this fragment will randomize. However, onee equipped
	// and dropped, an item should retain the same value, so randomization should not occur.
	bool bRandomizeOnManifest{true};

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FText Text_Label{};

	UPROPERTY(VisibleAnywhere, Category = "AZ|Inventory")
	float Value{0.f};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float Min{0};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float Max{0};
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	bool bCollapseLabel{false};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	bool bCollapseValue{false};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 MinFractionalDigits{1};
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	int32 MaxFractionalDigits{1};
};

// Consume Fragments

USTRUCT(BlueprintType)
struct FAZ_Inv_ConsumeModifier : public FAZ_Inv_LabeledNumberFragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FAZ_Inv_ConsumableFragment : public FAZ_Inv_InventoryItemFragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC);
	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const override;
	virtual void Manifest() override;
private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FAZ_Inv_ConsumeModifier>> ConsumeModifiers;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_HealthPotionFragment : public FAZ_Inv_ConsumeModifier
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_ManaPotionFragment : public FAZ_Inv_ConsumeModifier
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) override;
};

// Equipment
//

USTRUCT(BlueprintType)
struct FAZ_Inv_EquipModifier : public FAZ_Inv_LabeledNumberFragment
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) {}
	virtual void OnUnequip(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FAZ_Inv_StrengthModifier : public FAZ_Inv_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_ArmorModifier : public FAZ_Inv_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_DamageModifier : public FAZ_Inv_EquipModifier
{
	GENERATED_BODY()

	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnequip(APlayerController* PC) override;
};

class AAZ_Inv_EquipActor;
USTRUCT(BlueprintType)
struct FAZ_Inv_EquipmentFragment : public FAZ_Inv_InventoryItemFragment
{
	GENERATED_BODY()

	bool bEquipped{false};
	void OnEquip(APlayerController* PC);
	void OnUnequip(APlayerController* PC);
	virtual void Assimilate(UAZ_Inv_CompositeBase* Composite) const override;
	virtual void Manifest() override;

	AAZ_Inv_EquipActor* SpawnAttachedActor(USkeletalMeshComponent* AttachMesh) const;
	void DestroyAttachedActor() const;
	FGameplayTag GetEquipmentType() const { return EquipmentType; }
	void SetEquippedActor(AAZ_Inv_EquipActor* EquipActor);
	
private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TArray<TInstancedStruct<FAZ_Inv_EquipModifier>> EquipModifiers;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<AAZ_Inv_EquipActor> EquipActorClass = nullptr;

	TWeakObjectPtr<AAZ_Inv_EquipActor> EquippedActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FName SocketAttachPoint{NAME_None};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag EquipmentType = FGameplayTag::EmptyTag;
};
