// Copyright Artur. AZ project.

#include "AnimGraphNode_AZWeaponGrip.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_AZWeaponGrip)

#define LOCTEXT_NAMESPACE "AZWeaponGrip"

FText UAnimGraphNode_AZWeaponGrip::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "AZ Weapon Grip");
}

FText UAnimGraphNode_AZWeaponGrip::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_AZWeaponGrip::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Holds the weapon by its grip data: left hand two-bone IK (position + rotation) onto the "
		"weapon's LeftHandGrip relative to the bone the weapon hangs on, and both hands' fingers from the weapon's grip "
		"pose. Per-hand weight = Grip Alpha x the AZ_Grip_L / AZ_Grip_R animation curves (Default Hand Alpha when a clip "
		"has none).");
}

#undef LOCTEXT_NAMESPACE
