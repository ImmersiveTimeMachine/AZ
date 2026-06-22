// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/AZ_QuickBarComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"


// Sets default values for this component's properties
UAZ_QuickBarComponent::UAZ_QuickBarComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// Replicated so a client can send Server_Select to the authority (which owns ability grants).
	SetIsReplicatedByDefault(true);
}

void UAZ_QuickBarComponent::Select(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;   // archetype Slots exist on both client and server

	// Equip grants abilities = authority-only. The equip input runs on the owning client, so a
	// client hops to the server; the host/server does it directly. Without this, a remote client's
	// EquipSlot bailed on !HasAuthority -> no grant -> the punch never activated on that client.
	if (GetOwner()->HasAuthority())
	{
		SelectInternal(SlotIndex);
	}
	else
	{
		Server_Select(SlotIndex);
	}
}

void UAZ_QuickBarComponent::Server_Select_Implementation(int32 SlotIndex)
{
	SelectInternal(SlotIndex);
}

void UAZ_QuickBarComponent::SelectInternal(int32 SlotIndex)
{
	// Authority only (server / listen-host). Toggle: re-selecting the active slot unequips.
	if (!Slots.IsValidIndex(SlotIndex)) return;
	if (SlotIndex == ActiveSlotIndex) { UnequipActive(); return; }  // re-press = fists-up toggle
	UnequipActive();
	EquipSlot(SlotIndex);
}

void UAZ_QuickBarComponent::CycleNext()
{
}

void UAZ_QuickBarComponent::CyclePrev()
{
}

UAZ_AbilitySystemComponent* UAZ_QuickBarComponent::GetASC() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->PlayerState) return nullptr;
	return Cast<UAZ_AbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PC->PlayerState));
}

void UAZ_QuickBarComponent::EquipSlot(const int32 SlotIndex)
{
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!ASC || !GetOwner()->HasAuthority()) return;             // grant is authority-only (Phase 2 = server RPC)
	const FAZ_QuickSlot& Slot = Slots[SlotIndex];

	ASC->OnWeaponEquipped(Slot.WeaponTag);                       // publish profile tag -> OwnedTags -> chooser

	// Combat-ready profiles (fists) flip to strafe on equip. REPLICATED loose tag so the chooser
	// (ChooserContext.bStrafe) and Mover (ProduceInput facing) see it on every role incl. sim
	// proxies. Authority-only here (EquipSlot is HasAuthority-gated), so this is the correct site.
	if (Slot.bStrafeOnEquip)
	{
		// Replicated state tag (local + FMinimalReplicationTagCountMap on authority — the project's
		// Iris-aligned surface; AZ_AbilitySystemComponent audit P1-12). Visible to the chooser
		// (ChooserContext.bStrafe) and Mover (ProduceInput facing) on every role incl. sim proxies.
		ASC->AddStateTag(FAZ_GameplayTags::Get().Movement_Strafe);
	}

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : Slot.WeaponAbilities)
	{
		if (!*AbilityClass) continue;
		FGameplayAbilitySpec Spec(AbilityClass);
		if (const UAZ_GameplayAbility* GameplayAbility = AbilityClass->GetDefaultObject<UAZ_GameplayAbility>())
			Spec.GetDynamicSpecSourceTags().AddTag(GameplayAbility->InputTag);   // seed InputTag so input rig can fire it
		GrantedHandles.Add(ASC->GiveAbility(Spec));
	}

	// Data-driven GEs on equip (e.g. GE_CombatReady on the fist slot). Authority-gated above, so applying here
	// records the granted tags in the replicated map -> they reach clients. The GE owns its own duration + refresh
	// (re-applied by the melee ability's EffectsOnActivate); no C++ timer.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Slot.EffectsOnEquip)
	{
		if (!*EffectClass) continue;
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Ctx);
		if (Spec.IsValid()) ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	ActiveSlotIndex = SlotIndex;
}

void UAZ_QuickBarComponent::UnequipActive()
{
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!ASC || !GetOwner()->HasAuthority()) return;
	for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles) ASC->ClearAbility(Handle);
	GrantedHandles.Reset();
	ASC->OnWeaponEquipped(FAZ_GameplayTags::Get().Weapon_None);   // empty hands (Q6)
	ASC->RemoveStateTag(FAZ_GameplayTags::Get().Movement_Strafe);  // drop strafe; next strafe equip re-adds
	ActiveSlotIndex = -1;
}


// Called when the game starts
void UAZ_QuickBarComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UAZ_QuickBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

