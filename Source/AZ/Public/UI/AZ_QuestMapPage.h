#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputTypeEnum.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "AZ_QuestMapPage.generated.h"

class UInputAction;
class UCommonUIActionRouterBase;
class UCommonInputSubsystem;
class UEnhancedInputLocalPlayerSubsystem;
class UAZ_QuestMapComponent;
class UAZ_MapCanvasWidget;
class UAZ_MapDefinition;
class UAZ_QuestJournalEntry;
class UScrollBox;
class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_MapBackRequested);

/** Approved Field Journal page: journal left, full map right. Presentation only. */
UCLASS(BlueprintType, Blueprintable)
class AZ_API UAZ_QuestMapPage : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map") TObjectPtr<UAZ_MapDefinition> DefaultMapDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map") TSubclassOf<UAZ_QuestJournalEntry> JournalEntryClass;
	/** Applied to every native-created section heading when the journal refreshes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FSlateFontInfo SectionHeadingFont;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style")
	FLinearColor SectionHeadingColor = FLinearColor::FromSRGBColor(FColor(181, 200, 183));
	/** Opt-in paper category headings. Archive keeps the neutral SectionHeadingColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") bool bUseCategorySectionStyle = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FSlateBrush StorySectionBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FSlateBrush SideSectionBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FLinearColor StorySectionColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FLinearColor SideSectionColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Style") FMargin SectionHeadingPadding = FMargin(0, 20, 0, 12);
	UPROPERTY(BlueprintAssignable) FAZ_MapBackRequested OnBackToInventory;
	UFUNCTION(BlueprintCallable, Category="Map") void RefreshJournal();
	UFUNCTION(BlueprintCallable, Category="Map") void SelectObjective(FName QuestId, FName ObjectiveId);
	UFUNCTION(BlueprintCallable, Category="Map") void TrackSelection();
	UFUNCTION(BlueprintCallable, Category="Map") void ClearPersonalWaypoint();
	UFUNCTION(BlueprintCallable, Category="Map") void RecenterMap();
	UFUNCTION(BlueprintCallable, Category="Map") void BackToInventory();
	UFUNCTION(BlueprintPure, Category="Map") UAZ_QuestMapComponent* GetNavigation() const { return QuestNavigation; }
	/** Prompt handle only: callers must not register another command for its glyph. */
	UFUNCTION(BlueprintPure, Category="Map|Input") FUIActionBindingHandle GetMapActionBinding(const UInputAction* Action) const;
	/** ConfigureBinding prompts refresh here after complete registration or removal. */
	UFUNCTION(BlueprintImplementableEvent, Category="Map|Input") void OnMapActionBindingsChanged();
	/** Missing map operations only. Back, Select and tab switching retain their current owners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapZoomInAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapZoomOutAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapRecenterAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapPlaceWaypointAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapClearWaypointAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TObjectPtr<UInputAction> MapTrackSelectionAction;
	/** Exact parent Back/Select/tab/inventory-toggle action objects, including configured CommonInput defaults.
	 *  Required before map commands register. Conflicting live keys fail closed on mapping rebuild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map|Input") TArray<TObjectPtr<UInputAction>> ReservedParentActions;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void ActivateMappingContext() override;
	virtual void DeactivateMappingContext() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UAZ_MapCanvasWidget> MapCanvas;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UScrollBox> QuestList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> QuestTitle;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> QuestDescription;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> TrackButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> ClearWaypointButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> RecenterButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> InventoryButton;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_QuestMapComponent> QuestNavigation;
	TMap<const UInputAction*, FUIActionBindingHandle> MapActionBindings;
	TMap<FKey, TWeakObjectPtr<UCommonUIActionRouterBase>> ForwardedMapKeys;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundMapInput;
	TWeakObjectPtr<UCommonInputSubsystem> MapPresentationInput;
	FDelegateHandle MapInputChangedHandle;
	void HandleMapInputMethodChanged(ECommonInputType InputType);
	bool bMapMappingContextActive = false;
	UFUNCTION() void RefreshMapActionBindings();
	void UnregisterMapActions();
	void ReleaseForwardedMapKeys();
	void UnbindMapInput();
	bool HasSafeMapMappingContext() const;
	bool CanRouteMapCommand() const;
	bool HasParentKeyConflict(const UInputAction* Action, const TArray<FKey>& Keys) const;
	bool IsMapCommandKey(FKey Key) const;
	void HandleMapZoomIn();
	void HandleMapZoomOut();
	void HandleMapRecenter();
	void HandleMapPlaceWaypoint();
	void HandleMapClearWaypoint();
	void HandleMapTrackSelection();
	FName SelectedQuestId;
	FName SelectedObjectiveId;
	void BindNavigation();
	void UnbindNavigation();
	void RefreshSelection();
};
