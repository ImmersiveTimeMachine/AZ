// Copyright Artur. AZ project.

#pragma once

#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_AZWeaponSwitchFullBody.h"
#include "AnimGraphNode_AZWeaponSwitchFullBody.generated.h"

/** AnimGraph node for FAnimNode_AZWeaponSwitchFullBody: a crouched weapon switch while standing still plays its clip on
 *  the whole body. */
UCLASS()
class AZEDITOR_API UAnimGraphNode_AZWeaponSwitchFullBody : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_AZWeaponSwitchFullBody Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	// UAnimGraphNode_Base
	virtual FString GetNodeCategory() const override;
};
