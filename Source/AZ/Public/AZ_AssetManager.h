#pragma once

#include <CoreMinimal.h>
#include <Engine/AssetManager.h>
#include "AZ_AssetManager.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_AssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:

	static UAZ_AssetManager& Get();

protected:

	virtual void StartInitialLoading() override;
	
	
};
