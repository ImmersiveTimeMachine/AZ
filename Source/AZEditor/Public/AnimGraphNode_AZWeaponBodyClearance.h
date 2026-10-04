// Copyright Artur. AZ project.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_AZWeaponBodyClearance.h"
#include "AnimGraphNode_AZWeaponBodyClearance.generated.h"

/** AnimGraph node for FAnimNode_AZWeaponBodyClearance: keeps the held weapon's stock out of the body. */
UCLASS()
class AZEDITOR_API UAnimGraphNode_AZWeaponBodyClearance : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_AZWeaponBodyClearance Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	// UAnimGraphNode_SkeletalControlBase
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
