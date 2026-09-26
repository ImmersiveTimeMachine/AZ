#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "AZ_CraftRecipe.generated.h"

/** One exact item type and the number of units consumed from backpack stacks. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CraftIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting")
	FGameplayTag ItemType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting", meta=(ClampMin="1"))
	int32 Quantity = 1;
};

/** A deliberately small, server-whitelisted recipe for one fresh throwable stack. */
UCLASS(BlueprintType)
class AZ_API UAZ_CraftRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting")
	TArray<FAZ_CraftIngredient> Ingredients;

	/** Exact item types that must remain owned after crafting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting")
	TArray<FGameplayTag> Tools;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting")
	FAZ_Inv_CommonUI_ItemManifest OutputManifest;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Crafting", meta=(ClampMin="1"))
	int32 OutputCount = 1;
};
