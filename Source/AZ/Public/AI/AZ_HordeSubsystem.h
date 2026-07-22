// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AZ_HordeSubsystem.generated.h"

class AAZ_InfectedAIController;

/** Crowd combat role — assigned by UAZ_HordeSubsystem (sole writer), consumed by BT (BB mirror), the
 *  AnimInstance (ASC tags: menace posture), and future GEs (taunt/fear can force roles). */
UENUM()
enum class EAZ_CombatRole : uint8
{
	None,      // not engaged (no fresh target)
	Active,    // holds one of the per-prey attack slots: press the attack
	Passive    // engaged, no slot: hold the ring at RingDistance, menace, get promoted when a slot frees
};

/**
 * Horde coordination layer — THE CROWD BRAIN (v2 doctrine, 2026-07-22).
 *
 * v1 was "the manager suggests, the BT decides". The combat build proved that decisions smeared across
 * controller code, BB keys, and BT decorators produce seam bugs (nothing-observes-this-key, abort-mode
 * defaults, binding caches). v2 splits the job cleanly:
 *
 *   controller perception -> FACTS (seen/heard/pain, fresh target)     [unchanged]
 *   THIS SUBSYSTEM        -> DECISIONS (who fights, who rings)          [combat roles, one tick, whole-pack view]
 *   behavior tree         -> EXECUTION (move/scan/attack per role)      [one decorator per branch, watching ONE key]
 *
 * COMBAT ROLES (rulebook rule 7): every Chalkie with a fresh target participates; per prey the closest
 * MaxAttackersPerPrey (with stickiness so pairs don't flicker) are Active, the rest Passive. Roles are
 * published atomically to BOTH consumers (ASC tags State.Combat.Engaged.* + BB bActiveFighter) so they
 * can never disagree. Investigation stays suggest-only (ArmInvestigation hints) — a single Chalkie in an
 * empty level still behaves exactly as it does inside a horde.
 *
 * Layers (build order):
 *  1. Registry + aggro alert fan-out — one Chalkie confirms prey -> nearby pack converges to search. [LIVE]
 *  2. Combat roles Active/Passive (THIS) — cap simultaneous attackers; the rest hold a ring.         [LIVE]
 *  3. Search-sector dedup — co-investigators of one noise fan out instead of stacking.               [later]
 *  4. Significance LOD — BT/perception/anim rate scaling by distance.                                [later]
 *  5. Director pacing — intensity curves over the alert stream already flowing through here.         [much later]
 */
UCLASS()
class AZ_API UAZ_HordeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game/PIE worlds only — no editor-preview instances. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Controllers self-register in OnPossess / unregister in OnUnPossess (server-only by construction —
	 *  AI controllers don't exist on clients). Unregister clears any held role (tags + BB best-effort). */
	void RegisterInfected(AAZ_InfectedAIController* Controller);
	void UnregisterInfected(AAZ_InfectedAIController* Controller);

	/** A Chalkie confirmed prey (entered Aggressive): wake the pack. Every OTHER registered infected within
	 *  AlertRadius of the SCREAMER gets an URGENT investigation armed at the prey's location — they converge
	 *  and search at run gait. Promotion to an actual chase stays each receiver's own perception's job. */
	void NotifyAggro(AAZ_InfectedAIController* Instigator, const FVector& PreyLocation);

	// ---- Combat roles (rule 7 owner) ----

	/** BT attack gate. True iff the attacker holds (or is granted right now) an Active slot on THIS prey.
	 *  Inline promotion kills the recompute-lag: a lone fresh attacker never waits for the next brain beat. */
	bool RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey);

	/** Attack-task exit path. NOT a demotion: between-swings is still Active. Only clears when the
	 *  attacker no longer has this prey fresh (break-off/escape) — reassignment handles the rest. */
	void ReleaseAttackToken(AAZ_InfectedAIController* Attacker);

	/** Current role (None if unknown). Diag/AnimInstance convenience — consumers should prefer the tags. */
	EAZ_CombatRole GetCombatRole(const AAZ_InfectedAIController* Controller) const;

	// ---- Knobs (constants for now; per-variant DA_ChalkieConfig in the config batch) ----

	/** How far the aggro callout carries (cm), measured from the instigator's pawn. */
	float AlertRadius = 2000.f;
	/** Attack slots per prey — the TLOU pack rule: two press, the rest menace. */
	int32 MaxAttackersPerPrey = 2;
	/** Ranking bonus (cm) a current Active holder gets, so the pair doesn't flicker on every step. */
	float ActiveStickinessCm = 200.f;
	/** Ring hold radius (cm) for Passive fighters — written to BB RingDistance for the BT ring MoveTo. */
	float RingDistanceCm = 400.f;
	/** Brain beat — full role recompute cadence (seconds). Event paths (RequestAttackToken, unregister)
	 *  keep the map correct BETWEEN beats; the beat handles distance-based rotation and cleanup. */
	float RoleRecomputeIntervalSeconds = 0.25f;

private:
	struct FCombatRoleState
	{
		EAZ_CombatRole Role = EAZ_CombatRole::None;
		TWeakObjectPtr<const AActor> Prey;
	};

	/** The one function that mutates role state: swaps ASC tags, mirrors BB keys, logs the transition.
	 *  Everything else (beat, token grant, unregister) funnels through here — tags and BB can't diverge. */
	void ApplyRole(AAZ_InfectedAIController* Controller, EAZ_CombatRole NewRole, const AActor* Prey);

	/** Whole-pack recompute: group engaged Chalkies by prey, rank by distance (stickiness-adjusted),
	 *  top MaxAttackersPerPrey = Active, rest = Passive, disengaged = None. */
	void AssignCombatRoles();

	int32 CountActiveOnPrey(const AActor* Prey) const;

	/** Weak on purpose — controllers die with their pawns/world; the registry owns nothing. Dead entries
	 *  are compacted during sweeps. */
	TArray<TWeakObjectPtr<AAZ_InfectedAIController>> Infected;

	/** Role ledger — the member the old file-static token map graduates into (batch promotion). */
	TMap<TWeakObjectPtr<AAZ_InfectedAIController>, FCombatRoleState> Roles;

	float RoleBeatAccumulator = 0.f;
};
