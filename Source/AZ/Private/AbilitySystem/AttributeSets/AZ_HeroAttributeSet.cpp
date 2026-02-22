#include "AZ/Public/AbilitySystem/AttributeSets/AZ_HeroAttributeSet.h"

#include <Net/UnrealNetwork.h>


UAZ_HeroAttributeSet::UAZ_HeroAttributeSet()
{
	
}

void UAZ_HeroAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Primary Attributes 
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Strength, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Agility, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Resilience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Intelligence, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Expertise, COND_None, REPNOTIFY_Always);

	// Vital Attributes
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_HeroAttributeSet, Mana, COND_None, REPNOTIFY_Always);
}

void UAZ_HeroAttributeSet::InitFromMetaDataTable(const UDataTable* DataTable)
{
	Super::InitFromMetaDataTable(DataTable);
	
}

// Primary Attributes
void UAZ_HeroAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldStrength) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Strength, OldStrength)
}

void UAZ_HeroAttributeSet::OnRep_Agility(const FGameplayAttributeData& OldAgility) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Agility, OldAgility)
}

void UAZ_HeroAttributeSet::OnRep_Resilience(const FGameplayAttributeData& OldResilience) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Resilience, OldResilience)
}

void UAZ_HeroAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldIntelligence) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Intelligence, OldIntelligence)
}

void UAZ_HeroAttributeSet::OnRep_Expertise(const FGameplayAttributeData& OldExpertise) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Expertise, OldExpertise)
}

void UAZ_HeroAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Health, OldHealth)
}

void UAZ_HeroAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_HeroAttributeSet, Mana, OldMana)
}

