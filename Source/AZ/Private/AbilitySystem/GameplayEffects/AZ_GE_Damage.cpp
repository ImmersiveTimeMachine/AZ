// Copyright Artur. AZ project.

#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"

#include "AbilitySystem/AZ_DamageExecCalc.h"

UAZ_GE_Damage::UAZ_GE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UAZ_DamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
