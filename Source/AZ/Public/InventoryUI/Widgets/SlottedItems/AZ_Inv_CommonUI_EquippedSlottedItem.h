// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_SlottedItem.h"
#include "GameplayTagContainer.h"
#include "AZ_Inv_CommonUI_EquippedSlottedItem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_EquippedSlottedItemClicked, class UAZ_Inv_CommonUI_EquippedSlottedItem*, SlottedItem);

UCLASS()
class AZ_API UAZ_Inv_CommonUI_EquippedSlottedItem : public UAZ_Inv_CommonUI_SlottedItem
{
	GENERATED_BODY()

public:

	void SetEquipmentTypeTag(const FGameplayTag& Tag) { EquipmentTypeTag = Tag; }
	FGameplayTag GetEquipmentTypeTag() const { return EquipmentTypeTag; }

	FCommonUI_EquippedSlottedItemClicked OnEquippedSlottedItemClicked;

private:

	UPROPERTY()
	FGameplayTag EquipmentTypeTag;

	virtual void NativeOnClicked() override;
};
