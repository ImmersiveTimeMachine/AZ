// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/AZ_GameModeBase.h"


void AAZ_GameModeBase::BeginPlay()
{
	Super::BeginPlay();
	Maps.Add(DefaultMapName, DefaultMap);
}
