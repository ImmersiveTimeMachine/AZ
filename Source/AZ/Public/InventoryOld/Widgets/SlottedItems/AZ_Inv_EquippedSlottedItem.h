// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_SlottedItem.h"
#include "AZ_Inv_EquippedSlottedItem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEquippedSlottedItemClicked, class UAZ_Inv_EquippedSlottedItem*, SlottedItem);

UCLASS()
class AZ_API UAZ_Inv_EquippedSlottedItem : public UAZ_Inv_SlottedItem
{
	GENERATED_BODY()
	
public:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	void SetEquipmentTypeTag(const FGameplayTag& Tag) { EquipmentTypeTag = Tag; }
	FGameplayTag GetEquipmentTypeTag() const { return EquipmentTypeTag; }

	FEquippedSlottedItemClicked OnEquippedSlottedItemClicked;
private:

	UPROPERTY()
	FGameplayTag EquipmentTypeTag;
};
