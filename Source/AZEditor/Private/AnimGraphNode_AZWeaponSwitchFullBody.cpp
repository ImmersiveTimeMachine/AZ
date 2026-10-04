// Copyright Artur. AZ project.

#include "AnimGraphNode_AZWeaponSwitchFullBody.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_AZWeaponSwitchFullBody)

#define LOCTEXT_NAMESPACE "AZWeaponSwitchFullBody"

FText UAnimGraphNode_AZWeaponSwitchFullBody::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "AZ Weapon Switch Full Body");
}

FText UAnimGraphNode_AZWeaponSwitchFullBody::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "While the anim instance asks for it (a crouched draw / holster while standing still), blends "
		"every bone but the root toward the weapon-switch clip's own pose: the crouch switch clips are full-body motions and "
		"layered over the locomotion legs the arm passes through the knee. Place right after the RifleFire slot's layered "
		"blend. Pass-through otherwise.");
}

FLinearColor UAnimGraphNode_AZWeaponSwitchFullBody::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.6f, 0.8f);
}

FString UAnimGraphNode_AZWeaponSwitchFullBody::GetNodeCategory() const
{
	return TEXT("AZ");
}

#undef LOCTEXT_NAMESPACE
