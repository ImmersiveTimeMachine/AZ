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

	/**
	 * BuildSectionedMontage with per-segment CLIP RANGES: segment i plays Sequences[i] from
	 * SegmentStartTimes[i] to SegmentEndTimes[i] (clip seconds). Both arrays must be empty (= whole clips)
	 * or Num entries; an entry with End <= Start means "whole clip" for that segment. Sections still start
	 * where their (trimmed) segment starts, so the section table stays paired-montage-safe.
	 *
	 * Built for the strike pair (2026-09-03): a PoseSearch Interaction asset shares ONE timeline across its
	 * roles, so the victim's half has to carry a walk-in BEFORE the impact (the Zombie_01 knockbacks all
	 * start ON the impact frame) and the hero's heavy has to stop before its 160cm walk-off — neither exists
	 * as a clip, both are ranges. FAnimSegment::AnimStartTime/AnimEndTime are plain fields Python CAN write,
	 * but nothing recomputes the montage length afterwards (measured: end 2.667 -> 0.9 leaves GetPlayLength
	 * at 2.667, and AnimMontage has no post_edit_change binding), so the trim happens where the length is
	 * laid out.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static UAnimMontage* BuildSectionedMontageRanged(
		const FString& PackagePath,
		FName SlotName,
		const TArray<FName>& SectionNames,
		const TArray<UAnimSequence*>& Sequences,
		const TArray<FName>& NextSectionNames,
		const TArray<float>& SegmentPlayRates,
		const TArray<float>& SegmentStartTimes,
		const TArray<float>& SegmentEndTimes,
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

	// ---- MOTION WARPING ----
	// UAnimNotifyState_MotionWarping carries an Instanced URootMotionModifier. Python can add a notify
	// state but cannot construct and populate the instanced modifier sub-object, so the window it makes
	// is inert. This bridge creates both halves together.

	/**
	 * Add (or replace) a MotionWarping notify window on a montage, with a SkewWarp modifier configured
	 * for target tracking. The window is what bounds the warp in TIME: outside it the clip's root motion
	 * is untouched, which is how a swing can commit and legitimately miss a late dodge.
	 *
	 * Rotation-only is the default (bWarpTranslation=false) and is the safe starting point — it can
	 * re-aim a swing but can never displace the pawn. Turn translation on once the rotation half reads
	 * correctly in PIE.
	 *
	 * WarpTargetName must match the name gameplay registers via
	 * UMotionWarpingComponent::AddOrUpdateWarpTarget* — they rendezvous by FName, and a mismatch is a
	 * silent no-op rather than an error.
	 *
	 * @param SectionName   Section to place the window in. NAME_None = whole montage.
	 * @param StartFraction Window start as a fraction (0-1) of that section's length.
	 * @param EndFraction   Window end as a fraction of the section's length. Must exceed StartFraction.
	 * @param RotationType  0 = Default (match target's rotation), 1 = Facing (turn to face it).
	 * @param RotationMethod 0 = Slerp, 1 = SlerpWithClampedRate, 2 = ConstantRate, 3 = Scale.
	 *                       NOTE: Scale is a no-op on a clip whose own root yaw is zero (its scale factor
	 *                       collapses to 0). For in-place clips use Slerp or SlerpWithClampedRate.
	 * @param MaxRotationRate Degrees/sec ceiling. Only read by ClampedRate/ConstantRate. 0 = uncapped.
	 * @param AddTranslationEasingFunc EAlphaBlendOption. ONLY used when the clip has no translation of its
	 *                       own — SkewWarp then eases the actor from where it stood at window-open to the
	 *                       warp target across the window. 0=Linear, 2=HermiteCubic, 9=CircularOut
	 *                       (fast early, settled by contact — the lunge shape).
	 * @param MaxSpeedClampRatio Caps warped speed at this multiple of the clip's authored speed. ONLY
	 *                       applies on the branch where the clip HAS translation; it is dead on in-place
	 *                       clips (their authored speed is 0, so the clamp would be 0). 0 = no clamp.
	 * @return true if the window was added (reason logged to LogAZ on failure). Does NOT save.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool AddMotionWarpingNotify(
		UAnimMontage* Montage,
		FName SectionName,
		float StartFraction,
		float EndFraction,
		FName WarpTargetName,
		bool bWarpTranslation = false,
		bool bWarpRotation = true,
		uint8 RotationType = 1,
		uint8 RotationMethod = 1,
		float MaxRotationRate = 360.f,
		float WarpRotationTimeMultiplier = 1.f,
		uint8 AddTranslationEasingFunc = 2,
		float MaxSpeedClampRatio = 0.f);

	/** Remove every MotionWarping notify window from a montage. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static int32 RemoveMotionWarpingNotifies(UAnimMontage* Montage);

	/** Remove every SendGameplayEvent notify carrying EventTagName (e.g. retiring the old single-frame
	 *  "Event.Montage.Melee.Hit" contact markers after the socket-sweep windows replaced them). Returns
	 *  how many were removed. Does NOT save. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static int32 RemoveGameplayEventNotifies(UAnimMontage* Montage, FName EventTagName);

	/**
	 * Add a hit window / anim-led gameplay beat: a UAZ_AnimNotify_SendGameplayEvent carrying EventTag,
	 * which fires that GameplayEvent on the owning actor's ASC at exactly that frame. This is what
	 * UAZ_AT_PlayMontageAndWaitForEvent is listening for — it subscribes to gameplay event TAGS
	 * (AddGameplayEventTagContainerDelegate), NOT to montage notify names.
	 *
	 * Use this for Event.Montage.Melee.Hit and friends. AddNamedNotify below will NOT work for that: a
	 * bare named notify sends no event and is silently inert.
	 *
	 * Python cannot build this (UAnimSequenceBase::Notifies is protected AND the notify object has to be
	 * constructed and have its tag set), so a montage made from a raw sequence has no other way to carry
	 * a hit window.
	 *
	 * Takes the tag as an FName and resolves it through FGameplayTag::RequestGameplayTag, which both
	 * validates against the tag registry (a typo fails loudly instead of authoring a dead notify) and
	 * makes this callable from Python — FGameplayTag::TagName is read-only there, so script has no way to
	 * build a tag struct from a string.
	 *
	 * @param TrackName Notify track to place it on; created if absent.
	 * @param bReplaceExisting Remove any existing SendGameplayEvent notify carrying the same tag first,
	 *                         so re-running a build script cannot stack duplicate hit windows.
	 * @return true on success. Does NOT save.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool AddGameplayEventNotify(
		UAnimMontage* Montage,
		FName EventTagName,
		float TriggerTime,
		FName TrackName = TEXT("1"),
		bool bReplaceExisting = true);

	/**
	 * Add (or replace) the strike window as ONE UAZ_AnimNotifyState_MeleeWindow — a notify STATE with a
	 * duration, superseding the WindowBegin+WindowEnd notify PAIR that AddGameplayEventNotify authored.
	 *
	 * The pair could desync (two objects that must stay paired) and its End notify never fired when the
	 * montage was interrupted, leaving the detector open. A state is one object whose length IS the
	 * window, and the engine ends an active state on interruption.
	 *
	 * Python cannot build this: UAnimSequenceBase::Notifies is protected AND the notify-state object has
	 * to be constructed and linked with a duration.
	 *
	 * @param StartTime        Window open, seconds.
	 * @param Duration         Window length, seconds. Must be > 0 and stay inside the clip.
	 * @param bReplaceExisting Remove existing windows CARRYING THE SAME BEGIN TAG first, so re-running a
	 *                         build script cannot stack two live windows — while still allowing a hit
	 *                         window and a cancel window to coexist on one clip.
	 * @param BeginTagName / EndTagName  Leave NAME_None for the hit window (defaults to
	 *                         Event.Montage.Melee.WindowBegin/End). Pass Event.Combat.CancelOpen /
	 *                         CancelClose to author the RECOVERY window on the same clip — same notify
	 *                         state class, only the tags differ, so one authoring path serves both.
	 * @return true on success (reason logged to LogAZ). Does NOT save.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool AddMeleeWindowNotifyState(
		UAnimMontage* Montage,
		float StartTime,
		float Duration,
		FName TrackName = TEXT("1"),
		bool bReplaceExisting = true,
		FName BeginTagName = NAME_None,
		FName EndTagName = NAME_None);

	/** Remove every melee-window notify state from a montage. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static int32 RemoveMeleeWindowNotifyStates(UAnimMontage* Montage);

	/**
	 * Add a plain NAMED notify — no notify object, no state.
	 *
	 * WARNING: this does NOT reach GAS. Our montage task listens for gameplay event tags, so a bare named
	 * notify fires nothing no matter what you name it (naming it "Event.Montage.Melee.Hit" is especially
	 * misleading — it LOOKS right in a notify dump and does nothing). For hit windows use
	 * AddGameplayEventNotify above. This one is only for AnimBP-side `AnimNotify_<Name>` handlers.
	 *
	 * @param TrackName Notify track to place it on; created if absent.
	 * @param bReplaceExisting Remove any existing notify with the same NotifyName first.
	 * @return true on success. Does NOT save.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static bool AddNamedNotify(
		UAnimMontage* Montage,
		FName NotifyName,
		float TriggerTime,
		FName TrackName = TEXT("Notifies"),
		bool bReplaceExisting = true);

	/** One line per notify/notify-state: "idx name track=.. start=.. dur=.. [warp target=.. rot=.. trans=..]".
	 *  The read-back for AddMotionWarpingNotify — verify placement before trusting it in PIE. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Montage")
	static TArray<FString> DumpMontageNotifies(UAnimMontage* Montage);
};
