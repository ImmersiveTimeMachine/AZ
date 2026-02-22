// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AZ_GameplayEffectTypes.h"


bool FAZ_GameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess) && TargetData.NetSerialize(Ar, Map, bOutSuccess);
}
