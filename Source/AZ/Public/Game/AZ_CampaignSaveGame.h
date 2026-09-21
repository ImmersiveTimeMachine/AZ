#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InventoryUI/AZ_InventorySnapshot.h"
#include "Quests/AZ_QuestTypes.h"
#include "Game/AZ_GameStateBase.h"
#include "AZ_CampaignSaveGame.generated.h"

class UAttributeSet;

USTRUCT()
struct AZ_API FAZ_CampaignPickupSnapshot
{
	GENERATED_BODY()
	UPROPERTY() FGuid WorldId;
	UPROPERTY() bool bRuntimeSpawned = false;
	UPROPERTY() bool bCollected = false;
	UPROPERTY() TSoftClassPtr<AActor> ActorClass;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FAZ_InventoryPickupRecord Root;
	UPROPERTY() TArray<FAZ_InventoryPickupRecord> Children;
};

USTRUCT()
struct AZ_API FAZ_CampaignWorldSnapshot
{
	GENERATED_BODY()
	UPROPERTY() TArray<FAZ_CampaignPickupSnapshot> Pickups;
	UPROPERTY() TMap<FName, bool> Facts;
};

USTRUCT()
struct AZ_API FAZ_CampaignAttributeSnapshot
{
	GENERATED_BODY()
	UPROPERTY() TSoftClassPtr<UAttributeSet> SetClass;
	UPROPERTY() FName AttributeName;
	UPROPERTY() float BaseValue = 0.f;
};

/** A versioned single-protagonist checkpoint. No runtime object, widget or ability-instance pointers. */
UCLASS()
class AZ_API UAZ_CampaignSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	static constexpr int32 CurrentVersion = 1;
	UPROPERTY() int32 SchemaVersion = CurrentVersion;
	UPROPERTY() int64 Sequence = 0;
	UPROPERTY() FGuid SnapshotId;
	UPROPERTY() FDateTime SavedAtUtc;
	UPROPERTY() FString WorldPackage;
	UPROPERTY() FName CheckpointId;
	UPROPERTY() FTransform PlayerTransform;
	UPROPERTY() float CapsuleHalfHeight = 0.f;
	UPROPERTY() FRotator ControlRotation;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() int32 XP = 0;
	UPROPERTY() int32 AttributePoints = 0;
	UPROPERTY() int32 SpellPoints = 0;
	UPROPERTY() TArray<FAZ_CampaignAttributeSnapshot> Attributes;
	UPROPERTY() FAZ_InventorySnapshot Inventory;
	UPROPERTY() FAZ_CampaignEquipmentState Equipment;
	UPROPERTY() FAZ_CampaignQuickBarState QuickBar;
	UPROPERTY() TArray<FAZ_QuestProgressRecord> Quests;
	UPROPERTY() FName TrackedQuestId;
	UPROPERTY() FName TrackedObjectiveId;
	UPROPERTY() FAZ_MapWaypoint Waypoint;
	UPROPERTY() FAZ_CampaignWorldSnapshot World;
	UPROPERTY() EDifficultyLevel Difficulty = EDifficultyLevel::Normal;
};
