// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AZ_JumpRequester.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAZ_JumpRequester : public UInterface
{
	GENERATED_BODY()
};

/**
 * Receives jump press/release requests from a GameplayAbility (UAZ_GA_PawnJump).
 *
 * GAS gates the request (tags, cost, cooldown, conditions). The pawn executes by
 * flipping its Mover input flag — the flag rides FCharacterDefaultInputs into the
 * deterministic NetworkPrediction InputCmd, so Mover-side prediction stays the only
 * mover simulating motion. Splitting "request" (GAS) from "execute" (Mover) keeps
 * one prediction model for movement.
 *
 * Multi-pawn: only pawns that can jump implement this. Vehicles don't — GA_PawnJump
 * cancels itself when the cast fails, so the same GA class works across the project
 * without per-pawn casts.
 *
 * Co-op: per-pawn impl, no globals. Each player's ASC fires its own GA on its own
 * pawn — no cross-talk.
 */
class AZ_API IAZ_JumpRequester
{
	GENERATED_BODY()

public:
	/** GA calls with true on input press, false on input release. The implementor
	 *  applies the request in whatever way suits its physics model — flip a Mover
	 *  input flag (v2 pawn), call ACharacter::Jump/StopJumping (legacy CMC pawn),
	 *  raise a vehicle hover impulse, etc. */
	virtual void SetJumpPressed(bool bPressed) = 0;

	/** Gate from CanActivateAbility. Default = true. Override to forward to the
	 *  pawn's per-frame "can jump now?" check (e.g. ACharacter::CanJump() —
	 *  enforces JumpMaxCount, JumpCurrentCount, JumpKeyHoldTime, gravity scale,
	 *  cooldown). Keeps the GA character-agnostic. */
	virtual bool CanRequestJump() const { return true; }
};
