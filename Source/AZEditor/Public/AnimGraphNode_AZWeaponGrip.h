// Copyright Artur. AZ project.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_AZWeaponGrip.h"
#include "AnimGraphNode_AZWeaponGrip.generated.h"

/** AnimGraph node for FAnimNode_AZWeaponGrip: left hand IK + finger grip onto the held weapon. */
UCLASS()
class AZEDITOR_API UAnimGraphNode_AZWeaponGrip : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_AZWeaponGrip Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	// UAnimGraphNode_SkeletalControlBase
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
