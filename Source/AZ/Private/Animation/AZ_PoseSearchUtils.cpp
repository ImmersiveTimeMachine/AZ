#include "Animation/AZ_PoseSearchUtils.h"
#include "AbilitySystem/AZ_AnimNotify_SendGameplayEvent.h"   // the beat-clock notify class
#include "AZ_GameplayTags.h"                                 // Event.Combat.BeatEnd (python-bridge default)
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

namespace
{
	// EDITOR VISIBILITY + TRACK-CACHE INTEGRITY (audit P3-25, the 2026-06-12 "notifies gone" saga):
	// a notify appended straight to UAnimSequenceBase::Notifies is fully FUNCTIONAL (MM/runtime read the raw
	// array) but INVISIBLE in the anim editor's notify panel, which only draws events registered on a named
	// track (AnimNotifyTracks) — they only reappear after PostLoad rebuilds the cache on the next restart.
	// Register the event on track 0 and refresh the editor cache. ALSO required after REMOVALS:
	// RemoveAt without RefreshCacheData leaves DANGLING FAnimNotifyEvent pointers inside
	// AnimNotifyTracks[].Notifies — a latent editor crash, not just a cosmetic gap.
	void RegisterNotifyOnTrackAndRefresh(UAnimSequence* Sequence, FAnimNotifyEvent* EvtOrNull)
	{
#if WITH_EDITOR
		if (Sequence->AnimNotifyTracks.Num() == 0)
		{
			Sequence->InitializeNotifyTrack();
		}
		if (EvtOrNull)
		{
			EvtOrNull->TrackIndex = 0;
		}
		Sequence->RefreshCacheData();
#endif
	}
}

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

bool UAZ_PoseSearchUtils::AddAnimAssetToDatabase(UPoseSearchDatabase* Database, UObject* AnimAsset)
{
	if (!Database || !AnimAsset)
	{
		return false;
	}
	// FPoseSearchDatabaseAnimationAsset is the 5.8 unified entry — AnimAsset takes any database-legal
	// UObject (sequence, montage, composite, blendspace, UMultiAnimAsset). Validation happens at index
	// build; an illegal type shows up in the DB editor as an invalid entry, not silently.
	FPoseSearchDatabaseAnimationAsset NewAsset;
	NewAsset.AnimAsset = AnimAsset;
	Database->AddAnimationAsset(NewAsset);
	Database->MarkPackageDirty();
	return true;
}

bool UAZ_PoseSearchUtils::AddGameplayEventNotify(UAnimSequenceBase* Animation, FGameplayTag EventTag, float TriggerTime)
{
	// PYTHON BRIDGE: Python cannot construct an FGameplayTag (TagName is read-only, no factory exposed),
	// so an INVALID tag means "the beat clock's tag" — the only one a scripted pass ever authors here.
	// C++/BP callers pass a real tag and this is a no-op.
	if (!EventTag.IsValid())
	{
		EventTag = FAZ_GameplayTags::Get().Event_Combat_BeatEnd;
	}
	if (!Animation || !EventTag.IsValid() || TriggerTime < 0.f || TriggerTime > Animation->GetPlayLength())
	{
		return false;
	}

	Animation->Modify();

	// IDEMPOTENT: re-running a beat pass must retune, never stack. Two SendGameplayEvent notifies with the
	// same tag would both fire and the earlier one would win the beat race silently.
	for (FAnimNotifyEvent& Existing : Animation->Notifies)
	{
		if (const UAZ_AnimNotify_SendGameplayEvent* Sender = Cast<UAZ_AnimNotify_SendGameplayEvent>(Existing.Notify))
		{
			if (Sender->EventTag == EventTag)
			{
				Existing.Link(Animation, TriggerTime);
				Existing.SetTime(TriggerTime);
				Animation->PostEditChange();
				Animation->MarkPackageDirty();
				UE_LOG(LogTemp, Display, TEXT("[AZ Notify] %s: MOVED %s -> %.3fs"),
					*Animation->GetName(), *EventTag.ToString(), TriggerTime);
				return true;
			}
		}
	}

	UAZ_AnimNotify_SendGameplayEvent* Sender =
		NewObject<UAZ_AnimNotify_SendGameplayEvent>(Animation, NAME_None, RF_Transactional);
	if (!Sender)
	{
		return false;
	}
	Sender->EventTag = EventTag;

	FAnimNotifyEvent& NotifyEvent = Animation->Notifies.AddDefaulted_GetRef();
	NotifyEvent.NotifyName = FName(*EventTag.ToString());
	NotifyEvent.Notify = Sender;          // a single-frame notify, NOT a notify STATE
	NotifyEvent.NotifyStateClass = nullptr;
	NotifyEvent.SetDuration(0.f);
	NotifyEvent.TriggerTimeOffset = 0.f;
	NotifyEvent.EndTriggerTimeOffset = 0.f;
	NotifyEvent.Link(Animation, TriggerTime);
	NotifyEvent.SetTime(TriggerTime);

	// Editor visibility + track-cache integrity (see RegisterNotifyOnTrackAndRefresh's comment): a notify
	// appended straight to Notifies works at runtime but is INVISIBLE in the montage editor until a
	// PostLoad rebuilds the track cache.
#if WITH_EDITOR
	if (Animation->AnimNotifyTracks.Num() == 0)
	{
		Animation->InitializeNotifyTrack();
	}
	NotifyEvent.TrackIndex = 0;
	Animation->RefreshCacheData();
#endif
	Animation->PostEditChange();
	Animation->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("[AZ Notify] %s: ADDED %s @ %.3fs"),
		*Animation->GetName(), *EventTag.ToString(), TriggerTime);
	return true;
}

bool UAZ_PoseSearchUtils::SetSamplingRangeOnEntry(UPoseSearchDatabase* Database, int32 EntryIndex, float MinTime, float MaxTime)
{
#if WITH_EDITOR
	if (!Database)
	{
		return false;
	}
	FPoseSearchDatabaseAnimationAssetBase* Entry = Database->GetMutableDatabaseAnimationAsset(EntryIndex);
	if (!Entry)
	{
		return false;
	}
	Database->Modify();
	Entry->SetSamplingRange(FFloatInterval(MinTime, MaxTime));
	UE_LOG(LogTemp, Display, TEXT("[AZ PoseSearch] %s entry %d (%s) SamplingRange = [%.3f, %.3f]"),
		*Database->GetName(), EntryIndex, *GetNameSafe(Entry->GetAnimationAsset()),
		Entry->GetSamplingRange().Min, Entry->GetSamplingRange().Max);
	Database->MarkPackageDirty();
	// Requests the async reindex; the rebuild still waits for an ACCESS (e.g. opening the DB editor) — R12.
	Database->PostEditChange();
	return true;
#else
	return false;
#endif
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
	NotifyEvent.Link(Sequence, StartTime);
	NotifyEvent.SetTime(StartTime);
	RegisterNotifyOnTrackAndRefresh(Sequence, &NotifyEvent);
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

bool UAZ_PoseSearchUtils::AddBranchInNotify(UAnimSequence* Sequence, UPoseSearchDatabase* Database, float StartTime, float Duration)
{
	// Database is MANDATORY — a BranchIn with a null Database is rejected by the engine search code, so the
	// notify would exist but MM would never link the clip to a database (-> SelectedAnim null). This is the
	// whole point of the fix.
	if (!Sequence || !Database) return false;
	Sequence->Modify();

	// BranchIn is a state notify (UAnimNotifyState_PoseSearchBranchIn). Cover [StartTime, end] by default so the
	// whole searchable portion is a valid branch-in window (the ExcludeFromDatabase notify, if any, still removes
	// the rise poses from the DB index, so MM only matches the fall).
	if (Duration <= 0.f)
	{
		Duration = FMath::Max(0.01f, Sequence->GetPlayLength() - StartTime);
	}

	auto* NotifyState = NewObject<UAnimNotifyState_PoseSearchBranchIn>(Sequence, NAME_None, RF_Transactional);
	if (!NotifyState) return false;
	NotifyState->Database = Database;   // the missing line: links this raw clip to the DB that indexes it

	FAnimNotifyEvent& Evt = Sequence->Notifies.AddDefaulted_GetRef();
	Evt.NotifyName = FName(TEXT("PoseSearchBranchIn"));
	Evt.Notify = nullptr;
	Evt.NotifyStateClass = NotifyState;
	Evt.SetDuration(Duration);
	Evt.TriggerTimeOffset = 0.f;
	Evt.EndTriggerTimeOffset = 0.f;
	Evt.Link(Sequence, StartTime);
	Evt.SetTime(StartTime);
	RegisterNotifyOnTrackAndRefresh(Sequence, &Evt);

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
	Evt.Link(Sequence, StartTime);
	Evt.SetTime(StartTime);
	RegisterNotifyOnTrackAndRefresh(Sequence, &Evt);

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
	Evt.Link(Sequence, StartTime);
	Evt.SetTime(StartTime);
	RegisterNotifyOnTrackAndRefresh(Sequence, &Evt);

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
	Evt.Link(Sequence, StartTime);
	Evt.SetTime(StartTime);
	RegisterNotifyOnTrackAndRefresh(Sequence, &Evt);

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
		// Rebuild the editor track cache — RemoveAt alone leaves dangling event pointers in
		// AnimNotifyTracks[].Notifies (latent editor crash; see helper doc above).
		RegisterNotifyOnTrackAndRefresh(Sequence, nullptr);
		Sequence->PostEditChange();
		Sequence->MarkPackageDirty();
	}
	return Removed;
}
