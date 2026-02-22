// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "AZ_Inv_CommonUI_Item_Fragment.generated.h"

/**
 * 
 */

class UAZ_Inv_CommonUI_CompositeBaseWidget;
class UAZ_Inv_CompositeBase;

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_Item_Fragment
{
	GENERATED_BODY()

	FAZ_Inv_CommonUI_Item_Fragment() = default;
	FAZ_Inv_CommonUI_Item_Fragment(const FAZ_Inv_CommonUI_Item_Fragment& Other) = default;
	FAZ_Inv_CommonUI_Item_Fragment(FAZ_Inv_CommonUI_Item_Fragment&& Other) noexcept = default;
	FAZ_Inv_CommonUI_Item_Fragment& operator=(const FAZ_Inv_CommonUI_Item_Fragment& Other) = default;
	FAZ_Inv_CommonUI_Item_Fragment& operator=(FAZ_Inv_CommonUI_Item_Fragment&& Other) noexcept = default;
	
	virtual ~FAZ_Inv_CommonUI_Item_Fragment() {}

private:

	UPROPERTY(EditAnywhere,Category = "AZ|Inventory")
	FGameplayTag FragmentTag;
	
public:
	
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(const FGameplayTag& InFragmentTag) { FragmentTag = InFragmentTag; }
	
	virtual void Manifest() {}
};	

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_InventoryItem_Fragment : public FAZ_Inv_CommonUI_Item_Fragment
{
	GENERATED_BODY()

	virtual void Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const;
protected:
	bool MatchesWidgetTag(const UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const;
};

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_Grid_Fragment : public FAZ_Inv_CommonUI_Item_Fragment
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
struct FAZ_Inv_CommonUI_Image_Fragment : public FAZ_Inv_CommonUI_InventoryItem_Fragment
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
struct FAZ_Inv_CommonUI_Stackable_Fragment : public FAZ_Inv_CommonUI_Item_Fragment
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