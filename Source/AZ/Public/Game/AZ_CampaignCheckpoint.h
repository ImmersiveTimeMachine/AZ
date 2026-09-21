#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystem/AZ_Interactable.h"
#include "AZ_CampaignCheckpoint.generated.h"

class USceneComponent;
class USphereComponent;
class APlayerController;

/** An authored save point. Presentation can be a campfire, safe room or another approved prop. */
UCLASS()
class AZ_API AAZ_CampaignCheckpoint : public AActor, public IAZ_Interactable
{
	GENERATED_BODY()
public:
	AAZ_CampaignCheckpoint();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName CheckpointId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1")) float SaveRadius = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign") bool bEnabled = true;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign") bool SaveForPlayer(APlayerController* Controller, FString& OutError);
	bool CanSaveForPlayer(APlayerController* Controller, FString& OutError) const;
	virtual bool IsAvailableForInteraction_Implementation(UPrimitiveComponent* Component) const override;
	virtual void PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* Component) override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> CheckpointRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> InteractionVolume;
};
