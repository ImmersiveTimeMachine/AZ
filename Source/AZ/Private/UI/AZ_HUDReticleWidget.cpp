#include "UI/AZ_HUDReticleWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Image.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "UI/AZ_HUDReticleDefinition.h"

void UAZ_HUDReticleWidget::SetReticleView(const FAZ_PlayerReticleView& View)
{
	CurrentReticleView = View;
	if (IsValid(View.Definition))
	{
		SetColorAndOpacity(View.Definition->Tint);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	OnReticleViewChanged(View);
}

void UAZ_HUDReticleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!CurrentReticleView.bVisible || IsDesignTime()) return;
	if (!ArmUp && !ArmDown && !ArmLeft && !ArmRight) return;

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UGameViewportClient* ViewportClient = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
	FSceneViewProjectionData Projection;
	if (!ViewportClient || !ViewportClient->Viewport
		|| !LocalPlayer->GetProjectionData(ViewportClient->Viewport, Projection)
		|| !Projection.IsValidViewRectangle() || !Projection.IsPerspectiveProjection()
		|| !FMath::IsFinite(CurrentReticleView.SpreadAngleDegrees))
	{
		SetSpreadTranslation(FVector2D::ZeroVector);
		return;
	}

	// Project the same full cone used by firearm traces through this local player's
	// actual camera projection, including aim FOV, aspect bars and split-screen size.
	const FIntRect& ViewRect = Projection.GetConstrainedViewRect();
	const FVector2D PixelCenter(ViewRect.Min.X + ViewRect.Width() * .5,
		ViewRect.Min.Y + ViewRect.Height() * .5);
	// A 180-degree cone reaches the projection horizon. Keep its markers finite
	// and outside the view instead of sending infinities into Slate transforms.
	const double HalfAngle = FMath::DegreesToRadians(
		FMath::Clamp(static_cast<double>(CurrentReticleView.SpreadAngleDegrees) * .5, 0.0, 89.99));
	const double Tangent = FMath::Tan(HalfAngle);
	const double MaxPixelRadius = FMath::Max(ViewRect.Width(), ViewRect.Height()) * 2.0;
	const FVector2D PixelRadius(
		FMath::Min(ViewRect.Width() * .5 * FMath::Abs(Projection.ProjectionMatrix.M[0][0]) * Tangent, MaxPixelRadius),
		FMath::Min(ViewRect.Height() * .5 * FMath::Abs(Projection.ProjectionMatrix.M[1][1]) * Tangent, MaxPixelRadius));
	if (PixelRadius.ContainsNaN())
	{
		SetSpreadTranslation(FVector2D::ZeroVector);
		return;
	}

	FVector2D LocalCenter, LocalRight, LocalDown;
	USlateBlueprintLibrary::ScreenToWidgetLocal(this, MyGeometry, PixelCenter, LocalCenter);
	USlateBlueprintLibrary::ScreenToWidgetLocal(this, MyGeometry,
		PixelCenter + FVector2D(PixelRadius.X, 0.0), LocalRight);
	USlateBlueprintLibrary::ScreenToWidgetLocal(this, MyGeometry,
		PixelCenter + FVector2D(0.0, PixelRadius.Y), LocalDown);
	// ScreenToWidgetLocal removes viewport/DPI and the host's style scaling once.
	// Move the authored arms without changing the 40x40 desired bounds; otherwise
	// ScaleToFit would shrink their strokes as the aperture grows.
	constexpr double AuthoredInnerRadius = 6.0;
	SetSpreadTranslation(FVector2D(
		FMath::Max(0.0, (LocalRight - LocalCenter).Size() - AuthoredInnerRadius),
		FMath::Max(0.0, (LocalDown - LocalCenter).Size() - AuthoredInnerRadius)));
}

void UAZ_HUDReticleWidget::SetSpreadTranslation(FVector2D ExtraRadius)
{
	auto Move = [](UImage* Arm, UImage* Outline, FVector2D Translation)
	{
		if (Arm) Arm->SetRenderTranslation(Translation);
		if (Outline) Outline->SetRenderTranslation(Translation);
	};
	Move(ArmUp, OutlineUp, FVector2D(0.0, -ExtraRadius.Y));
	Move(ArmDown, OutlineDown, FVector2D(0.0, ExtraRadius.Y));
	Move(ArmLeft, OutlineLeft, FVector2D(-ExtraRadius.X, 0.0));
	Move(ArmRight, OutlineRight, FVector2D(ExtraRadius.X, 0.0));
}
