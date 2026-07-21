// Copyright Artur. AZ project.

#include "AbilitySystem/AZ_DamageExecCalc.h"

#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AZ_GameplayTags.h"

namespace
{
struct FAZ_DamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);

	FAZ_DamageStatics()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAZ_VitalsAttributeSet, IncomingDamage, Target, false);
	}
};

const FAZ_DamageStatics& DamageStatics()
{
	static FAZ_DamageStatics Statics;
	return Statics;
}
}

UAZ_DamageExecCalc::UAZ_DamageExecCalc()
{
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageDef);
}

void UAZ_DamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// bWarnIfNotFound=false: a spec without the SetByCaller simply deals 0 (harmless during wiring).
	const float Damage = Spec.GetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage,
		/*bWarnIfNotFound*/ false, /*DefaultIfNotFound*/ 0.f);
	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			DamageStatics().IncomingDamageProperty, EGameplayModOp::Additive, Damage));
	}
}
