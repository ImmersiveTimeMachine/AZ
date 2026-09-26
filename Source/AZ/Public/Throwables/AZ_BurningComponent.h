// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_BurningComponent.generated.h"

class AActor;
class APawn;
class UAZ_ThrowableDefinition;
class UNiagaraComponent;
class UParticleSystemComponent;

/** One authority-owned, refreshable burn per actor. Damage remains on the shared GAS vitals path. */
UCLASS(ClassGroup=(AZ), BlueprintType, meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_BurningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_BurningComponent();

	/** Refresh the finite burn; overlapping areas never create additional damage timers. */
	void Ignite(const UAZ_ThrowableDefinition* InDefinition, APawn* Thrower, AActor* FireSource);

	UFUNCTION(BlueprintPure, Category="AZ|Throwable|Fire")
	bool IsBurning() const { return bBurning; }
	bool ConsumeInitialReaction() { const bool Result = bNeedsInitialReaction; bNeedsInitialReaction = false; return Result; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_BurningState();

	void BurnStep();
	void StopBurning();
	void RefreshVisuals();
	bool HasLivingVitals() const;

	UPROPERTY(ReplicatedUsing=OnRep_BurningState)
	bool bBurning = false;

	UPROPERTY(ReplicatedUsing=OnRep_BurningState)
	TObjectPtr<const UAZ_ThrowableDefinition> VisualDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BurningEffect;
	UPROPERTY(Transient) TObjectPtr<UParticleSystemComponent> BurningParticles;

	TWeakObjectPtr<APawn> SourceThrower;
	TWeakObjectPtr<AActor> SourceFire;
	FVector FireOrigin = FVector::ZeroVector;
	float DamagePerSecond = 0.f;
	bool bNeedsInitialReaction = false;
	double BurnExpiresAt = 0.0;
	double LastDamageAt = 0.0;
	FTimerHandle BurnTimer;
};
