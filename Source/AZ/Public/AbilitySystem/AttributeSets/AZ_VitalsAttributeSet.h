// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/AttributeSets/AZ_AttributeSet.h"
#include "AZ_VitalsAttributeSet.generated.h"

/**
 * Shared combat vitals — the ONE attribute set the damage spine reads and writes, granted to EVERY
 * damageable actor's ASC (hero PlayerState + infected pawns; vehicles later). Damage flows exclusively
 * through the IncomingDamage meta attribute (written by UAZ_DamageExecCalc from GE_Damage specs), which
 * PostGameplayEffectExecute consumes into Health. Health hitting 0 sends Event.Death to the owner's ASC
 * exactly once — death abilities (GA_ZombieDeath / hero game-over) trigger on that event.
 *
 * The hero's legacy AZ_HeroAttributeSet.Health stays for old UI bindings until migrated; COMBAT only
 * ever touches THIS set. UI (S5): CharacterVitalsPanel binds here.
 */
UCLASS()
class AZ_API UAZ_VitalsAttributeSet : public UAZ_AttributeSet
{
	GENERATED_BODY()

public:
	UAZ_VitalsAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Vitals", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAZ_VitalsAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Vitals", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAZ_VitalsAttributeSet, MaxHealth)

	/** Meta attribute — transient damage inbox, server-only (never replicated, never read directly).
	 *  UAZ_DamageExecCalc adds here; PostGameplayEffectExecute drains it into Health. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Vitals|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UAZ_VitalsAttributeSet, IncomingDamage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;

	/** Latch so Event.Death fires once per life, not on every overkill hit. */
	bool bOutOfHealth = false;
};
