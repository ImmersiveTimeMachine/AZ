// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "AZ_TraversalComponent.generated.h"

class UAnimMontage;
class UGameplayAbility;
class UPrimitiveComponent;
class USplineComponent;
struct FAZ_TraversalSurfaceGeometry;

/** Which of the two authored presentation styles plays. These are GASP's own source styles and carry NO
 *  gameplay meaning here — do not map them onto aiming or an equipped weapon without measuring the clips
 *  first. Exposed as a setting so both can be compared in PIE before any stance mapping is chosen. */
UENUM(BlueprintType)
enum class EAZ_MantleStyle : uint8
{
	Relaxed,
	Neutral
};

/** Which foot stays PLANTED through the entry.
 *
 *  ★ MEASURED, NEVER READ OFF THE FILENAME. The source suffix means OPPOSITE things in the two clip
 *  families: on `stand_F_Rfoot` the right foot is planted (left lifts at ~0.35s), but on `walk_F_Lfoot`
 *  and `run_F_Lfoot` the LEFT foot lifts first, so the RIGHT is planted. Selecting by suffix would put
 *  the character on the wrong foot for every moving mantle. Our copies are named by the measured planted
 *  foot so the meaning is uniform across all twelve. */
UENUM(BlueprintType)
enum class EAZ_MantleFoot : uint8
{
	Left,
	Right,
	/** The clip does not commit to a foot — both leave the ground together, as the standing hurdles do
	 *  (measured 0.32/0.32 and 0.24/0.24). Recorded honestly instead of inventing a left/right, and it
	 *  matches either live foot during selection. */
	Unknown
};

/** How fast the character was moving when the action was requested. A real selection dimension, not a
 *  label: the clip families differ in authored approach distance by a factor of six (stand starts ~51-67cm
 *  from the lip, walk ~200-266cm, run ~318-341cm) and carry different warp anchors. */
UENUM(BlueprintType)
enum class EAZ_MantleApproach : uint8
{
	Stand,
	Walk,
	Run
};

/**
 * What a Jump press resolved to. THREE outcomes, not a bool — a bool conflates "there is nothing to
 * traverse here" with "the body is already committed to something else", and the caller must treat those
 * oppositely: the first is permission to jump, the second is not.
 */
UENUM(BlueprintType)
enum class EAZ_MantleRequestResult : uint8
{
	/** A mantle activated. The press is consumed. */
	Started,
	/** No valid ledge, or no clip whose authored approach fits from here. Ordinary jump rules apply. */
	NoCandidate,
	/** A committed reaction (Brace/Stumble/HeadHit) or another action owns the body. The press is
	 *  consumed and DISCARDED — it must not become a jump, and it must not be buffered into a late
	 *  mantle either. A fresh press after the reaction finishes is the contract. */
	BodyBusy
};

/**
 * One legal entry point into a clip: how much authored approach is still ahead at that time, and which
 * foot is planted there.
 *
 * Why entry TIME is the whole game: the montage used to always start at 0, so a press made one metre from
 * a ledge still performed the clip's entire ~3 m approach while warping squeezed the travel into a metre.
 * That compression is the lurch — and it is why pressing early looked better, because early presses
 * happened to match the authored distance.
 *
 * Samples stop at the first warp window: entering after contact has begun would skip the authored
 * hand-plant setup. Baked offline from the real root track, never guessed.
 */
USTRUCT(BlueprintType)
struct FAZ_MantleEntrySample
{
	GENERATED_BODY()

	/** Montage time to start playing from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "s"))
	float Time = 0.f;

	/** Authored forward distance still to travel from Time to the contact anchor. Matched against the
	 *  real pawn-to-lip distance so warping only has to correct centimetres, not metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm"))
	float RemainingApproach = 0.f;

	/** Foot planted at this entry time. It CHANGES across the interval, so entry time can match the live
	 *  foot without switching clips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleFoot PlantedFoot = EAZ_MantleFoot::Right;
};

/**
 * Which traversal this clip belongs to. Legality is decided by GEOMETRY, not by this field:
 *  - Mantle needs usable SUPPORT ON TOP (you end up standing there);
 *  - Hurdle needs a crossable obstacle AND a validated FAR-SIDE LANDING (you end up beyond it).
 * Where both are legal, movement context breaks the tie — mantle standing/walking, hurdle running.
 */
UENUM(BlueprintType)
enum class EAZ_TraversalAction : uint8
{
	Mantle,
	Hurdle,
	/** Get onto a ledge too high to mantle. Same executor, same Traversing mode; what differs is the
	 *  height band and that the authored set tops out ON the platform rather than hanging — verified on
	 *  all six clips, every one of which ends standing at root z 247.9 with the feet on top. */
	Climb
};

/** One montage in the selection set, keyed by the three dimensions. */
USTRUCT(BlueprintType)
struct FAZ_MantleClipEntry
{
	GENERATED_BODY()

	/** Defaults to Mantle so the existing serialized clip set keeps its meaning unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_TraversalAction Action = EAZ_TraversalAction::Mantle;

	/**
	 * Centimetres of usable obstacle top this clip's CONTACTS require, measured from the animation.
	 *
	 * 0 = the clip never touches the top (a clearing hurdle) and so fits a thin wall. A positive value is
	 * a step-on clip: its BackLedge window puts the root at obstacle height, i.e. a real foot plant, and
	 * the measured requirement is 29-44cm for stand/walk and 70-73cm for run. This is exactly why a 20cm
	 * wall cannot be hurdled by any step-on clip — and why the requirement lives in DATA here rather than
	 * as a threshold buried in the ability.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RequiredTopSupport = 0.f;

	/**
	 * Where the ROOT should be at the FrontLedge window's end, relative to the obstacle's front face at
	 * ground level: X = forward, Y = up. Baked from the clip.
	 *
	 * MEASUREMENT ONLY as of the warp-point fix — nothing reads it to build a target any more. It existed
	 * because `attach` is un-keyed on these clips, so the Bone provider degenerated to identity and one
	 * baked offset had to stand in for the whole launch. That could never work: every hurdle clip has TWO
	 * FrontLedge windows with very different authored root ends, and a single offset collapsed both onto
	 * one transform. The assets now use a Static warp point at the obstacle's own origin, so each window
	 * derives its own destination. Kept because the baked numbers are the record of what the clip does,
	 * and because they are how a mis-authored window is spotted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	FVector2D FrontLedgeRootOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleApproach Approach = EAZ_MantleApproach::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleStyle Style = EAZ_MantleStyle::Relaxed;

	/** The foot planted at entry, as MEASURED from the clip — see EAZ_MantleFoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleFoot PlantedFoot = EAZ_MantleFoot::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** Legal entry points, baked from the clip's own root track. Empty means "start at 0 only", which is
	 *  correct for a clip whose first warp window opens on frame 0 and has no pre-contact room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	TArray<FAZ_MantleEntrySample> EntrySamples;

	/**
	 * The lowest point of the BODY as this clip crosses the barrier, in animation space: the minimum Z
	 * over every leg bone and the pelvis while its forward position lies within the authored barrier
	 * (y 0..20cm, front face at the origin). This is the number an obstacle height must be compared
	 * against — NOT the root track's peak, which is a ground-projected point and says nothing about what
	 * the body clears.
	 *
	 * Measured: 99-111cm on the clearing clips against their own 100cm barrier, so they pass the top edge
	 * with 0-11cm to spare and visibly graze it. The step-ons read higher (108-126cm) only because their
	 * lowest point over that span is the foot PLANTING on it — which is why ApexLift must never touch
	 * them: raising a step-on lifts its plant off the wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm"))
	float AuthoredClearHeight = 0.f;

	/** Per-clip override for the accepted distance mismatch. <= 0 uses the per-action default. Present
	 *  because a clip whose measurements justify a tighter or looser window should say so itself rather
	 *  than pushing the global number around and loosening every other clip with it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm"))
	float ApproachToleranceOverride = 0.f;
};

/** Per-approach speed gate and reach. Derived from the measured clips, not from the "_1_0" in the name. */
USTRUCT(BlueprintType)
struct FAZ_MantleApproachBand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleApproach Approach = EAZ_MantleApproach::Stand;

	/** Planar speed at the press, in cm/s. The first band whose range contains the speed wins, so keep
	 *  them ordered and non-overlapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm/s"))
	float MinSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm/s"))
	float MaxSpeed = 60.f;

	/** How far ahead to look for the obstacle face at this approach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm"))
	float ForwardReach = 120.f;

	/** Planar pawn-to-lip distance accepted at this approach. A run clip covers ~330cm of approach; the
	 *  standing band would reject every moving mantle outright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal", meta = (ForceUnits = "cm"))
	float MaxLedgeDistance = 110.f;
};

/**
 * One validated mantle opportunity. Produced by the detector, consumed by UAZ_GA_Mantle.
 *
 * LedgeLocation/LedgeNormal describe the PHYSICAL FRONT LIP — the contact anchor the animation's `attach`
 * bone was authored against — NOT the capsule's destination. Those are different points by design: the
 * clips finish 50cm in from the edge. LandingTransform carries the destination separately.
 */
USTRUCT(BlueprintType)
struct FAZ_MantleCandidate
{
	GENERATED_BODY()

	/** The primitive we mantle onto. Weak: a destroyed or detached target must invalidate the action
	 *  rather than resurrect a stale pointer mid-montage. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	TWeakObjectPtr<UPrimitiveComponent> TargetComponent;

	/** World location of the front lip (on the top surface, at the near edge). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FVector LedgeLocation = FVector::ZeroVector;

	/** Outward horizontal normal of the lip — points back toward the approaching pawn. The warp target's
	 *  rotation is built from its negation, so the character faces into the obstacle. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FVector LedgeNormal = FVector::ForwardVector;

	/** Validated capsule terminal pose for mantle/climb. For climb-and-drop this is clear of the far
	 *  face at lip height, with no standing support claimed; FarLandingLocation is the floor below. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FTransform LandingTransform;

	/** Lip height above the pawn's CURRENT feet — not above the obstacle's own base. Those differ on a
	 *  raised platform, and conflating them regresses stair/ramp handling. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float HeightAboveFeet = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	EAZ_MantleStyle Style = EAZ_MantleStyle::Relaxed;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	EAZ_MantleFoot PlantedFoot = EAZ_MantleFoot::Right;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	EAZ_MantleApproach Approach = EAZ_MantleApproach::Stand;

	/** Planar speed at the press that chose the approach band — logged, and the discriminator to look at
	 *  first when the wrong clip family plays. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float ApproachSpeed = 0.f;

	/** The montage chosen for Style x PlantedFoot. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** Montage time to START from — the crux of the entry fix. Non-zero means we skipped authored approach
	 *  the character does not have room to perform, instead of making warping compress it. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float StartTime = 0.f;

	/** Real planar pawn-to-lip distance at the press. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float LedgeDistance = 0.f;

	/** Authored approach still ahead at StartTime. The gap between this and LedgeDistance is exactly how
	 *  much warping has been asked to correct — keep it small and the lurch goes away. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float RemainingApproach = 0.f;

	// ---- Crossing geometry. Used by hurdles and explicitly permitted climb-and-drop actions. ----

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	EAZ_TraversalAction Action = EAZ_TraversalAction::Mantle;

	/** Only an explicitly permitted climb across a surface may finish unsupported and enter Falling. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	bool bClimbAndDrop = false;

	/** Far top edge of the obstacle, used to validate crossing distance and far-face clearance. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FVector FarEdgeLocation = FVector::ZeroVector;

	/** Validated far-side floor for a hurdle or climb-and-drop. This is a feet location, not an airborne
	 *  terminal pose or a promise that the falling capsule has already reached the floor. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FVector FarLandingLocation = FVector::ZeroVector;

	/** Front lip to far lip. Compared against each clip's RequiredTopSupport, so a thin wall can only ever
	 *  select a clearing clip. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float ObstacleDepth = 0.f;

	/** Baked launch offset for the FrontLedge target of the selected clip (forward, up). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	FVector2D FrontLedgeRootOffset = FVector2D::ZeroVector;

	/** Extra height added to the FrontLedgeApex target so the body clears the REAL obstacle top by
	 *  HurdleApexClearance. Non-zero only for CLEARING clips: a step-on's lowest point over the barrier is
	 *  the foot it plants there, and lifting that is how you get a foot hovering above the wall. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Traversal")
	float ApexLift = 0.f;

	bool IsValid() const { return Montage != nullptr && TargetComponent.IsValid(); }
};

/**
 * UAZ_TraversalComponent — ledge DETECTION and action REQUEST. It owns no motion and plays no animation;
 * UAZ_GA_Mantle does both. Split deliberately so the geometry provider can be replaced without touching
 * the executor.
 *
 * PROVIDER BOUNDARY: explicit surface data on the hit component or mesh supplies authored edges and
 * action permissions. Only an unconfigured surface falls back to the spline components carried by
 * GASP's LevelBlock_Traversable. Both providers feed the same physical support, landing and clip checks;
 * neither needs Blueprint result-struct interop. Unmarked generic meshes remain unsupported.
 *
 * NOT A SENSOR. It does not tick. Everything here runs once, on a Jump press, and only far enough to
 * answer "is there a mantle here right now". UAZ_ObstacleSensorComponent (cosmetic impact flinches) and
 * UAZ_MovementDirectionCapabilityComponent (walking intent clamp) are different layers with different
 * shapes and lifetimes; three reaction bands are not a landing test, and a clamped walking intent is not
 * evidence about a ledge.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_TraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_TraversalComponent();

	/** Geometry query only — no side effects, safe to call for debug draw or UI prompts. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Traversal")
	bool FindMantleCandidate(FAZ_MantleCandidate& OutCandidate) const;

	/**
	 * The Jump-press entry point. Finds a candidate, stores it, and asks the ASC to activate the mantle
	 * ability. Returns true ONLY when the ability actually activated — the caller uses that to decide
	 * whether the press was consumed, so a false here must leave the ordinary jump free to run.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Traversal")
	EAZ_MantleRequestResult TryStartMantle();

	/**
	 * How far the top surface actually extends back from the lip, in cm, capped at MaxMeasuredTopSupport.
	 *
	 * Exists because "we never found a far side" is not the same statement as "there is unlimited room up
	 * there", and the code used to conflate them. Steps inward along -Normal and rays down at each step;
	 * the depth is where the surface stops being the ledge top. One ray at LandingInset proves the capsule
	 * fits at ONE point; this proves the top is continuous up to it.
	 */
	float MeasureTopSupportDepth(const FVector& Lip, const FVector& Normal,
		const FCollisionQueryParams& Params) const;

	/** The candidate the in-flight (or just-requested) mantle is using. */
	const FAZ_MantleCandidate& GetPendingCandidate() const { return PendingCandidate; }

	void ClearPendingCandidate() { PendingCandidate = FAZ_MantleCandidate(); }

	/**
	 * Pick the clip AND the entry time to start it from, given how far the ledge actually is, how fast we
	 * are going, and which foot is down. Returns false when nothing fits within ApproachTolerance — an
	 * explicit "no", rather than warping an arbitrary clip across an arbitrary distance.
	 * The optional unsupported-exit policy applies only to Climb and requires measured forward travel
	 * of at least MinClimbExitForwardDistance; every normal contact and approach gate remains in force.
	 */
	bool SelectEntry(EAZ_TraversalAction InAction, EAZ_MantleApproach InApproach, EAZ_MantleFoot InFoot,
		float LedgeDistance, float UsableTopSupport,
		UAnimMontage*& OutMontage, float& OutStartTime, float& OutRemainingApproach,
		FVector2D& OutFrontLedgeRootOffset, float& OutAuthoredClearHeight,
		float& OutRequiredTopSupport, bool bAllowUnsupportedClimbExit = false,
		float MinClimbExitForwardDistance = 0.f) const;

	/**
	 * Measure the far side of an obstacle: its far top edge, its depth, and whether there is a landing
	 * beyond it we are willing to commit to. Returns false when the landing cannot be measured or fails
	 * the drop/rise/slope/clearance limits — which is a REFUSAL, not a reason to guess.
	 *
	 * Uses SuppliedFarLip for an explicit surface, otherwise the legacy authored opposite ledge spline.
	 * A supplied point skips spline discovery but never skips physical landing validation.
	 */
	bool FindFarSide(const AActor* HitActor, const FVector& Lip, const FVector& Normal, float ApproachFeetZ,
		float CapsuleRadius, float CapsuleHalfHeight, const FCollisionQueryParams& Params,
		FVector& OutFarEdge, FVector& OutLanding, float& OutDepth,
		const FVector* SuppliedFarLip = nullptr) const;

	// ---- Selection ----

	/** Which authored style is PREFERRED. A plain setting on purpose: no native Relaxed/Neutral stance
	 *  signal exists in this project, and inventing one from aim or weapon state would be a guess.
	 *
	 *  Preferred, not required — selection ranks the other style far below this one but will still use it
	 *  rather than refuse a press the geometry allows. As a hard filter this setting silently decided
	 *  which obstacles existed: the Relaxed set has no thin-wall clip inside 61cm while the Neutral one
	 *  enters from 89cm, so the carriage the character happened to be in changed what could be crossed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	EAZ_MantleStyle Style = EAZ_MantleStyle::Relaxed;

	/** Used only when no anim instance can report a planted foot. The live signal is preferred: a moving
	 *  mantle entered on the wrong foot visibly skips a step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	EAZ_MantleFoot FallbackPlantedFoot = EAZ_MantleFoot::Right;

	/** The gameplay-owned montage set, keyed by approach x style x planted foot. Assigned in the pawn
	 *  Blueprint — never a hardcoded /Game/ path in C++. Each entry must route through the FullBody slot
	 *  and carry its FrontLedge warp windows, or the capsule will not follow the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TArray<FAZ_MantleClipEntry> Clips;

	/** Speed gates and reach per approach. Defaults are seeded in the constructor from the measured
	 *  clips; an empty array falls back to the standing band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TArray<FAZ_MantleApproachBand> ApproachBands;

	/** The ability that executes a mantle. Granted through the hero's StartupAbilities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TSubclassOf<UGameplayAbility> MantleAbilityClass;

	/** The ability that executes a hurdle. Unset means hurdling is simply unavailable — the detector will
	 *  still refuse cleanly rather than activating the wrong action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TSubclassOf<UGameplayAbility> HurdleAbilityClass;

	/** The ability that executes a high-ledge climb. Same rule as the other two: unset disables the
	 *  action, it does not make the detector fall through to a wrong one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TSubclassOf<UGameplayAbility> ClimbAbilityClass;

	// ---- Detection bands ----
	// Derived from the real capsule (r30 / half-height 90) and from the measured clips, NOT from the
	// "_1_0" in the asset name. The clips rise exactly 99.15cm and start 51-67cm out from the lip;
	// warping absorbs the difference within these bands and starts to read wrong outside them.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "1", ForceUnits = "cm"))
	float ProbeRadius = 20.f;

	/** Lip height above the pawn's feet, for MANTLE. The single authored height is ~100cm; warping
	 *  stretches it, but too far and the hands visibly miss the edge. Detection itself accepts anything up
	 *  to the climb band — these two only decide whether MANTLE is one of the legal answers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLedgeHeight = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLedgeHeight = 125.f;

	/** The CLIMB band, bracketing the one authored height. Every climb clip tops out at exactly 247.9cm
	 *  above its own origin, so this is a warp tolerance around a single number, not a range the content
	 *  covers. Deliberately disjoint from the mantle band above: a ledge between them has no clip, and
	 *  stretching either set to cover the gap is what the work order forbids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ClimbMinHeight = 205.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ClimbMaxHeight = 285.f;

	/** How far past the lip the character finishes, matching the clips' authored 50cm. Doubles as the
	 *  support test: a top too shallow to hold the capsule at this inset is rejected, which is what keeps
	 *  a standing mantle off the level's 20cm-thick walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LandingInset = 50.f;

	/** Max angle between the pawn's facing and the inward lip normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float FacingToleranceDeg = 50.f;

	/** How far the real distance may differ from a clip's authored approach before that entry is rejected.
	 *  This IS the calibrated spatial-correction limit: warping silently absorbs the difference, and the
	 *  bigger it is the more the approach is visibly compressed or stretched. Tighten until entries read
	 *  clean; if that starts refusing mantles you want, the clip set is missing an approach length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ApproachTolerance = 70.f;

	/** Hurdle's own tolerance. Kept separate from mantle's because the two families' authored approaches
	 *  differ, and from detection REACH because how far we look and how much animation distance we are
	 *  willing to distort are different questions. Raising this to close a coverage gap is the wrong fix —
	 *  it buys availability with visible spatial compression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HurdleApproachTolerance = 70.f;

	/** Allow dropping to the next SHORTER approach family when the ledge is too close for the one the
	 *  current speed implies (Run -> Walk, Walk -> Stand). Never Run -> Stand: a standing clip played at
	 *  450 cm/s reads as the character teleporting into a different animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection")
	bool bAllowShorterApproachFallback = true;

	// ---- Hurdle-only far-side validation ----
	// A hurdle ends BEYOND the obstacle, so the landing has to be measured before committing. Never
	// commit to an unmeasured drop: a character that hurdles a parapet off a roof is not a bug you get
	// to discover in review.

	/** How far past the far edge to look for the landing surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FarSideProbeDistance = 90.f;

	/** Biggest drop from the approach surface to the landing that still counts as a hurdle rather than a
	 *  fall. Beyond this we refuse and let the ordinary jump rules decide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLandingDrop = 60.f;

	/** And the biggest rise — landing onto something higher than we took off from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLandingRise = 30.f;

	/** Body margin wanted over the obstacle top. The authored clearing clips carry 0-11cm of their own
	 *  against a 100cm barrier, so on that wall this asks for roughly 3-14cm of lift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HurdleApexClearance = 14.f;

	/** Above this, hurdling is not a legal answer at all. Needed the moment ApexLift became real: the
	 *  formula would happily ask for 160cm of lift on a 250cm wall and turn a clearing hop into a
	 *  levitation. 110 is the authored ~100cm barrier plus the lift the arc can absorb without reading
	 *  wrong — anything taller is a mantle or a climb, or nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxHurdleHeight = 110.f;

	/** Obstacle depth beyond which hurdling stops making sense regardless of the clips — at some width it
	 *  is a platform to get onto, not a barrier to cross. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Hurdle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxHurdleDepth = 120.f;

	/** Minimum surface normal Z for the top to count as standable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ClampMax = "1"))
	float MinWalkableNormalZ = 0.7f;

	/** Refuse a traversal started while airborne. Every authored clip begins with a foot on the floor, so
	 *  an air press would warp the character out of a fall into a run-up it never had. Off only for
	 *  deliberately authored air catches, which need their own coverage first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection")
	bool bRequireGroundedEntry = true;

	/** Ceiling for MeasureTopSupportDepth. Past this a platform is "plenty" and stepping further is wasted
	 *  traces — the deepest contact any clip asks for is 73cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "10", ForceUnits = "cm"))
	float MaxMeasuredTopSupport = 200.f;

	/** Step size for that walk. Smaller is more exact and more traces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "5", ForceUnits = "cm"))
	float TopSupportStep = 10.f;

	/** Channel for every traversal query. Must be one the authored blocks actually Block — they are
	 *  Movable/WorldDynamic, so a WorldStatic-only OBJECT query silently misses all of them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Debug")
	bool bDrawDebug = false;

private:
	/** True while a cosmetic impact reaction (Brace / Stumble / HeadHit) already owns the body. A reaction
	 *  COMMITTED before the press finishes first — the press is simply not consumed, and no late mantle is
	 *  queued behind it. Found through the mesh carrying a UAZ_MoverAnimInstance rather than by casting the
	 *  owner, so this stays pawn-agnostic (garments have no anim instance; the face runs a different one). */
	bool IsImpactReactionOwningBody() const;

	/** Pick the approach band whose speed range contains Speed. Falls back to the standing defaults when
	 *  ApproachBands is empty or nothing matches, so a misconfigured array degrades to "standing only"
	 *  rather than to no mantle at all. */
	FAZ_MantleApproachBand ResolveApproachBand(float Speed) const;

	/** The planted foot as the animation layer currently reports it (curve-driven contact_l), falling back
	 *  to FallbackPlantedFoot when no UAZ_MoverAnimInstance is present. */
	EAZ_MantleFoot ResolveLivePlantedFoot() const;

	/** Pick the best authored ledge spline on HitActor for an approach from PawnLocation/Forward. */
	bool SelectLedgeFromSplines(const AActor* HitActor, const FVector& PawnLocation, const FVector& Forward,
		float FeetZ, float MaxDistance, FVector& OutLip, FVector& OutNormal) const;

	/** Select an explicitly configured surface edge, keeping the capsule clear of both edge ends. */
	bool SelectLedgeFromSurface(const FAZ_TraversalSurfaceGeometry& Surface,
		const FVector& PawnLocation, const FVector& Forward, float FeetZ, float MaxDistance,
		float CapsuleRadius, FVector& OutLip, FVector& OutNormal, FVector& OutFarLip,
		bool& OutHasFarLip, float& OutDeclaredDepth, FString& OutReason) const;

	FAZ_MantleCandidate PendingCandidate;
};
