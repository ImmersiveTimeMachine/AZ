// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "AZ_DamageExecCalc.generated.h"

/**
 * The damage spine's single execution: reads the SetByCaller.Damage magnitude off the GE_Damage spec
 * and writes it additively into the target's Vitals IncomingDamage meta attribute. Mitigation
 * (armor / Resilience / weakpoints / the RPG pass) slots in HERE later without touching any caller.
 */
UCLASS()
class AZ_API UAZ_DamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UAZ_DamageExecCalc();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
