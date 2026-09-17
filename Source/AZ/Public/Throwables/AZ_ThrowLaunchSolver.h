// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_ThrowLaunchSolver.generated.h"

class APawn;
class USkeletalMeshComponent;
class UAZ_ThrowableDefinition;
class UAZ_ThrowPresentationProfile;

/**
 * UAZ_ThrowLaunchSolver — the ONE producer of a throw's launch solution.
 *
 * The failure this class exists to prevent is a preview tuned by its own constants that quietly disagrees
 * with what the server launches. The owner's preview and the authoritative release call the same function
 * with the same inputs; the only difference is `bUseLiveGrip`, which is false while aiming (the hand is in
 * the Loop pose, not the release pose, so the calibrated anchor is used) and true at the release cue.
 *
 * Camera chooses intent. The character's validated hand supplies the origin. Nothing here trusts a client.
 */
UCLASS()
class AZ_API UAZ_ThrowLaunchSolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Build the launch snapshot.
	 *
	 * @param Thrower       Throwing pawn. Supplies the capsule (its ROOT component) and world velocity.
	 * @param Mesh          The thrower's animated mesh, passed in rather than discovered: the AZ hero is a
	 *                      Mover APawn, not an ACharacter, and it carries LeaderPose garment meshes alongside
	 *                      the body — so FindComponentByClass would pick an arbitrary one of them.
	 * @param AimRotation   Accepted aim intent. While aiming this follows the camera; once Windup begins the
	 *                      caller freezes it, so the throw goes where the player committed rather than where
	 *                      the camera happened to drift during the wind-up.
	 * @param AimDistance   Distance to what the player is aiming at. Drives SPEED and origin lift, which vary
	 *                      continuously with it - two fixed speeds left a band of distances nothing could
	 *                      reach. The Arc above selects PRESENTATION only.
	 * @param bUseLiveGrip  false: calibrated release anchor (preview, before the arm has moved).
	 *                      true:  the grip bone's actual transform right now (the release cue).
	 */
	static FAZ_ThrowLaunchSolution BuildSolution(
		const APawn* Thrower,
		const USkeletalMeshComponent* Mesh,
		const UAZ_ThrowableDefinition* Definition,
		const UAZ_ThrowPresentationProfile* Profile,
		EAZ_ThrowArc Arc,
		float AimDistance,
		const FRotator& AimRotation,
		bool bUseLiveGrip);

	/**
	 * Predicted flight up to the FIRST blocking hit.
	 *
	 * Deliberately not a bounce or final-rest prediction: the engine predictor integrates constant-Z gravity
	 * with sphere sweeps and breaks on its first collision. Advertising a resting position from it would be
	 * a lie, and a first bounce is not a grenade's detonation point.
	 *
	 * @param Horizon   Seconds of flight to display. Bounded so a throw into open sky costs a fixed amount.
	 * @param Frequency Simulation Hz. Set explicitly — the C++ default is 20 while the Blueprint wrappers
	 *                  default to 15, and inheriting either silently would make the preview frame-dependent.
	 */
	/**
	 * Distance to WHAT THE PLAYER IS AIMING AT — a straight ray from the view point, independent of which
	 * arc is selected.
	 *
	 * ★ It has to be independent, and that is the whole point. Deciding the arc from where the currently
	 * selected arc's throw lands is circular: Close lands short, a short landing reads as Close, and the
	 * action latches to Close forever (measured 2026-09-16 — every throw came out arc=0 |v|=900 whatever
	 * the player aimed at). The distance is measured from the THROWER rather than from the release origin
	 * for the same reason: the two arcs' origins are a metre apart.
	 *
	 * @param MaxRange Returned unchanged when the ray hits nothing. Callers pass a range comfortably above
	 *                 the far threshold plus hysteresis, because an unobstructed throw into open space IS
	 *                 the far case — returning exactly FarArcDistance here sits below the Close->Far
	 *                 threshold and can never promote.
	 */
	static float MeasureAimDistance(
		const UObject* WorldContext,
		const FVector& ViewLocation,
		const FRotator& AimRotation,
		const FVector& MeasureFrom,
		const AActor* IgnoredActor,
		float MaxRange);

	static FAZ_ThrowPreviewResult Predict(
		const UObject* WorldContext,
		const FAZ_ThrowLaunchSolution& Solution,
		const AActor* IgnoredActor,
		float Horizon,
		float Frequency);
};
