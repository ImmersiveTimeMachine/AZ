// Copyright Artur. AZ project.

#include "UI/AZ_QuickSelectWidget.h"

#include "AZ_GameplayTags.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeWidget.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Text.h"
#include "Player/AZ_PlayerController.h"
#include "UI/AZ_QuickSelectComponent.h"
#include "UI/AZ_QuickSelectEntryWidget.h"
#include "UI/AZ_QuickSelectPresentation.h"

void UAZ_QuickSelectWidget::InitializeSelector(UAZ_QuickSelectComponent* Component)
{
	Selector = Component;
}

void UAZ_QuickSelectWidget::NativeConstruct()
{
	bIsBackHandler = true; // CommonUI registers its Back binding during Super::NativeConstruct.
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UAZ_QuickSelectWidget::NativeDestruct()
{
	if (Selector.IsValid() && CurrentView.State != EAZ_QuickSelectState::Closed) Selector->Close(false);
	for (const auto& Pair : EntryWidgets)
	{
		if (Pair.Value) Pair.Value->OnSlotHovered.RemoveAll(this);
	}
	EntryWidgets.Reset();
	Selector.Reset();
	Super::NativeDestruct();
}

void UAZ_QuickSelectWidget::NativeOnDeactivated()
{
	// Unexpected CommonUI takeover cancels; component-initiated closing publishes
	// Closed first, so it does not recursively submit another close or activation.
	if (Selector.IsValid() && CurrentView.State != EAZ_QuickSelectState::Closed) Selector->Close(false);
	Super::NativeOnDeactivated();
}

UWidget* UAZ_QuickSelectWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UAZ_QuickSelectWidget*>(this);
}

bool UAZ_QuickSelectWidget::NativeOnHandleBackAction()
{
	if (!CanRouteInput()) return false;
	Selector->Cancel();
	return true;
}

bool UAZ_QuickSelectWidget::CanRouteInput() const
{
	return IsActivated() && Selector.IsValid() && CurrentView.State != EAZ_QuickSelectState::Closed;
}

USizeBox* UAZ_QuickSelectWidget::FindHost(EAZ_QuickSlotPosition Position, int32 PositionOrdinal) const
{
	if (PositionOrdinal < 0 || PositionOrdinal > 1) return nullptr;
	switch (Position)
	{
	case EAZ_QuickSlotPosition::Center: return PositionOrdinal == 0 ? CenterSlot.Get() : nullptr;
	case EAZ_QuickSlotPosition::Left: return PositionOrdinal == 0 ? LeftSlot.Get() : LeftSlotSecond.Get();
	case EAZ_QuickSlotPosition::Right: return PositionOrdinal == 0 ? RightSlot.Get() : RightSlotSecond.Get();
	case EAZ_QuickSlotPosition::Up: return PositionOrdinal == 0 ? UpSlot.Get() : UpSlotSecond.Get();
	case EAZ_QuickSlotPosition::Down: return PositionOrdinal == 0 ? DownSlot.Get() : DownSlotSecond.Get();
	default: return nullptr;
	}
}

void UAZ_QuickSelectWidget::ApplyView(const FAZ_QuickSelectView& View)
{
	CurrentView = View;
	UpdateFocusDetails(View);
	if (ModeText)
	{
		ModeText->SetText(View.ModeText);
		ModeText->SetToolTipText(View.ModeActionText);
	}
	if (HintText) HintText->SetText(View.HintText);
	if (StatusText)
	{
		StatusText->SetText(View.StatusText);
		StatusText->SetVisibility(View.StatusText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	TSet<int32> VisibleSlots;
	TSet<USizeBox*> OccupiedHosts;
	for (const FAZ_QuickSelectEntryView& EntryView : View.Entries)
	{
		USizeBox* Host = FindHost(EntryView.Position, EntryView.PositionOrdinal);
		if (!Host || EntryView.SlotIndex == INDEX_NONE || OccupiedHosts.Contains(Host)) continue;
		const TSubclassOf<UAZ_QuickSelectEntryWidget> DesiredClass = EntryView.SlotIndex == 0
			&& CenterEntryWidgetClass ? CenterEntryWidgetClass : EntryWidgetClass;
		UAZ_QuickSelectEntryWidget* Entry = EntryWidgets.FindRef(EntryView.SlotIndex);
		if (Entry && Entry->GetClass() != DesiredClass.Get())
		{
			Entry->OnSlotHovered.RemoveAll(this);
			Entry->RemoveFromParent();
			EntryWidgets.Remove(EntryView.SlotIndex);
			Entry = nullptr;
		}
		if (!Entry && DesiredClass && GetOwningPlayer())
		{
			Entry = CreateWidget<UAZ_QuickSelectEntryWidget>(GetOwningPlayer(), DesiredClass);
			if (!Entry) continue;
			Entry->OnSlotHovered.AddUObject(this, &ThisClass::HandleSlotHovered);
			EntryWidgets.Add(EntryView.SlotIndex, Entry);
		}
		if (!Entry) continue;
		if (Host->GetContent() != Entry)
		{
			Entry->RemoveFromParent();
			Host->SetContent(Entry);
		}
		Entry->ApplyEntryView(EntryView);
		Host->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		OccupiedHosts.Add(Host);
		VisibleSlots.Add(EntryView.SlotIndex);
	}
	for (auto It = EntryWidgets.CreateIterator(); It; ++It)
	{
		if (VisibleSlots.Contains(It.Key())) continue;
		if (It.Value())
		{
			It.Value()->OnSlotHovered.RemoveAll(this);
			It.Value()->RemoveFromParent();
		}
		It.RemoveCurrent();
	}
	for (USizeBox* Host : {CenterSlot.Get(), LeftSlot.Get(), RightSlot.Get(), UpSlot.Get(), DownSlot.Get(),
		LeftSlotSecond.Get(), RightSlotSecond.Get(), UpSlotSecond.Get(), DownSlotSecond.Get()})
	{
		if (Host && !OccupiedHosts.Contains(Host)) Host->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_QuickSelectWidget::UpdateFocusDetails(const FAZ_QuickSelectView& View)
{
	const bool bCompositeItem = FocusDetails && IsValid(View.FocusItem) && View.FocusItem->IsInitialized();
	if (FocusNameText)
	{
		FocusNameText->SetText(bCompositeItem ? FText::GetEmpty() : View.FocusNameText);
		FocusNameText->SetVisibility(!bCompositeItem && !View.FocusNameText.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (FocusDescriptionText)
	{
		FocusDescriptionText->SetText(bCompositeItem ? FText::GetEmpty() : View.FocusDescriptionText);
		FocusDescriptionText->SetVisibility(!bCompositeItem && !View.FocusDescriptionText.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!FocusDetails) return;
	FocusDetails->Collapse();
	FocusDetails->SetVisibility(ESlateVisibility::Collapsed);
	if (!bCompositeItem) return;
	const auto& Manifest = View.FocusItem->GetItemManifest();
	const auto& Tags = FAZ_GameplayTags::Get();
	const auto* NameFragment = AZQuickSelectPresentation::FindItemName(Manifest);
	const auto* DescriptionFragment = AZQuickSelectPresentation::FindItemDescription(Manifest);
	const FGameplayTag NameTag = NameFragment ? NameFragment->GetFragmentTag() : Tags.Item_Fragment_Name;
	const FGameplayTag DescriptionTag = DescriptionFragment ? DescriptionFragment->GetFragmentTag() : Tags.Item_Fragment_Description;
	// These roles are explicit in the focused-details composite. Do not assign
	// every text leaf the name tag, which would duplicate the name as a description.
	FocusDetails->ApplyFunction([NameTag, DescriptionTag](UAZ_Inv_CommonUI_CompositeBaseWidget* Leaf)
	{
		if (!Cast<UAZ_Inv_CommonUI_LeafWidget_Text>(Leaf)) return;
		if (Leaf->GetFName() == TEXT("ItemName")) Leaf->SetFragmentTag(NameTag);
		else if (Leaf->GetFName() == TEXT("ItemDescription")) Leaf->SetFragmentTag(DescriptionTag);
	});
	Manifest.AssimilateInventoryFragments(FocusDetails);
	FocusDetails->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAZ_QuickSelectWidget::HandleSlotHovered(int32 SlotIndex)
{
	if (CanRouteInput()) Selector->HoverSlot(SlotIndex);
}

void UAZ_QuickSelectWidget::UpdateHoveredSlotAt(const FVector2D& ScreenPosition)
{
	if (!CanRouteInput() || CurrentView.State == EAZ_QuickSelectState::EditingAssignment) return;
	for (const auto& Pair : EntryWidgets)
	{
		if (Pair.Value && Pair.Value->IsVisible()
			&& Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			Selector->HoverSlot(Pair.Key);
			return;
		}
	}
	Selector->HoverSlot(INDEX_NONE);
}

FReply UAZ_QuickSelectWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!CanRouteInput()) return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	if (InKeyEvent.IsRepeat()) return FReply::Handled();
	const FKey Key = InKeyEvent.GetKey();
	if (Selector->IsInventoryKey(Key))
	{
		if (AAZ_PlayerController* Player = Cast<AAZ_PlayerController>(GetOwningPlayer())) Player->ToggleCommonUI_InventoryMenu();
	}
	else if (Selector->IsToggleKey(Key)) Selector->Close();
	else if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right) Selector->Cancel();
	else
	{
		const int32 SlotIndex = Selector->FindSlotForKey(Key);
		if (SlotIndex != INDEX_NONE && CurrentView.State == EAZ_QuickSelectState::Browsing && !CurrentView.bPending)
			Selector->ActivateSlot(SlotIndex);
	}
	// Other gameplay keys do not reach the pawn or trigger inventory actions while
	// the selector has focus. Number-key and toggle identity comes from live mappings.
	return FReply::Handled();
}

FReply UAZ_QuickSelectWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return CanRouteInput() ? FReply::Handled() : Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UAZ_QuickSelectWidget::HandlePointerPress(const FPointerEvent& Event)
{
	if (!CanRouteInput()) return FReply::Unhandled();
	UpdateHoveredSlotAt(Event.GetScreenSpacePosition());
	if (!CurrentView.bPending)
	{
		if (Event.GetEffectingButton() == EKeys::MiddleMouseButton) Selector->ToggleAssignment();
		else if ((Event.GetEffectingButton() == EKeys::RightMouseButton || Event.GetEffectingButton() == EKeys::LeftMouseButton)
			&& CurrentView.State == EAZ_QuickSelectState::Browsing) Selector->ActivateHovered();
	}
	// The component captures outgoing held RMB before closing, then requires a new
	// gameplay press. Handling this event alone would not suppress a held aim input.
	return FReply::Handled();
}

FReply UAZ_QuickSelectWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return CanRouteInput() ? HandlePointerPress(InMouseEvent) : Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAZ_QuickSelectWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Slate reports a fast second MMB press as a double-click rather than another
	// button-down. It still means the explicit second confirmation in this UI.
	return CanRouteInput() ? HandlePointerPress(InMouseEvent) : Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

FReply UAZ_QuickSelectWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return CanRouteInput() ? FReply::Handled() : Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UAZ_QuickSelectWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!CanRouteInput()) return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	if (CurrentView.State == EAZ_QuickSelectState::EditingAssignment && !CurrentView.bPending)
	{
		const float Delta = InMouseEvent.GetWheelDelta();
		if (FMath::IsFinite(Delta) && !FMath::IsNearlyZero(Delta)) Selector->CycleCandidate(Delta > 0.f ? 1 : -1);
	}
	return FReply::Handled(); // Outside assignment, wheel must not switch the equipped weapon.
}
