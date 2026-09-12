#pragma once

#include "AZ_GameplayTags.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"

/** Consistent inventory fragment roles for cards and focused details. */
namespace AZQuickSelectPresentation
{
	inline const FAZ_Inv_CommonUI_Text_Fragment* FindItemName(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
	{
		const auto& Tags = FAZ_GameplayTags::Get();
		for (const FGameplayTag& Tag : {Tags.Item_Fragment_Name_StaticText, Tags.Item_Fragment_Name, Tags.Item_Fragment_Ammo_Primary_Name})
			if (const auto* Name = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tag)) return Name;
		return nullptr;
	}

	inline const FAZ_Inv_CommonUI_Text_Fragment* FindItemDescription(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
	{
		const auto& Tags = FAZ_GameplayTags::Get();
		if (const auto* Description = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Description)) return Description;
		return Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Text);
	}
}
