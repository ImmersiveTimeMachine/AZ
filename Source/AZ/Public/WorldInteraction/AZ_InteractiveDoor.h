#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystem/AZ_Interactable.h"
#include "AZ_InteractiveDoor.generated.h"

class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;
class AAZ_PlayerController;

USTRUCT(BlueprintType)
struct FAZ_ContainerContentSlot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contents") TSubclassOf<AActor> PickupClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contents", meta=(ClampMin="1")) int32 Quantity = 1;
	/** Relative to the moving pivot for drawers, or the container root for shelves/chests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contents") FTransform LocalTransform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contents") bool bMovesWithPanel = false;
	/** Stable world identity assigned on first setup; retain it when adjusting placement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Contents") FGuid PickupId;
};

USTRUCT()
struct FAZ_DoorMotion
{
	GENERATED_BODY()
	UPROPERTY() float From = 0.f;
	UPROPERTY() float To = 0.f;
	UPROPERTY() double StartedAt = 0.;
	UPROPERTY() float Duration = 1.f;
	UPROPERTY() bool bMoving = false;
	UPROPERTY() bool bLocked = false;
	UPROPERTY() bool bBlocked = false;
};

/** Authored hinged door. Uses the player's existing immediate-interaction route. */
UCLASS(Blueprintable)
class AZ_API AAZ_InteractiveDoor : public AActor, public IAZ_Interactable
{
	GENERATED_BODY()
public:
	AAZ_InteractiveDoor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> FrameMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> DoorPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> InteractionVolume;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> NavigationBlocker;
	UPROPERTY(EditInstanceOnly, Category="Door|Persistence") FGuid DoorId;
	UPROPERTY(EditAnywhere, Category="Door") FText DisplayName;
	UPROPERTY(EditAnywhere, Category="Door|Interaction") bool bUseCustomInteractionPoint = false;
	/** Local to the moving mesh, so a handle prompt follows the leaf/drawer. */
	UPROPERTY(EditAnywhere, Category="Door|Interaction", meta=(EditCondition="bUseCustomInteractionPoint")) FVector InteractionPoint = FVector::ZeroVector;
	FVector GetInteractionPoint() const;
	UPROPERTY(EditAnywhere, Category="Door") bool bInitiallyLocked = false;
	UPROPERTY(EditAnywhere, Category="Door") bool bInitiallyOpen = false;
	UPROPERTY(EditAnywhere, Category="Door") FGameplayTag RequiredKeyType;
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="-170", ClampMax="170")) float OpenAngle = 100.f;
	/** Axis and translation are in the actor's local space; defaults preserve hinged doors. */
	UPROPERTY(EditAnywhere, Category="Door|Motion") FVector RotationAxis = FVector::UpVector;
	UPROPERTY(EditAnywhere, Category="Door|Motion") FVector ClosedPivotLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Door|Motion") FVector OpenTranslation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Door|Motion") TArray<TObjectPtr<AActor>> AssemblyActors;
	UPROPERTY(EditAnywhere, Category="Door|Container") bool bPassageDoor = true;
	/** Placed pickups remain owned by the campaign; opening only changes access. */
	UPROPERTY(EditInstanceOnly, Category="Door|Container") TArray<FGuid> ContentPickupIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Container", meta=(TitleProperty="PickupClass")) TArray<FAZ_ContainerContentSlot> Contents;
	/** Editor authoring only. Never refills a played container or creates loot on opening. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Door|Container") void ApplyContentSetup();
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="0.1")) float OpenDuration = 1.f;
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="50")) float InteractionRadius = 180.f;
	UPROPERTY(EditAnywhere, Category="Door|Sound", meta=(ClampMin="0")) float NoiseLoudness = 0.6f;
	UPROPERTY(EditAnywhere, Category="Door|Sound", meta=(ClampMin="0")) float NoiseRange = 700.f;
	UPROPERTY(EditAnywhere, Category="Door|Sound") TObjectPtr<USoundBase> OpenSound;
	UPROPERTY(EditAnywhere, Category="Door|Sound") TObjectPtr<USoundBase> CloseSound;
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Door") void RefreshDoorPreview();
	bool CanFocus(const AAZ_PlayerController* Player) const;
	FText GetInteractionCaption(const AAZ_PlayerController* Player) const;
	bool TryInteractForPlayer(AAZ_PlayerController* Player, FString& Error);
	bool IsMoving() const { return Motion.bMoving; }
	bool IsLocked() const { return Motion.bLocked; }
	float GetOpenFraction() const { return AppliedFraction; }
	void RestoreDoorState(float Fraction, bool bLocked);
	bool CanAccessContents() const { return !Motion.bMoving && !Motion.bLocked && AppliedFraction >= 0.99f; }
	void RefreshContentAccess();
	virtual bool IsAvailableForInteraction_Implementation(UPrimitiveComponent*) const override;
	virtual float GetInteractionDuration_Implementation(UPrimitiveComponent*) const override { return 0.f; }
	virtual void PostInteract_Implementation(AActor* Actor, UPrimitiveComponent*) override;
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Motion) FAZ_DoorMotion Motion;
	UFUNCTION() void OnRep_Motion();
	UFUNCTION(NetMulticast, Unreliable) void PlayDoorSound(bool bOpening);
	bool HasKey(const AAZ_PlayerController* Player) const;
	bool IsSweepClear(float From, float To, float& SafeFraction) const;
	void ApplyFraction(float Fraction);
	void UpdateNavigation();
	double ServerTime() const;
	float AppliedFraction = 0.f;
	FTransform PivotAt(float Fraction) const;
};
