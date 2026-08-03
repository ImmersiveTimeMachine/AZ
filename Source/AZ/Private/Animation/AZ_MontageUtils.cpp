// Copyright Artur. AZ project.

#include "Animation/AZ_MontageUtils.h"

#include "AZ/AZ.h"
#include "AbilitySystem/AZ_AnimNotify_SendGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "AnimNotifyState_MotionWarping.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

namespace
{
	/** Effective on-timeline duration of one segment once its play rate is applied. */
	float SegmentDuration(const UAnimSequence* Sequence, float PlayRate)
	{
		if (!Sequence)
		{
			return 0.f;
		}
		return Sequence->GetPlayLength() / FMath::Max(PlayRate, UE_KINDA_SMALL_NUMBER);
	}
}

UAnimMontage* UAZ_MontageUtils::BuildSectionedMontage(
	const FString& PackagePath,
	FName SlotName,
	const TArray<FName>& SectionNames,
	const TArray<UAnimSequence*>& Sequences,
	const TArray<FName>& NextSectionNames,
	const TArray<float>& SegmentPlayRates,
	float BlendIn,
	float BlendOut)
{
	const int32 Num = SectionNames.Num();
	if (Num == 0)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': no sections supplied."), *PackagePath);
		return nullptr;
	}
	if (Sequences.Num() != Num || NextSectionNames.Num() != Num)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': parallel arrays disagree (names=%d seqs=%d next=%d)."),
			*PackagePath, Num, Sequences.Num(), NextSectionNames.Num());
		return nullptr;
	}
	if (SegmentPlayRates.Num() != 0 && SegmentPlayRates.Num() != Num)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': SegmentPlayRates must be empty or %d entries (got %d)."),
			*PackagePath, Num, SegmentPlayRates.Num());
		return nullptr;
	}

	// One skeleton for the whole montage, and no duplicate section names (a duplicate would make
	// MontageSync_Follow's same-name test ambiguous, and GetSectionIndex would resolve to the first).
	USkeleton* Skeleton = nullptr;
	for (int32 i = 0; i < Num; ++i)
	{
		if (!Sequences[i])
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': sequence for section '%s' is null."),
				*PackagePath, *SectionNames[i].ToString());
			return nullptr;
		}
		USkeleton* SeqSkeleton = Sequences[i]->GetSkeleton();
		if (!Skeleton)
		{
			Skeleton = SeqSkeleton;
		}
		else if (Skeleton != SeqSkeleton)
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': section '%s' is on skeleton '%s', expected '%s'."),
				*PackagePath, *SectionNames[i].ToString(),
				*GetNameSafe(SeqSkeleton), *GetNameSafe(Skeleton));
			return nullptr;
		}
		for (int32 j = 0; j < i; ++j)
		{
			if (SectionNames[j] == SectionNames[i])
			{
				UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': duplicate section name '%s'."),
					*PackagePath, *SectionNames[i].ToString());
				return nullptr;
			}
		}
	}
	if (!Skeleton)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': could not resolve a skeleton."), *PackagePath);
		return nullptr;
	}

	// Every NextSectionName must name a section that exists here, or be None (= stop).
	for (int32 i = 0; i < Num; ++i)
	{
		if (NextSectionNames[i].IsNone() || SectionNames.Contains(NextSectionNames[i]))
		{
			continue;
		}
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': section '%s' links to unknown section '%s'."),
			*PackagePath, *SectionNames[i].ToString(), *NextSectionNames[i].ToString());
		return nullptr;
	}

	// Rebuild IN PLACE when the asset already exists, so montage references already assigned on
	// abilities and data assets survive a regeneration.
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *PackagePath);
	const bool bCreated = (Montage == nullptr);
	if (bCreated)
	{
		const FString AssetName = FPackageName::GetShortName(PackagePath);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': CreatePackage failed."), *PackagePath);
			return nullptr;
		}
		Montage = NewObject<UAnimMontage>(Package, FName(*AssetName),
			RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!Montage)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': montage allocation failed."), *PackagePath);
		return nullptr;
	}

	Montage->Modify();
	Montage->SetSkeleton(Skeleton);

	// --- Slot track: the clips laid end to end, in order ---
	Montage->SlotAnimTracks.Empty();
	FSlotAnimationTrack Track;
	Track.SlotName = SlotName;

	TArray<float> Durations;
	Durations.Reserve(Num);
	float Cursor = 0.f;
	for (int32 i = 0; i < Num; ++i)
	{
		const float Rate = SegmentPlayRates.Num() ? SegmentPlayRates[i] : 1.f;

		FAnimSegment Segment;
		Segment.SetAnimReference(Sequences[i], /*bInitialize=*/true);   // sets start/end from the clip
		Segment.AnimPlayRate = FMath::Max(Rate, UE_KINDA_SMALL_NUMBER);  // AFTER: bInitialize resets it to 1
		Segment.StartPos = Cursor;
		Segment.LoopingCount = 1;
		Track.AnimTrack.AnimSegments.Add(Segment);

		const float Duration = SegmentDuration(Sequences[i], Rate);
		Durations.Add(Duration);
		Cursor += Duration;
	}
	Montage->SlotAnimTracks.Add(Track);
	Montage->SetCompositeLength(Cursor);

	// --- Section table: one section per segment, starting where that segment starts ---
	Montage->CompositeSections.Empty();
	float SectionStart = 0.f;
	for (int32 i = 0; i < Num; ++i)
	{
		Montage->AddAnimCompositeSection(SectionNames[i], SectionStart);
		SectionStart += Durations[i];
	}

	// Link by NAME, not by index: AddAnimCompositeSection sorts by start position, so the table order
	// is not guaranteed to match the order we added in.
	for (int32 i = 0; i < Num; ++i)
	{
		const int32 Index = Montage->GetSectionIndex(SectionNames[i]);
		if (Montage->CompositeSections.IsValidIndex(Index))
		{
			Montage->CompositeSections[Index].NextSectionName = NextSectionNames[i];
		}
		else
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] '%s': section '%s' vanished after add."),
				*PackagePath, *SectionNames[i].ToString());
		}
	}

	Montage->BlendIn.SetBlendTime(BlendIn);
	Montage->BlendOut.SetBlendTime(BlendOut);

#if WITH_EDITOR
	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Montage);
	}
	// Also reconciles the montage's common target frame rate from its segments (that updater is
	// private + editor-only, so PostEditChange is the supported way to reach it).
	Montage->PostEditChange();
#endif
	Montage->MarkPackageDirty();

	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] %s '%s': %d sections, slot '%s', %.3fs, skeleton '%s'."),
		bCreated ? TEXT("created") : TEXT("rebuilt"), *PackagePath, Num,
		*SlotName.ToString(), Cursor, *GetNameSafe(Skeleton));

	return Montage;
}

bool UAZ_MontageUtils::SetSectionNext(UAnimMontage* Montage, FName SectionName, FName NextSectionName)
{
	if (!Montage)
	{
		return false;
	}
	const int32 Index = Montage->GetSectionIndex(SectionName);
	if (!Montage->CompositeSections.IsValidIndex(Index))
	{
		UE_LOG(Log_AZ, Warning, TEXT("[MontageUtils] '%s' has no section '%s'."),
			*Montage->GetName(), *SectionName.ToString());
		return false;
	}
	if (!NextSectionName.IsNone() && Montage->GetSectionIndex(NextSectionName) == INDEX_NONE)
	{
		UE_LOG(Log_AZ, Warning, TEXT("[MontageUtils] '%s': next section '%s' does not exist."),
			*Montage->GetName(), *NextSectionName.ToString());
		return false;
	}

	Montage->Modify();
	Montage->CompositeSections[Index].NextSectionName = NextSectionName;
#if WITH_EDITOR
	Montage->PostEditChange();
#endif
	Montage->MarkPackageDirty();
	return true;
}

bool UAZ_MontageUtils::SetMontageBlendTimes(UAnimMontage* Montage, float BlendIn, float BlendOut)
{
	if (!Montage)
	{
		return false;
	}
	Montage->Modify();
	Montage->BlendIn.SetBlendTime(BlendIn);
	Montage->BlendOut.SetBlendTime(BlendOut);
#if WITH_EDITOR
	Montage->PostEditChange();
#endif
	Montage->MarkPackageDirty();
	return true;
}

TArray<FString> UAZ_MontageUtils::DumpMontageSections(UAnimMontage* Montage)
{
	TArray<FString> Lines;
	if (!Montage)
	{
		return Lines;
	}

	const int32 Num = Montage->CompositeSections.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		const FCompositeSection& Section = Montage->CompositeSections[i];
		const float Start = Section.GetTime();
		const float End = (i + 1 < Num) ? Montage->CompositeSections[i + 1].GetTime() : Montage->GetPlayLength();
		Lines.Add(FString::Printf(TEXT("%d %s start=%.3f len=%.3f next=%s"),
			i, *Section.SectionName.ToString(), Start, End - Start,
			Section.NextSectionName.IsNone() ? TEXT("<stop>") : *Section.NextSectionName.ToString()));
	}
	return Lines;
}

bool UAZ_MontageUtils::VerifyPairedMontages(UAnimMontage* MontageA, UAnimMontage* MontageB, float ToleranceSeconds)
{
	if (!MontageA || !MontageB)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] VerifyPaired: null montage."));
		return false;
	}

	const int32 NumA = MontageA->CompositeSections.Num();
	const int32 NumB = MontageB->CompositeSections.Num();
	if (NumA != NumB)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] VerifyPaired: section count %d vs %d."), NumA, NumB);
		return false;
	}

	bool bOk = true;
	for (int32 i = 0; i < NumA; ++i)
	{
		const FCompositeSection& A = MontageA->CompositeSections[i];
		const FCompositeSection& B = MontageB->CompositeSections[i];

		if (A.SectionName != B.SectionName)
		{
			// The one failure MontageSync_Follow cannot survive: it only mirrors section jumps while
			// both instances sit in a section of the same name.
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] VerifyPaired: section %d named '%s' vs '%s'."),
				i, *A.SectionName.ToString(), *B.SectionName.ToString());
			bOk = false;
			continue;
		}
		if (FMath::Abs(A.GetTime() - B.GetTime()) > ToleranceSeconds)
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] VerifyPaired: '%s' starts %.3f vs %.3f."),
				*A.SectionName.ToString(), A.GetTime(), B.GetTime());
			bOk = false;
		}
		if (A.NextSectionName != B.NextSectionName)
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] VerifyPaired: '%s' links to '%s' vs '%s'."),
				*A.SectionName.ToString(), *A.NextSectionName.ToString(), *B.NextSectionName.ToString());
			bOk = false;
		}
	}

	if (FMath::Abs(MontageA->GetPlayLength() - MontageB->GetPlayLength()) > ToleranceSeconds)
	{
		UE_LOG(Log_AZ, Warning, TEXT("[MontageUtils] VerifyPaired: total length %.3f vs %.3f."),
			MontageA->GetPlayLength(), MontageB->GetPlayLength());
	}

	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] VerifyPaired '%s' / '%s': %s"),
		*MontageA->GetName(), *MontageB->GetName(), bOk ? TEXT("MATCH") : TEXT("MISMATCH"));
	return bOk;
}

// ---------------------------------------------------------------------------------------------------
// Motion warping
// ---------------------------------------------------------------------------------------------------

bool UAZ_MontageUtils::AddMotionWarpingNotify(
	UAnimMontage* Montage,
	FName SectionName,
	float StartFraction,
	float EndFraction,
	FName WarpTargetName,
	bool bWarpTranslation,
	bool bWarpRotation,
	uint8 RotationType,
	uint8 RotationMethod,
	float MaxRotationRate,
	float WarpRotationTimeMultiplier,
	uint8 AddTranslationEasingFunc,
	float MaxSpeedClampRatio)
{
	if (!Montage)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddWarpNotify: null montage."));
		return false;
	}
	if (WarpTargetName.IsNone())
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddWarpNotify: WarpTargetName is None - the window would "
			"never resolve a target. Pass the same name gameplay registers."));
		return false;
	}
	if (EndFraction <= StartFraction)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddWarpNotify: EndFraction %.3f must exceed StartFraction %.3f."),
			EndFraction, StartFraction);
		return false;
	}

	// Resolve the section window to absolute montage time. NAME_None = the whole montage.
	float SectionStart = 0.f;
	float SectionLength = Montage->GetPlayLength();
	if (!SectionName.IsNone())
	{
		const int32 Index = Montage->GetSectionIndex(SectionName);
		if (Index == INDEX_NONE)
		{
			UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddWarpNotify: no section '%s' on '%s'."),
				*SectionName.ToString(), *Montage->GetName());
			return false;
		}
		SectionStart = Montage->CompositeSections[Index].GetTime();
		const float SectionEnd = (Index + 1 < Montage->CompositeSections.Num())
			? Montage->CompositeSections[Index + 1].GetTime()
			: Montage->GetPlayLength();
		SectionLength = SectionEnd - SectionStart;
	}

	const float TriggerTime = SectionStart + SectionLength * FMath::Clamp(StartFraction, 0.f, 1.f);
	const float EndTime     = SectionStart + SectionLength * FMath::Clamp(EndFraction, 0.f, 1.f);
	const float Duration    = EndTime - TriggerTime;
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddWarpNotify: zero-length window on '%s'."), *Montage->GetName());
		return false;
	}

	Montage->Modify();

#if WITH_EDITORONLY_DATA
	// One dedicated track keeps warp windows out of the way of gameplay-event notifies (hit windows),
	// and gives RemoveMotionWarpingNotifies something unambiguous to clear.
	static const FName WarpTrackName(TEXT("MotionWarping"));
	int32 TrackIndex = INDEX_NONE;
	for (int32 i = 0; i < Montage->AnimNotifyTracks.Num(); ++i)
	{
		if (Montage->AnimNotifyTracks[i].TrackName == WarpTrackName)
		{
			TrackIndex = i;
			break;
		}
	}
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(WarpTrackName, FLinearColor::White));
	}

	// Replace any overlapping warp window rather than stacking a second one - running two modifiers over
	// the same span is how you get a swing that fights itself.
	for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
	{
		const FAnimNotifyEvent& Existing = Montage->Notifies[i];
		if (Existing.NotifyStateClass && Existing.NotifyStateClass->IsA(UAnimNotifyState_MotionWarping::StaticClass())
			&& Existing.GetTriggerTime() < EndTime && Existing.GetEndTriggerTime() > TriggerTime)
		{
			Montage->Notifies.RemoveAt(i);
		}
	}

	UAnimNotifyState_MotionWarping* WarpNotify =
		NewObject<UAnimNotifyState_MotionWarping>(Montage, UAnimNotifyState_MotionWarping::StaticClass(), NAME_None, RF_Transactional);

	// The modifier is an Instanced sub-object of the notify - this is the half Python cannot build, and
	// without it the notify is an inert marker on the timeline.
	URootMotionModifier_SkewWarp* Modifier =
		NewObject<URootMotionModifier_SkewWarp>(WarpNotify, URootMotionModifier_SkewWarp::StaticClass(), NAME_None, RF_Transactional);
	Modifier->WarpTargetName = WarpTargetName;
	Modifier->bWarpTranslation = bWarpTranslation;
	Modifier->bWarpRotation = bWarpRotation;
	Modifier->RotationType = static_cast<EMotionWarpRotationType>(RotationType);
	Modifier->RotationMethod = static_cast<EMotionWarpRotationMethod>(RotationMethod);
	Modifier->WarpMaxRotationRate = MaxRotationRate;
	Modifier->WarpRotationTimeMultiplier = WarpRotationTimeMultiplier;
	// Which of these two actually does anything depends on the CLIP, not on us: SkewWarp branches on
	// whether the animation carries translation of its own. In-place clips (the Chalkie claw cycles) take
	// the "add translation" branch, where only AddTranslationEasingFunc is read and MaxSpeedClampRatio is
	// dead code. Clips WITH root translation take the warp branch, where the reverse is true.
	Modifier->AddTranslationEasingFunc = static_cast<EAlphaBlendOption>(AddTranslationEasingFunc);
	// MaxSpeedClampRatio is protected on URootMotionModifier_SkewWarp, but it is EditAnywhere +
	// BlueprintReadWrite, so reflection is the sanctioned way to reach it from outside the class.
	if (const FFloatProperty* ClampProp = FindFProperty<FFloatProperty>(
			URootMotionModifier_SkewWarp::StaticClass(), TEXT("MaxSpeedClampRatio")))
	{
		ClampProp->SetPropertyValue_InContainer(Modifier, MaxSpeedClampRatio);
	}
	// Warp to the capsule, not the feet: we are warping toward another pawn's root component, and
	// foot-space would fold in the mesh's -90 offset for no benefit on a rotation-only warp.
	Modifier->bWarpToFeetLocation = false;
	WarpNotify->RootMotionModifier = Modifier;

	FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
	Event.Guid = FGuid::NewGuid();
	Event.NotifyName = FName(TEXT("MotionWarping"));
	Event.TrackIndex = TrackIndex;
	Event.NotifyStateClass = WarpNotify;
	// Link both ends to the montage timeline so the window tracks the asset rather than a raw seconds
	// value (SetTime alone drifts if the montage is later re-timed).
	Event.Link(Montage, TriggerTime);
	Event.SetDuration(Duration);
	Event.EndLink.Link(Montage, EndTime);

	Montage->RefreshCacheData();
	Montage->PostEditChange();
#endif   // WITH_EDITORONLY_DATA
	Montage->MarkPackageDirty();

	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] AddWarpNotify '%s': section='%s' window=%.3f..%.3f (dur %.3f) "
		"target='%s' trans=%d rot=%d type=%d method=%d maxRate=%.1f"),
		*Montage->GetName(), SectionName.IsNone() ? TEXT("<all>") : *SectionName.ToString(),
		TriggerTime, EndTime, Duration, *WarpTargetName.ToString(),
		bWarpTranslation ? 1 : 0, bWarpRotation ? 1 : 0, RotationType, RotationMethod, MaxRotationRate);
	return true;
}

bool UAZ_MontageUtils::AddGameplayEventNotify(UAnimMontage* Montage, FName EventTagName,
	float TriggerTime, FName TrackName, bool bReplaceExisting)
{
	if (!Montage)
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddGameplayEventNotify: null montage."));
		return false;
	}
	// ErrorIfNotFound: a typo must fail here rather than author a notify carrying an empty tag, which
	// would sit on the timeline looking correct and fire nothing.
	const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(EventTagName, /*ErrorIfNotFound*/ false);
	if (!EventTag.IsValid())
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddGameplayEventNotify: '%s' is not a registered "
			"gameplay tag — nothing added."), *EventTagName.ToString());
		return false;
	}
	if (TriggerTime < 0.f || TriggerTime > Montage->GetPlayLength())
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddGameplayEventNotify: t=%.3f is outside '%s' (0..%.3f)."),
			TriggerTime, *Montage->GetName(), Montage->GetPlayLength());
		return false;
	}

	Montage->Modify();

#if WITH_EDITORONLY_DATA
	int32 TrackIndex = INDEX_NONE;
	for (int32 i = 0; i < Montage->AnimNotifyTracks.Num(); ++i)
	{
		if (Montage->AnimNotifyTracks[i].TrackName == TrackName)
		{
			TrackIndex = i;
			break;
		}
	}
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TrackName, FLinearColor::White));
	}

	if (bReplaceExisting)
	{
		for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
		{
			const UAZ_AnimNotify_SendGameplayEvent* Existing =
				Cast<UAZ_AnimNotify_SendGameplayEvent>(Montage->Notifies[i].Notify);
			if (Existing && Existing->EventTag == EventTag)
			{
				Montage->Notifies.RemoveAt(i);
			}
		}
	}

	UAZ_AnimNotify_SendGameplayEvent* Sender = NewObject<UAZ_AnimNotify_SendGameplayEvent>(
		Montage, UAZ_AnimNotify_SendGameplayEvent::StaticClass(), NAME_None, RF_Transactional);
	Sender->EventTag = EventTag;

	FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
	Event.Guid = FGuid::NewGuid();
	Event.TrackIndex = TrackIndex;
	Event.Notify = Sender;
	Event.NotifyStateClass = nullptr;
	// Mirror the engine's caching of the display name (the notify derives it from its tag) so the asset
	// reads the same as the hand-authored hit windows on the sibling montages.
	Event.NotifyName = FName(*Sender->GetNotifyName());
	Event.Link(Montage, TriggerTime);
	Event.SetDuration(0.f);

	Montage->RefreshCacheData();
	Montage->PostEditChange();
#endif   // WITH_EDITORONLY_DATA
	Montage->MarkPackageDirty();

	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] AddGameplayEventNotify '%s': %s @ %.3f on track '%s'."),
		*Montage->GetName(), *EventTag.ToString(), TriggerTime, *TrackName.ToString());
	return true;
}

bool UAZ_MontageUtils::AddNamedNotify(UAnimMontage* Montage, FName NotifyName, float TriggerTime,
	FName TrackName, bool bReplaceExisting)
{
	if (!Montage || NotifyName.IsNone())
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddNamedNotify: null montage or empty name."));
		return false;
	}
	if (TriggerTime < 0.f || TriggerTime > Montage->GetPlayLength())
	{
		UE_LOG(Log_AZ, Error, TEXT("[MontageUtils] AddNamedNotify: t=%.3f is outside '%s' (0..%.3f)."),
			TriggerTime, *Montage->GetName(), Montage->GetPlayLength());
		return false;
	}

	Montage->Modify();

#if WITH_EDITORONLY_DATA
	int32 TrackIndex = INDEX_NONE;
	for (int32 i = 0; i < Montage->AnimNotifyTracks.Num(); ++i)
	{
		if (Montage->AnimNotifyTracks[i].TrackName == TrackName)
		{
			TrackIndex = i;
			break;
		}
	}
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TrackName, FLinearColor::White));
	}

	if (bReplaceExisting)
	{
		for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
		{
			if (Montage->Notifies[i].NotifyName == NotifyName)
			{
				Montage->Notifies.RemoveAt(i);
			}
		}
	}

	FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
	Event.Guid = FGuid::NewGuid();
	Event.NotifyName = NotifyName;
	Event.TrackIndex = TrackIndex;
	// No Notify / NotifyStateClass: a bare named notify is exactly what the montage task matches on.
	Event.Notify = nullptr;
	Event.NotifyStateClass = nullptr;
	Event.Link(Montage, TriggerTime);
	Event.SetDuration(0.f);

	Montage->RefreshCacheData();
	Montage->PostEditChange();
#endif   // WITH_EDITORONLY_DATA
	Montage->MarkPackageDirty();

	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] AddNamedNotify '%s': '%s' @ %.3f on track '%s'."),
		*Montage->GetName(), *NotifyName.ToString(), TriggerTime, *TrackName.ToString());
	return true;
}

int32 UAZ_MontageUtils::RemoveGameplayEventNotifies(UAnimMontage* Montage, FName EventTagName)
{
	if (!Montage || EventTagName.IsNone())
	{
		return 0;
	}
	// Compare by NAME, not RequestGameplayTag: a retired tag may already be unregistered, and removal
	// must still work on old data.
	Montage->Modify();
	int32 Removed = 0;
	for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
	{
		const UAZ_AnimNotify_SendGameplayEvent* Sender =
			Cast<UAZ_AnimNotify_SendGameplayEvent>(Montage->Notifies[i].Notify);
		if (Sender && Sender->EventTag.GetTagName() == EventTagName)
		{
			Montage->Notifies.RemoveAt(i);
			++Removed;
		}
	}
	if (Removed > 0)
	{
#if WITH_EDITOR
		Montage->RefreshCacheData();
		Montage->PostEditChange();
#endif
		Montage->MarkPackageDirty();
	}
	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] RemoveGameplayEventNotifies '%s' tag=%s: %d removed."),
		*Montage->GetName(), *EventTagName.ToString(), Removed);
	return Removed;
}

int32 UAZ_MontageUtils::RemoveMotionWarpingNotifies(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return 0;
	}
	Montage->Modify();
	int32 Removed = 0;
	for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
	{
		const FAnimNotifyEvent& Event = Montage->Notifies[i];
		if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_MotionWarping::StaticClass()))
		{
			Montage->Notifies.RemoveAt(i);
			++Removed;
		}
	}
	if (Removed > 0)
	{
#if WITH_EDITOR
		Montage->RefreshCacheData();
		Montage->PostEditChange();
#endif
		Montage->MarkPackageDirty();
	}
	UE_LOG(Log_AZ, Log, TEXT("[MontageUtils] RemoveWarpNotifies '%s': %d removed."), *Montage->GetName(), Removed);
	return Removed;
}

TArray<FString> UAZ_MontageUtils::DumpMontageNotifies(UAnimMontage* Montage)
{
	TArray<FString> Lines;
	if (!Montage)
	{
		return Lines;
	}

	for (int32 i = 0; i < Montage->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& Event = Montage->Notifies[i];
#if WITH_EDITORONLY_DATA
		const FString TrackName = Montage->AnimNotifyTracks.IsValidIndex(Event.TrackIndex)
			? Montage->AnimNotifyTracks[Event.TrackIndex].TrackName.ToString()
			: FString(TEXT("<bad-track>"));
#else
		const FString TrackName(TEXT("<no-editor-data>"));
#endif

		FString Line = FString::Printf(TEXT("%d %s track=%s start=%.3f dur=%.3f"),
			i, *Event.NotifyName.ToString(), *TrackName, Event.GetTriggerTime(), Event.GetDuration());

		// Report the notify OBJECT, not just the name. A bare named notify sends nothing to GAS, but its
		// name can be spelled exactly like a gameplay tag — so name-only output makes an inert notify look
		// like a working hit window. Cost us a silent no-damage punch once; never again.
		if (const UAZ_AnimNotify_SendGameplayEvent* Sender = Cast<UAZ_AnimNotify_SendGameplayEvent>(Event.Notify))
		{
			Line += FString::Printf(TEXT(" [SendGameplayEvent tag=%s]"), *Sender->EventTag.ToString());
		}
		else if (Event.Notify)
		{
			Line += FString::Printf(TEXT(" [notify=%s]"), *Event.Notify->GetClass()->GetName());
		}
		else if (!Event.NotifyStateClass)
		{
			Line += TEXT(" [BARE NAME - sends no gameplay event]");
		}

		// Read the warp config back out - the whole point of this dump is to prove the instanced modifier
		// actually landed, not merely that a notify state is present.
		if (const UAnimNotifyState_MotionWarping* Warp = Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass))
		{
			if (const URootMotionModifier_Warp* Modifier = Cast<URootMotionModifier_Warp>(Warp->RootMotionModifier))
			{
				Line += FString::Printf(TEXT(" [warp target=%s trans=%d rot=%d type=%d method=%d maxRate=%.1f timeMult=%.2f]"),
					*Modifier->WarpTargetName.ToString(),
					Modifier->bWarpTranslation ? 1 : 0, Modifier->bWarpRotation ? 1 : 0,
					static_cast<int32>(Modifier->RotationType), static_cast<int32>(Modifier->RotationMethod),
					Modifier->WarpMaxRotationRate, Modifier->WarpRotationTimeMultiplier);
			}
			else
			{
				Line += TEXT(" [warp MODIFIER MISSING - window is inert]");
			}
		}
		Lines.Add(Line);
	}
	return Lines;
}
