#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_QuestInventoryAdapter.generated.h"

class APlayerController;
class UAZ_QuestProgressComponent;

/** A bridge to the controller's canonical CommonUI inventory, not a second item store. */
UCLASS()
class AZ_API UAZ_QuestInventoryAdapter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Top-level Backpack stacks; equipped items already use that same record. Excludes inserted children. */
	UFUNCTION(BlueprintPure, Category="AZ|Quests|Inventory")
	static int32 CountOwnedItems(APlayerController* Controller, FGameplayTag ItemType);

	/** Authority only. A receipt retries without spending again; no callbacks observe a half delivery. */
	static bool TryDeliverObjective(UAZ_QuestProgressComponent* Progress, FName QuestId,
		FName ObjectiveId, AActor* Recipient, FGuid ReceiptId, FString& OutError);
};
