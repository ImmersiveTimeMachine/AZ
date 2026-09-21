#include "Game/AZ_CampaignTeleportEffect.h"

bool FAZ_CampaignTeleportEffect::ApplyMovementEffect(FApplyMovementEffectParams& Params, FMoverSyncState& Output)
{
	if (!Token) return false;
	int32 Expected = 0;
	if (!Token->State.compare_exchange_strong(Expected, 1)) return false;
	const bool bApplied = FTeleportEffect::ApplyMovementEffect(Params, Output);
	Token->bSucceeded.store(bApplied);
	Token->State.store(2);
	return bApplied;
}

bool FAZ_CampaignTeleportEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async&, FMoverSyncState&)
{
	// Do not claim cancellation while an asynchronous backend still has a pose
	// queued for publication. That backend requires its own acknowledged protocol.
	if (Token)
	{
		int32 Expected = 0;
		if (Token->State.compare_exchange_strong(Expected, 1))
		{ Token->bSucceeded.store(false); Token->State.store(2); }
	}
	return false;
}
