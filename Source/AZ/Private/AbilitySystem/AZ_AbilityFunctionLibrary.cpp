// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AZ_AbilityFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"
#include "AbilitySystem/AZ_GameplayEffectTypes.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AbilitySystem/Abilities/AZ_AbilityTypes.h"
#include "Data/AZ_CharacterClassInfo.h"
#include "Game/AZ_GameModeBase.h"
#include "Kismet/GameplayStatics.h"

/*UOverlayWidgetController* UAZ_AbilityFunctionLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	// Early out if WorldContextObject is invalid
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	// Using a single line to get the HUD and check if it's valid.
	if (const auto HUD = Cast<AEchoBaseHud>(UGameplayStatics::GetPlayerController(WorldContextObject, 0)
		                                        ? UGameplayStatics::GetPlayerController(WorldContextObject, 0)->GetHUD()
		                                        : nullptr); !HUD)
	{
		return nullptr;
	}
	else
	{
		// Get PlayerState and check its validity once
		const auto PS = UGameplayStatics::GetPlayerController(WorldContextObject, 0)->GetPlayerState<AEchoPlayerState>();
		if (!IsValid(PS))
		{
			return nullptr;
		}

		// Get necessary components and attributes, checking for null in a single line
		const auto ASC = PS->GetAbilitySystemComponent();
		const auto AS = PS->GetAttributeSet();
		const auto AmmoAS = PS->GetAmmoAttributeSet();
		const auto HeroAS = PS->GetHeroAttributeSet();
		if (!ASC || !AS || !AmmoAS || !HeroAS)
		{
			return nullptr;
		}

		const auto PlayerWidgetData = FWidgetControllerData(UGameplayStatics::GetPlayerController(WorldContextObject, 0), PS, ASC, AS, AmmoAS, HeroAS);
		return HUD->InitializeOverlayWidgetController(PlayerWidgetData);
	}
}*/

FString UAZ_AbilityFunctionLibrary::GetPlayerEditorWindowRole(UWorld* World)
{
	FString Prefix;
	if (World)
	{
		if (World->WorldType == EWorldType::PIE)
		{
			switch (World->GetNetMode())
			{
			case NM_Client:
				Prefix = FString::Printf(TEXT("Client %d "), UE::GetPlayInEditorID() - 1);
				break;
			case NM_DedicatedServer:
			case NM_ListenServer:
				Prefix = FString::Printf(TEXT("Server "));
				break;
			case NM_Standalone:
				break;
			}
		}
	}

	return Prefix;
}

UAZ_GameplayAbility* UAZ_AbilityFunctionLibrary::GetPrimaryAbilityInstanceFromHandle(UAbilitySystemComponent* AbilitySystemComponent,
                                                                                       FGameplayAbilitySpecHandle Handle)
{
	if (AbilitySystemComponent)
	{
		FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec)
		{
			const auto Ability = AbilitySpec->GetPrimaryInstance();
			return Cast<UAZ_GameplayAbility>(Ability);
		}
	}

	return nullptr;
}

UAZ_GameplayAbility* UAZ_AbilityFunctionLibrary::GetPrimaryAbilityInstanceFromClass(UAbilitySystemComponent* AbilitySystemComponent,
                                                                                      TSubclassOf<UGameplayAbility> InAbilityClass)
{
	if (AbilitySystemComponent)
	{
		FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(InAbilityClass);
		if (AbilitySpec)
		{
			return Cast<UAZ_GameplayAbility>(AbilitySpec->GetPrimaryInstance());
		}
	}

	return nullptr;
}

bool UAZ_AbilityFunctionLibrary::IsPrimaryAbilityInstanceActive(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle)
{
	if (AbilitySystemComponent)
	{
		FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec)
		{
			return Cast<UAZ_GameplayAbility>(AbilitySpec->GetPrimaryInstance())->IsActive();
		}
	}

	return false;
}

bool UAZ_AbilityFunctionLibrary::IsAbilitySpecHandleValid(FGameplayAbilitySpecHandle Handle)
{
	return Handle.IsValid();
}

bool UAZ_AbilityFunctionLibrary::DoesEffectContainerSpecHaveEffects(const FAZ_GameplayEffectContainerSpec& ContainerSpec)
{
	return ContainerSpec.HasValidEffects();
}

bool UAZ_AbilityFunctionLibrary::DoesEffectContainerSpecHaveTargets(const FAZ_GameplayEffectContainerSpec& ContainerSpec)
{
	return ContainerSpec.HasValidTargets();
}

void UAZ_AbilityFunctionLibrary::ClearEffectContainerSpecTargets(FAZ_GameplayEffectContainerSpec& ContainerSpec)
{
	ContainerSpec.ClearTargets();
}

void UAZ_AbilityFunctionLibrary::AddTargetsToEffectContainerSpec(FAZ_GameplayEffectContainerSpec& ContainerSpec,
                                                                  const TArray<FGameplayAbilityTargetDataHandle>& TargetData,
                                                                  const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors)
{
	ContainerSpec.AddTargets(TargetData, HitResults, TargetActors);
}

TArray<FActiveGameplayEffectHandle> UAZ_AbilityFunctionLibrary::ApplyExternalEffectContainerSpec(const FAZ_GameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	// Iterate list of gameplay effects
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		if (SpecHandle.IsValid())
		{
			// If effect is valid, iterate list of targets and apply to all
			for (TSharedPtr<FGameplayAbilityTargetData> Data : ContainerSpec.TargetData.Data)
			{
				AllEffects.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get()));
			}
		}
	}
	return AllEffects;
}

FGameplayAbilityTargetDataHandle UAZ_AbilityFunctionLibrary::EffectContextGetTargetData(FGameplayEffectContextHandle EffectContextHandle)
{
	if (FAZ_GameplayEffectContext* EffectContext = static_cast<FAZ_GameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return EffectContext->GetTargetData();
	}

	return FGameplayAbilityTargetDataHandle();
}

void UAZ_AbilityFunctionLibrary::EffectContextAddTargetData(FGameplayEffectContextHandle EffectContextHandle,
                                                             const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (FAZ_GameplayEffectContext* EffectContext = static_cast<FAZ_GameplayEffectContext*>(EffectContextHandle.Get()))
	{
		EffectContext->AddTargetData(TargetData);
	}
}

void UAZ_AbilityFunctionLibrary::ClearTargetData(FGameplayAbilityTargetDataHandle& TargetData)
{
	TargetData.Clear();
}

void UAZ_AbilityFunctionLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, EAZCharacterClass CharacterClass, const float Level, UAbilitySystemComponent* ASC)
{
	// Nullptr Safety Checks
	if (!WorldContextObject || !ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldContextObject or ASC is null."));
		return;
	}

	const AActor* AvatarActor = ASC->GetAvatarActor();
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AvatarActor not found for ASC."));
		return;
	}

	UAZ_CharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (!CharacterClassInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get CharacterClassInfo."));
		return;
	}

	const FAZCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);

	// Check Attributes
	if (!ClassDefaultInfo.PrimaryAttributes || !CharacterClassInfo->SecondaryAttributes || !CharacterClassInfo->VitalAttributes)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing attribute data for CharacterClass."));
		return;
	}

	// Primary Attributes
	FGameplayEffectContextHandle PrimaryAttributesContextHandle = ASC->MakeEffectContext();
	PrimaryAttributesContextHandle.AddSourceObject(AvatarActor);
	if (const FGameplayEffectSpecHandle PrimaryAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.PrimaryAttributes, Level,
	                                                                                        PrimaryAttributesContextHandle); PrimaryAttributesSpecHandle.Data.
		IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*PrimaryAttributesSpecHandle.Data.Get());
	}

	// Secondary Attributes
	FGameplayEffectContextHandle SecondaryAttributesContextHandle = ASC->MakeEffectContext();
	SecondaryAttributesContextHandle.AddSourceObject(AvatarActor);
	if (const FGameplayEffectSpecHandle SecondaryAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->SecondaryAttributes, Level,
	                                                                                          SecondaryAttributesContextHandle); SecondaryAttributesSpecHandle.
		Data.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SecondaryAttributesSpecHandle.Data.Get());
	}

	// Vital Attributes
	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	if (const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->VitalAttributes, Level,
	                                                                                      VitalAttributesContextHandle); VitalAttributesSpecHandle.Data.
		IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
	}
}

UAZ_CharacterClassInfo* UAZ_AbilityFunctionLibrary::GetCharacterClassInfo(const UObject* ContextObject)
{
	// Get the game mode from the given context object
	const AGameModeBase* GameMode = UGameplayStatics::GetGameMode(ContextObject);
	if (!GameMode)
		return nullptr;

	// Cast the game mode to the specific AEchoGameMode type
	const auto* CharacterGameMode = Cast<AAZ_GameModeBase>(GameMode);
	if (!CharacterGameMode)
		return nullptr;

	// Return the CharacterClassInfo pointer
	return CharacterGameMode->CharacterClassInfo;
}

// This ensures the entire function is compiled out and removed for shipping builds.

void UAZ_AbilityFunctionLibrary::PrintDebugMessage(const UObject* WorldContextObject, const FString& Message, FColor InColor, float Duration)
{
	if (!GEngine || !WorldContextObject || !WorldContextObject->GetWorld())
	{
		return;
	}
	
	const UWorld* World = WorldContextObject->GetWorld();
	FString Prefix;
	FColor FinalColor = InColor; // Use the provided color by default

	// Check the network mode and set the prefix and color accordingly.
	switch (World->GetNetMode())
	{
	case NM_ListenServer:
	case NM_DedicatedServer:
		Prefix = TEXT("[SERVER]");
		FinalColor = FColor::Green; // Green for server messages
		break;
	case NM_Client:
		// GPlayInEditorID helps differentiate clients in PIE. Client 1 will be blue, Client 2 will be yellow, etc.
		Prefix = FString::Printf(TEXT("[CLIENT %d]"), UE::GetPlayInEditorID() - 1);
		FinalColor = ( UE::GetPlayInEditorID()  == 2) ? FColor::Yellow : FColor::Cyan; // Cyan for Client 1, Yellow for Client 2
		break;
	default:
		Prefix = TEXT("[STANDALONE]");
		FinalColor = FColor::White;
		break;
	}

	const FString ContextString = WorldContextObject->GetName();
	const FString FinalMessage = FString::Printf(TEXT("%s [%s]: %s"), *Prefix, *ContextString, *Message);

	GEngine->AddOnScreenDebugMessage(-1, Duration, FinalColor, FinalMessage);
}
