// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AZ_HordeSubsystem.generated.h"

class AAZ_InfectedAIController;

/**
 * Horde coordination layer — the manager SUGGESTS, each Chalkie's BT DECIDES.
 *
 * Server-authoritative registry of possessed infected controllers plus pack-level signals. It never
 * drives movement, never picks a target for an individual, never overrides a BT branch — it only feeds
 * the same entry points stimuli already use (ArmInvestigation → blackboard hints). A single Chalkie in
 * an empty test level behaves EXACTLY as it does inside a horde.
 *
 * Layers (build order):
 *  1. Registry + aggro alert fan-out (THIS) — one Chalkie confirms prey → nearby pack converges to search.
 *  2. Engagement tokens (with melee) — cap simultaneous attackers; the rest hold a ring.
 *  3. Search-sector dedup — co-investigators of one noise fan out instead of stacking on the same point.
 *  4. Significance LOD — BT/perception/anim rate scaling by distance.
 *  5. Director pacing (much later) — intensity curves over the alert stream already flowing through here.
 */
UCLASS()
class AZ_API UAZ_HordeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game/PIE worlds only — no editor-preview instances. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Controllers self-register in OnPossess / unregister in OnUnPossess (server-only by construction —
	 *  AI controllers don't exist on clients). */
	void RegisterInfected(AAZ_InfectedAIController* Controller);
	void UnregisterInfected(AAZ_InfectedAIController* Controller);

	/** A Chalkie confirmed prey (entered Aggressive): wake the pack. Every OTHER registered infected within
	 *  AlertRadius of the SCREAMER gets an URGENT investigation armed at the prey's location — they converge
	 *  and search at run gait. Promotion to an actual chase stays each receiver's own perception's job. */
	void NotifyAggro(AAZ_InfectedAIController* Instigator, const FVector& PreyLocation);

	/** How far the aggro callout carries (cm), measured from the instigator's pawn. Mirrors the controller's
	 *  TeamAlertRadius intent; lives here because the scream is a PACK event, not a per-listener sense. */
	float AlertRadius = 2000.f;

	// ---- Engagement tokens (layer 2): cap simultaneous attackers per prey so a crowd reads as a
	// coordinated pack, not a blender. Token storage is file-static in the cpp until the batch
	// promotes it to a member (Live-Coding can't add fields to a live UObject class). ----
	/** True if the attacker holds (or is granted) one of the per-prey attack slots (max 2). */
	bool RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey);
	/** Release the attacker's slot (no-op if it holds none). Call from every attack-task exit path. */
	void ReleaseAttackToken(AAZ_InfectedAIController* Attacker);

private:
	/** Weak on purpose — controllers die with their pawns/world; the registry owns nothing. Dead entries
	 *  are compacted during NotifyAggro sweeps. */
	TArray<TWeakObjectPtr<AAZ_InfectedAIController>> Infected;
};
