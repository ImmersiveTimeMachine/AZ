// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowPreviewComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace AZThrowPreview
{
	/** Material parameter names. Named once so the component and the authored material cannot drift. */
	static const FName P_Color(TEXT("Color"));
	static const FName P_CenterColor(TEXT("CenterColor"));
	static const FName P_FilamentAlpha(TEXT("FilamentAlpha"));
	static const FName P_PulseAlpha(TEXT("PulseAlpha"));
	static const FName P_FilamentWidth(TEXT("FilamentWidth"));
	static const FName P_PulseSpacing(TEXT("PulseSpacing"));
	static const FName P_PulseDuty(TEXT("PulseDuty"));
	/** Per-segment: where this segment starts along the whole arc, and how long it is. */
	static const int32 D_StartDistance = 0;
	static const int32 D_SegmentLength = 1;
}

UAZ_ThrowPreviewComponent::UAZ_ThrowPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The children carry world-space path points. Without this the whole arc would ride the pawn's transform
	// and swing around the character as they walk and turn while aiming.
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UAZ_ThrowPreviewComponent::ConfigureStyle(const FAZ_ThrowPreviewStyle& InStyle)
{
	Style = InStyle;

	// ★ Rebuild a dynamic instance when its PARENT changed, and push the new one onto everything already
	// pooled. Keeping the old MID because one exists is how a style swap silently does nothing.
	if (ArcMIDParent.Get() != Style.ArcMaterial)
	{
		ArcMID = Style.ArcMaterial ? UMaterialInstanceDynamic::Create(Style.ArcMaterial, this) : nullptr;
		ArcMIDParent = Style.ArcMaterial;
		for (USplineMeshComponent* Segment : Segments)
		{
			if (Segment) { Segment->SetMaterial(0, ArcMID); }
		}
	}
	if (MarkerMIDParent.Get() != Style.MarkerMaterial)
	{
		MarkerMID = Style.MarkerMaterial ? UMaterialInstanceDynamic::Create(Style.MarkerMaterial, this) : nullptr;
		MarkerMIDParent = Style.MarkerMaterial;
		if (Marker) { Marker->SetMaterial(0, MarkerMID); }
	}
	// Meshes can change too; pooled components keep whatever they were given otherwise.
	for (USplineMeshComponent* Segment : Segments)
	{
		if (Segment) { Segment->SetStaticMesh(Style.ArcMesh); }
	}
	if (Marker) { Marker->SetStaticMesh(Style.MarkerMesh); }

	// Cached once: world size divided by these gives the scale each mesh needs.
	if (Style.ArcMesh)
	{
		const FVector Extent = Style.ArcMesh->GetBounds().BoxExtent * 2.0;
		ArcMeshWidth = FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(FMath::Max(Extent.Y, Extent.Z)));
	}
	if (Style.MarkerMesh)
	{
		const FVector Extent = Style.MarkerMesh->GetBounds().BoxExtent * 2.0;
		MarkerMeshSize = FVector2D(FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(Extent.X)),
			FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(Extent.Y)));
	}

	using namespace AZThrowPreview;
	if (ArcMID)
	{
		ArcMID->SetVectorParameterValue(P_Color, Style.ArcColor);
		ArcMID->SetScalarParameterValue(P_FilamentAlpha, Style.FilamentAlpha);
		ArcMID->SetScalarParameterValue(P_PulseAlpha, Style.PulseAlpha);
		ArcMID->SetScalarParameterValue(P_FilamentWidth, FMath::Clamp(Style.FilamentWidthFraction, 0.01f, 1.f));
		// Spacing in CENTIMETRES of arc length, so the pattern is stable however the path is sampled.
		ArcMID->SetScalarParameterValue(P_PulseSpacing, 100.f / FMath::Max(0.1f, Style.PulsesPerMetre));
		ArcMID->SetScalarParameterValue(P_PulseDuty, FMath::Clamp(Style.PulseDuty, 0.01f, 1.f));
	}
	// ★ The marker's tint is NOT overwritten here.
	//
	// Each supplied contact material carries its own state-appropriate palette — MI_QS_Contact is sage
	// corners with a warm-white centre, MI_QS_Blocked is warm white throughout. Pushing ArcColor onto every
	// marker made switching to the blocked variant a no-op, because the material changed and then had its
	// colour immediately overwritten with the arc's. The art owns the palette; the runtime owns which
	// material is shown. Only the arc, whose pulse parameters ARE runtime calibration, is driven below.
}

void UAZ_ThrowPreviewComponent::ResampleByArcLength(const TArray<FVector>& In, const int32 Count,
	TArray<FVector>& Out)
{
	Out.Reset();
	if (In.Num() < 2 || Count < 2)
	{
		Out = In;
		return;
	}
	// Cumulative length, so point i lands at the same FRACTION ALONG THE CURVE every frame regardless of how
	// many samples the solver produced — and so the first and last points, including the true first-contact
	// endpoint, are preserved exactly rather than dropped by a cap.
	TArray<double> Lengths;
	Lengths.Reserve(In.Num());
	Lengths.Add(0.0);
	for (int32 Index = 1; Index < In.Num(); ++Index)
	{
		Lengths.Add(Lengths.Last() + FVector::Dist(In[Index - 1], In[Index]));
	}
	const double Total = Lengths.Last();
	Out.Reserve(Count);
	if (Total <= UE_SMALL_NUMBER)
	{
		for (int32 Index = 0; Index < Count; ++Index) { Out.Add(In[0]); }
		return;
	}
	int32 Segment = 1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Target = Total * Index / (Count - 1);
		while (Segment < Lengths.Num() - 1 && Lengths[Segment] < Target) { ++Segment; }
		const double Span = Lengths[Segment] - Lengths[Segment - 1];
		const double Alpha = Span > UE_SMALL_NUMBER ? (Target - Lengths[Segment - 1]) / Span : 0.0;
		Out.Add(FMath::Lerp(In[Segment - 1], In[Segment], Alpha));
	}
}

bool UAZ_ThrowPreviewComponent::GetViewInfo(FVector& OutLocation, float& OutTanHalfFov) const
{
	const AActor* Owner = GetOwner();
	const APlayerController* Player = Owner ? Cast<APlayerController>(Owner->GetInstigatorController()) : nullptr;
	if (!Player)
	{
		Player = Owner ? Owner->GetWorld()->GetFirstPlayerController() : nullptr;
	}
	const APlayerCameraManager* Camera = Player ? Player->PlayerCameraManager : nullptr;
	if (!Camera)
	{
		return false;
	}
	OutLocation = Camera->GetCameraLocation();
	// Vertical half-angle. GetFOVAngle is horizontal, so convert through the viewport aspect ratio.
	const float HorizontalFov = FMath::Clamp(Camera->GetFOVAngle(), 1.f, 179.f);
	const float Aspect = Camera->GetCameraCacheView().AspectRatio > KINDA_SMALL_NUMBER
		? Camera->GetCameraCacheView().AspectRatio : 16.f / 9.f;
	OutTanHalfFov = FMath::Tan(FMath::DegreesToRadians(HorizontalFov * 0.5f)) / Aspect;
	return true;
}

float UAZ_ThrowPreviewComponent::PixelsToWorld(const float Pixels, const double Distance) const
{
	// Half the visible world height at this distance is Distance * tan(halfFov); the reference screen spans
	// that whole height in ReferenceScreenHeight pixels. Cached per call site rather than per segment.
	float TanHalfFov = 0.f;
	FVector ViewLocation;
	if (!GetViewInfo(ViewLocation, TanHalfFov))
	{
		return Pixels;   // no camera: fall back to treating the number as centimetres
	}
	const double WorldHeight = 2.0 * FMath::Max(1.0, Distance) * TanHalfFov;
	return static_cast<float>(WorldHeight * Pixels / FMath::Max(1.f, Style.ReferenceScreenHeight));
}

USplineMeshComponent* UAZ_ThrowPreviewComponent::AcquireSegment(const int32 Index)
{
	if (Segments.IsValidIndex(Index))
	{
		return Segments[Index];
	}
	USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
	Segment->SetMobility(EComponentMobility::Movable);
	Segment->SetupAttachment(this);
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The preview is a picture. It must never be findable by a trace, cast a shadow, or influence the
	// prediction it is drawing.
	Segment->SetCastShadow(false);
	Segment->bAffectDistanceFieldLighting = false;
	Segment->bAffectDynamicIndirectLighting = false;
	// Owner-only: another player must not see where this one is thinking about throwing.
	Segment->SetOnlyOwnerSee(true);
	Segment->SetStaticMesh(Style.ArcMesh);
	if (ArcMID)
	{
		Segment->SetMaterial(0, ArcMID);
	}
	Segment->RegisterComponent();
	Segments.Add(Segment);
	return Segment;
}

UStaticMeshComponent* UAZ_ThrowPreviewComponent::AcquireMarker()
{
	if (Marker)
	{
		return Marker;
	}
	Marker = NewObject<UStaticMeshComponent>(this);
	Marker->SetMobility(EComponentMobility::Movable);
	Marker->SetupAttachment(this);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	Marker->bAffectDistanceFieldLighting = false;
	Marker->bAffectDynamicIndirectLighting = false;
	Marker->SetOnlyOwnerSee(true);
	Marker->SetStaticMesh(Style.MarkerMesh);
	if (MarkerMID)
	{
		Marker->SetMaterial(0, MarkerMID);
	}
	Marker->RegisterComponent();
	return Marker;
}

void UAZ_ThrowPreviewComponent::ShowSolution(const FAZ_ThrowPreviewResult& Result)
{
	const UWorld* World = GetWorld();
	if (!World || Result.Points.Num() < 2)
	{
		HidePreview();
		return;
	}
	FVector ViewLocation = FVector::ZeroVector;
	float TanHalfFov = 0.f;
	const bool bHasView = GetViewInfo(ViewLocation, TanHalfFov);

	const int32 PointCount = FMath::Clamp(Style.DisplaySamples, 2, FMath::Max(2, Style.MaxSegments + 1));
	TArray<FVector> Target;
	ResampleByArcLength(Result.Points, PointCount, Target);
	if (Target.Num() != PointCount)
	{
		HidePreview();
		return;
	}

	// ---- Ease the drawn path toward the new solution ---------------------------------------------
	const double Now = World->GetTimeSeconds();
	const float Delta = LastDrawTime >= 0.0 ? static_cast<float>(Now - LastDrawTime) : 0.f;
	LastDrawTime = Now;
	const float Alpha = (Style.SmoothingTime > KINDA_SMALL_NUMBER && bVisible)
		? 1.f - FMath::Exp(-Delta / Style.SmoothingTime) : 1.f;

	// Snap on the first draw, and whenever the path STARTS or STOPS being blocked: a wall that has just come
	// into the arc must shorten it on the frame it is found, not ease into place. Everything else — a change
	// of speed, of origin, of aim — eases across the whole curve at once.
	const bool bSnap = DrawnPoints.Num() != PointCount || Result.bHitBlocking != bLastHitBlocking;
	bLastHitBlocking = Result.bHitBlocking;
	if (bSnap)
	{
		DrawnPoints = Target;
		DrawnImpact = Result.ImpactPoint;
		DrawnNormal = Result.ImpactNormal;
	}
	else
	{
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			DrawnPoints[Index] = FMath::Lerp(DrawnPoints[Index], Target[Index], Alpha);
		}
		// The marker rides the same easing, so it stays on the end of the line instead of arriving first.
		DrawnImpact = FMath::Lerp(DrawnImpact, Result.ImpactPoint, Alpha);
		DrawnNormal = FMath::Lerp(DrawnNormal, Result.ImpactNormal, Alpha).GetSafeNormal(
			UE_SMALL_NUMBER, FVector::UpVector);
	}
	ContactRangeMetres = Result.bHitBlocking
		? static_cast<float>(FVector::Dist(DrawnPoints[0], DrawnImpact) / 100.0) : 0.f;

	// ---- Ribbon ----------------------------------------------------------------------------------
	// Cumulative distance is accumulated here and handed to each segment, so the pulse pattern is continuous
	// across the whole arc instead of restarting at every sample.
	double Travelled = 0.0;
	for (int32 Index = 0; Index + 1 < PointCount; ++Index)
	{
		USplineMeshComponent* Segment = AcquireSegment(Index);
		if (!Segment)
		{
			continue;
		}
		const FVector& Start = DrawnPoints[Index];
		const FVector& End = DrawnPoints[Index + 1];
		// Tangents from the neighbouring samples, so the ribbon reads as one curve rather than a chain of
		// straight sticks with a visible corner at every sample.
		const FVector Prev = DrawnPoints[FMath::Max(0, Index - 1)];
		const FVector Next = DrawnPoints[FMath::Min(PointCount - 1, Index + 2)];
		Segment->SetStartAndEnd(Start, (End - Prev) * 0.5f, End, (Next - Start) * 0.5f, false);

		// ★ Face the camera. A flat quad is invisible edge-on, so the ribbon's up vector is aimed at the
		// viewer; this is what turns a strip of geometry into a readable line from any angle.
		if (bHasView)
		{
			const FVector Mid = (Start + End) * 0.5;
			Segment->SetSplineUpDir((ViewLocation - Mid).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector), false);
		}
		// Constant apparent thickness: each end is sized for its own distance, so a receding arc keeps its
		// stroke instead of tapering away.
		const float StartWidth = PixelsToWorld(Style.PulseWidthPixels, FVector::Dist(ViewLocation, Start)) / ArcMeshWidth;
		const float EndWidth = PixelsToWorld(Style.PulseWidthPixels, FVector::Dist(ViewLocation, End)) / ArcMeshWidth;
		Segment->SetStartScale(FVector2D(StartWidth, StartWidth), false);
		Segment->SetEndScale(FVector2D(EndWidth, EndWidth), false);
		Segment->UpdateMesh();

		const double Length = FVector::Dist(Start, End);
		Segment->SetCustomPrimitiveDataFloat(AZThrowPreview::D_StartDistance, static_cast<float>(Travelled));
		Segment->SetCustomPrimitiveDataFloat(AZThrowPreview::D_SegmentLength, static_cast<float>(Length));
		Travelled += Length;
		Segment->SetVisibility(true, false);
	}
	// Surplus segments from a previously longer arc are hidden, not destroyed: the next solve usually wants
	// them straight back.
	for (int32 Index = FMath::Max(0, PointCount - 1); Index < Segments.Num(); ++Index)
	{
		Segments[Index]->SetVisibility(false, false);
	}

	UpdateMarker(ViewLocation, TanHalfFov);
	if (!bVisible)
	{
		LogFirstDraw(PointCount);
	}
	bVisible = true;
}

void UAZ_ThrowPreviewComponent::UpdateMarker(const FVector& ViewLocation, float /*TanHalfFov*/)
{
	if (!bLastHitBlocking)
	{
		// Open sky inside the horizon. The line simply ends; inventing a contact marker at the last sample
		// would advertise a landing spot the throw was never predicted to reach.
		if (Marker) { Marker->SetVisibility(false, false); }
		return;
	}
	UStaticMeshComponent* Component = AcquireMarker();
	if (!Component)
	{
		return;
	}
	const FVector SurfaceNormal = DrawnNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Centre = DrawnImpact + SurfaceNormal * Style.MarkerSurfaceOffset;

	// Keep the complete quad parallel to the hit plane. Tilting around a centre only 1.5cm above it
	// buried the lower glyph in the floor. Compensate foreshortening by stretching within the plane below,
	// preserving both the real contact anchor and normal depth occlusion.
	const FVector ToViewer = (ViewLocation - Centre).GetSafeNormal(UE_SMALL_NUMBER, SurfaceNormal);

	// ★ Stable tangent. The marker's long axis is the VIEWER's right vector projected onto the contact
	// surface: it lies flat on whatever was hit, keeps its horizon, and — unlike MakeFromZ — cannot spin or
	// flip as the normal passes an axis.
	FVector Right = FVector::CrossProduct(-ToViewer, SurfaceNormal);
	if (!Right.Normalize())
	{
		Right = FVector::CrossProduct(FVector::UpVector, SurfaceNormal).GetSafeNormal(
			UE_SMALL_NUMBER, FVector::RightVector);
	}
	const FVector Forward = FVector::CrossProduct(SurfaceNormal, Right).GetSafeNormal(
		UE_SMALL_NUMBER, FVector::ForwardVector);
	// The quad's own +Z is its normal; its +X/+Y span the drawing.
	Component->SetWorldLocationAndRotation(Centre, FMatrix(Right, Forward, SurfaceNormal, FVector::ZeroVector).ToQuat());

	// The short axis foreshortens by |normal dot view|. Restore its projected size up to 4x; a near-grazing
	// view must not create an unbounded footprint. Width and the contact position remain unchanged.
	const float Facing = FMath::Clamp(FMath::Abs(static_cast<float>(FVector::DotProduct(SurfaceNormal, ToViewer))), 0.f, 1.f);
	const float HeightCompensation = 1.f / FMath::Max(0.25f, Facing);
	const double Distance = FVector::Dist(ViewLocation, Centre);
	const float Width = PixelsToWorld(Style.MarkerWidthPixels, Distance);
	const float Height = PixelsToWorld(Style.MarkerHeightPixels, Distance) * HeightCompensation;
	Component->SetWorldScale3D(FVector(Width / MarkerMeshSize.X, Height / MarkerMeshSize.Y, 1.f));
	Component->SetVisibility(true, false);
}

void UAZ_ThrowPreviewComponent::LogFirstDraw(const int32 PointCount) const
{
	UE_LOG(LogTemp, Warning,
		TEXT("[ThrowPreview] draw points=%d segments=%d | arcMesh=%s arcMat=%s arcMID=%s | markerMesh=%s ")
		TEXT("markerMID=%s | range=%.1fm contact=%d owner=%s"),
		PointCount, Segments.Num(), *GetNameSafe(Style.ArcMesh), *GetNameSafe(Style.ArcMaterial),
		*GetNameSafe(ArcMID), *GetNameSafe(Style.MarkerMesh), *GetNameSafe(MarkerMID),
		ContactRangeMetres, bLastHitBlocking ? 1 : 0, *GetNameSafe(GetOwner()));
}

void UAZ_ThrowPreviewComponent::HidePreview()
{
	for (USplineMeshComponent* Segment : Segments)
	{
		if (Segment) { Segment->SetVisibility(false, false); }
	}
	if (Marker) { Marker->SetVisibility(false, false); }
	// Drop the eased path so the next throw starts from its own first solution instead of sweeping the
	// ribbon across the world from wherever the last one ended.
	DrawnPoints.Reset();
	LastDrawTime = -1.0;
	bLastHitBlocking = false;
	ContactRangeMetres = 0.f;
	bVisible = false;
}
