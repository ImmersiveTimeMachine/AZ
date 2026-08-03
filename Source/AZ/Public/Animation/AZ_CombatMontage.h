// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AZ_CombatMontage.generated.h"

class UAnimMontage;

/**
 * ONE combat clip + its gameplay timing, authored together in the anim-set DA (arch step B).
 *
 * Replaces the scattered "this clip is longer than the beat" knobs (BiteSeconds / RootMotionSeconds /
 * FlinchRootMotionSeconds / HoldSeconds) whose numbers lived on abilities, pawns and callers, while the
 * fact they encode — where the clip's USEFUL beat ends — is a property of the CLIP. The measured case
 * that forced this: Zombie_Chase_2_KnockBack_Chase peaks its recoil at 1.39s, Chase_5 at 1.81s; one
 * shared pawn-wide default cannot be right for both, and both clips then WALK BACK toward the player
 * (they are knockback+resume-chase round trips, not knockbacks).
 *
 * Semantics live in the Resolve* helpers and ONLY there — call sites must never re-derive them:
 *   ActiveSeconds      0 = the whole clip; else the montage blends out here (over BlendOutTime).
 *   RootMotionSeconds  0 = match the active beat; else the capsule detaches here (may be shorter than
 *                      the beat: the pose keeps acting while the body stops travelling).
 *
 * Caller beats stay CALLER decisions (crowd pacing like the step-back HoldSeconds, attack cadence like
 * BiteSeconds): this struct is the per-clip default and ceiling, not a straitjacket — a caller may
 * shorten the beat, never lengthen it past the clip.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CombatMontage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Combat")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** The gameplay beat: when the montage blends out. 0 = play the whole clip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Combat", meta = (ClampMin = "0", ForceUnits = "s"))
	float ActiveSeconds = 0.f;

	/** How long the capsule follows the clip's root motion. 0 = match the beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Combat", meta = (ClampMin = "0", ForceUnits = "s"))
	float RootMotionSeconds = 0.f;

	/** Blend used when the beat cuts the clip early (and folded into the stagger gate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Combat", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendOutTime = 0.4f;

	bool IsSet() const { return Montage != nullptr; }

	/** Montage length; 0 when unset. */
	float ClipSeconds() const;

	/** When the montage blends out: min(ActiveSeconds, clip), or the whole clip when ActiveSeconds == 0. */
	float ResolveBeat() const;

	/** How long the capsule follows root motion: RootMotionSeconds, or (0 ⇒) the beat. Capped at the clip. */
	float ResolveRootMotion() const;

	/** True when the beat ends before the clip does — i.e. a Montage_Stop cut will happen. */
	bool IsCutEarly() const;

	/** The stagger-gate window a viewer perceives: the beat, plus the blend-out when cut early. */
	float ResolveGate() const;
};
