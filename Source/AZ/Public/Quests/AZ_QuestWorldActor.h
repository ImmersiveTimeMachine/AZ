#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystem/AZ_Interactable.h"
#include "AZ_QuestWorldActor.generated.h"

class APlayerController;
class USphereComponent;
class UStaticMeshComponent;
class UAZ_NavigationTargetComponent;
class UAZ_QuestInteractionComponent;
class UAZ_QuestDefinition;

UENUM(BlueprintType)
enum class EAZ_QuestWorldAction : uint8 { OfferQuest, InteractObjective, DeliverItems };

/** Immediate authored world use. Only a validated authoritative commit can advance an objective. */
UCLASS(Blueprintable)
class AZ_API AAZ_QuestWorldActor : public AActor, public IAZ_Interactable
{
	GENERATED_BODY()

public:
	AAZ_QuestWorldActor();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<USphereComponent> InteractionVolume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UStaticMeshComponent> DisplayMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UAZ_NavigationTargetComponent> NavigationTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UAZ_QuestInteractionComponent> InteractionCommit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") EAZ_QuestWorldAction Action = EAZ_QuestWorldAction::InteractObjective;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UAZ_QuestDefinition> OfferDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName ObjectiveId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Quest") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin="25.0", ClampMax="1000.0", Units="cm")) float InteractionRadius = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FText InteractionPrompt;
	/** The immediate use's observable state; gameplay progress itself is persisted on PlayerState. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Quest") int32 CommittedUseCount = 0;

	UFUNCTION(BlueprintPure, Category="Quest") FText GetInteractionPrompt() const;
	UFUNCTION(BlueprintPure, Category="Quest") bool CanInteractForPlayer(APlayerController* Controller, FString& OutError) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
	bool TryInteractForPlayer(APlayerController* Controller, FGuid ReceiptId, FString& OutError);
	UFUNCTION(BlueprintImplementableEvent, Category="Quest")
	void OnWorldActionCommitted(APlayerController* Controller, FGuid ReceiptId);

	virtual bool IsAvailableForInteraction_Implementation(UPrimitiveComponent* Component) const override;
	virtual float GetInteractionDuration_Implementation(UPrimitiveComponent* Component) const override { return 0.0f; }
	virtual void PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* Component) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool bCommitting = false;
	TSet<FGuid> CommittedReceipts;
};
