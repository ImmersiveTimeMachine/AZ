#include "UI/AZ_MapCanvasWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Navigation/AZ_MapDefinition.h"
#include "Navigation/AZ_NavigationLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

UAZ_MapCanvasWidget::UAZ_MapCanvasWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChalkColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224));
	TrackedColor = FLinearColor::FromSRGBColor(FColor(255, 186, 140));
	PersonalColor = FLinearColor::FromSRGBColor(FColor(181, 200, 183));
	StoryColor = ChalkColor;
	SideQuestColor = ChalkColor;
	SelectionColor = ChalkColor;
	TrackingColor = ChalkColor;
	BackgroundBrush = FSlateColorBrush(FLinearColor::White);
	MapBrush.DrawAs = ESlateBrushDrawType::Image;
	SetIsFocusable(true);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
}

bool UAZ_MapCanvasWidget::CanUseNavigation() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return IsValid(QuestNavigation) && IsValid(Controller) && Controller->IsLocalController()
		&& QuestNavigation->GetOwner() == Controller;
}

bool UAZ_MapCanvasWidget::InitializeNavigation(UAZ_QuestMapComponent* InNavigation)
{
	ResetNavigation();
	QuestNavigation = InNavigation;
	if (!CanUseNavigation())
	{
		QuestNavigation = nullptr;
		return false;
	}
	QuestNavigation->OnViewChanged.AddUniqueDynamic(this, &ThisClass::HandleNavigationChanged);
	HandleNavigationChanged();
	return true;
}

void UAZ_MapCanvasWidget::ResetNavigation()
{
	if (IsValid(QuestNavigation))
	{
		QuestNavigation->OnViewChanged.RemoveDynamic(this, &ThisClass::HandleNavigationChanged);
	}
	QuestNavigation = nullptr;
	Definition = nullptr;
	Markers.Reset();
	MapBrush.SetResourceObject(nullptr);
	bPointerDown = false;
	bDragging = false;
	SelectedQuestId = NAME_None;
	SelectedObjectiveId = NAME_None;
	FitToMap();
}

void UAZ_MapCanvasWidget::HandleNavigationChanged()
{
	if (!CanUseNavigation()) { return; }
	UAZ_MapDefinition* NewDefinition = QuestNavigation->GetMapDefinition();
	if (Definition != NewDefinition)
	{
		Definition = NewDefinition;
		SelectedQuestId = NAME_None;
		SelectedObjectiveId = NAME_None;
		FitToMap();
	}
	MapBrush.SetResourceObject(IsValid(Definition) ? Definition->MapTexture.Get() : nullptr);
	Markers = QuestNavigation->GetMapMarkers();
	InvalidateLayoutAndVolatility();
}

void UAZ_MapCanvasWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Only this canvas needs per-frame paint for the local player's arrow. Hidden widgets do not paint.
	ForceVolatile(true);
}

void UAZ_MapCanvasWidget::NativeDestruct()
{
	ResetNavigation();
	Super::NativeDestruct();
}

void UAZ_MapCanvasWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsVisible() || !CanUseNavigation()) { return; }
	// The component updates moving targets without rebuilding the journal. Read its cached view only while visible.
	Markers = QuestNavigation->GetMapMarkers();
	const FVector2D NewSize = MyGeometry.GetLocalSize();
	if (!NewSize.ContainsNaN() && NewSize.X > 0.0 && NewSize.Y > 0.0 && NewSize != ViewportSize)
	{
		ViewportSize = NewSize;
		ClampView();
	}
}

FVector2D UAZ_MapCanvasWidget::ImageSizeForViewport(FVector2D Size) const
{
	if (!IsValid(Definition) || Size.ContainsNaN() || Size.X <= 0.0 || Size.Y <= 0.0
		|| !FMath::IsFinite(Zoom) || Zoom < 1.0)
	{
		return FVector2D::ZeroVector;
	}
	FVector2D ImageSize = Definition->WorldSizeCm;
	if (IsValid(Definition->MapTexture))
	{
		ImageSize = FVector2D(Definition->MapTexture->GetSizeX(), Definition->MapTexture->GetSizeY());
	}
	if (ImageSize.ContainsNaN() || ImageSize.X <= 0.0 || ImageSize.Y <= 0.0)
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D Result = ImageSize * (FMath::Min(Size.X / ImageSize.X, Size.Y / ImageSize.Y) * Zoom);
	return Result.ContainsNaN() ? FVector2D::ZeroVector : Result;
}

FVector2D UAZ_MapCanvasWidget::MapToLocal(FVector2D UV, FVector2D Size) const
{
	return Size * 0.5 + (UV - ViewCenter) * ImageSizeForViewport(Size);
}

bool UAZ_MapCanvasWidget::LocalToMap(FVector2D LocalPosition, FVector2D Size, FVector2D& OutUV) const
{
	OutUV = FVector2D::ZeroVector;
	const FVector2D DrawSize = ImageSizeForViewport(Size);
	if (!CanUseNavigation() || !IsValid(Definition) || LocalPosition.ContainsNaN()
		|| DrawSize.X <= UE_SMALL_NUMBER || DrawSize.Y <= UE_SMALL_NUMBER
		|| LocalPosition.X < 0.0 || LocalPosition.Y < 0.0 || LocalPosition.X > Size.X || LocalPosition.Y > Size.Y)
	{
		return false;
	}
	const FVector2D Result = ViewCenter + (LocalPosition - Size * 0.5) / DrawSize;
	if (!Definition->ContainsMapNormalized(Result)) { return false; }
	OutUV = Result;
	return true;
}

void UAZ_MapCanvasWidget::ClampView()
{
	if (ViewCenter.ContainsNaN()) { ViewCenter = FVector2D(0.5, 0.5); }
	const FVector2D DrawSize = ImageSizeForViewport(ViewportSize);
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		if (ViewportSize[Axis] <= 0.0 || DrawSize[Axis] <= UE_SMALL_NUMBER) { continue; }
		if (DrawSize[Axis] <= ViewportSize[Axis]) { ViewCenter[Axis] = 0.5; }
		else
		{
			const double HalfVisible = ViewportSize[Axis] / (2.0 * DrawSize[Axis]);
			ViewCenter[Axis] = FMath::Clamp(ViewCenter[Axis], HalfVisible, 1.0 - HalfVisible);
		}
	}
}

void UAZ_MapCanvasWidget::FitToMap()
{
	Zoom = 1.0;
	ViewCenter = FVector2D(0.5, 0.5);
	InvalidateLayoutAndVolatility();
}

bool UAZ_MapCanvasWidget::RecenterPlayer()
{
	APawn* Pawn = GetOwningPlayerPawn();
	FVector2D UV;
	if (!CanUseNavigation() || !IsValid(Pawn) || !IsValid(Definition)
		|| !Definition->WorldToMapNormalized(Pawn->GetActorLocation(), UV) || !Definition->ContainsMapNormalized(UV))
	{
		return false;
	}
	ViewCenter = UV;
	ClampView();
	InvalidateLayoutAndVolatility();
	return true;
}

void UAZ_MapCanvasWidget::PanNormalized(FVector2D Delta)
{
	if (!CanUseNavigation() || Delta.ContainsNaN()) { return; }
	ViewCenter += Delta;
	ClampView();
	InvalidateLayoutAndVolatility();
}

void UAZ_MapCanvasWidget::ZoomAt(FVector2D LocalPosition, double Factor)
{
	const FVector2D OldSize = ImageSizeForViewport(ViewportSize);
	if (!CanUseNavigation() || LocalPosition.ContainsNaN() || !FMath::IsFinite(Factor) || Factor <= 0.0
		|| OldSize.X <= UE_SMALL_NUMBER || OldSize.Y <= UE_SMALL_NUMBER) { return; }
	const FVector2D CursorUV = ViewCenter + (LocalPosition - ViewportSize * 0.5) / OldSize;
	const double Limit = FMath::IsFinite(MaximumZoom) ? FMath::Clamp(MaximumZoom, 1.0, 32.0) : 8.0;
	const double NewZoom = Zoom * Factor;
	if (!FMath::IsFinite(NewZoom)) { return; }
	Zoom = FMath::Clamp(NewZoom, 1.0, Limit);
	const FVector2D NewSize = ImageSizeForViewport(ViewportSize);
	if (NewSize.X > UE_SMALL_NUMBER && NewSize.Y > UE_SMALL_NUMBER)
	{
		ViewCenter = CursorUV - (LocalPosition - ViewportSize * 0.5) / NewSize;
	}
	ClampView();
	InvalidateLayoutAndVolatility();
}

void UAZ_MapCanvasWidget::ZoomAtCenter(double Factor)
{
	ZoomAt(ViewportSize * 0.5, Factor);
}

bool UAZ_MapCanvasWidget::PlaceWaypointAt(FVector2D LocalPosition)
{
	FVector2D UV;
	FAZ_MapWaypoint Waypoint;
	return LocalToMap(LocalPosition, ViewportSize, UV)
		&& UAZ_NavigationLibrary::MakeWaypointFromMap(Definition, UV, Waypoint)
		&& QuestNavigation->SetWaypoint(Waypoint);
}

bool UAZ_MapCanvasWidget::PlaceWaypointAtCenter()
{
	return PlaceWaypointAt(ViewportSize * 0.5);
}

bool UAZ_MapCanvasWidget::IsMarkerOnCurrentMap(const FAZ_QuestMapMarkerView& Marker) const
{
	return IsValid(Definition) && Marker.bResolved && !Marker.WorldLocation.ContainsNaN()
		&& !Marker.Target.MapId.IsNone() && Marker.Target.MapId == Definition->MapId
		&& !Marker.Target.LayerId.IsNone() && Marker.Target.LayerId == Definition->LayerId
		&& FMath::IsFinite(Marker.Target.SearchRadius) && Marker.Target.SearchRadius >= 0.0f;
}

bool UAZ_MapCanvasWidget::SelectMarker(FName QuestId, FName ObjectiveId)
{
	if (!CanUseNavigation() || QuestId.IsNone() || ObjectiveId.IsNone()) { return false; }
	for (const FAZ_QuestMapMarkerView& Marker : Markers)
	{
		FVector2D UV;
		if (!Marker.bPersonal && Marker.QuestId == QuestId && Marker.ObjectiveId == ObjectiveId
			&& IsMarkerOnCurrentMap(Marker) && Definition->WorldToMapNormalized(Marker.WorldLocation, UV)
			&& Definition->ContainsMapNormalized(UV))
		{
			SetSelectedObjective(QuestId, ObjectiveId);
			OnObjectiveSelected.Broadcast(QuestId, ObjectiveId);
			InvalidateLayoutAndVolatility();
			return true;
		}
	}
	return false;
}

void UAZ_MapCanvasWidget::SetSelectedObjective(FName QuestId, FName ObjectiveId)
{
	SelectedQuestId = QuestId;
	SelectedObjectiveId = ObjectiveId;
	InvalidateLayoutAndVolatility();
}

bool UAZ_MapCanvasWidget::SelectAt(FVector2D LocalPosition)
{
	if (!CanUseNavigation() || !IsValid(Definition) || LocalPosition.ContainsNaN()) { return false; }
	const FAZ_QuestMapMarkerView* Best = nullptr;
	double BestDistanceSquared = FMath::Square(20.0);
	for (const FAZ_QuestMapMarkerView& Marker : Markers)
	{
		FVector2D UV;
		if (Marker.bPersonal || !IsMarkerOnCurrentMap(Marker)
			|| !Definition->WorldToMapNormalized(Marker.WorldLocation, UV) || !Definition->ContainsMapNormalized(UV)) { continue; }
		const double DistanceSquared = (MapToLocal(UV, ViewportSize) - LocalPosition).SizeSquared();
		if (DistanceSquared <= BestDistanceSquared) { Best = &Marker; BestDistanceSquared = DistanceSquared; }
	}
	return Best && SelectMarker(Best->QuestId, Best->ObjectiveId);
}

bool UAZ_MapCanvasWidget::SelectMarkerAtCenter()
{
	return SelectAt(ViewportSize * 0.5);
}

bool UAZ_MapCanvasWidget::SelectNextMarker(bool bForward)
{
	if (!CanUseNavigation()) { return false; }
	TArray<int32> Eligible;
	int32 Current = INDEX_NONE;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FAZ_QuestMapMarkerView& Marker = Markers[Index];
		FVector2D UV;
		if (!Marker.bPersonal && !Marker.QuestId.IsNone() && !Marker.ObjectiveId.IsNone()
			&& IsMarkerOnCurrentMap(Marker) && Definition->WorldToMapNormalized(Marker.WorldLocation, UV)
			&& Definition->ContainsMapNormalized(UV))
		{
			if (Marker.QuestId == SelectedQuestId && Marker.ObjectiveId == SelectedObjectiveId) { Current = Eligible.Num(); }
			Eligible.Add(Index);
		}
	}
	if (Eligible.IsEmpty()) { return false; }
	const int32 Next = Current == INDEX_NONE ? (bForward ? 0 : Eligible.Num() - 1)
		: (Current + (bForward ? 1 : -1) + Eligible.Num()) % Eligible.Num();
	const FAZ_QuestMapMarkerView Marker = Markers[Eligible[Next]];
	FVector2D UV;
	Definition->WorldToMapNormalized(Marker.WorldLocation, UV);
	ViewCenter = UV;
	ClampView();
	return SelectMarker(Marker.QuestId, Marker.ObjectiveId);
}

FReply UAZ_MapCanvasWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!CanUseNavigation()) { return Super::NativeOnMouseButtonDown(Geometry, Event); }
	ViewportSize = Geometry.GetLocalSize();
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		MouseDownPosition = LastMousePosition = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		bPointerDown = true;
		bDragging = false;
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
	}
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		PlaceWaypointAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UAZ_MapCanvasWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bPointerDown || !CanUseNavigation()) { return Super::NativeOnMouseMove(Geometry, Event); }
	ViewportSize = Geometry.GetLocalSize();
	const FVector2D Position = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const double Threshold = FMath::IsFinite(DragThreshold) ? FMath::Clamp(DragThreshold, 1.0f, 32.0f) : 6.0;
	if ((Position - MouseDownPosition).SizeSquared() >= FMath::Square(Threshold)) { bDragging = true; }
	const FVector2D DrawSize = ImageSizeForViewport(ViewportSize);
	if (bDragging && DrawSize.X > UE_SMALL_NUMBER && DrawSize.Y > UE_SMALL_NUMBER)
	{
		PanNormalized(-(Position - LastMousePosition) / DrawSize);
	}
	LastMousePosition = Position;
	return FReply::Handled();
}

FReply UAZ_MapCanvasWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && (bPointerDown || HasMouseCapture()))
	{
		ViewportSize = Geometry.GetLocalSize();
		const FVector2D Position = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		const double Threshold = FMath::IsFinite(DragThreshold) ? FMath::Clamp(DragThreshold, 1.0f, 32.0f) : 6.0;
		if (bPointerDown && !bDragging && (Position - MouseDownPosition).SizeSquared() <= FMath::Square(Threshold)
			&& Position.X >= 0.0 && Position.Y >= 0.0
			&& Position.X <= ViewportSize.X && Position.Y <= ViewportSize.Y) { SelectAt(Position); }
		bPointerDown = false;
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

FReply UAZ_MapCanvasWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!CanUseNavigation()) { return Super::NativeOnMouseWheel(Geometry, Event); }
	ViewportSize = Geometry.GetLocalSize();
	ZoomAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), FMath::Pow(1.2, double(Event.GetWheelDelta())));
	return FReply::Handled();
}

void UAZ_MapCanvasWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	bPointerDown = false;
	bDragging = false;
	Super::NativeOnMouseCaptureLost(Event);
}

FReply UAZ_MapCanvasWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!CanUseNavigation()) { return Super::NativeOnKeyDown(Geometry, Event); }
	ViewportSize = Geometry.GetLocalSize();
	const FKey Key = Event.GetKey();
	const double Step = 0.08 / Zoom;
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) { PanNormalized(FVector2D(-Step, 0.0)); }
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) { PanNormalized(FVector2D(Step, 0.0)); }
	else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) { PanNormalized(FVector2D(0.0, -Step)); }
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) { PanNormalized(FVector2D(0.0, Step)); }
	else if (Key == EKeys::Home) { RecenterPlayer(); }
	else if (Key == EKeys::Add || Key == EKeys::Equals) { ZoomAtCenter(1.2); }
	else if (Key == EKeys::Subtract || Key == EKeys::Hyphen) { ZoomAtCenter(1.0 / 1.2); }
	else if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) { SelectMarkerAtCenter(); }
	else { return Super::NativeOnKeyDown(Geometry, Event); }
	return FReply::Handled();
}

int32 UAZ_MapCanvasWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, WidgetStyle, bParentEnabled);
	if (!IsVisible()) { return Base; }
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.ContainsNaN() || Size.X <= 0.0 || Size.Y <= 0.0) { return Base; }
	const FLinearColor InheritedTint = WidgetStyle.GetColorAndOpacityTint();
	Elements.PushClip(FSlateClippingZone(Geometry));
	FSlateDrawElement::MakeBox(Elements, Base + 1, Geometry.ToPaintGeometry(), &BackgroundBrush,
		ESlateDrawEffect::None, BackgroundColor * InheritedTint);
	FText Error;
	if (!CanUseNavigation() || !IsValid(Definition) || !Definition->ValidateDefinition(Error))
	{
		Elements.PopClip();
		return Base + 1;
	}
	const FVector2D DrawSize = ImageSizeForViewport(Size);
	if (DrawSize.X <= UE_SMALL_NUMBER || DrawSize.Y <= UE_SMALL_NUMBER)
	{
		Elements.PopClip();
		return Base + 1;
	}
	if (MapBrush.GetResourceObject())
	{
		const FVector2D TopLeft = MapToLocal(FVector2D::ZeroVector, Size);
		FSlateDrawElement::MakeBox(Elements, Base + 2,
			Geometry.ToPaintGeometry(FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(TopLeft))),
			&MapBrush, ESlateDrawEffect::None, InheritedTint);
	}
	const float Radius = FMath::IsFinite(MarkerRadius) ? FMath::Clamp(MarkerRadius, 4.0f, 32.0f) : 7.0f;
	const FSlateFontInfo Font = LabelFont.FontObject ? LabelFont
		: FCoreStyle::GetDefaultFontStyle("Regular", FMath::Clamp(LabelFontSize, 8, 32));
	const float InactiveOpacity = FMath::IsFinite(UntrackedMarkerOpacity) ? FMath::Clamp(UntrackedMarkerOpacity, 0.f, 1.f) : 0.65f;
	const float LineWidth = FMath::IsFinite(MarkerLineThickness) ? FMath::Clamp(MarkerLineThickness, 0.5f, 6.f) : 1.5f;
	const float TrackedLineWidth = FMath::IsFinite(TrackedMarkerLineThickness) ? FMath::Clamp(TrackedMarkerLineThickness, 0.5f, 6.f) : 2.5f;
	const float FramePadding = FMath::IsFinite(SelectionFramePadding) ? FMath::Clamp(SelectionFramePadding, 0.f, 16.f) : 4.f;
	auto DrawLines = [&](const TArray<FVector2D>& Points, FLinearColor Color, float Width = 1.5f)
	{
		FSlateDrawElement::MakeLines(Elements, Base + 3, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color * InheritedTint, true, Width);
	};
	// Reuse one scratch buffer across side-quest glyphs. Category metadata is
	// already cached in Markers; painting never resolves quests or loads assets.
	TArray<FVector2D> GlyphPoints;
	if (bUseCategoryMarkerStyle) { GlyphPoints.Reserve(25); }
	for (const FAZ_QuestMapMarkerView& Marker : Markers)
	{
		FVector2D UV;
		if (!IsMarkerOnCurrentMap(Marker) || !Definition->WorldToMapNormalized(Marker.WorldLocation, UV)
			|| !Definition->ContainsMapNormalized(UV)) { continue; }
		const bool bSelected = !Marker.QuestId.IsNone() && Marker.QuestId == SelectedQuestId && Marker.ObjectiveId == SelectedObjectiveId;
		FLinearColor Color = bUseCategoryMarkerStyle
			? (Marker.bPersonal ? PersonalColor : Marker.QuestCategory == EAZ_QuestCategory::Side ? SideQuestColor : StoryColor)
			: (Marker.bPersonal ? PersonalColor : ((Marker.bTracked || bSelected) ? TrackedColor : ChalkColor));
		if (!Marker.bTracked && !bSelected && !Marker.bPersonal) { Color.A *= InactiveOpacity; }
		const FVector2D P = MapToLocal(UV, Size);
		if (Marker.Target.SearchRadius > 0.0f)
		{
			TArray<FVector2D> Circle;
			for (int32 Segment = 0; Segment <= 48; ++Segment)
			{
				const double Angle = 2.0 * UE_PI * Segment / 48.0;
				const FVector World = Marker.WorldLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Marker.Target.SearchRadius;
				FVector2D CircleUV;
				if (!Definition->WorldToMapNormalized(World, CircleUV)) { Circle.Reset(); break; }
				Circle.Add(MapToLocal(CircleUV, Size));
			}
			if (Circle.Num() > 1) { FLinearColor AreaColor = Color; AreaColor.A *= 0.5f; DrawLines(Circle, AreaColor, 1.0f); }
		}
		if (P.X < -Radius || P.Y < -Radius || P.X > Size.X + Radius || P.Y > Size.Y + Radius) { continue; }
		if (Marker.bPersonal)
		{
			for (int32 XSign : {-1, 1})
			{
				for (int32 YSign : {-1, 1})
				{
					const FVector2D Corner = P + FVector2D(XSign * Radius, YSign * Radius);
					DrawLines({Corner - FVector2D(XSign * Radius * 0.55, 0.0), Corner,
						Corner - FVector2D(0.0, YSign * Radius * 0.55)}, Color, bUseCategoryMarkerStyle ? LineWidth : 2.0f);
				}
			}
		}
		else if (bUseCategoryMarkerStyle)
		{
			const float CategoryLineWidth = Marker.bTracked ? TrackedLineWidth : LineWidth;
			if (Marker.QuestCategory == EAZ_QuestCategory::Side)
			{
				GlyphPoints.Reset();
				for (int32 Segment = 0; Segment <= 24; ++Segment)
				{
					const double Angle = 2.0 * UE_PI * Segment / 24.0;
					GlyphPoints.Add(P + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
				}
				DrawLines(GlyphPoints, Color, CategoryLineWidth);
				const float DotRadius = FMath::Max(1.0f, Radius * 0.15f);
				FSlateDrawElement::MakeBox(Elements, Base + 3,
					Geometry.ToPaintGeometry(FVector2f(DotRadius * 2.f, DotRadius * 2.f),
						FSlateLayoutTransform(FVector2f(P - FVector2D(DotRadius, DotRadius)))),
					&BackgroundBrush, ESlateDrawEffect::None, Color * InheritedTint);
			}
			else
			{
				for (const float GlyphRadius : {Radius, Radius * 0.57f})
				{
					DrawLines({P + FVector2D(0, -GlyphRadius), P + FVector2D(GlyphRadius, 0),
						P + FVector2D(0, GlyphRadius), P + FVector2D(-GlyphRadius, 0),
						P + FVector2D(0, -GlyphRadius)}, Color, CategoryLineWidth);
				}
			}
		}
		else
		{
			DrawLines({P + FVector2D(0, -Radius), P + FVector2D(Radius, 0), P + FVector2D(0, Radius),
				P + FVector2D(-Radius, 0), P + FVector2D(0, -Radius)}, Color, bSelected ? 2.5f : 1.5f);
		}
		float LabelOffsetX = Radius + 8.0f;
		if (bUseCategoryMarkerStyle)
		{
			if (bSelected)
			{
				const float Extent = Radius + FramePadding;
				DrawLines({P + FVector2D(-Extent, -Extent), P + FVector2D(Extent, -Extent),
					P + FVector2D(Extent, Extent), P + FVector2D(-Extent, Extent),
					P + FVector2D(-Extent, -Extent)}, SelectionColor, LineWidth);
				LabelOffsetX += FramePadding;
			}
			if (Marker.bTracked && !Marker.bPersonal)
			{
				const float TickOffsetX = Radius + (bSelected ? FramePadding : 0.f) + 6.f;
				DrawLines({P + FVector2D(TickOffsetX, -5.f), P + FVector2D(TickOffsetX, 5.f)}, TrackingColor, TrackedLineWidth);
				LabelOffsetX = TickOffsetX + 6.f;
			}
		}
		if (bSelected || Marker.bTracked || Marker.bPersonal)
		{
			const FText Label = Marker.Label.IsEmpty() && Marker.bPersonal ? NSLOCTEXT("AZMap", "Waypoint", "Waypoint") : Marker.Label;
			FSlateDrawElement::MakeText(Elements, Base + 4,
				Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(P + FVector2D(LabelOffsetX, -8.0)))),
				Label, Font, ESlateDrawEffect::None, Color * InheritedTint);
		}
	}
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		FVector2D PlayerUV, ForwardUV;
		if (Definition->WorldToMapNormalized(Pawn->GetActorLocation(), PlayerUV) && Definition->ContainsMapNormalized(PlayerUV)
			&& Definition->WorldToMapNormalized(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.0, ForwardUV))
		{
			const FVector2D P = MapToLocal(PlayerUV, Size);
			const FVector2D Forward = ((ForwardUV - PlayerUV) * DrawSize).GetSafeNormal();
			const FVector2D Side(-Forward.Y, Forward.X);
			DrawLines({P + Forward * 11.0, P - Forward * 7.0 + Side * 6.0, P - Forward * 3.0,
				P - Forward * 7.0 - Side * 6.0, P + Forward * 11.0}, ChalkColor, 2.0f);
		}
	}
	if (HasAnyUserFocus())
	{
		FLinearColor ReticleColor = bUseCategoryMarkerStyle ? SelectionColor : ChalkColor; ReticleColor.A *= 0.45f;
		const FVector2D P = Size * 0.5;
		DrawLines({P - FVector2D(4.0, 0.0), P + FVector2D(4.0, 0.0)}, ReticleColor, 1.0f);
		DrawLines({P - FVector2D(0.0, 4.0), P + FVector2D(0.0, 4.0)}, ReticleColor, 1.0f);
	}
	Elements.PopClip();
	return Base + 4;
}
