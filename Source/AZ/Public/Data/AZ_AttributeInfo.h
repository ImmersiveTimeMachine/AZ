#pragma once

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <GameplayTagContainer.h>
#include "AZ_AttributeInfo.generated.h"

USTRUCT(BlueprintType)
struct FAZAttributeInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AttributeTag = FGameplayTag();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeName = FText();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeDescription = FText();

	UPROPERTY(BlueprintReadOnly)
	float AttributeValue = 0.f;
};

UCLASS()
class AZ_API UAZ_AttributeInfo : public UDataAsset
{
	GENERATED_BODY()

public:
	
	FAZAttributeInfo FindAttributeInfoForTag(const FGameplayTag& AttributeTag, bool bLogNotFound = false) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FAZAttributeInfo> AttributeInformation;
		
};
