#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Quests/AZ_QuestTypes.h"
#include "AZ_QuestDefinition.generated.h"

UCLASS(BlueprintType)
class AZ_API UAZ_QuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") FName QuestId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") FText Title;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true)) FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") EAZ_QuestCategory Category = EAZ_QuestCategory::Story;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") TArray<FName> PrerequisiteQuests;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") TArray<FAZ_QuestObjectiveDefinition> Objectives;
	/** False: all required objectives; an all-optional quest requires all objectives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") bool bRequireOptionalObjectives = false;
	UFUNCTION(BlueprintPure, Category="Quest") bool ValidateDefinition(FString& OutError) const;
	/** Validate the complete authored catalog before enabling its acceptance providers. */
	UFUNCTION(BlueprintCallable, Category="Quest") static bool ValidateQuestCatalog(const TArray<UAZ_QuestDefinition*>& Definitions, FString& OutError);
	const FAZ_QuestObjectiveDefinition* FindObjective(FName ObjectiveId) const;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
