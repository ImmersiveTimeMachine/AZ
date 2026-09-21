#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/AZ_NavigationTypes.h"
#include "Quests/AZ_QuestTypes.h"
#include "AZ_QuestMapComponent.generated.h"

class APlayerController;
class UAZ_QuestProgressComponent;
class UAZ_NavigationTargetSubsystem;
class UAZ_MapDefinition;
class USceneComponent;

USTRUCT(BlueprintType)
struct AZ_API FAZ_QuestMapMarkerView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName QuestId;
	UPROPERTY(BlueprintReadOnly) FName ObjectiveId;
	/** Cached display metadata from the quest definition; ignored for personal markers. */
	UPROPERTY(BlueprintReadOnly) EAZ_QuestCategory QuestCategory = EAZ_QuestCategory::Story;
	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) FAZ_NavigationTargetDescriptor Target;
	UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bResolved = false;
	UPROPERTY(BlueprintReadOnly) bool bTracked = false;
	UPROPERTY(BlueprintReadOnly) bool bOptional = false;
	UPROPERTY(BlueprintReadOnly) bool bPersonal = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_QuestMapViewChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAZ_QuestMapMarkerUpsert, UObject*, MarkerObject, FText, Label, bool, bPersonal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_QuestMapMarkerRemove, UObject*, MarkerObject);

/** Local controller presentation state. Quest truth stays on PlayerState; map input never completes an objective. */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuestMapComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuestMapComponent();
	UPROPERTY(BlueprintAssignable, Category="Quest Map") FAZ_QuestMapViewChanged OnViewChanged;
	/** Owned BP adapter forwards these exact keys to the existing compass/world bridge. */
	UPROPERTY(BlueprintAssignable, Category="Quest Map") FAZ_QuestMapMarkerUpsert OnMarkerUpsert;
	UPROPERTY(BlueprintAssignable, Category="Quest Map") FAZ_QuestMapMarkerRemove OnMarkerRemove;
	UFUNCTION(BlueprintPure, Category="Quest Map") FAZ_MapWaypoint GetWaypoint() const { return Waypoint; }
	UFUNCTION(BlueprintCallable, Category="Quest Map") bool SetWaypoint(const FAZ_MapWaypoint& Value);
	UFUNCTION(BlueprintCallable, Category="Quest Map") void ClearWaypoint();
	/** Persistence caller has already validated its complete snapshot. */
	void RestoreWaypoint(const FAZ_MapWaypoint& Value, bool bPublish = true);
	UFUNCTION(BlueprintCallable, Category="Quest Map") void RefreshBindings();
	UFUNCTION(BlueprintCallable, Category="Quest Map") void RefreshNavigation();
	/** Call after a HUD adapter subscribes; replay only this component's current registrations. */
	UFUNCTION(BlueprintCallable, Category="Quest Map") void ReplayNavigationMarkers();
	UFUNCTION(BlueprintCallable, Category="Quest Map") bool SetMapDefinition(UAZ_MapDefinition* Definition);
	UFUNCTION(BlueprintPure, Category="Quest Map") UAZ_MapDefinition* GetMapDefinition() const { return MapDefinition; }
	UFUNCTION(BlueprintPure, Category="Quest Map") UAZ_QuestProgressComponent* GetQuestProgress() const { return QuestProgress; }
	UFUNCTION(BlueprintPure, Category="Quest Map") TArray<FAZ_QuestMapMarkerView> GetMapMarkers() const { return Markers; }
	UFUNCTION(BlueprintPure, Category="Quest Map") static UAZ_QuestMapComponent* FindForController(APlayerController* Controller);
	/** Local-controller only; useful before HUD construction and for save restoration. */
	UFUNCTION(BlueprintCallable, Category="Quest Map") static UAZ_QuestMapComponent* GetOrCreateForController(APlayerController* Controller);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_QuestProgressComponent> QuestProgress;
	UPROPERTY(Transient) TObjectPtr<UAZ_NavigationTargetSubsystem> TargetRegistry;
	UPROPERTY(Transient) TObjectPtr<UAZ_MapDefinition> MapDefinition;
	UPROPERTY(Transient) FAZ_MapWaypoint Waypoint;
	UPROPERTY(Transient) TArray<FAZ_QuestMapMarkerView> Markers;
	/** Stable per-purpose component keys avoid collisions when two views refer to one world actor. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> QuestAnchor;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> WaypointAnchor;
	bool bQuestAnchorRegistered = false;
	bool bWaypointAnchorRegistered = false;
	FText QuestAnchorLabel;
	FName PublishedQuestId;
	FName PublishedObjectiveId;
	UFUNCTION() void HandleQuestChanged();
	UFUNCTION() void HandleTargetChanged(FName TargetId);
	bool IsLocalOwner() const;
	USceneComponent* EnsureAnchor(TObjectPtr<USceneComponent>& Anchor, FName Name);
	void RetireAnchor(USceneComponent* Anchor, bool& bWasPublished);
};
