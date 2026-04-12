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

	/** Remove animation at a specific index from a PoseSearch database. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static void RemoveAnimationAtIndex(UPoseSearchDatabase* Database, int32 Index);

	/** Clear all animations from a PoseSearch database. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static void ClearDatabase(UPoseSearchDatabase* Database);

	/** Add a PoseSearchBlockTransition notify state to an animation.
	 *  Blocks MM from transitioning into this animation during the marked range.
	 *  @param Sequence The animation to modify
	 *  @param StartTime Start of the block window (seconds)
	 *  @param Duration Duration of the block window (seconds)
	 *  @return true if notify was added successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddBlockTransitionNotify(UAnimSequence* Sequence, float StartTime, float Duration);

	/** Add BlockTransition notifies to all animations in a PoseSearch database.
	 *  Blocks 80% of each animation (10% margin at start/end for transition windows).
	 *  @param Database The database whose animations will be modified
	 *  @return Number of animations modified
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static int32 AddBlockTransitionToDatabase(UPoseSearchDatabase* Database);
};
