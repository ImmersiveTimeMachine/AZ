#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AZ_QuestReachArea.generated.h"

class UBoxComponent;
class UAZ_NavigationTargetComponent;
class APlayerState;

/** Authored 3D objective volume. It never accepts quests or uses UI distance. */
UCLASS()
class AZ_API AAZ_QuestReachArea : public AActor
{
	GENERATED_BODY()
public:
	AAZ_QuestReachArea();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UBoxComponent> Volume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UAZ_NavigationTargetComponent> NavigationTarget;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName ObjectiveId;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") void ReconcilePlayer(APlayerState* Player);
protected:
	virtual void BeginPlay() override;
private:
	UFUNCTION() void OnEntered(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
