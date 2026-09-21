#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "AZ_QuestJournalEntry.generated.h"

class UButton;
class UTextBlock;
class UBorder;
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
	void SetEntry(FName InQuestId, FName InObjectiveId, const FText& InTitle, const FText& InSubtitle, bool bInSelected, bool bInMuted);
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
private:
	FName QuestId;
	FName ObjectiveId;
	FText Title;
	FText Subtitle;
	bool bSelected = false;
	bool bMuted = false;
	UFUNCTION() void HandlePicked();
	void ApplyView();
};
