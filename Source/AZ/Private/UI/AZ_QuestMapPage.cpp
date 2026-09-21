#include "UI/AZ_QuestMapPage.h"
#include "UI/AZ_MapCanvasWidget.h"
#include "UI/AZ_QuestJournalEntry.h"
#include "UI/AZ_QuestMapComponent.h"
#include "Quests/AZ_QuestDefinition.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "CommonUITypes.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Input/CommonUIInputTypes.h"
#include "Input/CommonUIActionRouterBase.h"

void UAZ_QuestMapPage::NativeConstruct()
{
	// The inventory owner returns from Map before closing the inventory. A second
	// automatic Back handler here would only deactivate the child page.
	bIsBackHandler = false;
	Super::NativeConstruct();
	if (MapCanvas) MapCanvas->OnObjectiveSelected.AddUniqueDynamic(this, &ThisClass::SelectObjective);
	if (TrackButton) TrackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::TrackSelection);
	if (ClearWaypointButton) ClearWaypointButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClearPersonalWaypoint);
	if (RecenterButton) RecenterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RecenterMap);
	if (InventoryButton) InventoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::BackToInventory);
}

void UAZ_QuestMapPage::NativeDestruct()
{
	UnbindMapInput();
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
	UnbindMapInput();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		BoundMapInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (BoundMapInput.IsValid()) BoundMapInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::RefreshMapActionBindings);
		MapPresentationInput = UCommonInputSubsystem::Get(LocalPlayer);
		if (MapPresentationInput.IsValid())
			MapInputChangedHandle = MapPresentationInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleMapInputMethodChanged);
	}
	RefreshMapActionBindings();
}

void UAZ_QuestMapPage::NativeOnDeactivated()
{
	UnbindMapInput();
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
				const FLinearColor HeadingColor = !bUseCategorySectionStyle || Group == 2 ? SectionHeadingColor : Group == 0 ? StorySectionColor : SideSectionColor;
				Heading->SetColorAndOpacity(HeadingColor);
				Heading->SetText(Group == 0 ? NSLOCTEXT("CHALK", "StoryQuests", "STORY") : Group == 1 ? NSLOCTEXT("CHALK", "SideQuests", "SIDE QUESTS") : NSLOCTEXT("CHALK", "QuestArchive", "ARCHIVE"));
				UWidget* Section = Heading;
				if (bUseCategorySectionStyle && Group != 2)
				{
					UHorizontalBox* HeadingRow = NewObject<UHorizontalBox>(this);
					const FSlateBrush& Brush = Group == 0 ? StorySectionBrush : SideSectionBrush;
					if (Brush.GetResourceObject())
					{
						UImage* Glyph = NewObject<UImage>(this);
						Glyph->SetBrush(Brush);
						Glyph->SetColorAndOpacity(HeadingColor);
						Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
						UHorizontalBoxSlot* GlyphSlot = HeadingRow->AddChildToHorizontalBox(Glyph);
						GlyphSlot->SetPadding(FMargin(0, 0, 12, 0));
						GlyphSlot->SetVerticalAlignment(VAlign_Center);
					}
					HeadingRow->AddChildToHorizontalBox(Heading)->SetVerticalAlignment(VAlign_Center);
					Section = HeadingRow;
				}
				if (UScrollBoxSlot* HeadingSlot = Cast<UScrollBoxSlot>(QuestList->AddChild(Section)); HeadingSlot && bUseCategorySectionStyle)
				{
					HeadingSlot->SetPadding(SectionHeadingPadding);
				}
				bHeadingAdded = true;
			}
			UAZ_QuestJournalEntry* Entry = CreateWidget<UAZ_QuestJournalEntry>(GetOwningPlayer(), EntryClass);
			if (!Entry) continue;
			const FText State = Record.Status == EAZ_QuestStatus::Completed ? NSLOCTEXT("CHALK", "QuestCompleted", "Completed") :
				Record.Status == EAZ_QuestStatus::Failed ? NSLOCTEXT("CHALK", "QuestFailed", "Failed") :
				Record.Status == EAZ_QuestStatus::Cancelled ? NSLOCTEXT("CHALK", "QuestCancelled", "Cancelled") :
				Progress->GetTrackedQuestId() == Record.QuestId ? NSLOCTEXT("CHALK", "QuestTracked", "Tracked") : NSLOCTEXT("CHALK", "QuestActive", "Active");
			Entry->SetEntry(Record.QuestId, NAME_None, Definition->Title, State, SelectedQuestId == Record.QuestId, bTerminal);
			Entry->SetPresentation(Definition->Category, Record.Status, EAZ_QuestObjectiveStatus::Locked, false,
				Progress->GetTrackedQuestId() == Record.QuestId);
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
					const FText ProgressText = Spec->RequiredCount > 1 || !ObjectiveEntry->bUseObjectivePresentation ? FText::Format(NSLOCTEXT("CHALK", "ObjectiveCount", "{0} / {1}"),
						FText::AsNumber(Objective.CurrentCount), FText::AsNumber(Spec->RequiredCount)) : FText::GetEmpty();
					ObjectiveEntry->SetEntry(Record.QuestId, Objective.ObjectiveId, Spec->Description, ProgressText,
						SelectedObjectiveId == Objective.ObjectiveId, bTerminal || Objective.Status != EAZ_QuestObjectiveStatus::Active);
					ObjectiveEntry->SetPresentation(Definition->Category, Record.Status, Objective.Status, Spec->bOptional,
						Progress->GetTrackedQuestId() == Record.QuestId && Progress->GetTrackedObjectiveId() == Objective.ObjectiveId);
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

FUIActionBindingHandle UAZ_QuestMapPage::GetMapActionBinding(const UInputAction* Action) const
{
	if (const FUIActionBindingHandle* Handle = MapActionBindings.Find(Action)) return *Handle;
	return FUIActionBindingHandle();
}

UWidget* UAZ_QuestMapPage::NativeGetDesiredFocusTarget() const
{
	return MapCanvas ? MapCanvas.Get() : Super::NativeGetDesiredFocusTarget();
}

bool UAZ_QuestMapPage::CanRouteMapCommand() const
{
	const APlayerController* Player = GetOwningPlayer();
	return IsActivated() && IsVisible() && Player && Player->IsLocalController()
		&& QuestNavigation && MapCanvas && MapCanvas->IsVisible();
}

bool UAZ_QuestMapPage::HasParentKeyConflict(const UInputAction* Action, const TArray<FKey>& Keys) const
{
	// Asset authoring supplies actual action identities. Never guess Back/Select
	// from hardcoded controller buttons or a similarly named vendor action.
	if (ReservedParentActions.IsEmpty()) return true;
	for (const UInputAction* Reserved : ReservedParentActions)
	{
		if (!Reserved || Reserved == Action) return true;
		TArray<FKey> ParentKeys;
		CommonUI::GetEnhancedInputActionKeys(GetOwningLocalPlayer(), Reserved, ParentKeys);
		for (const FKey& Key : Keys)
			if (ParentKeys.ContainsByPredicate([Key](const FKey& ParentKey) { return ParentKey.IsSameResolvedKey(Key); })) return true;
	}
	return false;
}

bool UAZ_QuestMapPage::HasSafeMapMappingContext() const
{
	if (!InputMapping) return false;
	const TSet<const UInputAction*> Allowed = { MapZoomInAction, MapZoomOutAction, MapRecenterAction,
		MapPlaceWaypointAction, MapClearWaypointAction, MapTrackSelectionAction };
	// Validate every default/profile mapping before the base class can install it.
	// These actions must not mask parent mappings during EnhancedInput's rebuild.
	TArray<FString> Profiles = InputMapping->GetProfilesWithOverridenMappings();
	Profiles.AddUnique(TEXT(""));
	for (const FString& Profile : Profiles)
	{
		for (const FEnhancedActionKeyMapping& Mapping : InputMapping->GetMappingsForProfile(Profile))
		{
			const UInputAction* Action = Mapping.Action;
			if (!Action || !Allowed.Contains(Action) || Action->bConsumeInput || Action->bConsumesActionAndAxisMappings
				|| Action->ValueType != EInputActionValueType::Boolean || !Action->Triggers.IsEmpty()
				|| !Mapping.Triggers.IsEmpty() || !Action->Modifiers.IsEmpty() || !Mapping.Modifiers.IsEmpty()) return false;
			const UCommonInputMetadata* Metadata = CommonUI::GetEnhancedInputActionMetadata(Action);
			if (Metadata && !Metadata->bIsGenericInputAction) return false;
		}
	}
	return true;
}

void UAZ_QuestMapPage::ActivateMappingContext()
{
	if (!HasSafeMapMappingContext())
	{
		UE_LOG(LogTemp, Warning, TEXT("Map InputMapping must contain only its non-consuming Boolean UI commands; parent mappings are preserved."));
		return;
	}
	Super::ActivateMappingContext();
	bMapMappingContextActive = true;
}

void UAZ_QuestMapPage::DeactivateMappingContext()
{
	if (bMapMappingContextActive) Super::DeactivateMappingContext();
	bMapMappingContextActive = false;
}

void UAZ_QuestMapPage::RefreshMapActionBindings()
{
	UnregisterMapActions();
	if (!HasSafeMapMappingContext())
	{
		// A bad context must not stay installed while commands are refused.
		DeactivateMappingContext();
		return;
	}
	if (!bMapMappingContextActive || !CanRouteMapCommand() || !GetOwningLocalPlayer()) return;
	TArray<FKey> ClaimedKeys;
	const auto Bind = [this, &ClaimedKeys](const UInputAction* Action, void (ThisClass::*Callback)())
	{
		if (!Action) return;
		const UCommonInputMetadata* Metadata = CommonUI::GetEnhancedInputActionMetadata(Action);
		if (!ensureMsgf(Action->ValueType == EInputActionValueType::Boolean
			&& !Action->bConsumeInput && !Action->bConsumesActionAndAxisMappings
			&& (!Metadata || Metadata->bIsGenericInputAction) && !MapActionBindings.Contains(Action),
			TEXT("Map commands require unique non-consuming Boolean generic CommonUI actions: %s"), *GetNameSafe(Action))) return;
		TArray<FKey> Keys;
		CommonUI::GetEnhancedInputActionKeys(GetOwningLocalPlayer(), Action, Keys);
		// Activation can precede the mapping rebuild. Leave an unbound action
		// unregistered until the engine's existing mappings-rebuilt event arrives.
		if (Keys.IsEmpty()) return;
		if (HasParentKeyConflict(Action, Keys))
		{
			UE_LOG(LogTemp, Warning, TEXT("Map command conflicts with a reserved parent route: %s"), *GetNameSafe(Action));
			return;
		}
		for (const FKey& Key : Keys)
			if (ClaimedKeys.ContainsByPredicate([Key](const FKey& Existing) { return Existing.IsSameResolvedKey(Key); })) return;
		FBindUIActionArgs Args(Action, false, FSimpleDelegate::CreateUObject(this, Callback));
		// Consume at the active CommonUI owner, never at EnhancedInput mapping rebuild.
		Args.bConsumeInput = true;
		const FUIActionBindingHandle Handle = RegisterUIActionBinding(Args);
		if (Handle.IsValid()) { MapActionBindings.Add(Action, Handle); ClaimedKeys.Append(Keys); }
	};
	Bind(MapZoomInAction, &ThisClass::HandleMapZoomIn);
	Bind(MapZoomOutAction, &ThisClass::HandleMapZoomOut);
	Bind(MapRecenterAction, &ThisClass::HandleMapRecenter);
	Bind(MapPlaceWaypointAction, &ThisClass::HandleMapPlaceWaypoint);
	Bind(MapClearWaypointAction, &ThisClass::HandleMapClearWaypoint);
	Bind(MapTrackSelectionAction, &ThisClass::HandleMapTrackSelection);
	OnMapActionBindingsChanged();
}

void UAZ_QuestMapPage::UnregisterMapActions()
{
	ReleaseForwardedMapKeys();
	for (auto& Pair : MapActionBindings)
	{
		Pair.Value.Unregister();
		RemoveActionBinding(Pair.Value);
	}
	MapActionBindings.Reset();
	OnMapActionBindingsChanged();
}

void UAZ_QuestMapPage::ReleaseForwardedMapKeys()
{
	const auto Pressed = MoveTemp(ForwardedMapKeys);
	ForwardedMapKeys.Reset();
	for (const auto& Pair : Pressed)
		if (Pair.Value.IsValid()) Pair.Value->ProcessInput(Pair.Key, IE_Released);
}

void UAZ_QuestMapPage::UnbindMapInput()
{
	if (MapPresentationInput.IsValid()) MapPresentationInput->OnInputMethodChangedNative.Remove(MapInputChangedHandle);
	MapPresentationInput.Reset();
	MapInputChangedHandle.Reset();
	if (BoundMapInput.IsValid()) BoundMapInput->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::RefreshMapActionBindings);
	BoundMapInput.Reset();
	UnregisterMapActions();
}

void UAZ_QuestMapPage::HandleMapInputMethodChanged(ECommonInputType InputType)
{
	if (IsActivated()) OnMapActionBindingsChanged();
}

bool UAZ_QuestMapPage::IsMapCommandKey(FKey Key) const
{
	for (const auto& Pair : MapActionBindings)
	{
		if (!Pair.Value.IsValid()) continue;
		TArray<FKey> Keys;
		CommonUI::GetEnhancedInputActionKeys(GetOwningLocalPlayer(), Pair.Key, Keys);
		if (Keys.ContainsByPredicate([Key](const FKey& BoundKey) { return BoundKey.IsSameResolvedKey(Key); })) return true;
	}
	return false;
}

FReply UAZ_QuestMapPage::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (CanRouteMapCommand() && IsMapCommandKey(Event.GetKey()))
	{
		if (!Event.IsRepeat())
		{
			if (UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(*this))
			{
				if (!ForwardedMapKeys.Contains(Event.GetKey()))
				{
					ForwardedMapKeys.Add(Event.GetKey(), Router);
					Router->ProcessInput(Event.GetKey(), IE_Pressed);
				}
			}
		}
		return FReply::Handled();
	}
	// Existing canvas D-pad/keyboard pan, mouse input, journal focus, Select and
	// parent Back/tab commands keep their existing event paths.
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UAZ_QuestMapPage::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (TWeakObjectPtr<UCommonUIActionRouterBase>* Router = ForwardedMapKeys.Find(Event.GetKey()))
	{
		const TWeakObjectPtr<UCommonUIActionRouterBase> PressRouter = *Router;
		ForwardedMapKeys.Remove(Event.GetKey());
		if (PressRouter.IsValid()) PressRouter->ProcessInput(Event.GetKey(), IE_Released);
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(Geometry, Event);
}

void UAZ_QuestMapPage::HandleMapZoomIn() { if (CanRouteMapCommand()) MapCanvas->ZoomAtCenter(1.2); }
void UAZ_QuestMapPage::HandleMapZoomOut() { if (CanRouteMapCommand()) MapCanvas->ZoomAtCenter(1.0 / 1.2); }
void UAZ_QuestMapPage::HandleMapRecenter() { if (CanRouteMapCommand()) RecenterMap(); }
// The personal marker lands on the pointer, which is the whole point of having one; with no pointer showing
// this still resolves to the view centre, so the mouse and keyboard paths are unchanged.
void UAZ_QuestMapPage::HandleMapPlaceWaypoint() { if (CanRouteMapCommand()) MapCanvas->PlaceWaypointAtPointer(); }
void UAZ_QuestMapPage::HandleMapClearWaypoint() { if (CanRouteMapCommand()) ClearPersonalWaypoint(); }
void UAZ_QuestMapPage::HandleMapTrackSelection() { if (CanRouteMapCommand()) TrackSelection(); }
