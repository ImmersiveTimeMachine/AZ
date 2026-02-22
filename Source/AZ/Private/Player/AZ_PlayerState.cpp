// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_AttributeSet.h"
#include "Inventory/AZ_InventoryComponent.h"
#include "Net/UnrealNetwork.h"


AAZ_PlayerState::AAZ_PlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	InventoryComponent = CreateDefaultSubobject<UAZ_InventoryComponent>(TEXT("Inventory"));
	
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* AAZ_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAZ_PlayerState::SetAttributeSet(UAttributeSet* InAttributeSet)
{
	AttributeSet = InAttributeSet;
}

void AAZ_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AAZ_PlayerState, Level);
	DOREPLIFETIME(AAZ_PlayerState, XP);
	DOREPLIFETIME(AAZ_PlayerState, AttributePoints);
	DOREPLIFETIME(AAZ_PlayerState, SpellPoints);
}

void AAZ_PlayerState::AddToXP(int32 InXP)
{
	XP += InXP;
	OnXPChangedDelegate.Broadcast(XP);
}

void AAZ_PlayerState::AddToLevel(int32 InLevel)
{
	Level += InLevel;
	OnLevelChangedDelegate.Broadcast(Level, true);
}

void AAZ_PlayerState::SetXP(int32 InXP)
{
	XP = InXP;
	OnXPChangedDelegate.Broadcast(XP);
}

void AAZ_PlayerState::SetLevel(int32 InLevel)
{
	Level = InLevel;
	OnLevelChangedDelegate.Broadcast(Level, false);
}

void AAZ_PlayerState::SetAttributePoints(int32 InPoints)
{
	AttributePoints = InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAZ_PlayerState::SetSpellPoints(int32 InPoints)
{
	SpellPoints = InPoints;
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}

void AAZ_PlayerState::OnRep_Level(int32 OldLevel)
{
	OnLevelChangedDelegate.Broadcast(Level, true);
}

void AAZ_PlayerState::OnRep_XP(int32 OldXP)
{
	OnXPChangedDelegate.Broadcast(XP);
}

void AAZ_PlayerState::OnRep_AttributePoints(int32 OldAttributePoints)
{
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAZ_PlayerState::OnRep_SpellPoints(int32 OldSpellPoints)
{
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}

void AAZ_PlayerState::AddToAttributePoints(int32 InPoints)
{
	AttributePoints += InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAZ_PlayerState::AddToSpellPoints(int32 InPoints)
{
	SpellPoints += InPoints;
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}
