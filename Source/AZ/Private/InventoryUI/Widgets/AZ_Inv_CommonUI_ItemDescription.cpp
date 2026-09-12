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
	const bool bMagazineItem = Item->IsMagazine();
	const UAZ_Inv_CommonUI_InventoryItem* Magazine = bMagazineItem ? Item
		: (Inventory ? Inventory->FindItemById(Item->GetInsertedMagazineId()) : nullptr);
	if (IsValid(Magazine) && (!Magazine->IsMagazine() || Magazine->GetMagazineCapacity() <= 0
		|| Magazine->GetMagazineRounds() < 0 || Magazine->GetMagazineRounds() > Magazine->GetMagazineCapacity()
		|| (!bMagazineItem && (Magazine->GetParentItemId() != Item->GetInstanceId()
			|| Magazine->GetLocation() != EAZ_InventoryItemLocation::WeaponMagazine)))) Magazine = nullptr;
	const bool bNoMagazine = !bMagazineItem && !Item->GetInsertedMagazineId().IsValid();
	const FText AmmoStateText = bNoMagazine ? NSLOCTEXT("AZ_Inventory", "NoMagazine", "NO MAG")
		: NSLOCTEXT("AZ_Inventory", "MagazineReadingUnavailable", "--");
	FText StateText = bNoMagazine ? NSLOCTEXT("AZ_Inventory", "NoInsertedMagazineDescription", "No magazine inserted.")
		: NSLOCTEXT("AZ_Inventory", "MagazineUnavailableDescription", "Magazine unavailable.");
	const auto ItemName = [&Tags](const UAZ_Inv_CommonUI_InventoryItem* NamedItem, const FText& Fallback)
	{
		if (!IsValid(NamedItem)) return Fallback;
		const auto& NamedManifest = NamedItem->GetItemManifest();
		const auto* Name = NamedManifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Name_StaticText);
		if (!Name) Name = NamedManifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Name);
		return Name && !Name->GetText().IsEmpty() ? Name->GetText() : Fallback;
	};
	if (IsValid(Magazine))
	{
		StateText = FText::Format(Magazine->GetMagazineRounds() == 0
			? NSLOCTEXT("AZ_Inventory", "EmptyMagazine", "EMPTY - {0}/{1} rounds")
			: NSLOCTEXT("AZ_Inventory", "CurrentMagazineRounds", "{0}/{1} rounds"),
			FText::AsNumber(Magazine->GetMagazineRounds()), FText::AsNumber(Magazine->GetMagazineCapacity()));
		if (!bMagazineItem)
		{
			// The inserted item is linked beneath the rifle rather than duplicated in backpack cells.
			StateText = FText::Format(NSLOCTEXT("AZ_Inventory", "RifleInsertedMagazine", "Inserted magazine: {0}\n{1}"),
				ItemName(Magazine, NSLOCTEXT("AZ_Inventory", "MagazineNameFallback", "Magazine")), StateText);
		}
		else if (Magazine->GetLocation() == EAZ_InventoryItemLocation::WeaponMagazine)
		{
			const UAZ_Inv_CommonUI_InventoryItem* Rifle = Inventory ? Inventory->FindItemById(Magazine->GetParentItemId()) : nullptr;
			if (IsValid(Rifle) && Rifle->IsWeapon() && Rifle->GetInsertedMagazineId() == Magazine->GetInstanceId())
			{
				StateText = FText::Format(NSLOCTEXT("AZ_Inventory", "MagazineInsertedInRifle", "{0}\nInserted in {1}"),
					StateText, ItemName(Rifle, NSLOCTEXT("AZ_Inventory", "WeaponNameFallback", "weapon")));
			}
		}
		else if (Magazine->GetLocation() == EAZ_InventoryItemLocation::Backpack)
		{
			StateText = FText::Format(NSLOCTEXT("AZ_Inventory", "MagazineInBackpack", "{0}\nIn backpack"), StateText);
		}
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
				Text->SetText(IsValid(Magazine) ? FText::AsNumber(Magazine->GetMagazineRounds()) : AmmoStateText);
				Text->Expand();
			}
			else if (Tag.MatchesTagExact(Tags.Item_Fragment_Ammo_Text))
			{
				Text->SetText(bMagazineItem ? NSLOCTEXT("AZ_Inventory", "MagazineRoundsLabel", "Rounds:")
					: NSLOCTEXT("AZ_Inventory", "InsertedMagazineLabel", "Inserted:"));
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
