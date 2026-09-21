#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_QuestInteractionComponent.generated.h"

class APawn;

/** Add to the actual interactable. Call only from its successful authoritative
 * action commit, with that action's stable receipt. GA start/input is not success. */
UCLASS(ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuestInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuestInteractionComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName ObjectiveId;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
	bool ReportSuccessfulInteraction(APawn* InstigatorPawn, FGuid ReceiptId);
};
