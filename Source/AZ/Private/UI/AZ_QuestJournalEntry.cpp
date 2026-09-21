#include "UI/AZ_QuestJournalEntry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UAZ_QuestJournalEntry::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EntryButton"));
		WidgetTree->RootWidget = EntryButton;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		EntryButton->AddChild(Row);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Row->Slot)) { ContentSlot->SetPadding(FMargin(14, 12)); ContentSlot->SetHorizontalAlignment(HAlign_Fill); }
		EntryGlyph = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EntryGlyph"));
		UHorizontalBoxSlot* GlyphSlot = Row->AddChildToHorizontalBox(EntryGlyph);
		GlyphSlot->SetPadding(FMargin(0, 0, 10, 0));
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToHorizontalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleText"));
		Content->AddChildToVerticalBox(TitleText);
		Content->AddChildToVerticalBox(SubtitleText)->SetPadding(FMargin(0, 5, 0, 0));
		TitleText->SetAutoWrapText(true);
		SubtitleText->SetAutoWrapText(true);
	}
	return Super::RebuildWidget();
}

void UAZ_QuestJournalEntry::NativeConstruct()
{
	Super::NativeConstruct();
	if (EntryButton) EntryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePicked);
	ApplyView();
}

void UAZ_QuestJournalEntry::NativeDestruct()
{
	if (EntryButton) EntryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePicked);
	Super::NativeDestruct();
}

void UAZ_QuestJournalEntry::SetEntry(FName InQuestId, FName InObjectiveId, const FText& InTitle, const FText& InSubtitle, bool bInSelected, bool bInMuted)
{
	QuestId = InQuestId; ObjectiveId = InObjectiveId; Title = InTitle; Subtitle = InSubtitle;
	bSelected = bInSelected; bMuted = bInMuted;
	bObjectiveRow = !ObjectiveId.IsNone();
	bHasPresentation = false;
	bOptionalObjective = false;
	bTrackedEntry = false;
	ApplyView();
}

void UAZ_QuestJournalEntry::SetPresentation(EAZ_QuestCategory InCategory, EAZ_QuestStatus InQuestStatus,
	EAZ_QuestObjectiveStatus InObjectiveStatus, bool bInOptional, bool bInTracked)
{
	EntryCategory = InCategory;
	EntryQuestStatus = InQuestStatus;
	EntryObjectiveStatus = InObjectiveStatus;
	bOptionalObjective = bObjectiveRow && bInOptional;
	bTrackedEntry = bInTracked;
	bHasPresentation = true;
	ApplyView();
}

void UAZ_QuestJournalEntry::HandlePicked() { OnPicked.Broadcast(QuestId, ObjectiveId); }

void UAZ_QuestJournalEntry::FocusEntry()
{
	if (EntryButton && GetOwningPlayer()) EntryButton->SetUserFocus(GetOwningPlayer());
}

void UAZ_QuestJournalEntry::ApplyView()
{
	const bool bSemanticObjective = bUseObjectivePresentation && bHasPresentation && bObjectiveRow;
	const bool bActiveObjective = bSemanticObjective && EntryQuestStatus == EAZ_QuestStatus::Active
		&& EntryObjectiveStatus == EAZ_QuestObjectiveStatus::Active;
	const FLinearColor CategoryColor = EntryCategory == EAZ_QuestCategory::Side ? SideGlyphColor : StoryGlyphColor;
	if (TitleText)
	{
		TitleText->SetText(Title);
		const FSlateFontInfo& AppliedFont = bSemanticObjective ? ObjectiveTitleFont : TitleFont;
		TitleText->SetFont(AppliedFont.FontObject ? AppliedFont : FCoreStyle::GetDefaultFontStyle("Regular", bSemanticObjective ? 15 : 18));
		FLinearColor AppliedTitleColor = bSemanticObjective ? (bActiveObjective && bTrackedEntry ? CategoryColor : bOptionalObjective ? SubtitleColor : ObjectiveTitleColor)
			: (bSelected ? SelectedTitleColor : TitleColor);
		if (bMuted && !bSelected)
		{
			AppliedTitleColor.A *= FMath::IsFinite(MutedTitleOpacity) ? FMath::Clamp(MutedTitleOpacity, 0.f, 1.f) : 0.58f;
		}
		TitleText->SetColorAndOpacity(AppliedTitleColor);
	}
	if (SubtitleText)
	{
		FText AppliedSubtitle = Subtitle;
		if (bSemanticObjective && EntryObjectiveStatus == EAZ_QuestObjectiveStatus::Cancelled)
		{
			const FText Cancelled = NSLOCTEXT("CHALK", "ObjectiveCancelled", "Cancelled");
			AppliedSubtitle = AppliedSubtitle.IsEmpty() ? Cancelled : FText::Format(NSLOCTEXT("CHALK", "ObjectiveDetailPair", "{0} · {1}"), AppliedSubtitle, Cancelled);
		}
		if (bSemanticObjective && bOptionalObjective)
		{
			const FText Optional = NSLOCTEXT("CHALK", "OptionalObjective", "optional");
			AppliedSubtitle = AppliedSubtitle.IsEmpty() ? Optional : FText::Format(NSLOCTEXT("CHALK", "ObjectiveDetailPair", "{0} · {1}"), AppliedSubtitle, Optional);
		}
		SubtitleText->SetText(AppliedSubtitle);
		if (bUseObjectivePresentation && bHasPresentation)
		{
			SubtitleText->SetVisibility(bSemanticObjective && AppliedSubtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		SubtitleText->SetFont(SubtitleFont.FontObject ? SubtitleFont : FCoreStyle::GetDefaultFontStyle("Regular", 12));
		SubtitleText->SetColorAndOpacity(SubtitleColor);
	}
	if (EntryButton)
	{
		FButtonStyle Style = bSemanticObjective ? ObjectiveButtonStyle : (bOverrideRowButtonStyle ? RowButtonStyle : EntryButton->GetStyle());
		if (bSemanticObjective)
		{
			if (bSelected) Style.Normal.TintColor = FSlateColor(ObjectiveSelectedBackgroundColor);
		}
		else
		{
			Style.Normal.TintColor = FSlateColor(bSelected ? SelectedBackgroundColor : NormalBackgroundColor);
			Style.Hovered.TintColor = FSlateColor(HoveredBackgroundColor);
			Style.Pressed.TintColor = FSlateColor(PressedBackgroundColor);
		}
		EntryButton->SetStyle(Style);
	}
	if (EntryGlyph)
	{
		const FSlateBrush* Brush = nullptr;
		FLinearColor Color = CategoryColor;
		if (bSemanticObjective)
		{
			if (EntryObjectiveStatus == EAZ_QuestObjectiveStatus::Completed) { Brush = &CompleteGlyphBrush; Color = CompleteGlyphColor; }
			else if (EntryObjectiveStatus == EAZ_QuestObjectiveStatus::Failed) { Brush = &FailedGlyphBrush; Color = FailedGlyphColor; }
			else if (bActiveObjective) { Brush = EntryCategory == EAZ_QuestCategory::Side ? &SideGlyphBrush : &StoryGlyphBrush; }
		}
		const bool bHasGlyph = Brush && Brush->GetResourceObject();
		if (bHasGlyph) { EntryGlyph->SetBrush(*Brush); EntryGlyph->SetColorAndOpacity(Color); }
		EntryGlyph->SetVisibility(bHasGlyph ? ESlateVisibility::HitTestInvisible : bSemanticObjective ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed);
	}
	if (TrackingIndicator)
	{
		TrackingIndicator->SetBrushColor(SelectionIndicatorColor);
		TrackingIndicator->SetVisibility(bActiveObjective && bTrackedEntry ? ESlateVisibility::HitTestInvisible : bSemanticObjective ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed);
	}
	if (SelectionIndicator)
	{
		SelectionIndicator->SetBrushColor(SelectionIndicatorColor);
		// Hidden retains an authored margin's geometry, avoiding selection reflow.
		SelectionIndicator->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
