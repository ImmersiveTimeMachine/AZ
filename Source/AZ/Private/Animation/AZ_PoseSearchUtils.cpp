#include "Animation/AZ_PoseSearchUtils.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

bool UAZ_PoseSearchUtils::AddSequenceToDatabase(UPoseSearchDatabase* Database, UAnimSequence* Sequence)
{
	if (!Database || !Sequence)
	{
		return false;
	}

	FPoseSearchDatabaseAnimationAsset NewAsset;
	NewAsset.AnimAsset = Sequence;

	Database->AddAnimationAsset(NewAsset);
	return true;
}

int32 UAZ_PoseSearchUtils::AddSequencesToDatabase(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Sequences)
{
	if (!Database)
	{
		return 0;
	}

	int32 Added = 0;
	for (UAnimSequence* Seq : Sequences)
	{
		if (Seq)
		{
			FPoseSearchDatabaseAnimationAsset NewAsset;
			NewAsset.AnimAsset = Seq;
			Database->AddAnimationAsset(NewAsset);
			Added++;
		}
	}

	return Added;
}

void UAZ_PoseSearchUtils::RemoveAnimationAtIndex(UPoseSearchDatabase* Database, int32 Index)
{
	if (Database && Index >= 0 && Index < Database->GetNumAnimationAssets())
	{
		Database->RemoveAnimationAssetAt(Index);
	}
}

void UAZ_PoseSearchUtils::ClearDatabase(UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return;
	}

	while (Database->GetNumAnimationAssets() > 0)
	{
		Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets() - 1);
	}
}

bool UAZ_PoseSearchUtils::AddBlockTransitionNotify(UAnimSequence* Sequence, float StartTime, float Duration)
{
	if (!Sequence || Duration <= 0.f)
	{
		return false;
	}

	Sequence->Modify();

	// Create the notify state
	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchBlockTransition>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState)
	{
		return false;
	}

	// Create the anim notify event
	FAnimNotifyEvent& NotifyEvent = Sequence->Notifies.AddDefaulted_GetRef();
	NotifyEvent.NotifyName = FName(TEXT("PoseSearchBlockTransition"));
	NotifyEvent.Notify = nullptr;
	NotifyEvent.NotifyStateClass = NotifyState;
	NotifyEvent.SetDuration(Duration);
	NotifyEvent.TriggerTimeOffset = 0.f;
	NotifyEvent.EndTriggerTimeOffset = 0.f;

	// Set time via link
	NotifyEvent.LinkSequence(Sequence, StartTime);
	NotifyEvent.SetTime(StartTime);
	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();

	return true;
}

int32 UAZ_PoseSearchUtils::AddBlockTransitionToDatabase(UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return 0;
	}

	int32 Modified = 0;
	const int32 NumAnims = Database->GetNumAnimationAssets();

	for (int32 i = 0; i < NumAnims; ++i)
	{
		UAnimSequence* Seq = Cast<UAnimSequence>(Database->GetAnimationAsset(i));
		{
			if (!Seq)
			{
				continue;
			}

			const float Length = Seq->GetPlayLength();
			if (Length <= 0.2f)
			{
				continue; // too short
			}

			// 10% margin at start/end, block the middle 80%
			const float Margin = FMath::Max(Length * 0.1f, 0.1f);
			const float BlockStart = Margin;
			const float BlockDuration = Length - (2.f * Margin);

			if (BlockDuration > 0.f && AddBlockTransitionNotify(Seq, BlockStart, BlockDuration))
			{
				UE_LOG(LogTemp, Log, TEXT("BlockTransition: %s [%.2f - %.2f]"),
					*Seq->GetName(), BlockStart, BlockStart + BlockDuration);
				Modified++;
			}
		}
	}

	return Modified;
}

bool UAZ_PoseSearchUtils::AddBranchInNotify(UAnimSequence* Sequence, float TriggerTime)
{
	if (!Sequence) return false;
	Sequence->Modify();

	// BranchIn is a state notify (UAnimNotifyState_PoseSearchBranchIn), not instant.
	// Use a minimal duration so it acts as a "branch point" window.
	const float MinDuration = 0.01f;

	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchBranchIn>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState) return false;

	FAnimNotifyEvent& Evt = Sequence->Notifies.AddDefaulted_GetRef();
	Evt.NotifyName = FName(TEXT("PoseSearchBranchIn"));
	Evt.Notify = nullptr;
	Evt.NotifyStateClass = NotifyState;
	Evt.SetDuration(MinDuration);
	Evt.TriggerTimeOffset = 0.f;
	Evt.EndTriggerTimeOffset = 0.f;
	Evt.LinkSequence(Sequence, TriggerTime);
	Evt.SetTime(TriggerTime);

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}

bool UAZ_PoseSearchUtils::AddExcludeFromDatabaseNotify(UAnimSequence* Sequence, float StartTime, float Duration)
{
	if (!Sequence || Duration <= 0.f) return false;
	Sequence->Modify();

	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchExcludeFromDatabase>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState) return false;

	FAnimNotifyEvent& Evt = Sequence->Notifies.AddDefaulted_GetRef();
	Evt.NotifyName = FName(TEXT("PoseSearchExcludeFromDatabase"));
	Evt.Notify = nullptr;
	Evt.NotifyStateClass = NotifyState;
	Evt.SetDuration(Duration);
	Evt.TriggerTimeOffset = 0.f;
	Evt.EndTriggerTimeOffset = 0.f;
	Evt.LinkSequence(Sequence, StartTime);
	Evt.SetTime(StartTime);

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}

bool UAZ_PoseSearchUtils::AddModifyCostNotify(UAnimSequence* Sequence, float StartTime, float Duration, float CostAddend)
{
	if (!Sequence || Duration <= 0.f) return false;
	Sequence->Modify();

	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchModifyCost>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState) return false;

	NotifyState->CostAddend = CostAddend;

	FAnimNotifyEvent& Evt = Sequence->Notifies.AddDefaulted_GetRef();
	Evt.NotifyName = FName(TEXT("PoseSearchModifyCost"));
	Evt.Notify = nullptr;
	Evt.NotifyStateClass = NotifyState;
	Evt.SetDuration(Duration);
	Evt.TriggerTimeOffset = 0.f;
	Evt.EndTriggerTimeOffset = 0.f;
	Evt.LinkSequence(Sequence, StartTime);
	Evt.SetTime(StartTime);

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}

bool UAZ_PoseSearchUtils::AddOverrideContinuingPoseCostBiasNotify(UAnimSequence* Sequence, float StartTime, float Duration, float CostBias)
{
	if (!Sequence || Duration <= 0.f) return false;
	Sequence->Modify();

	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState) return false;

	NotifyState->CostAddend = CostBias;

	FAnimNotifyEvent& Evt = Sequence->Notifies.AddDefaulted_GetRef();
	Evt.NotifyName = FName(TEXT("PoseSearchOverrideContinuingPoseCostBias"));
	Evt.Notify = nullptr;
	Evt.NotifyStateClass = NotifyState;
	Evt.SetDuration(Duration);
	Evt.TriggerTimeOffset = 0.f;
	Evt.EndTriggerTimeOffset = 0.f;
	Evt.LinkSequence(Sequence, StartTime);
	Evt.SetTime(StartTime);

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}

int32 UAZ_PoseSearchUtils::RemoveAllPoseSearchNotifies(UAnimSequence* Sequence)
{
	if (!Sequence) return 0;
	Sequence->Modify();

	int32 Removed = 0;
	for (int32 i = Sequence->Notifies.Num() - 1; i >= 0; --i)
	{
		const FString NotifyName = Sequence->Notifies[i].NotifyName.ToString();
		if (NotifyName.StartsWith(TEXT("PoseSearch")))
		{
			Sequence->Notifies.RemoveAt(i);
			++Removed;
		}
	}

	if (Removed > 0)
	{
		Sequence->PostEditChange();
		Sequence->MarkPackageDirty();
	}
	return Removed;
}
