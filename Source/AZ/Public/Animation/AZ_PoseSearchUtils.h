#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_PoseSearchUtils.generated.h"

class UPoseSearchDatabase;
class UAnimSequenceBase;

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

	/** Add ANY database-legal animation asset (montage, composite, blendspace, UMultiAnimAsset/PSIA) as
	 *  one entry. Replaces the 2026-08-31 TEMP injected-body hack that added PSD_AZ_Catch's PSIA. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddAnimAssetToDatabase(UPoseSearchDatabase* Database, UObject* AnimAsset);

	/** Author a UAZ_AnimNotify_SendGameplayEvent (the class GA beat clocks look for) on a montage or
	 *  sequence at TriggerTime. IDEMPOTENT: an existing notify with the same tag is MOVED, not doubled —
	 *  two BeatEnd notifies would race the beat. Python cannot do this (Notifies is protected).
	 *  Returns false only on bad input. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Anim")
	static bool AddGameplayEventNotify(UAnimSequenceBase* Animation, FGameplayTag EventTag, float TriggerTime);

	/** Set the SamplingRange (indexed window, seconds; [0,0] = whole asset) on one DB entry — the array
	 *  is private with no scriptable API (second TEMP-hack replacement). Editor only; marks dirty and
	 *  PostEditChange()s so the async reindex is requested; the rebuild still needs an ACCESS (open the
	 *  DB editor) before "BuildIndex Succeeded" appears — R12. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool SetSamplingRangeOnEntry(UPoseSearchDatabase* Database, int32 EntryIndex, float MinTime, float MaxTime);

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

	/** Add a PoseSearchBranchIn notify STATE that LINKS this raw sequence to a PoseSearchDatabase.
	 *  REQUIRED for single-clip MM over a raw UAnimSequence: UPoseSearchLibrary::MotionMatch can only search a
	 *  raw sequence THROUGH its BranchIn notify's Database (which must already index the sequence). A null
	 *  Database is rejected by the engine ("improperly setup ... null Database"), which is why the old
	 *  signature (no Database) produced a notify MM ignored -> SelectedAnim == null -> fallback to frame 0.
	 *  @param Database the DB that indexes this sequence (e.g. PSD_v2_Jump). MUST contain the sequence.
	 *  @param StartTime branch-in window start (s). @param Duration window length (s); <=0 = to end of clip. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddBranchInNotify(UAnimSequence* Sequence, UPoseSearchDatabase* Database, float StartTime = 0.f, float Duration = 0.f);

	/** Add a PoseSearchExcludeFromDatabase notify state covering [StartTime, StartTime+Duration].
	 *  Excludes that time range from being matched by the MM search. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddExcludeFromDatabaseNotify(UAnimSequence* Sequence, float StartTime, float Duration);

	/** Add a PoseSearchModifyCost notify state covering a range. Used on Land anims
	 *  to differentiate Heavy vs Light cost. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddModifyCostNotify(UAnimSequence* Sequence, float StartTime, float Duration, float CostAddend);

	/** Add a PoseSearchOverrideContinuingPoseCostBias notify state. Used on turn-in-place
	 *  and reface-start anims to bias toward completing the current anim. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static bool AddOverrideContinuingPoseCostBiasNotify(UAnimSequence* Sequence, float StartTime, float Duration, float CostBias);

	/** Remove ALL PoseSearch-related notifies from an animation. Useful for re-applying clean. */
	UFUNCTION(BlueprintCallable, Category = "AZ|PoseSearch")
	static int32 RemoveAllPoseSearchNotifies(UAnimSequence* Sequence);
};
