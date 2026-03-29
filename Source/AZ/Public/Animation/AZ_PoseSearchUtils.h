#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_PoseSearchUtils.generated.h"

class UPoseSearchDatabase;

/**
 * Utility functions for PoseSearch database manipulation.
 * Exposes AddAnimationAsset to Blueprint/Python since the engine doesn't.
 */
UCLASS()
class AZ_API UAZ_PoseSearchUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Add an animation sequence to a PoseSearch database. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddSequenceToDatabase(UPoseSearchDatabase* Database, UAnimSequence* Sequence);

	/** Add multiple animation sequences to a PoseSearch database at once. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static int32 AddSequencesToDatabase(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Sequences);
};
