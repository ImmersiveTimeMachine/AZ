#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/AZ_QuestMapComponent.h"
#include "AZ_MapCanvasWidget.generated.h"

class UAZ_MapDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_MapObjectiveSelected, FName, QuestId, FName, ObjectiveId);

/** Local full-map viewport. Projection comes only from its map definition; view pan/zoom never advances a quest. */
UCLASS(BlueprintType, Blueprintable)
class AZ_API UAZ_MapCanvasWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UAZ_MapCanvasWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Map")
	bool InitializeNavigation(UAZ_QuestMapComponent* InNavigation);

	UFUNCTION(BlueprintCallable, Category="Map")
	void ResetNavigation();

	UFUNCTION(BlueprintCallable, Category="Map")
	void FitToMap();

	UFUNCTION(BlueprintCallable, Category="Map")
	bool RecenterPlayer();

	UFUNCTION(BlueprintCallable, Category="Map")
	void PanNormalized(FVector2D Delta);

	UFUNCTION(BlueprintCallable, Category="Map")
	void ZoomAtCenter(double Factor);

	UFUNCTION(BlueprintCallable, Category="Map")
	bool PlaceWaypointAtCenter();

	UFUNCTION(BlueprintCallable, Category="Map")
	bool SelectMarker(FName QuestId, FName ObjectiveId);

	UFUNCTION(BlueprintCallable, Category="Map")
	bool SelectMarkerAtCenter();

	/**
	 * Place or replace the personal marker where the CONTROLLER POINTER is, falling back to the view centre
	 * when no pointer is showing (mouse session, or the stick has not been touched yet).
	 *
	 * ★ Centre-only targeting cannot reach an arbitrary place. ClampView pins the view centre to 0.5 at fit
	 * zoom and to an interior interval at any finite zoom, so panning alone can never put the centre over a
	 * map edge or corner — those points were simply unreachable with a controller. The pointer moves
	 * independently of the view, which is what makes them reachable.
	 */
	UFUNCTION(BlueprintCallable, Category="Map")
	bool PlaceWaypointAtPointer();

	/** Quest selection aimed at the same indicated point, so both commands agree about what is targeted. */
	UFUNCTION(BlueprintCallable, Category="Map")
	bool SelectMarkerAtPointer();

	/** True while the stick owns the on-screen pointer; false once the mouse takes the surface back. */
	UFUNCTION(BlueprintPure, Category="Map")
	bool IsControllerPointerActive() const { return bPointerActive; }

	UFUNCTION(BlueprintCallable, Category="Map")
	bool SelectNextMarker(bool bForward = true);

	/** Journal-driven selection without broadcasting another selection request. Empty IDs clear the highlight. */
	UFUNCTION(BlueprintCallable, Category="Map")
	void SetSelectedObjective(FName QuestId, FName ObjectiveId);

	UPROPERTY(BlueprintAssignable, Category="Map")
	FAZ_MapObjectiveSelected OnObjectiveSelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style")
	FLinearColor ChalkColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style")
	FLinearColor TrackedColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style")
	FLinearColor PersonalColor;
	/** Asset opt-in: preserve legacy shapes/tints until the matching style is authored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers")
	bool bUseCategoryMarkerStyle = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(EditCondition="bUseCategoryMarkerStyle"))
	FLinearColor StoryColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(EditCondition="bUseCategoryMarkerStyle"))
	FLinearColor SideQuestColor;
	/** Neutral focus frame and tracked tick are independent of the category colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(EditCondition="bUseCategoryMarkerStyle"))
	FLinearColor SelectionColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(EditCondition="bUseCategoryMarkerStyle"))
	FLinearColor TrackingColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(ClampMin="0", ClampMax="1"))
	float UntrackedMarkerOpacity = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(ClampMin="0.5", ClampMax="6.0", EditCondition="bUseCategoryMarkerStyle"))
	float MarkerLineThickness = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(ClampMin="0.5", ClampMax="6.0", EditCondition="bUseCategoryMarkerStyle"))
	float TrackedMarkerLineThickness = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style|Markers", meta=(ClampMin="0.0", ClampMax="16.0", EditCondition="bUseCategoryMarkerStyle"))
	float SelectionFramePadding = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style")
	FLinearColor BackgroundColor = FLinearColor(0.008f, 0.012f, 0.012f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style", meta=(ClampMin="4.0", ClampMax="32.0"))
	float MarkerRadius = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style", meta=(ClampMin="8", ClampMax="32"))
	int32 LabelFontSize = 14;
	/** Optional authored font (for example Oswald). LabelFontSize is the fallback font size only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Style")
	FSlateFontInfo LabelFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Input", meta=(ClampMin="1.0", ClampMax="32.0"))
	double MaximumZoom = 8.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Input", meta=(ClampMin="1.0", ClampMax="32.0"))
	float DragThreshold = 6.0f;

	/** Pointer travel at full stick deflection, in slate units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Input", meta=(ClampMin="100.0", ClampMax="4000.0"))
	float PointerSpeed = 900.0f;

	/** Radial dead zone for the pointer stick. Deflection past it is remapped from zero, so the slowest
	 *  usable nudge stays slow instead of jumping to the dead-zone speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map|Input", meta=(ClampMin="0.0", ClampMax="0.6"))
	float PointerDeadZone = 0.15f;

	UFUNCTION(BlueprintPure, Category="Map")
	double GetZoom() const { return Zoom; }
	UFUNCTION(BlueprintPure, Category="Map")
	FVector2D GetViewCenter() const { return ViewCenter; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient) TObjectPtr<UAZ_QuestMapComponent> QuestNavigation;
	UPROPERTY(Transient) TObjectPtr<UAZ_MapDefinition> Definition;
	UPROPERTY(Transient) TArray<FAZ_QuestMapMarkerView> Markers;
	UPROPERTY(Transient) FSlateBrush MapBrush;
	UPROPERTY(Transient) FSlateBrush BackgroundBrush;
	FVector2D ViewCenter = FVector2D(0.5, 0.5);
	FVector2D ViewportSize = FVector2D::ZeroVector;
	FVector2D MouseDownPosition = FVector2D::ZeroVector;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	double Zoom = 1.0;
	bool bPointerDown = false;
	bool bDragging = false;

	/**
	 * The controller pointer, in WIDGET-LOCAL space rather than map UV.
	 *
	 * Local space is the honest home for it: it is a thing on screen, it has to stay on screen through zoom,
	 * pan and resize, and every placement path already starts from a local position. Holding it in UV would
	 * make it drift under the view instead of staying where the player put it.
	 */
	FVector2D PointerLocal = FVector2D::ZeroVector;
	bool bPointerActive = false;
	bool bPointerInitialised = false;
	FName SelectedQuestId;
	FName SelectedObjectiveId;

	UFUNCTION() void HandleNavigationChanged();
	bool CanUseNavigation() const;
	bool IsMarkerOnCurrentMap(const FAZ_QuestMapMarkerView& Marker) const;
	FVector2D ImageSizeForViewport(FVector2D Size) const;
	FVector2D MapToLocal(FVector2D UV, FVector2D Size) const;
	bool LocalToMap(FVector2D LocalPosition, FVector2D Size, FVector2D& OutUV) const;
	void ClampView();
	void ZoomAt(FVector2D LocalPosition, double Factor);
	bool PlaceWaypointAt(FVector2D LocalPosition);
	bool SelectAt(FVector2D LocalPosition);
	void UpdateControllerPointer(float DeltaTime);
	FVector2D ClampPointer(FVector2D Local) const;
	FVector2D PointerOrCentre() const;
};
