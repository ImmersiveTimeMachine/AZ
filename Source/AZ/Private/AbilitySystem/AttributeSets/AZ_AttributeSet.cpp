#include "AZ/Public/AbilitySystem/AttributeSets/AZ_AttributeSet.h"

#include <AbilitySystemBlueprintLibrary.h>
#include <AbilitySystemComponent.h>
#include <GameplayEffect.h>
#include <GameplayEffectExtension.h>
#include <GameFramework/Character.h>

void UAZ_AttributeSet::ExtractEffectContextInfo(const FGameplayEffectModCallbackData& Data, FEffectContextInfo& ContextInfo) const
{
	ContextInfo.EffectContextHandle = Data.EffectSpec.GetContext();

	// Extract source information
	ContextInfo.SourceAbilitySystemComponent = ContextInfo.EffectContextHandle.GetOriginalInstigatorAbilitySystemComponent();
	ContextInfo.SourceAvatarActor = ContextInfo.SourceAbilitySystemComponent ? ContextInfo.SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
	ContextInfo.SourceController = ContextInfo.SourceAbilitySystemComponent ? ContextInfo.SourceAbilitySystemComponent->AbilityActorInfo->PlayerController.Get() : nullptr;
	ContextInfo.SourceCharacter = ContextInfo.SourceAvatarActor ? Cast<ACharacter>(ContextInfo.SourceAvatarActor) : nullptr;

	// Extract target information
	ContextInfo.TargetAvatarActor = Data.Target.AbilityActorInfo ? Data.Target.AbilityActorInfo->AvatarActor.Get() : nullptr;
	ContextInfo.TargetController = Data.Target.AbilityActorInfo ? Data.Target.AbilityActorInfo->PlayerController.Get() : nullptr;
	ContextInfo.TargetCharacter = ContextInfo.TargetAvatarActor ? Cast<ACharacter>(ContextInfo.TargetAvatarActor) : nullptr;
	ContextInfo.TargetAbilitySystemComponent = ContextInfo.TargetAvatarActor ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ContextInfo.TargetAvatarActor) : nullptr;
}
