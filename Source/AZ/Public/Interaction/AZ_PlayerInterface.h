#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>
#include "AZ_PlayerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAZ_PlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class AZ_API IAZ_PlayerInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent)
	int32 FindLevelForXP(int32 InXP) const;

	UFUNCTION(BlueprintNativeEvent)
	int32 GetXP() const;

	UFUNCTION(BlueprintNativeEvent)
	int32 GetAttributePointsReward(int32 Level) const;

	UFUNCTION(BlueprintNativeEvent)
	int32 GetSpellPointsReward(int32 Level) const;
	
	UFUNCTION(BlueprintNativeEvent)
	void AddToXP(int32 InXP);

	UFUNCTION(BlueprintNativeEvent)
	void AddToPlayerLevel(int32 InPlayerLevel);

	UFUNCTION(BlueprintNativeEvent)
	void AddToAttributePoints(int32 InAttributePoints);

	UFUNCTION(BlueprintNativeEvent)
	int32 GetAttributePoints() const;

	UFUNCTION(BlueprintNativeEvent)
	void AddToSpellPoints(int32 InSpellPoints);

	UFUNCTION(BlueprintNativeEvent)
	int32 GetSpellPoints() const;
	
	UFUNCTION(BlueprintNativeEvent)
	void LevelUp();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ShowMagicCircle(UMaterialInterface* DecalMaterial = nullptr);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void HideMagicCircle();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SaveProgress(const FName& CheckpointTag);
	
};
