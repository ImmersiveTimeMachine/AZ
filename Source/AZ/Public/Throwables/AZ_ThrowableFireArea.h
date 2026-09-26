// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AZ_ThrowableFireArea.generated.h"

class APawn;
class UAZ_ThrowableDefinition;
class UNiagaraComponent;
class UParticleSystemComponent;
class UAudioComponent;
class USceneComponent;

/** A single finite patch of ground fire, spawned at the impact surface on authority. */
UCLASS()
class AZ_API AAZ_ThrowableFireArea : public AActor
{
	GENERATED_BODY()

public:
	AAZ_ThrowableFireArea();

	void Initialize(const UAZ_ThrowableDefinition* InDefinition, APawn* InThrower,
		const FVector& SurfaceNormal);

	UFUNCTION(BlueprintPure, Category="AZ|Throwable|Fire")
	bool HasActiveHazard() const { return bActive; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_FireVisualState();

	void FireStep();
	void FinishFire();
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(ReplicatedUsing=OnRep_FireVisualState)
	TObjectPtr<const UAZ_ThrowableDefinition> Definition;

	UPROPERTY(ReplicatedUsing=OnRep_FireVisualState)
	bool bActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_FireVisualState)
	FVector_NetQuantizeNormal GroundNormal = FVector::UpVector;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> GroundEffect;
	UPROPERTY(Transient) TObjectPtr<UParticleSystemComponent> GroundParticles;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio;

	TWeakObjectPtr<APawn> Thrower;
	float Radius = 0.f;
	double ExpiresAt = 0.0;
	FTimerHandle FireTimer;
};
