// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/AZ_Inv_CommonUI_ItemDescription.h"
#include "AZ_GameplayTags.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Text.h"

void UAZ_Inv_CommonUI_ItemDescription::ShowItem(UAZ_Inv_CommonUI_InventoryItem* Item, const UAZ_Inv_CommonUI_InventoryComponent* Inventory)
{
	SetVisibility(IsValid(Item) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!IsValid(Item)) return;
	const auto& Manifest = Item->GetItemManifest();
	Manifest.AssimilateInventoryFragments(this);
	const auto* Weapon = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	if (!Item->IsMagazine() && (!Weapon || !Weapon->bUsesDetachableMagazines)) return;

	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const UAZ_Inv_CommonUI_InventoryItem* Magazine = Item->IsMagazine() ? Item
		: (Inventory ? Inventory->FindItemById(Item->GetInsertedMagazineId()) : nullptr);
	FText StateText = NSLOCTEXT("AZ_Inventory", "NoMagazine", "NO MAG");
	if (IsValid(Magazine))
	{
		StateText = FText::Format(Magazine->GetMagazineRounds() == 0
			? NSLOCTEXT("AZ_Inventory", "EmptyMagazine", "EMPTY - {0}/{1} rounds")
			: NSLOCTEXT("AZ_Inventory", "MagazineRounds", "Magazine: {0}/{1} rounds"),
			FText::AsNumber(Magazine->GetMagazineRounds()), FText::AsNumber(Magazine->GetMagazineCapacity()));
	}
	const auto* Description = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Description);
	const FText DescriptionText = Description && !Description->GetText().IsEmpty()
		? FText::Format(NSLOCTEXT("AZ_Inventory", "DescriptionWithMagazine", "{0}\n{1}"), Description->GetText(), StateText)
		: StateText;
	ApplyFunction([&](UAZ_Inv_CommonUI_CompositeBaseWidget* Widget)
	{
		const FGameplayTag Tag = Widget->GetFragmentTag();
		// Keep the authored ammo row and item title, replacing the old clip/reserve demo values.
		if (Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_Diff) || Tag.MatchesTag(Tags.Item_Fragment_MagazineSize))
		{
			Widget->Collapse();
			return;
		}
		if (auto* Text = Cast<UAZ_Inv_CommonUI_LeafWidget_Text>(Widget))
		{
			if (Tag.MatchesTagExact(Tags.Item_Fragment_Description) || Tag.MatchesTagExact(Tags.Item_Fragment_Text))
			{
				Text->SetText(DescriptionText);
				Text->Expand();
			}
			else if (Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_Value))
			{
				Text->SetText(IsValid(Magazine) ? FText::AsNumber(Magazine->GetMagazineRounds()) : StateText);
				Text->Expand();
			}
			else if (Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_MaxValue))
			{
				Text->SetText(IsValid(Magazine) ? FText::AsNumber(Magazine->GetMagazineCapacity()) : FText::GetEmpty());
				Text->SetVisibility(IsValid(Magazine) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
			else if (!IsValid(Magazine) && (Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_Separator)
				|| Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_StaticText)))
			{
				Text->Collapse();
			}
		}
	});
}

FVector2D UAZ_Inv_CommonUI_ItemDescription::GetBoxSize() const
{
	return SizeBox->GetDesiredSize();
}

void UAZ_Inv_CommonUI_ItemDescription::SetVisibility(ESlateVisibility InVisibility)
{
	for (auto Child : GetChildren())
	{
		Child->Collapse();
	}
	Super::SetVisibility(InVisibility);
}
