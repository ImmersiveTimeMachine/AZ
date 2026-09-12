// Copyright Artur. AZ project.

#include "UI/AZ_QuickSelectEntryWidget.h"

#include "AZ_GameplayTags.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeWidget.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Image.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Text.h"
#include "UI/AZ_QuickSelectPresentation.h"

namespace
{
	float AssignmentBorderOpacity()
	{
		// UI time keeps the one-second pulse smooth through pauses and candidate
		// changes. Only the border pulses; item text and artwork stay steady.
		const float Phase = static_cast<float>(FMath::Fmod(FPlatformTime::Seconds(), 1.0));
		return .35f + .65f * (.5f + .5f * FMath::Cos(2.f * PI * Phase));
	}

	void SetOptionalText(UTextBlock* Widget, const FText& Text)
	{
		if (!Widget) return;
		Widget->SetText(Text);
		Widget->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UAZ_QuickSelectEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false); // The selector keeps keyboard focus while the pointer browses cards.
}

void UAZ_QuickSelectEntryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsDesignTime() && HighlightBorder && EntryView.bEditing)
	{
		HighlightBorder->SetRenderOpacity(AssignmentBorderOpacity());
	}
}

void UAZ_QuickSelectEntryWidget::ApplyEntryView(const FAZ_QuickSelectEntryView& View)
{
	EntryView = View;
	// Physical cards display the item name only, without a fixed slot/category heading.
	SetOptionalText(CategoryText, FText::GetEmpty());
	SetOptionalText(NameText, ItemDetails || View.bEmpty ? FText::GetEmpty() : View.DisplayName);
	SetOptionalText(AmmoText, View.bEmpty ? FText::GetEmpty() : View.AmmoText);
	SetOptionalText(KeyText, View.KeyText);
	SetOptionalText(StateText, View.bEditing ? FText::GetEmpty() : View.StateText);
	SetOptionalText(EmptyMarkText, View.bEmpty ? FText::FromString(TEXT("\u2014")) : FText::GetEmpty());
	if (ItemDetails)
	{
		// Collapse every leaf before applying another manifest so a missing fragment
		// cannot leave the previous candidate's name or icon visible.
		ItemDetails->Collapse();
		ItemDetails->SetVisibility(ESlateVisibility::Collapsed);
		if (!View.bEmpty && IsValid(View.Item) && View.Item->IsInitialized())
		{
			const auto& Manifest = View.Item->GetItemManifest();
			const auto& Tags = FAZ_GameplayTags::Get();
			const auto* NameFragment = AZQuickSelectPresentation::FindItemName(Manifest);
			const FGameplayTag NameTag = NameFragment ? NameFragment->GetFragmentTag() : Tags.Item_Fragment_Name;
			ItemDetails->ApplyFunction([NameTag, &Tags](UAZ_Inv_CommonUI_CompositeBaseWidget* Leaf)
			{
				if (Cast<UAZ_Inv_CommonUI_LeafWidget_Text>(Leaf)) Leaf->SetFragmentTag(NameTag);
				else if (Cast<UAZ_Inv_CommonUI_LeafWidget_Image>(Leaf)) Leaf->SetFragmentTag(Tags.Item_Fragment_Icon);
			});
			Manifest.AssimilateInventoryFragments(ItemDetails);
			ItemDetails->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	// Optional authored alternate visual in the mode card. It represents the
	// action offered by clicking, while bEquipped carries committed Fight state.
	UImage* ExploreGlyph = View.SlotIndex == 0 ? Cast<UImage>(GetWidgetFromName(TEXT("ExploreActionIcon"))) : nullptr;
	const bool bShowExplore = ExploreGlyph && View.bEquipped;
	if (ExploreGlyph) ExploreGlyph->SetVisibility(bShowExplore ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Icon)
	{
		UTexture2D* PresentedIcon = !ItemDetails && !View.bEmpty ? View.Icon.Get() : nullptr;
		Icon->SetBrushFromTexture(PresentedIcon, false);
		if (PresentedIcon)
		{
			const FVector2D Dimensions = FMath::IsFinite(View.IconDimensions.X) && FMath::IsFinite(View.IconDimensions.Y)
				&& View.IconDimensions.X > 0.f && View.IconDimensions.Y > 0.f ? View.IconDimensions
				: FVector2D(PresentedIcon->GetSizeX(), PresentedIcon->GetSizeY());
			// The authored ScaleBox fits this natural aspect ratio inside the card.
			Icon->SetDesiredSizeOverride(Dimensions);
		}
		Icon->SetVisibility(PresentedIcon && !bShowExplore ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HighlightBorder)
	{
		// Assignment confirmation/cancellation restores a steady border immediately.
		HighlightBorder->SetRenderOpacity(View.bEditing ? AssignmentBorderOpacity() : 1.f);
		HighlightBorder->SetBrushColor(View.bEditing || View.bPending ? EditingColor
			: View.bHovered ? HoveredColor : (View.bEquipped && View.SlotIndex != 0) || View.bReady ? EquippedColor : IdleColor);
	}
	// An empty/disabled activation target still accepts hover and assignment input.
	SetRenderOpacity(View.bAvailable || View.bEditing || View.bHovered || View.bPending
		? 1.f : FMath::Clamp(UnavailableOpacity, 0.f, 1.f));
	SetVisibility(ESlateVisibility::Visible);
	OnEntryViewChanged(View);
}

void UAZ_QuickSelectEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	OnSlotHovered.Broadcast(EntryView.SlotIndex);
}

void UAZ_QuickSelectEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnSlotHovered.Broadcast(INDEX_NONE);
}
