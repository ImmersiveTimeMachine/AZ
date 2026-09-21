#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "AZ_QuestMapPage.generated.h"

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
	UPROPERTY(BlueprintAssignable) FAZ_MapBackRequested OnBackToInventory;
	UFUNCTION(BlueprintCallable, Category="Map") void RefreshJournal();
	UFUNCTION(BlueprintCallable, Category="Map") void SelectObjective(FName QuestId, FName ObjectiveId);
	UFUNCTION(BlueprintCallable, Category="Map") void TrackSelection();
	UFUNCTION(BlueprintCallable, Category="Map") void ClearPersonalWaypoint();
	UFUNCTION(BlueprintCallable, Category="Map") void RecenterMap();
	UFUNCTION(BlueprintCallable, Category="Map") void BackToInventory();
	UFUNCTION(BlueprintPure, Category="Map") UAZ_QuestMapComponent* GetNavigation() const { return QuestNavigation; }
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
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
	FName SelectedQuestId;
	FName SelectedObjectiveId;
	void BindNavigation();
	void UnbindNavigation();
	void RefreshSelection();
};
