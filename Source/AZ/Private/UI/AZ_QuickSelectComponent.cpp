#include "UI/AZ_QuickSelectComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AZ_GameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AZ_PlayerController.h"
#include "UI/AZ_QuickSelectWidget.h"
#include "UI/AZ_QuickSelectPresentation.h"

#define LOCTEXT_NAMESPACE "AZQuickSelect"

namespace
{
	FGameplayTagContainer SelectorBlockedTags()
	{
		const auto& Tags = FAZ_GameplayTags::Get();
		return FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{Tags.Character_Dead, Tags.Character_Dying,
			Tags.Character_Stunned, Tags.State_Grabbed, Tags.State_Combat_Staggered,
			Tags.State_Combat_Grabbing, Tags.State_Combat_StruckPair});
	}
}

UAZ_QuickSelectComponent::UAZ_QuickSelectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAZ_QuickSelectComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshBindings();
	if (Controller.IsValid() && Controller->IsLocalController() && FSlateApplication::IsInitialized())
	{
		ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged()
			.AddUObject(this, &ThisClass::HandleApplicationActivation);
	}
}

void UAZ_QuickSelectComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	Close();
	Unbind();
	if (FSlateApplication::IsInitialized())
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
	if (Widget) Widget->RemoveFromParent();
	Widget = nullptr;
	Super::EndPlay(Reason);
}

void UAZ_QuickSelectComponent::Unbind()
{
	if (Controller.IsValid()) Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	if (QuickBar.IsValid())
	{
		QuickBar->OnBindingsChanged.RemoveDynamic(this, &ThisClass::HandleSourcesChanged);
		QuickBar->OnReadyItemChanged.RemoveDynamic(this, &ThisClass::HandleSourcesChanged);
		QuickBar->OnRequestResult.RemoveDynamic(this, &ThisClass::HandleRequestResult);
	}
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleSourcesChanged);
	if (Equipment.IsValid()) Equipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleSourcesChanged);
	if (BoundASC.IsValid())
	{
		BoundASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		for (const auto& Pair : GateHandles)
			BoundASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
	}
	GateHandles.Reset();
	HealthHandle.Reset();
	BoundASC.Reset();
	QuickBar.Reset();
	Inventory.Reset();
	Equipment.Reset();
}

void UAZ_QuickSelectComponent::RefreshBindings()
{
	Unbind();
	Controller = Cast<AAZ_PlayerController>(GetOwner());
	if (bEndingPlay || !Controller.IsValid() || !Controller->IsLocalController()) return;
	Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	QuickBar = Controller->FindComponentByClass<UAZ_QuickBarComponent>();
	Inventory = Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	Equipment = Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	BoundASC = Controller->GetAbilitySystemComponent();
	if (QuickBar.IsValid())
	{
		QuickBar->OnBindingsChanged.AddUniqueDynamic(this, &ThisClass::HandleSourcesChanged);
		QuickBar->OnReadyItemChanged.AddUniqueDynamic(this, &ThisClass::HandleSourcesChanged);
		QuickBar->OnRequestResult.AddUniqueDynamic(this, &ThisClass::HandleRequestResult);
	}
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleSourcesChanged);
	if (Equipment.IsValid()) Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleSourcesChanged);
	if (BoundASC.IsValid())
	{
		HealthHandle = BoundASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealth);
		for (const FGameplayTag& Tag : SelectorBlockedTags())
			GateHandles.Add(Tag, BoundASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &ThisClass::HandleGate));
	}
	HandleSourcesChanged();
}

bool UAZ_QuickSelectComponent::CanUseSelector() const
{
	if (bEndingPlay || !Controller.IsValid() || !Controller->IsLocalController()
		|| !IsValid(Controller->GetPawn()) || Controller->GetPawn()->IsActorBeingDestroyed()
		|| Controller->IsInventoryMenuOpen() || UGameplayStatics::IsGamePaused(this)
		|| !Inventory.IsValid() || Inventory->IsMenuOpen() || !QuickBar.IsValid() || !Equipment.IsValid()
		|| !BoundASC.IsValid() || BoundASC->HasAnyMatchingGameplayTags(SelectorBlockedTags())) return false;
	const UAZ_VitalsAttributeSet* Vitals = BoundASC->GetSet<UAZ_VitalsAttributeSet>();
	return Vitals && FMath::IsFinite(Vitals->GetHealth()) && Vitals->GetHealth() > 0.f;
}

void UAZ_QuickSelectComponent::Toggle()
{
	if (IsOpen()) Close();
	else Open();
}

void UAZ_QuickSelectComponent::Open()
{
	if (!CanUseSelector() || !WidgetClass) return;
	if (!Widget)
	{
		Widget = CreateWidget<UAZ_QuickSelectWidget>(Controller.Get(), WidgetClass);
		if (!Widget) return;
	}
	Widget->InitializeSelector(this);
	if (!Widget->IsInViewport()) Widget->AddToPlayerScreen(50);
	View = FAZ_QuickSelectView();
	View.State = EAZ_QuickSelectState::Browsing;
	View.HoveredSlot = QuickBar->GetReadySlotIndex() != INDEX_NONE
		? QuickBar->GetReadySlotIndex() : QuickBar->GetActiveSlotIndex();
	bPreviousCursorVisible = Controller->bShowMouseCursor;
	Controller->SetQuickSelectInputCaptured(true);
	// Capture can synchronously cancel gameplay and publish state changes.
	if (!IsOpen() || !CanUseSelector()) { Close(); return; }
	RebuildView();
	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->ActivateWidget();
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(InputMode);
	Controller->SetShowMouseCursor(true);
	Widget->SetUserFocus(Controller.Get());
}

void UAZ_QuickSelectComponent::Close(bool bRestoreGameplayInput)
{
	if (!IsOpen()) return;
	// Invalidate the draft/session before focus or input teardown emits callbacks.
	View = FAZ_QuickSelectView();
	Candidates.Reset();
	CandidateId.Invalidate();
	AssignmentRequestId.Invalidate();
	AcknowledgedRevision = INDEX_NONE;
	bCloseAfterActivation = false;
	if (Widget)
	{
		Widget->ApplyView(View);
		Widget->DeactivateWidget();
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Controller.IsValid())
	{
		Controller->SetQuickSelectInputCaptured(false);
		// Inventory established its focus before broadcasting its open event.
		if (bRestoreGameplayInput && !Controller->IsInventoryMenuOpen() && !UGameplayStatics::IsGamePaused(this) && !bEndingPlay)
		{
			Controller->SetInputMode(FInputModeGameOnly());
			Controller->SetShowMouseCursor(bPreviousCursorVisible);
		}
	}
}

void UAZ_QuickSelectComponent::Cancel()
{
	if (View.State == EAZ_QuickSelectState::EditingAssignment && !AssignmentRequestId.IsValid())
	{
		View.State = EAZ_QuickSelectState::Browsing;
		View.EditingSlot = INDEX_NONE;
		View.StatusText = FText::GetEmpty();
		CandidateId.Invalidate();
		Candidates.Reset();
		RebuildView();
	}
	else Close();
}

void UAZ_QuickSelectComponent::HoverSlot(int32 SlotIndex)
{
	if (View.State != EAZ_QuickSelectState::Browsing || !QuickBar.IsValid()) return;
	if (SlotIndex == INDEX_NONE)
	{
		if (View.HoveredSlot != INDEX_NONE) { View.HoveredSlot = INDEX_NONE; RebuildView(); }
		return;
	}
	const FAZ_QuickSlot* Slot = QuickBar->GetSlotDefinition(SlotIndex);
	if (!Slot || !Slot->bEnabled || View.HoveredSlot == SlotIndex) return;
	View.HoveredSlot = SlotIndex;
	View.StatusText = FText::GetEmpty();
	RebuildView();
}

void UAZ_QuickSelectComponent::RebuildCandidates(bool bChooseInitial)
{
	Candidates.Reset();
	if (!QuickBar.IsValid()) return;
	for (UAZ_Inv_CommonUI_InventoryItem* Item : QuickBar->GetCompatibleItems(View.EditingSlot))
		if (IsValid(Item) && Item->IsInitialized()) Candidates.Add(Item->GetInstanceId());
	// Stable ordering: an inventory notification must not reshuffle the wheel.
	Candidates.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
	if (!Candidates.Contains(CandidateId))
	{
		CandidateId.Invalidate();
		if (bChooseInitial && !Candidates.IsEmpty())
		{
			const FGuid BoundId = QuickBar->GetBoundItemId(View.EditingSlot);
			CandidateId = Candidates.Contains(BoundId) ? BoundId : Candidates[0];
		}
	}
}

void UAZ_QuickSelectComponent::ToggleAssignment()
{
	if (!IsOpen() || !CanUseSelector() || AssignmentRequestId.IsValid()) return;
	if (View.State == EAZ_QuickSelectState::Browsing)
	{
		const FAZ_QuickSlot* Slot = QuickBar->GetSlotDefinition(View.HoveredSlot);
		if (!Slot || !Slot->bEnabled) return;
		if (!Slot->bInventoryBacked)
		{
			ReportStatus(LOCTEXT("Intrinsic", "This slot is always available."));
			return;
		}
		View.State = EAZ_QuickSelectState::EditingAssignment;
		View.EditingSlot = View.HoveredSlot;
		EditingRevision = QuickBar->GetBindingRevision();
		CandidateId.Invalidate();
		View.StatusText = FText::GetEmpty();
		RebuildCandidates(true);
		RebuildView();
		return;
	}
	RebuildCandidates(false);
	if (!CandidateId.IsValid() || !Candidates.Contains(CandidateId))
	{
		ReportStatus(LOCTEXT("NoCandidate", "No available item selected."));
		return;
	}
	if (EditingRevision != QuickBar->GetBindingRevision())
	{
		// An explicit second click after this notice uses the refreshed revision.
		EditingRevision = QuickBar->GetBindingRevision();
		ReportStatus(LOCTEXT("BindingsChanged", "Loadout changed. Check the item and confirm again."));
		return;
	}
	AssignmentRequestId = FGuid::NewGuid();
	AcknowledgedRevision = INDEX_NONE;
	View.StatusText = LOCTEXT("Assigning", "Assigning...");
	RebuildView();
	QuickBar->RequestAssignItem(View.EditingSlot, CandidateId, EditingRevision, AssignmentRequestId);
}

void UAZ_QuickSelectComponent::CycleCandidate(int32 Direction)
{
	if (View.State != EAZ_QuickSelectState::EditingAssignment || AssignmentRequestId.IsValid() || Direction == 0) return;
	RebuildCandidates(false);
	if (Candidates.IsEmpty()) return;
	int32 Index = Candidates.IndexOfByKey(CandidateId);
	if (Index == INDEX_NONE) Index = Direction > 0 ? -1 : 0;
	Index = (Index + (Direction > 0 ? 1 : -1) + Candidates.Num()) % Candidates.Num();
	CandidateId = Candidates[Index];
	View.StatusText = FText::GetEmpty();
	RebuildView();
}

void UAZ_QuickSelectComponent::ActivateHovered() { ActivateSlot(View.HoveredSlot); }

void UAZ_QuickSelectComponent::ActivateSlot(int32 SlotIndex)
{
	if (View.State == EAZ_QuickSelectState::EditingAssignment || !CanUseSelector()) return;
	const FAZ_QuickSlot* Slot = QuickBar->GetSlotDefinition(SlotIndex);
	if (!Slot || !Slot->bEnabled) return;
	if (SlotIndex == 0)
	{
		ActivationRequestId = FGuid::NewGuid();
		bCloseAfterActivation = IsOpen();
		QuickBar->RequestToggleCombatMode(QuickBar->GetCombatModeRevision(), ActivationRequestId);
		return;
	}
	const FGuid ItemId = QuickBar->GetBoundItemId(SlotIndex);
	if (Slot->bInventoryBacked && (!ItemId.IsValid() || !IsValid(QuickBar->GetBoundItem(SlotIndex))))
	{
		ReportStatus(LOCTEXT("EmptySlot", "No item assigned."));
		return;
	}
	ActivationRequestId = FGuid::NewGuid();
	bCloseAfterActivation = IsOpen();
	QuickBar->RequestActivateSlot(SlotIndex, ItemId, QuickBar->GetBindingRevision(), ActivationRequestId);
}

void UAZ_QuickSelectComponent::FinishAssignment()
{
	AssignmentRequestId.Invalidate();
	AcknowledgedRevision = INDEX_NONE;
	View.State = EAZ_QuickSelectState::Browsing;
	View.EditingSlot = INDEX_NONE;
	View.StatusText = LOCTEXT("Assigned", "Assigned");
	Candidates.Reset();
	CandidateId.Invalidate();
}

void UAZ_QuickSelectComponent::HandleRequestResult(const FAZ_QuickBarRequestResult& Result)
{
	if (Result.RequestId == AssignmentRequestId)
	{
		if (Result.Outcome == EAZ_QuickBarRequestOutcome::Assigned)
		{
			AcknowledgedRevision = Result.BindingRevision;
			if (QuickBar.IsValid() && QuickBar->GetBindingRevision() >= AcknowledgedRevision) FinishAssignment();
		}
		else
		{
			AssignmentRequestId.Invalidate();
			AcknowledgedRevision = INDEX_NONE;
			EditingRevision = QuickBar.IsValid() ? QuickBar->GetBindingRevision() : 0;
			View.StatusText = Result.Reason;
		}
		RebuildView();
	}
	if (Result.RequestId == ActivationRequestId)
	{
		const bool bAccepted = Result.Outcome == EAZ_QuickBarRequestOutcome::Activated
			|| Result.Outcome == EAZ_QuickBarRequestOutcome::Deferred;
		if (bAccepted && bCloseAfterActivation) Close();
		if (Result.Outcome != EAZ_QuickBarRequestOutcome::Activated) ReportStatus(Result.Reason);
		if (Result.Outcome != EAZ_QuickBarRequestOutcome::Deferred) ActivationRequestId.Invalidate();
	}
}

void UAZ_QuickSelectComponent::HandleSourcesChanged()
{
	if (!IsOpen()) return;
	if (!CanUseSelector()) { Close(); return; }
	if (AssignmentRequestId.IsValid() && AcknowledgedRevision >= 0
		&& QuickBar->GetBindingRevision() >= AcknowledgedRevision) FinishAssignment();
	if (View.State == EAZ_QuickSelectState::EditingAssignment) RebuildCandidates(false);
	RebuildView();
}

void UAZ_QuickSelectComponent::RebuildView()
{
	View.Entries.Reset();
	View.FocusItem = nullptr;
	View.FocusNameText = FText::GetEmpty();
	View.FocusDescriptionText = FText::GetEmpty();
	View.bPending = AssignmentRequestId.IsValid();
	if (!IsOpen() || !QuickBar.IsValid()) return;
	View.ModeText = QuickBar->IsFightMode() ? LOCTEXT("FightMode", "FIGHT") : LOCTEXT("ExploreMode", "EXPLORE");
	View.ModeActionText = QuickBar->IsFightMode() ? LOCTEXT("LeaveFight", "Return to exploration") : LOCTEXT("EnterFight", "Enter fight mode");
	TMap<EAZ_QuickSlotPosition, int32> PositionCounts;
	const TArray<FKey> ToggleKeys = Controller.IsValid() ? KeysForAction(Controller->QuickSelectToggleAction) : TArray<FKey>();
	const FText ToggleLabel = ToggleKeys.IsEmpty() ? LOCTEXT("BackKey", "Esc") : ToggleKeys[0].GetDisplayName();
	View.HintText = View.State == EAZ_QuickSelectState::EditingAssignment
		? LOCTEXT("EditHint", "Wheel  Browse     MMB  Assign     Esc  Cancel")
		: FText::Format(LOCTEXT("BrowseHint", "Click  Select     MMB  Assign     {0}  Close"), ToggleLabel);
	for (int32 Index = 0; Index < QuickBar->GetSlotCount(); ++Index)
	{
		const FAZ_QuickSlot* Slot = QuickBar->GetSlotDefinition(Index);
		if (!Slot || !Slot->bEnabled) continue;
		FAZ_QuickSelectEntryView Entry;
		Entry.SlotIndex = Index;
		Entry.Position = Slot->Position;
		Entry.PositionOrdinal = PositionCounts.FindOrAdd(Slot->Position)++;
		// Physical slots have no category heading or placeholder weapon artwork.
		// Their actual item name/icon are assimilated by inventory composite leaves.
		if (!Slot->bInventoryBacked)
		{
			Entry.DisplayName = Slot->DisplayName;
			if (Index == 0) Entry.DisplayName = QuickBar->IsFightMode()
				? LOCTEXT("ExploreAction", "EXPLORE") : LOCTEXT("FightAction", "FIGHT");
			Entry.Icon = Slot->Icon;
			if (Entry.Icon) Entry.IconDimensions = FVector2D(Entry.Icon->GetSizeX(), Entry.Icon->GetSizeY());
		}
		Entry.KeyText = ShortcutText(Index);
		Entry.bHovered = View.HoveredSlot == Index;
		Entry.bEditing = View.State == EAZ_QuickSelectState::EditingAssignment && View.EditingSlot == Index;
		Entry.bPending = Entry.bEditing && View.bPending;
		Entry.ItemId = Entry.bEditing ? CandidateId : QuickBar->GetBoundItemId(Index);
		UAZ_Inv_CommonUI_InventoryItem* Item = Inventory.IsValid() ? Inventory->FindItemById(Entry.ItemId) : nullptr;
		Entry.bAssigned = !Slot->bInventoryBacked || QuickBar->GetBoundItemId(Index).IsValid();
		Entry.bEmpty = Slot->bInventoryBacked && !Entry.ItemId.IsValid();
		Entry.bAvailable = !Slot->bInventoryBacked || (IsValid(Item) && Item->IsInitialized()
			&& (Entry.bEditing ? Candidates.Contains(Entry.ItemId) : QuickBar->GetBoundItem(Index) == Item));
		Entry.bEquipped = Equipment.IsValid() && (Slot->bInventoryBacked
			? IsValid(Item) && Equipment->GetActiveItem() == Item
			: Equipment->GetActiveIntrinsicSlotIndex() == Index && Equipment->GetActiveItem() == nullptr);
		if (Index == 0) Entry.bEquipped = QuickBar->IsFightMode(); // Mode highlight, not a physical item marker.
		if (IsValid(Item) && Item->IsInitialized())
		{
			Entry.Item = Item;
			Entry.bReady = Item == QuickBar->GetReadyItem();
			const auto& Manifest = Item->GetItemManifest();
			const auto& Tags = FAZ_GameplayTags::Get();
			const auto* Name = AZQuickSelectPresentation::FindItemName(Manifest);
			if (Name) Entry.DisplayName = Name->GetText();
			const auto* Image = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_ImageFragment>(Tags.Item_Fragment_Icon);
			if (Image) { Entry.Icon = Image->GetIcon(); Entry.IconDimensions = Image->GetIconDimensions(); }
			const auto* Weapon = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
			if (Weapon && Weapon->bUsesDetachableMagazines)
			{
				const FAZ_WeaponAmmoSnapshot Ammo = Inventory->GetWeaponAmmoSnapshot(Entry.ItemId);
				Entry.AmmoText = Ammo.MagazineState == EAZ_WeaponMagazineState::Unavailable ? LOCTEXT("AmmoUnknown", "-- / --")
					: Ammo.MagazineState == EAZ_WeaponMagazineState::NoMagazine ? LOCTEXT("NoMag", "NO MAG")
					: FText::Format(LOCTEXT("Ammo", "{0} / {1}"), FText::AsNumber(Ammo.Rounds), FText::AsNumber(Ammo.Capacity));
			}
			else if (Item->IsConsumable())
			{
				Entry.AmmoText = FText::AsNumber(FMath::Max(0, Item->GetTotalStackCount()));
			}
		}
		// The editing border supplies assignment feedback without competing with
		// the live ammo/count row for space inside the compact card.
		if (Entry.bEditing || Index == 0) Entry.StateText = FText::GetEmpty();
		else if (Entry.bReady) Entry.StateText = LOCTEXT("Ready", "READY");
		else if (Entry.bEquipped) Entry.StateText = LOCTEXT("Equipped", "EQUIPPED");
		else if (!Entry.bAvailable) Entry.StateText = Entry.bAssigned ? LOCTEXT("Loading", "UNAVAILABLE") : LOCTEXT("Empty", "EMPTY");
		const int32 FocusSlot = View.State == EAZ_QuickSelectState::EditingAssignment ? View.EditingSlot : View.HoveredSlot;
		if (FocusSlot == Index)
		{
			if (Entry.Item && Entry.Item->IsInitialized())
			{
				View.FocusItem = Entry.Item;
				View.FocusNameText = Entry.DisplayName;
				const auto* Description = AZQuickSelectPresentation::FindItemDescription(Entry.Item->GetItemManifest());
				if (Description) View.FocusDescriptionText = Description->GetText();
			}
			else if (Index == 0)
			{
				View.FocusNameText = Entry.DisplayName;
				View.FocusDescriptionText = View.ModeActionText;
			}
			else if (Entry.bEmpty)
			{
				View.FocusNameText = LOCTEXT("EmptyFocus", "EMPTY SLOT");
				View.FocusDescriptionText = Entry.bEditing
					? LOCTEXT("NoAssignableItemsFocus", "No available inventory items for this slot.")
					: LOCTEXT("AssignFocus", "Middle-click to assign an inventory item.");
			}
		}
		View.Entries.Add(Entry);
	}
	if (Widget) Widget->ApplyView(View);
}

void UAZ_QuickSelectComponent::ReportStatus(const FText& Text)
{
	if (Text.IsEmpty()) return;
	if (IsOpen()) { View.StatusText = Text; RebuildView(); }
	else if (Controller.IsValid() && Controller->HUDWidget) Controller->HUDWidget->ShowTransientInfo(Text);
}

TArray<FKey> UAZ_QuickSelectComponent::KeysForAction(const UInputAction* Action) const
{
	if (Action && Controller.IsValid())
		if (const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
			return Subsystem->QueryKeysMappedToAction(Action);
	return {};
}

bool UAZ_QuickSelectComponent::IsToggleKey(FKey Key) const
{
	return Controller.IsValid() && KeysForAction(Controller->QuickSelectToggleAction).Contains(Key);
}

bool UAZ_QuickSelectComponent::IsInventoryKey(FKey Key) const
{
	return Controller.IsValid() && KeysForAction(Controller->OpenInventoryAction).Contains(Key);
}

int32 UAZ_QuickSelectComponent::FindSlotForKey(FKey Key) const
{
	if (Controller.IsValid())
		for (int32 Index = 0; Index < Controller->WeaponSlotActions.Num(); ++Index)
			if (KeysForAction(Controller->WeaponSlotActions[Index]).Contains(Key)) return Index;
	return INDEX_NONE;
}

FText UAZ_QuickSelectComponent::ShortcutText(int32 SlotIndex) const
{
	if (Controller.IsValid() && Controller->WeaponSlotActions.IsValidIndex(SlotIndex))
	{
		for (const FKey& Key : KeysForAction(Controller->WeaponSlotActions[SlotIndex]))
			if (!Key.IsGamepadKey()) return Key.GetDisplayName();
	}
	return FText::GetEmpty();
}

void UAZ_QuickSelectComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) { Close(); RefreshBindings(); }
void UAZ_QuickSelectComponent::HandleGate(FGameplayTag Tag, int32 Count) { if (IsOpen() && !CanUseSelector()) Close(); }
void UAZ_QuickSelectComponent::HandleHealth(const FOnAttributeChangeData& Change) { if (IsOpen() && !CanUseSelector()) Close(); }
void UAZ_QuickSelectComponent::HandleApplicationActivation(bool bActive) { if (!bActive) Close(); }
void UAZ_QuickSelectComponent::CheckPause() { if (IsOpen() && UGameplayStatics::IsGamePaused(this)) Close(); }

#undef LOCTEXT_NAMESPACE
