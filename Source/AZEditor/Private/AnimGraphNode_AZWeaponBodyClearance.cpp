// Copyright Artur. AZ project.

#include "AnimGraphNode_AZWeaponBodyClearance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_AZWeaponBodyClearance)

#define LOCTEXT_NAMESPACE "AZWeaponBodyClearance"

FText UAnimGraphNode_AZWeaponBodyClearance::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "AZ Weapon Body Clearance");
}

FText UAnimGraphNode_AZWeaponBodyClearance::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_AZWeaponBodyClearance::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Keeps the held weapon out of the body: the stock (StockFront -> StockButt markers) is tested "
		"against the Physics Asset capsules of the torso, clavicles, neck, head, pelvis and thighs; when it is inside, the "
		"right hand (the weapon hangs on it) is pushed out by two-bone IK of the right arm. While aiming the butt may rest "
		"in the shoulder pocket. Place it before AZ Weapon Grip.");
}

#undef LOCTEXT_NAMESPACE
