#include "Quests/AZ_QuestInventoryAdapter.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"

int32 UAZ_QuestInventoryAdapter::CountOwnedItems(APlayerController* Controller, FGameplayTag ItemType)
{
	const auto* Inventory = IsValid(Controller) ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	if (!Inventory || !ItemType.IsValid()) return 0;
	int64 Total = 0;
	TSet<FGuid> Counted;
	for (const auto* Item : Inventory->GetItems())
	{
		if (!IsValid(Item)) continue;
		if (Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || Item->GetParentItemId().IsValid() ||
			Counted.Contains(Item->GetInstanceId()) || !Item->GetItemManifest().GetItemTypeTag().MatchesTag(ItemType)) continue;
		Counted.Add(Item->GetInstanceId());
		Total += FMath::Max(0, Item->GetTotalStackCount());
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

bool UAZ_QuestInventoryAdapter::TryDeliverObjective(UAZ_QuestProgressComponent* Progress, FName QuestId,
	FName ObjectiveId, AActor* Recipient, FGuid ReceiptId, FString& OutError)
{
	const auto* State = IsValid(Progress) ? Cast<APlayerState>(Progress->GetOwner()) : nullptr;
	auto* Controller = State ? Cast<APlayerController>(State->GetOwner()) : nullptr;
	if (!State || !State->HasAuthority() || !Controller || !ReceiptId.IsValid())
	{ OutError = TEXT("Delivery requires authoritative player context and a receipt."); return false; }
	if (Progress->IsRestoreTransactionActive()) { OutError = TEXT("Campaign restore is active."); return false; }
	if (Progress->IsDeliveryReceiptCommitted(QuestId, ObjectiveId, ReceiptId)) return true;
	FGameplayTag ItemType;
	int32 Remaining = 0;
	if (!Progress->ValidateDelivery(QuestId, ObjectiveId, Recipient, ReceiptId, ItemType, Remaining, OutError)) return false;
	auto* Inventory = Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (!Inventory) { OutError = TEXT("Player inventory is unavailable."); return false; }
	// First slice requires the entire remaining delivery, not an implicit partial donation.
	if (!Inventory->TryConsumeForQuest(ItemType, Remaining,
		[&]() { return Progress->CommitDeliverySilent(QuestId, ObjectiveId, Remaining, ReceiptId); }, OutError)) return false;
	Progress->PublishProgressChanged();
	return true;
}
