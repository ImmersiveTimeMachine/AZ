#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include <atomic>
#include "AZ_CampaignTeleportEffect.generated.h"

/** Shared cancellation handshake; contains no UObject and is safe across an effect clone. */
struct FAZ_CampaignTeleportToken
{
	// 0 queued, 1 applying, 2 finished, 3 cancelled before application.
	std::atomic<int32> State{0};
	std::atomic<bool> bSucceeded{false};
	bool CancelBeforeApply()
	{
		int32 Expected = 0;
		return State.compare_exchange_strong(Expected, 3) || Expected == 3;
	}
};

/** Checkpoint-only effect for the current synchronous Mover backend. */
USTRUCT()
struct AZ_API FAZ_CampaignTeleportEffect : public FTeleportEffect
{
	GENERATED_BODY()
	TSharedPtr<FAZ_CampaignTeleportToken, ESPMode::ThreadSafe> Token;
	virtual FInstantMovementEffect* Clone() const override { return new FAZ_CampaignTeleportEffect(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& Params, FMoverSyncState& Output) override;
	virtual bool ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& Params, FMoverSyncState& Output) override;
};

template<> struct TStructOpsTypeTraits<FAZ_CampaignTeleportEffect> : TStructOpsTypeTraitsBase2<FAZ_CampaignTeleportEffect>
{
	enum { WithCopy = true };
};
