// Copyright Artur. AZ project.

#include "Animation/AZ_MontageUtils.h"

#include "AZ/AZ.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
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
