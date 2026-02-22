#include "AZ/Public/AZ_AssetManager.h"
#include "AZ/Public/AZ_GameplayTags.h"

#include <AbilitySystemGlobals.h>

UAZ_AssetManager& UAZ_AssetManager::Get()
{
	check(GEngine);
	const auto EchoAssetManager = Cast<UAZ_AssetManager>(GEngine->AssetManager);

	return *EchoAssetManager;
}

void UAZ_AssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	FAZ_GameplayTags::InitializeNativeGameplayTags();

	// This is required to use Target Data!
	UAbilitySystemGlobals::Get().InitGlobalData();
}




