#include "Inventory/AZ_EquipmentManagerComponent.h"
#include "Inventory/AZ_InventoryItemInstance.h"
#include "Inventory/AZ_InventoryItemDefinition.h"
// Replace old include with the new definition header
#include "Inventory/AZ_InventoryItemDefinition.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "Character/AZ_HeroCharacter.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/GameSession.h"
#include "Player/AZ_PlayerController.h"
#include "Player/AZ_PlayerState.h"
#include "Weapon/AZ_Weapon.h"

UAZ_EquipmentManagerComponent::UAZ_EquipmentManagerComponent()
{
	SetIsReplicatedByDefault(true);
}

void UAZ_EquipmentManagerComponent::OnRep_OverlappedItemsId()
{
	// Just trigger a prompt; UI decides what to show.
	OnShowPickupPrompt.Broadcast(!OverlappedItemsId.IsEmpty());
}

void UAZ_EquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_EquipmentManagerComponent, SlottedItems);
	DOREPLIFETIME(UAZ_EquipmentManagerComponent, ActiveSlotTag);

	DOREPLIFETIME_CONDITION_NOTIFY(UAZ_EquipmentManagerComponent, OverlappedItemsId, COND_OwnerOnly, REPNOTIFY_OnChanged);
}

// --- PUBLIC FUNCTIONS (API) ---

void UAZ_EquipmentManagerComponent::EquipItemToSlot(UAZ_InventoryItemInstance* ItemInstance, FGameplayTag SlotTag)
{
	if (!ItemInstance || !SlotTag.IsValid())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		HandleEquipItemToSlot_Server(ItemInstance, SlotTag);
	}
	else
	{
		Server_EquipItemToSlot(ItemInstance, SlotTag);
	}
}

void UAZ_EquipmentManagerComponent::DrawWeaponFromSlot(FGameplayTag SlotTag)
{
	if (GetOwner()->HasAuthority())
	{
		Server_DrawWeaponFromSlot_Implementation(SlotTag);
	}
	else
	{
		Server_DrawWeaponFromSlot(SlotTag);
	}
}

void UAZ_EquipmentManagerComponent::UnequipItemFromSlot(FGameplayTag SlotTag)
{
	if (GetOwner()->HasAuthority())
	{
		Server_UnequipItemFromSlot_Implementation(SlotTag);
	}
	else
	{
		Server_UnequipItemFromSlot(SlotTag);
	}
}

void UAZ_EquipmentManagerComponent::CollectItem()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const auto HeroCharacter = GetOwner<AAZ_HeroCharacter>();
	check(HeroCharacter);

	const auto PlayerState = HeroCharacter->GetPlayerState<AAZ_PlayerState>();
	check(PlayerState);

	const auto Inventory = PlayerState->GetInventoryComponent();
	check(Inventory);

	for (const auto Item : OverlappedItems)
	{
		if (!IsValid(Item))
		{
			continue;
		}

		if (OverlappedItemsId.Contains(Item->GetItemGuid()))
		{
			check(Item->ItemDefinition);
			check(Item->ItemDefinition->ItemClass);

			Inventory->AddItemToInventory(Item->ItemDefinition);

			Item->Destroy(false, false);

			// 🔹 If this is an AAZ_Item, do server-side attachment
			if (AAZ_Item* NewItem = Cast<AAZ_Item>(SpawnItem(Item->ItemDefinition->ItemClass)))
			{
				if (NewItem->ItemDefinition && NewItem->ItemDefinition->bAttachToOwner)
				{
					if (NewItem->PickupSphere)
					{
						NewItem->Multicast_SetCollisionEnabled(false);
					}
					
					const FName SocketName = HeroCharacter->GetEquipmentManagerComponent()->TagToSocket.FindRef(NewItem->ItemDefinition->AllowedSlotTag);
					HeroCharacter->AttachItem(NewItem);

					NewItem->ForceNetUpdate(); // replicate updated state
				}
			}
			break;
		}
	}
}

AActor* UAZ_EquipmentManagerComponent::SpawnItem(const TSubclassOf<AActor> ActorClass) const
{
	// Ensure this only runs on the server
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

	if (!ActorClass)
	{
		return nullptr;
	}

	AAZ_HeroCharacter* HeroCharacter = Cast<AAZ_HeroCharacter>(GetOwner());
	if (!HeroCharacter)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = HeroCharacter; // Logical owner
	SpawnParams.Instigator = HeroCharacter; // For damage/authority chain
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;

	// Spawn the actor on the SERVER
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnParams);
	if (!SpawnedActor)
	{
		return nullptr;
	}

	// Make sure it replicates
	SpawnedActor->SetReplicates(true);
	SpawnedActor->SetNetDormancy(DORM_Awake);

	return SpawnedActor;
}

void UAZ_EquipmentManagerComponent::Initialize(UAZ_InventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
}

bool UAZ_EquipmentManagerComponent::AddOverlappedItemId(const FGuid InItemId)
{
	if (!InItemId.IsValid())
	{
		return false;
	}

	const int32 PrevNum = OverlappedItemsId.Num();
	OverlappedItemsId.AddUnique(InItemId);
	return OverlappedItemsId.Num() != PrevNum;
}

bool UAZ_EquipmentManagerComponent::RemoveOverlappedItemId(const FGuid InItemId)
{
	if (!InItemId.IsValid())
	{
		return false;
	}

	return OverlappedItemsId.RemoveSingleSwap(InItemId) > 0;
}

bool UAZ_EquipmentManagerComponent::AddOverlappedItem(AAZ_Item* InItem)
{
	if (!IsValid(InItem))
	{
		return false;
	}

	const int32 PrevNum = OverlappedItems.Num();
	OverlappedItems.AddUnique(InItem);
	return OverlappedItems.Num() != PrevNum;
}

bool UAZ_EquipmentManagerComponent::RemoveOverlappedItem(AAZ_Item* InItem)
{
	if (!IsValid(InItem))
	{
		return false;
	}

	return OverlappedItems.RemoveSingleSwap(InItem) > 0;
}

void UAZ_EquipmentManagerComponent::ItemAddedToInventoryChanged(FGuid InInventoryItemInstanceId)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString("ItemAddedToInventory"));

	auto PlayerState = GetOwner<AAZ_HeroCharacter>()->GetPlayerState<AAZ_PlayerState>();
}

// --- SERVER RPC IMPLEMENTATIONS ---

void UAZ_EquipmentManagerComponent::Server_EquipItemToSlot_Implementation(UAZ_InventoryItemInstance* ItemInstance, FGameplayTag SlotTag)
{
	HandleEquipItemToSlot_Server(ItemInstance, SlotTag);
}

void UAZ_EquipmentManagerComponent::HandleEquipItemToSlot_Server(UAZ_InventoryItemInstance* ItemInstance, const FGameplayTag& SlotTag)
{
	if (!ensure(GetOwner()->HasAuthority()) || !ItemInstance || !SlotTag.IsValid())
	{
		return;
	}

	// Remove any existing item in that slot (server-internal call)
	Server_UnequipItemFromSlot_Implementation(SlotTag);

	SlottedItems.Emplace(SlotTag, ItemInstance);
	// No OnRep on server; clients will update via replication.
}

void UAZ_EquipmentManagerComponent::Server_DrawWeaponFromSlot_Implementation(FGameplayTag SlotTag)
{
	if (FindSlottedItem(SlotTag))
	{
		// Remove old active item's GAS before switching
		if (ActiveSlotTag.IsValid())
		{
			if (FSlottedItem* OldItem = FindSlottedItem(ActiveSlotTag))
			{
				const auto* OldItemDef = Cast<const UAZ_InventoryItemDefinition>(OldItem->ItemInstance->ItemDefinition);
				RemoveAbilitiesAndEffectsFor(OldItemDef);
			}
		}

		ActiveSlotTag = SlotTag;

		// Apply server-side functional changes (spawn weapon actor, grant GAS)
		HandleActiveSlotChanged_Server();
	}
}

void UAZ_EquipmentManagerComponent::Server_UnequipItemFromSlot_Implementation(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid()) return;

	if (FSlottedItem* ItemToRemove = FindSlottedItem(SlotTag))
	{
		if (SlotTag == ActiveSlotTag)
		{
			ActiveSlotTag = FGameplayTag::EmptyTag;

			// Tear down server-side weapon and abilities
			HandleActiveSlotChanged_Server();
		}

		SlottedItems.RemoveAll([&SlotTag](const FSlottedItem& Item) { return Item.SlotTag == SlotTag; });
		// Clients will update cosmetics via OnRep_SlottedItems; no OnRep call on server.
	}
}

// --- ONREP & HELPER FUNCTIONS ---

void UAZ_EquipmentManagerComponent::OnRep_SlottedItems()
{
	// Cosmetics only; skip on dedicated server
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	RefreshClientSheathedCosmetics();
}

void UAZ_EquipmentManagerComponent::OnRep_ActiveSlotTag()
{
	// Cosmetics/UI only; do not destroy or spawn replicated weapon actors on clients.
	// Ensure sheathed cosmetics reflect "all slots except ActiveSlotTag".
	if (GetNetMode() != NM_DedicatedServer)
	{
		RefreshClientSheathedCosmetics();

		// If a cosmetic exists for ActiveSlotTag, remove it as the weapon is now in hand
		if (TObjectPtr<AActor>* SheathedActorPtr = SheathedWeaponActors.Find(ActiveSlotTag))
		{
			if (AActor* SheathedActor = SheathedActorPtr->Get())
			{
				SheathedActor->Destroy();
			}
			SheathedWeaponActors.Remove(ActiveSlotTag);
		}
	}

	// Notify UI
	FSlottedItem* ActiveItem = FindSlottedItem(ActiveSlotTag);
	const auto* NewItemDef = ActiveItem ? Cast<const UAZ_InventoryItemDefinition>(ActiveItem->ItemInstance->ItemDefinition) : nullptr;
	OnActiveEquipmentChanged.Broadcast(NewItemDef);
}

FSlottedItem* UAZ_EquipmentManagerComponent::FindSlottedItem(const FGameplayTag& SlotTag)
{
	if (!SlotTag.IsValid()) return nullptr;
	return SlottedItems.FindByPredicate([&SlotTag](const FSlottedItem& Item) { return Item.SlotTag == SlotTag; });
}

UAZ_AbilitySystemComponent* UAZ_EquipmentManagerComponent::GetOwnerASC() const
{
	if (const AActor* Owner = GetOwner())
	{
		// First try to get PlayerState from Pawn (if the owner is a Pawn)
		if (const APawn* OwnerPawn = Cast<APawn>(Owner))
		{
			if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerPawn->GetPlayerState()))
			{
				return Cast<UAZ_AbilitySystemComponent>(ASI->GetAbilitySystemComponent());
			}
		}

		// If owner is not a Pawn or doesn't have PlayerState, try the owner directly
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
		{
			return Cast<UAZ_AbilitySystemComponent>(ASI->GetAbilitySystemComponent());
		}
	}
	return nullptr;
}

// Grant GAS for active item (server-only)
void UAZ_EquipmentManagerComponent::GrantAbilitiesAndEffectsFor(const UAZ_InventoryItemDefinition* ItemDef, UAZ_InventoryItemInstance* ItemInstance)
{
	if (!GetOwner()->HasAuthority() || !ItemDef || !ItemInstance) return;

	UAZ_AbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC) return;

	for (const TSubclassOf<UGameplayAbility> AbilityClass : ItemDef->AbilitiesToGrant)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, -1, ItemInstance);
			GrantedAbilityHandles.Add(ASC->GiveAbility(AbilitySpec));
		}
	}

	for (const TSubclassOf<UGameplayEffect> EffectClass : ItemDef->OngoingEffectsToApply)
	{
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(ItemInstance);
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
			if (SpecHandle.IsValid())
			{
				const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				if (Handle.IsValid())
				{
					GrantedEffectHandles.Add(Handle);
				}
			}
		}
	}
}

void UAZ_EquipmentManagerComponent::RemoveAbilitiesAndEffectsFor(const UAZ_InventoryItemDefinition* ItemDef)
{
	if (!GetOwner()->HasAuthority() || !ItemDef) return;

	UAZ_AbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC) return;

	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		ASC->ClearAbility(Handle);
	}
	GrantedAbilityHandles.Empty();

	// Remove exactly the effects we applied
	for (const FActiveGameplayEffectHandle& EffectHandle : GrantedEffectHandles)
	{
		ASC->RemoveActiveGameplayEffect(EffectHandle);
	}
	GrantedEffectHandles.Empty();
}

// Server-only functional changes when ActiveSlotTag changes
void UAZ_EquipmentManagerComponent::HandleActiveSlotChanged_Server()
{
	if (!ensure(GetOwner()->HasAuthority()))
	{
		return;
	}
	
	// Defensive cleanup of any previously tracked handles
	GrantedAbilityHandles.Empty();
	GrantedEffectHandles.Empty();

	// If no active slot, we're done
	FSlottedItem* ActiveItem = FindSlottedItem(ActiveSlotTag);
	const UAZ_InventoryItemDefinition* NewItemDef =
		ActiveItem ? Cast<const UAZ_InventoryItemDefinition>(ActiveItem->ItemInstance->ItemDefinition) : nullptr;

	if (!NewItemDef)
	{
		return; // Nothing active now
	}
	
	// Grant abilities/effects for the new active item
	GrantAbilitiesAndEffectsFor(NewItemDef, ActiveItem->ItemInstance);
}

// Client-only cosmetics refresh (sheathed actors)
void UAZ_EquipmentManagerComponent::RefreshClientSheathedCosmetics()
{
	AAZ_CharacterBase* Char = Cast<AAZ_CharacterBase>(GetOwner());
	if (!Char) return;

	// Destroy any sheathed actors for slots that are now empty.
	TArray<FGameplayTag> TagsInMap;
	SheathedWeaponActors.GetKeys(TagsInMap);
	for (const FGameplayTag& TagInMap : TagsInMap)
	{
		if (!FindSlottedItem(TagInMap))
		{
			if (AActor* ActorToDestroy = SheathedWeaponActors.FindRef(TagInMap))
			{
				if (ActorToDestroy) ActorToDestroy->Destroy();
			}
			SheathedWeaponActors.Remove(TagInMap);
		}
	}

	// Spawn new sheathed actors for newly added items (but not the active slot)
	for (const FSlottedItem& SlottedItem : SlottedItems)
	{
		if (SlottedItem.SlotTag == ActiveSlotTag)
		{
			// Active slot is in-hand; ensure no cosmetic actor remains
			if (AActor* Existing = SheathedWeaponActors.FindRef(SlottedItem.SlotTag))
			{
				Existing->Destroy();
				SheathedWeaponActors.Remove(SlottedItem.SlotTag);
			}
			continue;
		}

		if (!SheathedWeaponActors.Contains(SlottedItem.SlotTag))
		{
			const auto* ItemDef = Cast<const UAZ_InventoryItemDefinition>(SlottedItem.ItemInstance->ItemDefinition);
		}
	}
}

FName UAZ_EquipmentManagerComponent::ResolveSheathedSocketForSlot(const FGameplayTag& SlotTag) const
{
	const AAZ_CharacterBase* Char = Cast<AAZ_CharacterBase>(GetOwner());
	if (!Char) return FName("back_socket");

	const TMap<FGameplayTag, FName>& Map = Char->GetEquipmentSocketMap();
	if (const FName* Found = Map.Find(SlotTag))
	{
		return *Found;
	}
	return FName("back_socket");
}
