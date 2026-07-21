// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AZ_AT_MeleeSweep.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZMeleeSweepDelegate, const TArray<FHitResult>&, Hits);

/**
 * THE shared melee targeting primitive (batch item, wired in after the next restart): one configurable
 * forward sweep + angular cone + team filter, packaged as an ability task so EVERY melee ability —
 * fists, zombie claws, knife/bat later — resolves its hit window through the same node with per-weapon
 * numbers. Server-authoritative by convention: call it only on the authority path of the hit window.
 *
 * v1 resolves synchronously on Activate (melee is an instant test at the notify frame); it is still a
 * task (not a static) so BP weapon abilities get a latent-style node with a delegate, and so a future
 * multi-frame sweep window (sweeping the hand socket across several frames) keeps the same call shape.
 */
UCLASS()
class AZ_API UAZ_AT_MeleeSweep : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** SweepRange/Radius in cm from the avatar; ConeHalfAngleDegrees rejects hits off the facing;
	 *  bHostilesOnly applies the team-attitude filter; bSingleTarget keeps only the nearest hit. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAZ_AT_MeleeSweep* MeleeSweep(UGameplayAbility* OwningAbility, float SweepRange = 160.f,
		float SweepRadius = 60.f, float ConeHalfAngleDegrees = 55.f, bool bHostilesOnly = true,
		bool bSingleTarget = true);

	virtual void Activate() override;

	/** Filtered hits, nearest-first. Broadcast exactly once (empty array = whiff). */
	UPROPERTY(BlueprintAssignable)
	FAZMeleeSweepDelegate OnCompleted;

protected:
	float SweepRange = 160.f;
	float SweepRadius = 60.f;
	float ConeHalfAngleDegrees = 55.f;
	bool bHostilesOnly = true;
	bool bSingleTarget = true;
};
