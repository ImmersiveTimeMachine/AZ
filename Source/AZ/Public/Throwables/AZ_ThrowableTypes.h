// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AZ_ThrowableTypes.generated.h"

/**
 * Where the throw action currently is. One ability owns all of these; they are NOT separate abilities.
 *
 * The control scheme is RMB-hold-to-aim / RMB-release-to-throw / LMB-cancel (user, 2026-09-16), which is why
 * Aiming has no timeout and why Windup is entered from a button RELEASE rather than a press.
 */
UENUM(BlueprintType)
enum class EAZ_ThrowPhase : uint8
{
	/** Not throwing. */
	None,
	/** Start clip is playing. The unit is reserved, nothing is spent, and the preview is already live. */
	Preparing,
	/** Loop is playing and repeats indefinitely while the button is held. */
	Aiming,
	/** Release clip is playing and the release cue has not fired yet. Aim intent is frozen. */
	Windup,
	/** The cue fired and the projectile exists. Inventory has been spent. No refund past this point. */
	Released,
	/** Authored recovery tail after Released. Interruption here cleans up presentation only. */
	Recovering,
	/** Cancel clip is playing, or a hard interruption is unwinding. Spends nothing. */
	Cancelling
};

/**
 * Which authored release the solution chose.
 *
 * MEASURED, and the reason this is not merely a speed swap: the two clips release from hand heights 92.3cm
 * apart (Close z 52.9 underhand, Far z 145.2 overhand — see throwable-phase0-status.md §1.3). The arc is
 * chosen once at commit and then feeds the preview anchor; flapping between them per frame would move the
 * launch origin by most of a metre.
 */
UENUM(BlueprintType)
enum class EAZ_ThrowArc : uint8
{
	Close,
	Far
};

/** What the thrown object does when it lands. Data, not three copied ability state machines. */
UENUM(BlueprintType)
enum class EAZ_ThrowImpactBehavior : uint8
{
	/** Stone: bounce, settle, report authoritative hearing, optionally recoverable. */
	BounceAndSettle,
	/** Grenade: bounce, but a fuse started at release detonates once wherever it then is. */
	FuseAndDetonate,
	/** Knife: first blocking hit stops/embeds it. */
	ImpactAndEmbed,
	/** Bottle: break on a qualifying impact; otherwise bounce and recover intact. */
	Shatter
};

/** Why a launch solution is unusable. Surfaced to the preview so the HUD can say OBSTRUCTED honestly. */
UENUM(BlueprintType)
enum class EAZ_ThrowSolutionStatus : uint8
{
	Valid,
	/** No throwable/profile/definition resolved — the action should never have got this far. */
	NoDefinition,
	/**
	 * The corridor from the body to the release point is blocked, so the hand cannot clear.
	 * A shoulder camera routinely sees over cover the hand cannot get past; this is that case, and it must
	 * abort BEFORE consumption rather than spawning the object on the far side of the wall.
	 */
	BlockedAtHand
};

/**
 * One immutable launch snapshot, produced by UAZ_ThrowLaunchSolver and consumed by BOTH the owner's preview
 * and the authoritative release.
 *
 * Having one producer is the whole point: the plan's failure mode is a preview tuned by separate constants
 * that quietly disagrees with what the server launches. Camera chooses the intent; the character's validated
 * hand supplies the origin. Nothing here is ever taken from a client as trusted input.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_ThrowLaunchSolution
{
	GENERATED_BODY()

	/** Where the object actually leaves the hand. NOT the camera, and not the Loop hand pose. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	FVector Origin = FVector::ZeroVector;

	/** World velocity at release, already including whatever share of the thrower's motion the profile inherits. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	FVector Velocity = FVector::ZeroVector;

	/** Effective gravity Z. Kept explicit because PredictProjectilePath treats OverrideGravityZ==0 as
	 *  "use world gravity" rather than as zero gravity, so 0 can never be passed through as a real value. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	float GravityZ = 0.f;

	/** Collision sphere radius. The same number drives the predicted sweep and the runtime root. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	float Radius = 0.f;

	/** Which release clip this solution was calibrated against. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	EAZ_ThrowArc Arc = EAZ_ThrowArc::Far;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	EAZ_ThrowSolutionStatus Status = EAZ_ThrowSolutionStatus::NoDefinition;

	bool IsValid() const { return Status == EAZ_ThrowSolutionStatus::Valid; }
};

/**
 * What the preview found, kept separate from the solution because it is cosmetic and owner-only.
 *
 * `bHitBlocking` false is NOT an error and NOT a reason to refuse a throw: it only means nothing was struck
 * inside the display horizon, which is an ordinary throw into open space. The HUD fades the line out and
 * draws no marker rather than inventing one.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_ThrowPreviewResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	TArray<FVector> Points;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	bool bHitBlocking = false;

	/** Surface contact, from ImpactPoint — never the swept sphere centre, which floats a radius off the wall. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	float TimeToImpact = 0.f;

	/** The component struck, so a marker on a moving surface can follow it instead of hanging in the air. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Throw")
	TWeakObjectPtr<UPrimitiveComponent> HitComponent;
};
