// Copyright Artur. AZ project.
#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "AZ_MeleeAlignmentMove.generated.h"

/** A short combat alignment/hold that can be cancelled independently of animation root motion. */
USTRUCT()
struct AZ_API FLayeredMove_AZ_MeleeAlignment : public FLayeredMove_LinearVelocity
{
	GENERATED_BODY()

	FLayeredMove_AZ_MeleeAlignment();

	static FGameplayTag GetMoveTag();
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
};

template<>
struct TStructOpsTypeTraits<FLayeredMove_AZ_MeleeAlignment> : public TStructOpsTypeTraitsBase2<FLayeredMove_AZ_MeleeAlignment>
{
	enum { WithCopy = true };
};
