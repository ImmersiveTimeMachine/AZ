// Copyright Artur. AZ project.

#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UAZ_VitalsAttributeSet::UAZ_VitalsAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitIncomingDamage(0.f);
}

void UAZ_VitalsAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_VitalsAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_VitalsAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	// IncomingDamage: meta, deliberately not replicated.
}

void UAZ_VitalsAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UAZ_VitalsAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// Drain the inbox into Health (server-side — GEs execute on authority).
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage <= 0.f)
		{
			return;
		}

		const float NewHealth = FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth());
		SetHealth(NewHealth);
		UE_LOG(LogTemp, Display, TEXT("[Vitals] %s took %.0f damage -> %.0f/%.0f HP"),
			*GetNameSafe(GetOwningActor()), Damage, NewHealth, GetMaxHealth());

		if (NewHealth <= 0.f && !bOutOfHealth)
		{
			bOutOfHealth = true;
			// Tell the dying actor's OWN ASC. Payload carries the killer (effect causer / instigator)
			// so death abilities can pick a directional death and credit the kill.
			if (AActor* OwnerActor = GetOwningActor())
			{
				FGameplayEventData Payload;
				Payload.EventTag = FAZ_GameplayTags::Get().Event_Death;
				Payload.Target = OwnerActor;
				Payload.Instigator = Data.EffectSpec.GetEffectContext().GetEffectCauser();
				Payload.ContextHandle = Data.EffectSpec.GetEffectContext();
				Payload.EventMagnitude = Damage;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, Payload.EventTag, Payload);
			}
		}
		else if (NewHealth > 0.f)
		{
			bOutOfHealth = false;   // healed back up (future) — re-arm the death latch
		}
	}
}

void UAZ_VitalsAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_VitalsAttributeSet, Health, OldHealth);
}

void UAZ_VitalsAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_VitalsAttributeSet, MaxHealth, OldMaxHealth);
}
