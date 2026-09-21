#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Navigation/AZ_NavigationTypes.h"
#include "AZ_QuestTypes.generated.h"

class UAZ_QuestDefinition;

UENUM(BlueprintType)
enum class EAZ_QuestStatus : uint8 { Available, Active, Completed, Failed, Cancelled };

UENUM(BlueprintType)
enum class EAZ_QuestObjectiveStatus : uint8 { Locked, Active, Completed, Failed, Cancelled };

UENUM(BlueprintType)
enum class EAZ_QuestObjectiveKind : uint8 { ReachArea, Interact, PossessItem, DeliverItem };

UENUM(BlueprintType)
enum class EAZ_QuestCategory : uint8 { Story, Side };

USTRUCT(BlueprintType)
struct AZ_API FAZ_QuestObjectiveDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ObjectiveId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EAZ_QuestObjectiveKind Kind = EAZ_QuestObjectiveKind::ReachArea;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOptional = false;
	/** Explicit policy; an optional objective never fails its quest by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFailureFailsQuest = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag ItemType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FAZ_NavigationTargetDescriptor Target;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> PrerequisiteObjectives;
	/** Server distance gate for committed interaction and delivery providers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float InteractionRadius = 250.f;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_QuestObjectiveProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) FName ObjectiveId;
	UPROPERTY(BlueprintReadOnly, SaveGame) EAZ_QuestObjectiveStatus Status = EAZ_QuestObjectiveStatus::Locked;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 CurrentCount = 0;
	/** Committed fact/transaction identities, preserved across save and retries. */
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FGuid> ReceiptIds;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_QuestProgressRecord
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) FName QuestId;
	UPROPERTY(BlueprintReadOnly, SaveGame) TSoftObjectPtr<UAZ_QuestDefinition> Definition;
	UPROPERTY(BlueprintReadOnly, SaveGame) EAZ_QuestStatus Status = EAZ_QuestStatus::Active;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FAZ_QuestObjectiveProgress> Objectives;
};
