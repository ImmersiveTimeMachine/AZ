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
	Right
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

/** One montage in the selection set, keyed by the three dimensions. */
USTRUCT(BlueprintType)
struct FAZ_MantleClipEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleApproach Approach = EAZ_MantleApproach::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleStyle Style = EAZ_MantleStyle::Relaxed;

	/** The foot planted at entry, as MEASURED from the clip — see EAZ_MantleFoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	EAZ_MantleFoot PlantedFoot = EAZ_MantleFoot::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal")
	TObjectPtr<UAnimMontage> Montage = nullptr;
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

	/** Where the capsule ends up: on the top surface, inset from the lip, at capsule-centre height. */
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

	bool IsValid() const { return Montage != nullptr && TargetComponent.IsValid(); }
};

/**
 * UAZ_TraversalComponent — ledge DETECTION and action REQUEST. It owns no motion and plays no animation;
 * UAZ_GA_Mantle does both. Split deliberately so the geometry provider can be replaced without touching
 * the executor.
 *
 * PROVIDER BOUNDARY (the reason this is worth a component at all): the first delivery reads AUTHORED
 * ledges — the spline components GASP's LevelBlock_Traversable carries on each top edge. It reads them as
 * plain USplineComponents rather than calling the Blueprint's GetLedgeTransforms, so there is no BP
 * interop and no Blueprint result struct to adapt. A later generic-geometry provider only has to produce
 * the same FAZ_MantleCandidate from traces; nothing downstream changes.
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
	bool TryStartMantle();

	/** The candidate the in-flight (or just-requested) mantle is using. */
	const FAZ_MantleCandidate& GetPendingCandidate() const { return PendingCandidate; }

	void ClearPendingCandidate() { PendingCandidate = FAZ_MantleCandidate(); }

	/** Approach x style x foot -> montage, from the Clips array. Null when that combination is unassigned,
	 *  which fails the candidate rather than silently substituting a clip from another family. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Traversal")
	UAnimMontage* ResolveMontage(EAZ_MantleApproach InApproach, EAZ_MantleStyle InStyle, EAZ_MantleFoot InFoot) const;

	// ---- Selection ----

	/** Which authored style plays. A plain setting on purpose: no native Relaxed/Neutral stance signal
	 *  exists in this project, and inventing one from aim or weapon state would be a guess. */
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

	/** The ability that executes the action. Granted through the hero's StartupAbilities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Selection")
	TSubclassOf<UGameplayAbility> MantleAbilityClass;

	// ---- Detection bands ----
	// Derived from the real capsule (r30 / half-height 90) and from the measured clips, NOT from the
	// "_1_0" in the asset name. The clips rise exactly 99.15cm and start 51-67cm out from the lip;
	// warping absorbs the difference within these bands and starts to read wrong outside them.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "1", ForceUnits = "cm"))
	float ProbeRadius = 20.f;

	/** Lip height above the pawn's feet. The single authored height is ~100cm; warping stretches it, but
	 *  too far and the hands visibly miss the edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLedgeHeight = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLedgeHeight = 125.f;

	/** How far past the lip the character finishes, matching the clips' authored 50cm. Doubles as the
	 *  support test: a top too shallow to hold the capsule at this inset is rejected, which is what keeps
	 *  a standing mantle off the level's 20cm-thick walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LandingInset = 50.f;

	/** Max angle between the pawn's facing and the inward lip normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float FacingToleranceDeg = 50.f;

	/** Minimum surface normal Z for the top to count as standable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Traversal|Detection", meta = (ClampMin = "0", ClampMax = "1"))
	float MinWalkableNormalZ = 0.7f;

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

	FAZ_MantleCandidate PendingCandidate;
};
