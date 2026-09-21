#include "UI/AZ_QuestMapPage.h"
#include "UI/AZ_MapCanvasWidget.h"
#include "UI/AZ_QuestJournalEntry.h"
#include "UI/AZ_QuestMapComponent.h"
#include "Quests/AZ_QuestDefinition.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UAZ_QuestMapPage::NativeConstruct()
{
	Super::NativeConstruct();
	if (MapCanvas) MapCanvas->OnObjectiveSelected.AddUniqueDynamic(this, &ThisClass::SelectObjective);
	if (TrackButton) TrackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::TrackSelection);
	if (ClearWaypointButton) ClearWaypointButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClearPersonalWaypoint);
	if (RecenterButton) RecenterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RecenterMap);
	if (InventoryButton) InventoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::BackToInventory);
}

void UAZ_QuestMapPage::NativeDestruct()
{
	UnbindNavigation();
	if (MapCanvas) MapCanvas->OnObjectiveSelected.RemoveDynamic(this, &ThisClass::SelectObjective);
	if (TrackButton) TrackButton->OnClicked.RemoveDynamic(this, &ThisClass::TrackSelection);
	if (ClearWaypointButton) ClearWaypointButton->OnClicked.RemoveDynamic(this, &ThisClass::ClearPersonalWaypoint);
	if (RecenterButton) RecenterButton->OnClicked.RemoveDynamic(this, &ThisClass::RecenterMap);
	if (InventoryButton) InventoryButton->OnClicked.RemoveDynamic(this, &ThisClass::BackToInventory);
	Super::NativeDestruct();
}

void UAZ_QuestMapPage::NativeOnActivated()
{
	Super::NativeOnActivated();
	BindNavigation();
	RefreshJournal();
}

void UAZ_QuestMapPage::NativeOnDeactivated()
{
	UnbindNavigation();
	Super::NativeOnDeactivated();
}

void UAZ_QuestMapPage::BindNavigation()
{
	UnbindNavigation();
	QuestNavigation = UAZ_QuestMapComponent::GetOrCreateForController(GetOwningPlayer());
	if (!QuestNavigation) return;
	QuestNavigation->RefreshBindings();
	if (DefaultMapDefinition && !QuestNavigation->GetMapDefinition()) QuestNavigation->SetMapDefinition(DefaultMapDefinition);
	QuestNavigation->OnViewChanged.AddUniqueDynamic(this, &ThisClass::RefreshJournal);
	if (MapCanvas) MapCanvas->InitializeNavigation(QuestNavigation);
}

void UAZ_QuestMapPage::UnbindNavigation()
{
	if (QuestNavigation) QuestNavigation->OnViewChanged.RemoveDynamic(this, &ThisClass::RefreshJournal);
	if (MapCanvas) MapCanvas->ResetNavigation();
	QuestNavigation = nullptr;
}

void UAZ_QuestMapPage::RefreshJournal()
{
	if (!QuestList) return;
	const float Scroll = QuestList->GetScrollOffset();
	FName FocusedQuestId, FocusedObjectiveId;
	bool bRestoreRowFocus = false;
	for (UWidget* Child : QuestList->GetAllChildren())
	{
		if (UAZ_QuestJournalEntry* Row = Cast<UAZ_QuestJournalEntry>(Child);
			Row && (Row->HasUserFocus(GetOwningPlayer()) || Row->HasUserFocusedDescendants(GetOwningPlayer())))
		{
			FocusedQuestId = Row->GetQuestId();
			FocusedObjectiveId = Row->GetObjectiveId();
			bRestoreRowFocus = true;
			break;
		}
	}
	QuestList->ClearChildren();
	UAZ_QuestProgressComponent* Progress = QuestNavigation ? QuestNavigation->GetQuestProgress() : nullptr;
	if (!Progress)
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("CHALK", "JournalUnavailable", "Journal unavailable"));
		RefreshSelection();
		return;
	}
	const auto Records = Progress->GetQuestRecords();
	if (!Records.ContainsByPredicate([this](const FAZ_QuestProgressRecord& Record) { return Record.QuestId == SelectedQuestId; }))
	{
		SelectedQuestId = NAME_None;
		SelectedObjectiveId = NAME_None;
	}
	if (SelectedQuestId.IsNone())
	{
		SelectedQuestId = Progress->GetTrackedQuestId();
		SelectedObjectiveId = Progress->GetTrackedObjectiveId();
	}
	const TSubclassOf<UAZ_QuestJournalEntry> EntryClass = JournalEntryClass ? JournalEntryClass : TSubclassOf<UAZ_QuestJournalEntry>(UAZ_QuestJournalEntry::StaticClass());
	int32 Count = 0;
	for (int32 Group = 0; Group < 3; ++Group)
	{
		bool bHeadingAdded = false;
		for (const FAZ_QuestProgressRecord& Record : Records)
		{
			UAZ_QuestDefinition* Definition = Progress->FindQuestDefinition(Record.QuestId);
			if (!Definition) continue;
			const bool bTerminal = Record.Status == EAZ_QuestStatus::Completed || Record.Status == EAZ_QuestStatus::Failed || Record.Status == EAZ_QuestStatus::Cancelled;
			const int32 ActualGroup = bTerminal ? 2 : Definition->Category == EAZ_QuestCategory::Story ? 0 : 1;
			if (ActualGroup != Group) continue;
			if (!bHeadingAdded)
			{
				UTextBlock* Heading = NewObject<UTextBlock>(this);
				Heading->SetFont(SectionHeadingFont.FontObject ? SectionHeadingFont : FCoreStyle::GetDefaultFontStyle("Regular", 13));
				Heading->SetColorAndOpacity(SectionHeadingColor);
				Heading->SetText(Group == 0 ? NSLOCTEXT("CHALK", "StoryQuests", "STORY") : Group == 1 ? NSLOCTEXT("CHALK", "SideQuests", "SIDE QUESTS") : NSLOCTEXT("CHALK", "QuestArchive", "ARCHIVE"));
				QuestList->AddChild(Heading);
				bHeadingAdded = true;
			}
			UAZ_QuestJournalEntry* Entry = CreateWidget<UAZ_QuestJournalEntry>(GetOwningPlayer(), EntryClass);
			if (!Entry) continue;
			const FText State = Record.Status == EAZ_QuestStatus::Completed ? NSLOCTEXT("CHALK", "QuestCompleted", "Completed") :
				Record.Status == EAZ_QuestStatus::Failed ? NSLOCTEXT("CHALK", "QuestFailed", "Failed") :
				Record.Status == EAZ_QuestStatus::Cancelled ? NSLOCTEXT("CHALK", "QuestCancelled", "Cancelled") :
				Progress->GetTrackedQuestId() == Record.QuestId ? NSLOCTEXT("CHALK", "QuestTracked", "Tracked") : NSLOCTEXT("CHALK", "QuestActive", "Active");
			Entry->SetEntry(Record.QuestId, NAME_None, Definition->Title, State, SelectedQuestId == Record.QuestId, bTerminal);
			Entry->OnPicked.AddUniqueDynamic(this, &ThisClass::SelectObjective);
			QuestList->AddChild(Entry);
			++Count;
			if (SelectedQuestId == Record.QuestId)
			{
				// Spatial and nonspatial objectives must both be selectable; map pins alone cannot provide this.
				for (const FAZ_QuestObjectiveProgress& Objective : Record.Objectives)
				{
					if (Objective.Status == EAZ_QuestObjectiveStatus::Locked) continue;
					const FAZ_QuestObjectiveDefinition* Spec = Definition->FindObjective(Objective.ObjectiveId);
					if (!Spec) continue;
					UAZ_QuestJournalEntry* ObjectiveEntry = CreateWidget<UAZ_QuestJournalEntry>(GetOwningPlayer(), EntryClass);
					if (!ObjectiveEntry) continue;
					const FText ProgressText = FText::Format(NSLOCTEXT("CHALK", "ObjectiveCount", "{0} / {1}"),
						FText::AsNumber(Objective.CurrentCount), FText::AsNumber(Spec->RequiredCount));
					ObjectiveEntry->SetEntry(Record.QuestId, Objective.ObjectiveId, Spec->Description, ProgressText,
						SelectedObjectiveId == Objective.ObjectiveId, bTerminal || Objective.Status != EAZ_QuestObjectiveStatus::Active);
					ObjectiveEntry->OnPicked.AddUniqueDynamic(this, &ThisClass::SelectObjective);
					if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(QuestList->AddChild(ObjectiveEntry)))
					{
						ScrollBoxSlot->SetPadding(FMargin(12.0f, 2.0f, 0.0f, 2.0f));
					}
				}
			}
		}
	}
	QuestList->SetScrollOffset(Scroll);
	if (bRestoreRowFocus)
	{
		for (UWidget* Child : QuestList->GetAllChildren())
		{
			if (UAZ_QuestJournalEntry* Row = Cast<UAZ_QuestJournalEntry>(Child);
				Row && Row->GetQuestId() == FocusedQuestId && Row->GetObjectiveId() == FocusedObjectiveId)
			{
				Row->FocusEntry();
				break;
			}
		}
	}
	if (StatusText) StatusText->SetText(Count ? FText::GetEmpty() : NSLOCTEXT("CHALK", "NoKnownQuests", "No known tasks"));
	RefreshSelection();
}

void UAZ_QuestMapPage::SelectObjective(FName QuestId, FName ObjectiveId)
{
	UAZ_QuestProgressComponent* Progress = QuestNavigation ? QuestNavigation->GetQuestProgress() : nullptr;
	if (!Progress) return;
	const TArray<FAZ_QuestProgressRecord> Records = Progress->GetQuestRecords();
	const FAZ_QuestProgressRecord* Record = Records.FindByPredicate([QuestId](const auto& Item) { return Item.QuestId == QuestId; });
	if (!Record) return;
	if (ObjectiveId.IsNone())
	{
		const FAZ_QuestObjectiveProgress* FirstActive = Record->Objectives.FindByPredicate([](const auto& Item)
		{
			return Item.Status == EAZ_QuestObjectiveStatus::Active;
		});
		ObjectiveId = FirstActive ? FirstActive->ObjectiveId : NAME_None;
	}
	else if (!Record->Objectives.ContainsByPredicate([ObjectiveId](const auto& Item)
	{
		return Item.ObjectiveId == ObjectiveId && Item.Status != EAZ_QuestObjectiveStatus::Locked;
	})) return;
	SelectedQuestId = QuestId;
	SelectedObjectiveId = ObjectiveId;
	RefreshJournal();
}

void UAZ_QuestMapPage::RefreshSelection()
{
	UAZ_QuestProgressComponent* Progress = QuestNavigation ? QuestNavigation->GetQuestProgress() : nullptr;
	UAZ_QuestDefinition* Definition = Progress ? Progress->FindQuestDefinition(SelectedQuestId) : nullptr;
	if (QuestTitle) QuestTitle->SetText(Definition ? Definition->Title : NSLOCTEXT("CHALK", "ChooseQuest", "Choose a task"));
	FText Details = Definition ? Definition->Description : FText::GetEmpty();
	bool bCanTrack = false;
	if (Definition && Progress)
	{
		for (const FAZ_QuestProgressRecord& Record : Progress->GetQuestRecords())
		{
			if (Record.QuestId != SelectedQuestId) continue;
			FString Text = Details.ToString();
			for (const FAZ_QuestObjectiveProgress& Objective : Record.Objectives)
			{
				if (Objective.Status == EAZ_QuestObjectiveStatus::Locked) continue;
				const FAZ_QuestObjectiveDefinition* Spec = Definition->FindObjective(Objective.ObjectiveId);
				if (!Spec) continue;
				Text += TEXT("\n\n") + Spec->Description.ToString();
				if (Spec->RequiredCount > 1) Text += FString::Printf(TEXT("  %d / %d"), Objective.CurrentCount, Spec->RequiredCount);
				if (Spec->bOptional) Text += TEXT("  (") + NSLOCTEXT("CHALK", "OptionalObjective", "optional").ToString() + TEXT(")");
				if (SelectedObjectiveId.IsNone() && Record.Status == EAZ_QuestStatus::Active && Objective.Status == EAZ_QuestObjectiveStatus::Active)
				{
					SelectedObjectiveId = Objective.ObjectiveId;
				}
				bCanTrack |= Record.Status == EAZ_QuestStatus::Active && Objective.Status == EAZ_QuestObjectiveStatus::Active
					&& Objective.ObjectiveId == SelectedObjectiveId;
			}
			Details = FText::FromString(Text);
			break;
		}
	}
	if (QuestDescription) QuestDescription->SetText(Details);
	if (MapCanvas) MapCanvas->SetSelectedObjective(SelectedQuestId, SelectedObjectiveId);
	if (TrackButton) TrackButton->SetIsEnabled(bCanTrack);
	if (ClearWaypointButton) ClearWaypointButton->SetIsEnabled(QuestNavigation && QuestNavigation->GetWaypoint().bActive);
}

void UAZ_QuestMapPage::TrackSelection()
{
	UAZ_QuestProgressComponent* Progress = QuestNavigation ? QuestNavigation->GetQuestProgress() : nullptr;
	RefreshSelection();
	if (SelectedQuestId.IsNone() || SelectedObjectiveId.IsNone()) return;
	if (Progress && Progress->SetTrackedObjective(SelectedQuestId, SelectedObjectiveId)) RefreshJournal();
}

void UAZ_QuestMapPage::ClearPersonalWaypoint() { if (QuestNavigation) QuestNavigation->ClearWaypoint(); }
void UAZ_QuestMapPage::RecenterMap() { if (MapCanvas) MapCanvas->RecenterPlayer(); }
void UAZ_QuestMapPage::BackToInventory() { OnBackToInventory.Broadcast(); }
