#pragma once

#include "CoreMinimal.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "AZ_InventorySnapshot.generated.h"

/** Data records only. Runtime inventory UObjects, grants and actors are never saved. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_InventorySnapshot
{
	GENERATED_BODY()
	UPROPERTY() TArray<FAZ_InventoryPickupRecord> Items;
	UPROPERTY() TArray<FAZ_InventoryGridPlacement> Placements;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_CampaignEquipmentState
{
	GENERATED_BODY()
	UPROPERTY() FGuid ActiveItemId;
	UPROPERTY() int32 IntrinsicSlot = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_CampaignQuickBarState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FGuid> ItemIds;
	UPROPERTY() TArray<bool> ExplicitBindings;
	UPROPERTY() FGuid ReadyItemId;
	UPROPERTY() FGuid LastFightWeaponId;
};
