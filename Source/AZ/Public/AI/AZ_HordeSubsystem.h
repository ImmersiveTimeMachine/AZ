// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"   // FObjectKey (grab-token map key)
#include "AZ_HordeSubsystem.generated.h"

class AAZ_InfectedAIController;

/** Crowd combat role — assigned by UAZ_HordeSubsystem (sole writer), consumed by BT (BB mirror), the
 *  AnimInstance (ASC tags: menace posture), and future GEs (taunt/fear can force roles). */
UENUM()
enum class EAZ_CombatRole : uint8
{
	None,      // not engaged (no fresh target)
	Active,    // holds one of the per-prey attack slots: press the attack
	Passive    // engaged, no slot: circle at an assigned ring slot, menace, wait for a turn
};

/**
 * Horde coordination layer — THE CROWD BRAIN (v3 doctrine, 2026-07-22).
 *
 *   controller perception -> FACTS (seen/heard/pain, fresh target)
 *   THIS SUBSYSTEM        -> DECISIONS (who fights, who circles, WHERE they stand, whose TURN it is)
 *   behavior tree         -> EXECUTION (move/scan/attack per role)
 *
 * v2 added Active/Passive roles (rule 7). v3 adds the two ingredients that make a pack read as
 * choreography instead of a queue:
 *
 *  RING SLOTS ("kung-fu circle"): per prey, NumRingSlots angular positions at RingDistanceCm. Every
 *  Passive is assigned a slot and the BT Ring branch paths to BB SlotLocation — never at the prey — so
 *  observers SURROUND instead of stacking behind each other (pawn capsules block; staging must spread).
 *  The ring drifts slowly and observers occasionally shuffle to an adjacent slot: the TLOU circling read.
 *
 *  TURN ROTATION: an Active's turn expires after a random ActiveHold window; the beat then swaps it with
 *  the best ELIGIBLE Passive (MinPassiveSeconds cooldown prevents ping-pong; a mid-bite Active is never
 *  interrupted — the melee task publishes IsMeleeTaskActive). A lone attacker never rotates out (there is
 *  nobody to hand the turn to). Rotation is expressed as RANKING MODIFIERS inside one sort — grants,
 *  expiries, and eligibility can never disagree because there is only one decision pass.
 *
 * Roles/slots are published atomically through ApplyRole (ASC tags + BB keys + timers). Investigation
 * stays suggest-only. A single Chalkie in an empty level behaves exactly as it does inside a horde.
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
	 *  AI controllers don't exist on clients). Unregister clears any held role/slot (tags + BB best-effort). */
	void RegisterInfected(AAZ_InfectedAIController* Controller);
	void UnregisterInfected(AAZ_InfectedAIController* Controller);

	/** A Chalkie confirmed prey (entered Aggressive): wake the pack. Every OTHER registered infected within
	 *  AlertRadius of the SCREAMER gets an URGENT investigation armed at the prey's location. */
	void NotifyAggro(AAZ_InfectedAIController* Instigator, const FVector& PreyLocation);

	// ---- Combat roles (rule 7 owner) ----

	/** BT attack gate. True iff the attacker holds (or is granted right now) an Active slot on THIS prey.
	 *  Inline promotion kills the recompute-lag for ANY fresh attacker while a slot is open on this prey
	 *  (not only a lone first attacker — a second slot opening mid-fight promotes just as fast); a
	 *  recently rotated-out Chalkie (inside its MinPassiveSeconds window) is NOT inline-promoted — the
	 *  beat decides its return. */
	bool RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey);

	/** Attack-task exit path. NOT a demotion: between-swings is still Active. Only clears when the
	 *  attacker no longer has this prey fresh (break-off/escape) — reassignment handles the rest. */
	void ReleaseAttackToken(AAZ_InfectedAIController* Attacker);

	/** Current role (None if unknown). Diag/AnimInstance convenience — consumers should prefer the tags. */
	EAZ_CombatRole GetCombatRole(const AAZ_InfectedAIController* Controller) const;

	// ---- Grab arbitration (one grabber per prey; sibling of the attack token) ----

	/** Grab gate: true iff nobody else currently holds the grab on THIS prey (or the holder is stale).
	 *  Granted to the caller on success. The winner still holds its regular attack token — a grab IS an
	 *  attack from the crowd brain's point of view (SetMeleeTaskActive keeps it Locked). */
	bool RequestGrabToken(AAZ_InfectedAIController* Grabber, const AActor* Prey);

	/** Grab-task exit path — every grab exit (escape, timeout, cancel) funnels through this. */
	void ReleaseGrabToken(AAZ_InfectedAIController* Grabber);

	/** GRAB THEATRICS (user 2026-07-24): a pack-mate seized the prey — every OTHER Chalkie engaged on
	 *  this prey recoils: plays its variant KB montage (GrabEscapeMontage from its anim-set DA) for
	 *  StepBackSeconds, then blends back out (the full clip is a 5.5s knockback; this is just the beat).
	 *  Movement is stopped for the beat so the recoil doesn't play over locomotion. */
	void NotifyGrabStarted(AAZ_InfectedAIController* Grabber, const AActor* Prey, float StepBackSeconds);

	// ---- Crowd intensity (PER named crowd) ----

	/** Set the aggression level of ONE crowd, 1 (wary) .. 5 (frenzy). The level selects a row of knobs
	 *  (attacker count, ring distance, turn cadence, pack-alert radius) resolved on demand from a fixed
	 *  table — see GIntensityTable in the .cpp. Takes effect from the next brain beat: new grants use the
	 *  new row, running holds keep their already-seeded durations. Callable from encounter scripts. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Crowd")
	void SetCrowdIntensity(FName CrowdId, int32 Level);

	/** This crowd's current level (defaults to 3 for any crowd never seeded). */
	UFUNCTION(BlueprintPure, Category = "AZ|Crowd")
	int32 GetCrowdIntensity(FName CrowdId) const;

	// ---- Global knobs (NOT intensity-driven; shared by all crowds) ----
	// The intensity-driven knobs (alert radius, attacker count, hold/bench/shuffle windows, ring
	// distance) live in the per-level table in the .cpp — they are resolved through GetCrowdIntensity,
	// never stored here. Only the structural/quality knobs below are global constants.

	/** Ranking bonus (cm) a current Active holder gets, so the pair doesn't flicker on every step. */
	float ActiveStickinessCm = 200.f;
	/** Brain beat — full role/ring recompute cadence (seconds). */
	float RoleRecomputeIntervalSeconds = 0.25f;
	/** Angular slots on the ring. More than the pack can fill — free slots give shuffles room to breathe. */
	int32 NumRingSlots = 6;
	/** Whole-ring angular drift; keeps even a static pack subtly on the move. HARD CONSTRAINT: the
	 *  tangential speed (ring radius * drift in rad/s) must stay well under the Ring branch's WALK gait
	 *  speed, or observers chase slots they can never catch — never posted, never facing the prey. At
	 *  3 deg/s x the widest ring (500cm) the slot arcs at ~26cm/s, under the ~30-45cm/s walk. */
	float RingDriftDegPerSecond = 3.f;
	/** SlotLocation is rewritten only when the slot moved this far — BB write hygiene (the Ring MoveTo
	 *  observes the key; sub-threshold jitter must not spam re-paths). */
	float SlotWriteHysteresisCm = 50.f;
	/** Within this range of its slot an observer is "posted": stop-and-face-the-prey behavior. */
	float SlotArrivedToleranceCm = 150.f;

private:
	struct FCombatRoleState
	{
		EAZ_CombatRole Role = EAZ_CombatRole::None;
		TWeakObjectPtr<const AActor> Prey;
		FName CrowdId = NAME_None;              // the crowd this role was granted under (owns the ring/knobs)
		int32 SlotIndex = INDEX_NONE;          // ring slot while Passive
		double ActiveSinceSeconds = 0.0;       // turn timer (Active)
		double ActiveHoldSeconds = 0.0;        // randomized turn length (Active)
		double EligibleAfterSeconds = 0.0;     // earliest re-promotion time (after losing Active)
		double NextShuffleSeconds = 0.0;       // next slot hop (Passive)
		FVector LastWrittenSlot = FVector::ZeroVector;
		bool bWroteSlot = false;
		bool bFacingOverridden = false;        // we set the controller's facing override (must clear it)
		FVector LastFacingSet = FVector::ZeroVector;   // the value we last wrote — clear only if still current
	};

	struct FFightRing
	{
		float PhaseDeg = 0.f;
		TArray<TWeakObjectPtr<AAZ_InfectedAIController>> Slots;   // NumRingSlots entries, null = free
	};

	/** Rings are keyed by (crowd, prey), NOT prey alone: two crowds attacking the SAME prey each form
	 *  their own circle at their own intensity's radius, instead of fighting over one shared ring. */
	struct FCrowdPreyKey
	{
		FName CrowdId = NAME_None;
		TWeakObjectPtr<const AActor> Prey;
		bool operator==(const FCrowdPreyKey& Other) const
		{
			return CrowdId == Other.CrowdId && Prey == Other.Prey;
		}
		friend uint32 GetTypeHash(const FCrowdPreyKey& Key)
		{
			return HashCombine(GetTypeHash(Key.CrowdId), GetTypeHash(Key.Prey));
		}
	};

	/** The one function that mutates role state: swaps ASC tags, mirrors BB keys, seeds/clears timers,
	 *  releases slots and facing. Everything funnels through here — publishers can't diverge. */
	void ApplyRole(AAZ_InfectedAIController* Controller, EAZ_CombatRole NewRole, const AActor* Prey);

	/** Whole-pack decision pass: rank candidates per (crowd, prey); the top MaxAttackers of THAT crowd =
	 *  Active, rest = Passive, disengaged = None. Each crowd contests only its own attacker budget. */
	void AssignCombatRoles();

	/** Choreography pass: drift rings, assign/shuffle slots, project to navmesh, write BB SlotLocation,
	 *  manage posted-observer facing. Runs after AssignCombatRoles each beat. */
	void UpdateFightRings(float BeatDeltaSeconds);

	/** Active holders on a prey WITHIN one crowd (crowds don't consume each other's attacker slots). */
	int32 CountActiveOnPrey(const AActor* Prey, FName CrowdId) const;
	void ReleaseSlot(FCombatRoleState& State);
	void ClearManagedFacing(AAZ_InfectedAIController* Controller, FCombatRoleState& State);
	FVector ComputeSlotWorldLocation(const FFightRing& Ring, int32 SlotIndex, float RingRadius, const FVector& PreyLocation) const;

	/** Weak on purpose — controllers die with their pawns/world; the registry owns nothing. */
	TArray<TWeakObjectPtr<AAZ_InfectedAIController>> Infected;

	/** Role ledger. */
	TMap<TWeakObjectPtr<AAZ_InfectedAIController>, FCombatRoleState> Roles;

	/** Per-(crowd,prey) kung-fu circles. Entries die with their prey (weak keys, compacted each beat). */
	TMap<FCrowdPreyKey, FFightRing> Rings;

	/** Aggression level (1..5) per crowd id. Absent = never seeded = treated as level 3. Knob values are
	 *  resolved on demand from the intensity table — this map stores ONLY the level. */
	TMap<FName, int32> CrowdLevels;

	/** One grabber per prey. Weak holder: a dead/despawned grabber auto-frees the slot on next request. */
	TMap<FObjectKey, TWeakObjectPtr<AAZ_InfectedAIController>> GrabTokenByPrey;

	float RoleBeatAccumulator = 0.f;

	/** Last az.Crowd.Intensity value applied as a GLOBAL test override (0 = off). Re-applies to all known
	 *  crowds only when the CVar itself changes, so a scripted per-crowd SetCrowdIntensity isn't stomped. */
	int32 LastGlobalIntensityCVar = 0;
};
