// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_MontageUtils.generated.h"

class UAnimMontage;
class UAnimSequence;

/**
 * Montage authoring helpers. The engine exposes montage SECTIONS to neither Python nor Blueprint
 * (FCompositeSection::NextSectionName is read-only and UAnimMontage::CompositeSections is not
 * exposed at all), so a sectioned montage cannot be built by script without this bridge.
 *
 * Built for PAIRED animation: UAnimInstance::MontageSync_Follow only mirrors section jumps between
 * two montages while both are in a section of the SAME NAME, so the two sides' section tables must
 * match exactly. BuildSectionedMontage takes the names as a parameter precisely so both sides can be
 * generated from ONE shared list and cannot drift apart.
 */
UCLASS()
class AZ_API UAZ_MontageUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Build (or rebuild in place) a montage whose sections map 1:1 onto the supplied sequences, laid
	 * end to end in order. Rebuilding an existing asset REUSES the object, so montage references
	 * already assigned on abilities/data assets survive a regeneration.
	 *
	 * All three name/asset arrays are parallel and must be the same length. Every sequence must share
	 * one skeleton. Does NOT save — the returned asset is dirty in memory; save it from the caller
	 * (Python: unreal.EditorAssetLibrary.save_loaded_asset) so no package write happens from a context
	 * that cannot survive one.
	 *
	 * @param PackagePath        Full object path, e.g. /Game/AZ/Blueprints/Animation/Montage/AM_Grab_Hero
	 * @param SlotName           Montage slot (hero content uses FullBody, Chalkie content DefaultSlot)
	 * @param SectionNames       Section name per sequence, in play order
	 * @param Sequences          One clip per section
	 * @param NextSectionNames   Parallel: what each section links to on completion. NAME_None = stop.
	 *                           A section naming ITSELF loops (that is how the wrestle hold is built).
	 * @param SegmentPlayRates   Optional, parallel: per-segment play rate. Empty = all 1.0. Use this to
	 *                           reconcile a paired clip whose two sides were authored at different
	 *                           lengths — both sides' section boundaries must land on the same times.
	 * @return The montage, or nullptr on validation failure (reason is logged to LogAZ).
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static UAnimMontage* BuildSectionedMontage(
		const FString& PackagePath,
		FName SlotName,
		const TArray<FName>& SectionNames,
		const TArray<UAnimSequence*>& Sequences,
		const TArray<FName>& NextSectionNames,
		const TArray<float>& SegmentPlayRates,
		float BlendIn = 0.25f,
		float BlendOut = 0.25f);

	/** Re-link one section's next-section. NAME_None = stop at the end of that section. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool SetSectionNext(UAnimMontage* Montage, FName SectionName, FName NextSectionName);

	/** Blend in/out times (FAlphaBlendArgs is not reachable from Python either). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool SetMontageBlendTimes(UAnimMontage* Montage, float BlendIn, float BlendOut);

	/** One line per section: "idx name start=.. len=.. next=..". The only way to verify a section
	 *  table from script — read it back after every build. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static TArray<FString> DumpMontageSections(UAnimMontage* Montage);

	/** True when both montages carry the same section names in the same order at the same start times
	 *  (tolerance in seconds) — i.e. when MontageSync_Follow will actually mirror their section jumps.
	 *  Mismatches are logged to LogAZ. Run this on every paired build before trusting it in PIE. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool VerifyPairedMontages(UAnimMontage* MontageA, UAnimMontage* MontageB, float ToleranceSeconds = 0.01f);
};
