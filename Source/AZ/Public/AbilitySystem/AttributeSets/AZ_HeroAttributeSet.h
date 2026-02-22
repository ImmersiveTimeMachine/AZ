#pragma once

#include <CoreMinimal.h>
#include <AbilitySystemComponent.h>
#include "AZ/Public/AbilitySystem/AttributeSets/AZ_AttributeSet.h"
#include "AZ_HeroAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_HeroAttributeSet : public UAZ_AttributeSet
{
	GENERATED_BODY()

	public:

	UAZ_HeroAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void InitFromMetaDataTable(const UDataTable* DataTable) override;

	//-----------------------------------------------------------------------------------------------------------------------------------------
	// Primary Attributes
	//-----------------------------------------------------------------------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Primary Attributes", ReplicatedUsing = OnRep_Strength)
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Strength)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Primary Attributes", ReplicatedUsing = OnRep_Agility)
	FGameplayAttributeData Agility;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Agility)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Primary Attributes", ReplicatedUsing = OnRep_Resilience)
	FGameplayAttributeData Resilience;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Resilience)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Primary Attributes", ReplicatedUsing = OnRep_Intelligence)
	FGameplayAttributeData Intelligence;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Intelligence)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Primary Attributes", ReplicatedUsing = OnRep_Expertise)
	FGameplayAttributeData Expertise;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Expertise)

	/** Function to notify replication for Strength attribute */
	UFUNCTION()
	void OnRep_Strength(const FGameplayAttributeData& OldStrength) const;

	/** Function to notify replication for Agility attribute */
	UFUNCTION()
	void OnRep_Agility(const FGameplayAttributeData& OldAgility) const;

	/** Function to notify replication for Resilience attribute */
	UFUNCTION()
	void OnRep_Resilience(const FGameplayAttributeData& OldResilince) const;

	/** Function to notify replication for Intelligence attribute */
	UFUNCTION()
	void OnRep_Intelligence(const FGameplayAttributeData& OldIntelligence) const;

	/** Function to notify replication for Expertise attribute */
	UFUNCTION()
	void OnRep_Expertise(const FGameplayAttributeData& OldExpertise) const;

	//-----------------------------------------------------------------------------------------------------------------------------------------
	// Secondary Attributes
	//-----------------------------------------------------------------------------------------------------------------------------------------


	/*
	 * Vital Attributes
	 */

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Vital Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Vital Attributes", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UAZ_HeroAttributeSet, Mana)

	/** Function to notify replication for Health attribute */
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

	/** Function to notify replication for Mana attribute */
	UFUNCTION() 
	void OnRep_Mana(const FGameplayAttributeData& OldMana) const;

	
};
