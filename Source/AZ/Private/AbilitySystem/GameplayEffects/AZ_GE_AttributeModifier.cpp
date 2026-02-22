// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/GameplayEffects/AZ_GE_AttributeModifier.h"

#include "AZ_GameplayTags.h"

UAZ_GE_AttributeModifier::UAZ_GE_AttributeModifier()
{
	const auto& GameplayTags = FAZ_GameplayTags::Get();
	
	// Set the Duration Policy (Instant, Infinite, or Has Duration)
	DurationPolicy = EGameplayEffectDurationType::Instant; // Or Infinite, or HasDuration
	
	// Note: You don't need to set the attribute here in C++.
	// We'll be using "Set By Caller" and a Gameplay Tag to determine the attribute dynamically.

	// Create a modifier for the attribute
	FGameplayModifierInfo ModifierInfo;
	// This is just a placeholder, the actual attribute will be set by caller using a tag.
	// We still need to set the ModifierOp and Magnitude.

	// Set the Modifier Operation (e.g., Additive, Multiplicative, Override)
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	//ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FSetByCallerFloat());

	// Set the Modifier Magnitude to use "Set By Caller" with a tag
    
	// Set By Caller: Create a FSetByCallerFloat with a data tag
	FSetByCallerFloat SetByCallerMagnitude;

	// The gameplay tag will be set via SetSetByCaller when applying the Gameplay Effect
	// Don't forget to initialize this tag in your GameplayTags initialization function
	SetByCallerMagnitude.DataTag = GameplayTags.GameplayEffect_Input_Magnitude; // Use a consistent tag for input magnitude

	// Assign the SetByCallerMagnitude to ModifierMagnitude
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);

	// Add the modifier to the Modifiers array
	Modifiers.Add(ModifierInfo);
}
