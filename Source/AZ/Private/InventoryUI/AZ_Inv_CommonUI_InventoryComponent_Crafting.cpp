#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"

#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_CraftRecipe.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "UObject/StrongObjectPtr.h"
#include "Throwables/AZ_ThrowableDefinition.h"
#include "Player/AZ_PlayerController.h"

namespace
{
constexpr int32 MaxCraftIngredients = 32;
constexpr int32 MaxCraftTools = 16;
constexpr int32 MaxCraftUnits = 10000;
// Receipts are never evicted during this component lifetime: evicting one would permit a delayed replay.
constexpr int32 MaxCraftReceipts = 4096;

bool IsSupportedCraftOutput(const FAZ_Inv_CommonUI_ItemManifest& Manifest, int32 Count)
{
	if (!Manifest.GetItemTypeTag().IsValid() ||
		(Manifest.GetItemCategory() != EInv_ItemCategory::Craftable &&
		 Manifest.GetItemCategory() != EInv_ItemCategory::Consumable &&
		 Manifest.GetItemCategory() != EInv_ItemCategory::Equippable) ||
		Count <= 0 || Count > MaxCraftUnits) return false;

	const auto* Grid = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_GridFragment>();
	const auto* Throwable = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_ThrowableFragment>();
	if (!Grid || Grid->GetGridSize().X <= 0 || Grid->GetGridSize().Y <= 0 ||
		!Throwable || !IsValid(Throwable->ThrowableDefinition)) return false;

	const auto* Stack = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
	if (Stack)
	{
		if (Stack->GetMaxStackSize() <= 0 || Count > Stack->GetMaxStackSize()) return false;
	}
	else if (Count != 1) return false;

	// Crafting publishes a new inventory item directly. Stateful equipment, weapons, grants and
	// arbitrary fragment side effects need their own creation path; this recipe type supports only
	// simple throwable presentation and stack data.
	auto Copy = Manifest;
	int32 GridFragments = 0;
	int32 ThrowableFragments = 0;
	int32 StackFragments = 0;
	for (const auto& Fragment : Copy.GetFragmentsMutable())
	{
		if (!Fragment.IsValid()) return false;
		const UScriptStruct* Type = Fragment.GetScriptStruct();
		GridFragments += Type == FAZ_Inv_CommonUI_GridFragment::StaticStruct();
		ThrowableFragments += Type == FAZ_Inv_CommonUI_ThrowableFragment::StaticStruct();
		StackFragments += Type == FAZ_Inv_CommonUI_Stackable_Fragment::StaticStruct();
		if (Type != FAZ_Inv_CommonUI_GridFragment::StaticStruct() &&
			Type != FAZ_Inv_CommonUI_Stackable_Fragment::StaticStruct() &&
			Type != FAZ_Inv_CommonUI_ImageFragment::StaticStruct() &&
			Type != FAZ_Inv_CommonUI_Text_Fragment::StaticStruct() &&
			Type != FAZ_Inv_CommonUI_ThrowableFragment::StaticStruct()) return false;
	}
	return GridFragments == 1 && ThrowableFragments == 1 && StackFragments <= 1;
}
}

bool UAZ_Inv_CommonUI_InventoryComponent::BuildCraftPlan(const UAZ_CraftRecipe* Recipe,
	TArray<TPair<UAZ_Inv_CommonUI_InventoryItem*, int32>>& OutConsumption,
	TArray<FAZ_InventoryGridPlacement>& OutPlacements, int32& OutOutputIndex, FString& OutError) const
{
	OutError.Reset();
	OutConsumption.Reset();
	OutPlacements.Reset();
	OutOutputIndex = INDEX_NONE;
	const auto Fail = [&OutError](const TCHAR* Message) { OutError = Message; return false; };
	if (!GetOwner() || !GetOwner()->HasAuthority()) return Fail(TEXT("Crafting requires inventory authority."));
	if (const auto* PC = Cast<AAZ_PlayerController>(GetOwner()); PC && PC->FindActiveThrow())
		return Fail(TEXT("Put away the readied throwable before crafting."));
	if (bMagazineReloadMutation || bCampaignRestorePrepared || bCampaignRestoreCommitted ||
		MagazineReload.ReloadId.IsValid() || ThrowReservation.ThrowActionId.IsValid())
		return Fail(TEXT("Finish the active inventory action before crafting."));
	if (const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>())
	{
		FString EquipmentError;
		if (!Equipment->CanCaptureCampaignState(EquipmentError))
			return Fail(TEXT("Finish the active equipment action before crafting."));
	}
	if (!IsValid(Recipe) || !CraftingRecipes.ContainsByPredicate(
		[Recipe](const TObjectPtr<UAZ_CraftRecipe>& Allowed) { return Allowed.Get() == Recipe; }))
		return Fail(TEXT("Recipe is unavailable."));
	if (Recipe->Ingredients.IsEmpty() || Recipe->Ingredients.Num() > MaxCraftIngredients ||
		Recipe->Tools.Num() > MaxCraftTools ||
		!IsSupportedCraftOutput(Recipe->OutputManifest, Recipe->OutputCount))
		return Fail(TEXT("Recipe contains an unsupported quantity or output."));

	TSet<FGameplayTag> IngredientTypes;
	for (const auto& Ingredient : Recipe->Ingredients)
	{
		if (!Ingredient.ItemType.IsValid() || Ingredient.Quantity <= 0 ||
			Ingredient.Quantity > MaxCraftUnits || IngredientTypes.Contains(Ingredient.ItemType))
			return Fail(TEXT("Recipe has an invalid or duplicate ingredient."));
		IngredientTypes.Add(Ingredient.ItemType);
	}
	for (const FGameplayTag& Tool : Recipe->Tools)
		if (!Tool.IsValid()) return Fail(TEXT("Recipe has an invalid tool type."));

	TArray<UAZ_Inv_CommonUI_InventoryItem*> Candidates = GetItems();
	if (Candidates.Num() > MaxCraftUnits || GridPlacements.Num() > MaxCraftUnits)
		return Fail(TEXT("Inventory exceeds crafting limits."));
	TSet<FGuid> SeenItemIds;
	for (const auto* Item : Candidates)
	{
		if (!IsValid(Item) || !Item->IsInitialized() || SeenItemIds.Contains(Item->GetInstanceId()))
			return Fail(TEXT("Inventory contains an invalid or duplicate item."));
		SeenItemIds.Add(Item->GetInstanceId());
	}
	Candidates.Sort([](const auto& A, const auto& B)
	{
		return A.GetInstanceId().ToString() < B.GetInstanceId().ToString();
	});
	const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	TMap<FGuid, int64> PlacedCounts;
	TMap<EInv_ItemCategory, TSet<int32>> OccupiedCells;
	for (const auto& Placement : GridPlacements)
	{
		const auto* PlacedItem = FindItemById(Placement.ItemId);
		if (!PlacedItem || PlacedItem->GetLocation() != EAZ_InventoryItemLocation::Backpack ||
			Placement.StackCount <= 0 || Placement.GridIndex < 0)
			return Fail(TEXT("Inventory grid contains an invalid placement."));
		const auto& Manifest = PlacedItem->GetItemManifest();
		const FIntPoint Dimensions = GetGridDimensions(Manifest.GetItemCategory());
		const auto* Grid = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_GridFragment>();
		const FIntPoint Size = Grid ? Grid->GetGridSize() : FIntPoint(1, 1);
		const auto* Stack = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
		const int32 Limit = Manifest.IsStackable() && Stack ? Stack->GetMaxStackSize() : 1;
		if (Dimensions.X <= 0 || Dimensions.Y <= 0 || Size.X <= 0 || Size.Y <= 0 ||
			int64(Dimensions.X) * Dimensions.Y > MaxCraftUnits ||
			Placement.GridIndex >= int64(Dimensions.X) * Dimensions.Y ||
			Limit <= 0 || Placement.StackCount > Limit)
			return Fail(TEXT("Inventory placement exceeds grid or stack capacity."));
		const FIntPoint Start(Placement.GridIndex % Dimensions.X, Placement.GridIndex / Dimensions.X);
		if (Start.X + Size.X > Dimensions.X || Start.Y + Size.Y > Dimensions.Y)
			return Fail(TEXT("Inventory placement exceeds grid bounds."));
		auto& Cells = OccupiedCells.FindOrAdd(Manifest.GetItemCategory());
		for (int32 Y = Start.Y; Y < Start.Y + Size.Y; ++Y)
			for (int32 X = Start.X; X < Start.X + Size.X; ++X)
			{
				const int32 Cell = Y * Dimensions.X + X;
				if (Cells.Contains(Cell)) return Fail(TEXT("Inventory placements overlap."));
				Cells.Add(Cell);
			}
		PlacedCounts.FindOrAdd(Placement.ItemId) += Placement.StackCount;
	}
	const auto IsAvailable = [&](const UAZ_Inv_CommonUI_InventoryItem* Item)
	{
		return IsValid(Item) && Item->IsInitialized() &&
			Item->GetLocation() == EAZ_InventoryItemLocation::Backpack &&
			!Item->GetParentItemId().IsValid() && !Item->GetInsertedMagazineId().IsValid() &&
			Item->GetTotalStackCount() > 0 &&
			(Item->IsStackable() || Item->GetTotalStackCount() == 1) &&
			PlacedCounts.FindRef(Item->GetInstanceId()) == Item->GetTotalStackCount() &&
			!IsItemReloadReserved(Item->GetInstanceId()) &&
			!IsItemThrowReserved(Item->GetInstanceId());
	};

	TSet<FGuid> ConsumedItemIds;
	for (const auto& Ingredient : Recipe->Ingredients)
	{
		int32 Remaining = Ingredient.Quantity;
		for (auto* Item : Candidates)
		{
			if (!IsAvailable(Item) || ConsumedItemIds.Contains(Item->GetInstanceId()) ||
				(Equipment && Equipment->GetActiveItem() == Item) ||
				!Item->GetItemManifest().GetItemTypeTag().MatchesTagExact(Ingredient.ItemType)) continue;
			const int32 Taken = FMath::Min(Remaining, Item->GetTotalStackCount());
			OutConsumption.Emplace(Item, Taken);
			ConsumedItemIds.Add(Item->GetInstanceId());
			Remaining -= Taken;
			if (Remaining == 0) break;
		}
		if (Remaining != 0) return Fail(TEXT("Not enough available ingredients."));
	}

	for (const FGameplayTag& Tool : Recipe->Tools)
	{
		bool bFound = false;
		for (const auto* Item : Candidates)
		{
			if (!IsAvailable(Item) || !Item->GetItemManifest().GetItemTypeTag().MatchesTagExact(Tool)) continue;
			const auto* Consumption = OutConsumption.FindByPredicate(
				[Item](const auto& Entry) { return Entry.Key == Item; });
			if (!Consumption || Consumption->Value < Item->GetTotalStackCount()) { bFound = true; break; }
		}
		if (!bFound) return Fail(TEXT("A reusable tool is missing."));
	}

	// Simulate the exact placement debits first, including whole stacks. This lets an output use
	// cells freed by its ingredients without mutating live inventory during the capacity check.
	OutPlacements = GridPlacements;
	for (const auto& Entry : OutConsumption)
	{
		int32 Remaining = Entry.Value;
		for (int32 Index = OutPlacements.Num() - 1; Index >= 0 && Remaining > 0; --Index)
		{
			auto& Placement = OutPlacements[Index];
			if (Placement.ItemId != Entry.Key->GetInstanceId()) continue;
			const int32 Debit = FMath::Min(Remaining, Placement.StackCount);
			Placement.StackCount -= Debit;
			Remaining -= Debit;
			if (Placement.StackCount == 0) OutPlacements.RemoveAt(Index);
		}
		if (Remaining != 0) return Fail(TEXT("Ingredient placements changed before crafting."));
	}
	const FIntPoint Grid = GetGridDimensions(Recipe->OutputManifest.GetItemCategory());
	if (Grid.X <= 0 || Grid.Y <= 0 || int64(Grid.X) * Grid.Y > MaxCraftUnits)
		return Fail(TEXT("Output grid is unsupported."));
	for (int32 Index = 0; Index < Grid.X * Grid.Y; ++Index)
	{
		if (IsPlacementFree(Recipe->OutputManifest, Index, OutPlacements, false))
		{
			OutOutputIndex = Index;
			return true;
		}
	}
	return Fail(TEXT("No room for the crafted item."));
}

bool UAZ_Inv_CommonUI_InventoryComponent::CanCraftRecipe(const UAZ_CraftRecipe* Recipe, FString& OutError) const
{
	TArray<TPair<UAZ_Inv_CommonUI_InventoryItem*, int32>> Consumption;
	TArray<FAZ_InventoryGridPlacement> Placements;
	int32 OutputIndex = INDEX_NONE;
	return BuildCraftPlan(Recipe, Consumption, Placements, OutputIndex, OutError);
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryCraftRecipe(const UAZ_CraftRecipe* Recipe,
	const FGuid& RequestId, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !RequestId.IsValid())
	{ OutError = TEXT("Crafting requires authority and a valid request ID."); return false; }
	if (const auto* Receipt = CompletedCraftRequests.Find(RequestId))
	{
		if (IsValid(Recipe) && Receipt->Get() == Recipe) return true;
		OutError = TEXT("Craft request ID was already used for another recipe.");
		return false;
	}
	if (CompletedCraftRequests.Num() >= MaxCraftReceipts)
	{ OutError = TEXT("Craft request limit reached for this inventory session."); return false; }

	TArray<TPair<UAZ_Inv_CommonUI_InventoryItem*, int32>> Consumption;
	TArray<FAZ_InventoryGridPlacement> Placements;
	int32 OutputIndex = INDEX_NONE;
	if (!BuildCraftPlan(Recipe, Consumption, Placements, OutputIndex, OutError)) return false;

	// Stage a fresh item and identity before touching any ingredient. Only output creation may
	// run Manifest() randomization; existing inputs retain their exact manifest and state.
	auto OutputManifest = Recipe->OutputManifest;
	auto* OutputItem = OutputManifest.Manifest(GetOwner());
	if (!IsValid(OutputItem)) { OutError = TEXT("Could not create crafted item."); return false; }
	FGuid OutputId;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		OutputId = FGuid::NewGuid();
		if (OutputId.IsValid() && !FindItemById(OutputId)) break;
	}
	if (!OutputId.IsValid() || FindItemById(OutputId))
	{ OutError = TEXT("Could not assign a unique crafted item ID."); return false; }
	FAZ_InventoryItemState OutputState;
	OutputState.InstanceId = OutputId;
	OutputState.Location = EAZ_InventoryItemLocation::Backpack;
	if (auto* Stack = OutputItem->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>())
		Stack->SetStackCount(Recipe->OutputCount);
	OutputItem->InitializeInstance(OutputState, Recipe->OutputCount);
	if (!OutputItem->IsInitialized() || OutputItem->GetTotalStackCount() != Recipe->OutputCount)
	{ OutError = TEXT("Crafted item initialization failed."); return false; }

	TArray<TStrongObjectPtr<UAZ_Inv_CommonUI_InventoryItem>> RemovedItems;
	TStrongObjectPtr<UAZ_Inv_CommonUI_InventoryItem> KeepOutputAlive(OutputItem);
	{
	TGuardValue<bool> MutationGuard(bMagazineReloadMutation, true);
	for (const auto& Entry : Consumption)
	{
		if (Entry.Value == Entry.Key->GetTotalStackCount())
		{
			RemovedItems.Emplace(Entry.Key);
			RemoveOwnedItem(Entry.Key);
		}
		else Entry.Key->SetTotalStackCount(Entry.Key->GetTotalStackCount() - Entry.Value);
	}
	GridPlacements = MoveTemp(Placements);
	InventoryList.AddInventoryItem(OutputItem);
	auto& OutputPlacement = GridPlacements.AddDefaulted_GetRef();
	OutputPlacement.ItemId = OutputId;
	OutputPlacement.GridIndex = OutputIndex;
	OutputPlacement.StackCount = Recipe->OutputCount;
	CompletedCraftRequests.Add(RequestId, TWeakObjectPtr<const UAZ_CraftRecipe>(Recipe));

	// Receipt and every ownership/count/placement write precede reentrant listeners.
	for (const auto& Removed : RemovedItems) OnItemRemoved.Broadcast(Removed.Get());
	OnItemAdded.Broadcast(OutputItem);
	}
	// Refresh final availability after the mutation guard is released; callbacks above cannot
	// reenter the transaction, while the final UI refresh must not get stuck on "action busy".
	NotifyInventoryChanged();
	return true;
}
