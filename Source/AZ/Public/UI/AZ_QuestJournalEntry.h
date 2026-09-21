#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "Quests/AZ_QuestTypes.h"
#include "AZ_QuestJournalEntry.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UImage;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_QuestJournalEntryPicked, FName, QuestId, FName, ObjectiveId);

/** A journal selection row. It contains no gameplay state or completion logic. */
UCLASS(BlueprintType, Blueprintable)
class AZ_API UAZ_QuestJournalEntry : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FAZ_QuestJournalEntryPicked OnPicked;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style") FSlateFontInfo TitleFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style") FSlateFontInfo SubtitleFont;
	/** Row-local presentation only. Defaults retain the existing dark journal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Text")
	FLinearColor TitleColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Text")
	FLinearColor SelectedTitleColor = FLinearColor::FromSRGBColor(FColor(255, 186, 140));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Text")
	FLinearColor SubtitleColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224)).CopyWithNewOpacity(0.7f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Text", meta=(ClampMin="0", ClampMax="1"))
	float MutedTitleOpacity = 0.58f;
	/** Opt in to authored brush resources/padding; state tints below remain authoritative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button") bool bOverrideRowButtonStyle = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button", meta=(EditCondition="bOverrideRowButtonStyle"))
	FButtonStyle RowButtonStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button")
	FLinearColor NormalBackgroundColor = FLinearColor::FromSRGBColor(FColor(22, 33, 30));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button")
	FLinearColor SelectedBackgroundColor = FLinearColor::FromSRGBColor(FColor(53, 64, 56));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button")
	FLinearColor HoveredBackgroundColor = FLinearColor::FromSRGBColor(FColor(62, 74, 64));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Button")
	FLinearColor PressedBackgroundColor = FLinearColor::FromSRGBColor(FColor(76, 86, 73));
	/** Optional authored margin/frame. Selection never changes quest state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Selection")
	FLinearColor SelectionIndicatorColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224));
	/** Opt-in row hierarchy. Cached facts below are presentation only, never progression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Objectives") bool bUseObjectivePresentation = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Objectives") FSlateFontInfo ObjectiveTitleFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Objectives") FButtonStyle ObjectiveButtonStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Objectives") FLinearColor ObjectiveTitleColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Objectives") FLinearColor ObjectiveSelectedBackgroundColor = FLinearColor(0, 0, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FSlateBrush StoryGlyphBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FSlateBrush SideGlyphBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FSlateBrush CompleteGlyphBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FSlateBrush FailedGlyphBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FLinearColor StoryGlyphColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FLinearColor SideGlyphColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FLinearColor CompleteGlyphColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style|Glyphs") FLinearColor FailedGlyphColor = FLinearColor::White;
	void SetEntry(FName InQuestId, FName InObjectiveId, const FText& InTitle, const FText& InSubtitle, bool bInSelected, bool bInMuted);
	/** Supply already-resolved facts once, after SetEntry. No quest queries occur in this widget. */
	void SetPresentation(EAZ_QuestCategory InCategory, EAZ_QuestStatus InQuestStatus,
		EAZ_QuestObjectiveStatus InObjectiveStatus, bool bInOptional, bool bInTracked);
	FName GetQuestId() const { return QuestId; }
	FName GetObjectiveId() const { return ObjectiveId; }
	void FocusEntry();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> EntryButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> SelectionIndicator;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> EntryGlyph;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> TrackingIndicator;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") bool bObjectiveRow = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") EAZ_QuestCategory EntryCategory = EAZ_QuestCategory::Story;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") EAZ_QuestStatus EntryQuestStatus = EAZ_QuestStatus::Active;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") EAZ_QuestObjectiveStatus EntryObjectiveStatus = EAZ_QuestObjectiveStatus::Locked;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") bool bOptionalObjective = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Presentation") bool bTrackedEntry = false;
private:
	FName QuestId;
	FName ObjectiveId;
	FText Title;
	FText Subtitle;
	bool bSelected = false;
	bool bMuted = false;
	bool bHasPresentation = false;
	UFUNCTION() void HandlePicked();
	void ApplyView();
};
