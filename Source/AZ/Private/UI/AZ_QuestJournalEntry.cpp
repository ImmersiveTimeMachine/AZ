#include "UI/AZ_QuestJournalEntry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
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
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		EntryButton->AddChild(Content);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot)) { ContentSlot->SetPadding(FMargin(14, 12)); ContentSlot->SetHorizontalAlignment(HAlign_Fill); }
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
	ApplyView();
}

void UAZ_QuestJournalEntry::HandlePicked() { OnPicked.Broadcast(QuestId, ObjectiveId); }

void UAZ_QuestJournalEntry::FocusEntry()
{
	if (EntryButton && GetOwningPlayer()) EntryButton->SetUserFocus(GetOwningPlayer());
}

void UAZ_QuestJournalEntry::ApplyView()
{
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetFont(TitleFont.FontObject ? TitleFont : FCoreStyle::GetDefaultFontStyle("Regular", 18));
		FLinearColor AppliedTitleColor = bSelected ? SelectedTitleColor : TitleColor;
		if (bMuted && !bSelected)
		{
			AppliedTitleColor.A *= FMath::IsFinite(MutedTitleOpacity) ? FMath::Clamp(MutedTitleOpacity, 0.f, 1.f) : 0.58f;
		}
		TitleText->SetColorAndOpacity(AppliedTitleColor);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
		SubtitleText->SetFont(SubtitleFont.FontObject ? SubtitleFont : FCoreStyle::GetDefaultFontStyle("Regular", 12));
		SubtitleText->SetColorAndOpacity(SubtitleColor);
	}
	if (EntryButton)
	{
		FButtonStyle Style = bOverrideRowButtonStyle ? RowButtonStyle : EntryButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(bSelected ? SelectedBackgroundColor : NormalBackgroundColor);
		Style.Hovered.TintColor = FSlateColor(HoveredBackgroundColor);
		Style.Pressed.TintColor = FSlateColor(PressedBackgroundColor);
		EntryButton->SetStyle(Style);
	}
	if (SelectionIndicator)
	{
		SelectionIndicator->SetBrushColor(SelectionIndicatorColor);
		// Hidden retains an authored margin's geometry, avoiding selection reflow.
		SelectionIndicator->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
