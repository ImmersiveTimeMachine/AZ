// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_ThrowPreviewComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Quiet Sage / mockup03 — the approved look, expressed as calibration targets rather than world constants.
 *
 * ★ The sizes below are PIXELS AT 1080p, converted to world units per frame against the real camera distance
 * and field of view. The design is a 2D drawing: a 3px stroke is 3px whether the contact is 4m or 25m away,
 * and baking it as centimetres would make the arc a hairline at range and a plank underfoot.
 *
 * Authoring reference: UI Design/CHALK_Throw_v01/sources/build_throw_mockups.py, key '03'.
 */
USTRUCT(BlueprintType)
struct FAZ_ThrowPreviewStyle
{
	GENERATED_BODY()

	/** Ribbon cross-section. A FLAT quad, not a box: the cube gave the arc visible caps and tube facets. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Assets")
	TObjectPtr<UStaticMesh> ArcMesh;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Assets")
	TObjectPtr<UMaterialInterface> ArcMaterial;

	/** One quad carrying the WHOLE marker — four L corners, the incomplete ellipse and the centre — drawn in
	 *  UV space by the material. Thirteen little meshes would z-fight and flip independently. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Assets")
	TObjectPtr<UStaticMesh> MarkerMesh;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Assets")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	// ---- Palette ---------------------------------------------------------------------------------

	/** #B5C8B7 — arc filament, pulses and the four outer corners. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Palette")
	FLinearColor ArcColor = FLinearColor(FColor(0xB5, 0xC8, 0xB7));

	/** #EEEAE0 — warm white, the inner ellipse and centre oval. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Palette")
	FLinearColor MarkerColor = FLinearColor(FColor(0xEE, 0xEA, 0xE0));

	// ---- Arc, in reference pixels ----------------------------------------------------------------

	/** Sage pulse stroke, ~3px at 1080p. The ribbon is built at this width; the filament is a fraction of it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0.1"))
	float PulseWidthPixels = 3.f;

	/** Connecting filament as a fraction of the pulse stroke: 1.2px of 3px in the reference. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0.01", ClampMax = "1"))
	float FilamentWidthFraction = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0", ClampMax = "1"))
	float FilamentAlpha = 0.31f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0", ClampMax = "1"))
	float PulseAlpha = 0.94f;

	/**
	 * Pulses per metre of flight, and how much of each stride is lit.
	 *
	 * ★ Spacing is driven by CUMULATIVE DISTANCE along the arc, fed to the material per segment — never by
	 * each segment's own UV. Restarting the pattern at every ballistic sample is exactly what makes the
	 * segment boundaries visible, and it would make the spacing change whenever the sample count changed.
	 * The reference draws 28 pulses occupying about a third of the path.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0.1"))
	float PulsesPerMetre = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Arc", meta = (ClampMin = "0.01", ClampMax = "1"))
	float PulseDuty = 0.31f;

	// ---- Contact marker, in reference pixels -----------------------------------------------------

	/** Outer footprint of the four L corners: 104x30px projected in the reference. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Marker", meta = (ClampMin = "1"))
	float MarkerWidthPixels = 104.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Marker", meta = (ClampMin = "1"))
	float MarkerHeightPixels = 30.f;

	/** Legacy serialized field retained for asset compatibility; no longer rotates the marker. The quad
	 *  stays parallel to the contact surface and compensates foreshortening with bounded in-plane height. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Marker", meta = (ClampMin = "0", ClampMax = "90"))
	float MarkerMaxTiltDegrees = 55.f;

	/** Lifted off the surface so a marker on a floor does not z-fight with it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Marker", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MarkerSurfaceOffset = 1.5f;

	// ---- Budget and smoothing --------------------------------------------------------------------

	/**
	 * Points the displayed ribbon always uses, resampled along the solution by ARC LENGTH.
	 *
	 * ★ A fixed count is what makes a change of arc smooth. The solver returns however many samples the
	 * flight needed, so a different speed returns a different NUMBER of points and a per-index blend has
	 * nothing to blend against. Resampled, point i means the same fraction along the arc in both solutions —
	 * and because resampling keeps both ends, the true first-contact endpoint survives the cap.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Budget", meta = (ClampMin = "4"))
	int32 DisplaySamples = 32;

	/** Hard cap on pooled segments, so an arc into open sky costs a bounded amount. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Budget", meta = (ClampMin = "2"))
	int32 MaxSegments = 96;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Budget", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SmoothingTime = 0.12f;

	/** Reference screen height the pixel sizes above were authored against. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview|Budget", meta = (ClampMin = "1"))
	float ReferenceScreenHeight = 1080.f;
};

/**
 * UAZ_ThrowPreviewComponent — owner-only presentation of a solution someone else produced.
 *
 * It NEVER solves. It is handed an FAZ_ThrowPreviewResult and draws it, so there is exactly one producer of
 * launch parameters and no second set of constants that can quietly disagree with what the server launches.
 *
 * ★ Components are POOLED and hidden, never spawned or destroyed per frame: a ribbon that re-created its
 * segments every solve would allocate and register components for as long as the player held the button.
 *
 * ★ ABSOLUTE transform. The children hold world-space path points, so the component detaches itself from the
 * pawn's transform — otherwise walking while aiming would drag the drawn arc along with the body.
 *
 * ★ It must never affect the world it is drawn over: no collision, no shadows, no distance fields, and
 * owner-only visibility so another player never sees where this one is thinking about throwing.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_ThrowPreviewComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAZ_ThrowPreviewComponent();

	/** Assets and palette. Safe to call repeatedly; re-configuring with DIFFERENT materials rebuilds the
	 *  dynamic instances and re-assigns them to the pooled components, rather than silently keeping the old. */
	void ConfigureStyle(const FAZ_ThrowPreviewStyle& InStyle);

	/** Draw this solution. Owner-only; call on the locally controlled client. */
	void ShowSolution(const FAZ_ThrowPreviewResult& Result);

	/** Hide everything without destroying anything. */
	void HidePreview();

	/** Straight-line metres from the release origin to predicted first contact, or 0 when nothing was hit.
	 *  Real data for the HUD's range label — never a blast radius and never an illustrative constant. */
	float GetContactRangeMetres() const { return ContactRangeMetres; }

	bool HasContact() const { return bLastHitBlocking; }

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview")
	FAZ_ThrowPreviewStyle Style;

private:
	/** Resample a path to exactly Count points spaced evenly along its arc length, keeping both ends. */
	static void ResampleByArcLength(const TArray<FVector>& In, int32 Count, TArray<FVector>& Out);

	/** World size of Pixels reference-pixels at Distance from the camera. This is what keeps a 3px stroke
	 *  3px at any range instead of vanishing at distance. */
	float PixelsToWorld(float Pixels, double Distance) const;

	/** Camera location and half-FOV tangent for the local viewer, used for both sizing and ribbon facing. */
	bool GetViewInfo(FVector& OutLocation, float& OutTanHalfFov) const;

	USplineMeshComponent* AcquireSegment(int32 Index);
	UStaticMeshComponent* AcquireMarker();
	void UpdateMarker(const FVector& ViewLocation, float TanHalfFov);
	void LogFirstDraw(int32 PointCount) const;

	UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> Segments;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Marker;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ArcMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MarkerMID;

	/** Parents the current MIDs were built from, so a style swap is detected instead of ignored. */
	TWeakObjectPtr<UMaterialInterface> ArcMIDParent;
	TWeakObjectPtr<UMaterialInterface> MarkerMIDParent;

	/** Last drawn path, eased toward each new solution. Always DisplaySamples long once visible. */
	TArray<FVector> DrawnPoints;

	/** Eased contact, so the marker travels with the curve instead of teleporting ahead of it. */
	FVector DrawnImpact = FVector::ZeroVector;
	FVector DrawnNormal = FVector::UpVector;

	/** A CHANGE of blocking state snaps instead of easing: a wall that has just come into the path must
	 *  shorten the arc on the frame it is found, not slide into place over the next tenth of a second. */
	bool bLastHitBlocking = false;

	/** Source mesh sizes, cached in ConfigureStyle. A "scale" means nothing without the mesh it scales: the
	 *  engine Plane is 100cm, so a 3cm ribbon is scale 0.03, and any other art would need a different number. */
	float ArcMeshWidth = 100.f;
	FVector2D MarkerMeshSize = FVector2D(100.f, 100.f);

	float ContactRangeMetres = 0.f;
	double LastDrawTime = -1.0;
	bool bVisible = false;
};
