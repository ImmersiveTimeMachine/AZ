#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"

#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"
#include "GameplayTagsManager.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "Inventory/Items/HoverItem/AZ_Inv_HoverItem.h"
#include "Inventory/Widgets/Screens/AZ_Inv_InventoryMenuBase.h"

UAZ_Inv_InventoryComponent* UAZ_Inv_InventoryStatics::GetInventoryComponent(const APlayerController* PlayerController)
{
	
	if (!IsValid(PlayerController)) return nullptr;

	UAZ_Inv_InventoryComponent* InventoryComponent = PlayerController->FindComponentByClass<UAZ_Inv_InventoryComponent>();
	return InventoryComponent;
}

UAZ_Inv_CommonUI_InventoryComponent* UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(const APlayerController* PlayerController)
{
	if (!IsValid(PlayerController)) return nullptr;

	UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = PlayerController->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	return InventoryComponent;
}

// -------------------------------------------------------------------------
// 🔹 UI Category ↔ Gameplay Tag Conversion
// -------------------------------------------------------------------------

FGameplayTag UAZ_Inv_InventoryStatics::GetCategoryTag(EInv_ItemCategory Category)
{
	// Return a representative root tag for the UI category (optional)
	switch (Category)
	{
	case EInv_ItemCategory::Equippable:
		return FGameplayTag::RequestGameplayTag("Item.Weapon"); // Representative
	case EInv_ItemCategory::Consumable:
		return FGameplayTag::RequestGameplayTag("Item.Consumable");
	case EInv_ItemCategory::Craftable:
		return FGameplayTag::RequestGameplayTag("Item.Crafting");
	default:
		return FGameplayTag();
	}
}

EInv_ItemCategory UAZ_Inv_InventoryStatics::GetCategoryFromTag(const FGameplayTag& ItemTag)
{
	// Map detailed item tags to UI categories
	if (ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Weapon")) ||
		ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Armor")) ||
		ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Attachment")) ||
		ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Accessory")))
	{
		return EInv_ItemCategory::Equippable;
	}

	if (ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Consumable")) ||
		ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Effect.Psych")) ||
		ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag("Item.Tool")))
	{
		return EInv_ItemCategory::Consumable;
	}

	// Everything else goes under Craftable for UI purposes
	return EInv_ItemCategory::Craftable;
}

EInv_ItemCategory UAZ_Inv_InventoryStatics::GetItemCategoryFromItemComp(const UAZ_Inv_ItemComponent* ItemComponent)
{
    if (!IsValid(ItemComponent)) return EInv_ItemCategory::None;

    return ItemComponent->GetItemManifest().GetItemCategory();
}

void UAZ_Inv_InventoryStatics::ItemHovered(APlayerController* PC, UAZ_Inv_InventoryItem* Item)
{
	UAZ_Inv_InventoryComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return;

	UAZ_Inv_InventoryMenuBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return;

	if (InventoryBase->HasHoverItem()) return;

	InventoryBase->OnItemHovered(Item);
}

void UAZ_Inv_InventoryStatics::ItemUnhovered(APlayerController* PC)
{
	UAZ_Inv_InventoryComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return;

	UAZ_Inv_InventoryMenuBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return;

	InventoryBase->OnItemUnHovered();
}

UAZ_Inv_HoverItem* UAZ_Inv_InventoryStatics::GetHoverItem(APlayerController* PC)
{
	UAZ_Inv_InventoryComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return nullptr;

	UAZ_Inv_InventoryMenuBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return nullptr;

	return InventoryBase->GetHoverItem();
}

UAZ_Inv_InventoryMenuBase* UAZ_Inv_InventoryStatics::GetInventoryWidget(const APlayerController* PlayerController)
{
	const UAZ_Inv_InventoryComponent* IC = GetInventoryComponent(PlayerController);
	if (!IsValid(IC)) return nullptr;

	return IC->GetInventoryMenu();
}
