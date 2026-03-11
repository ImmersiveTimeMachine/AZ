// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "AZ_Inv_CommonUI_ItemFragment.generated.h"

/**
 * 
 */

class UAZ_Inv_CommonUI_CompositeBaseWidget;
class UAZ_Inv_CompositeBase;

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

class AAZ_Inv_EquipActor;
USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_EquipmentFragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
{
	GENERATED_BODY()

	bool bEquipped{false};
	void OnEquip(APlayerController* PC);
	void OnUnequip(APlayerController* PC);
	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const override;
	virtual void Manifest() override;

	AAZ_Inv_EquipActor* SpawnAttachedActor(USkeletalMeshComponent* AttachMesh) const;
	void DestroyAttachedActor() const;
	FGameplayTag GetEquipmentType() const { return EquipmentType; }
	void SetEquippedActor(AAZ_Inv_EquipActor* EquipActor);

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TArray<TInstancedStruct<FAZ_Inv_CommonUI_EquipModifier>> EquipModifiers;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<AAZ_Inv_EquipActor> EquipActorClass = nullptr;

	TWeakObjectPtr<AAZ_Inv_EquipActor> EquippedActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FName SocketAttachPoint{NAME_None};

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag EquipmentType = FGameplayTag::EmptyTag;
};