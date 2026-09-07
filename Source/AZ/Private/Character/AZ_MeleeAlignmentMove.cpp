// Copyright Artur. AZ project.
#include "Character/AZ_MeleeAlignmentMove.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Mover_MeleeAlignment, "Mover.MeleeAlignment");

FLayeredMove_AZ_MeleeAlignment::FLayeredMove_AZ_MeleeAlignment()
{
	// Locomotion transition root motion uses priority 0. On a tie the mixer picks the older
	// move, so this short combat commitment must outrank a transition already in progress.
	Priority = 1;
	MixMode = EMoveMixMode::OverrideVelocity;
}

FGameplayTag FLayeredMove_AZ_MeleeAlignment::GetMoveTag()
{
	return TAG_Mover_MeleeAlignment;
}

bool FLayeredMove_AZ_MeleeAlignment::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	const FGameplayTag MoveTag = GetMoveTag();
	return bExactMatch ? MoveTag.MatchesTagExact(TagToFind) : MoveTag.MatchesTag(TagToFind);
}

void FLayeredMove_AZ_MeleeAlignment::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(GetMoveTag());
}

FLayeredMoveBase* FLayeredMove_AZ_MeleeAlignment::Clone() const
{
	return new FLayeredMove_AZ_MeleeAlignment(*this);
}

void FLayeredMove_AZ_MeleeAlignment::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLayeredMove_AZ_MeleeAlignment::GetScriptStruct() const
{
	return StaticStruct();
}
