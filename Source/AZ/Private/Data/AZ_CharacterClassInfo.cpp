// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/AZ_CharacterClassInfo.h"

FAZCharacterClassDefaultInfo UAZ_CharacterClassInfo::GetClassDefaultInfo(EAZCharacterClass CharacterClass)
{
	return CharacterClassInformation.FindChecked(CharacterClass);
}
