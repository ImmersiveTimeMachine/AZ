// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"   // FAIStimulus (UFUNCTION param — needs the full type)
#include "AZ_InfectedAIController.generated.h"

class AAZ_PawnMoverInfectedCharacter;
class UAISenseConfig_Damage;
class UAISenseConfig_Hearing;
class UAISenseConfig_Prediction;
class UAISenseConfig_Sight;
class UAISenseConfig_Team;
class UAISenseConfig_Touch;
class UBehaviorTree;

/** Blackboard key names shared by the controller (writes at possess), the target-selection service (mirrors
 *  perception), and the BT asset (decorator/task bindings — must match the BB_Chalkie key names EXACTLY). */
namespace AZ_ChalkieBBKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName LastKnownLocation(TEXT("LastKnownLocation"));
	static const FName HomeLocation(TEXT("HomeLocation"));
	static const FName AttackRange(TEXT("AttackRange"));
	static const FName bAlerted(TEXT("bAlerted"));         // Phase 3: dormant->alerted->aggressive states
	static const FName bAggressive(TEXT("bAggressive"));   // Phase 3
}

/**
 * AAZ_InfectedAIController — controller for the Chalkie infected pawn.
 *
 * The AI side of the v2 universal-input design: it WRITES the pawn's intent surface
 * (SetMoveIntentWorld / SetDesiredFacingWorld / SetGait) on the server, and the pawn's ProduceInput turns that
 * into the deterministic Mover InputCmd — exactly where the player's Enhanced Input would feed the hero.
 *
 * NAV PATH-FOLLOW (Phase 0, verified): movement goes through the standard MoveTo/MoveToActor pipeline — the
 * inherited PathFollowingComponent auto-discovers the pawn's UNavMoverComponent (INavMovementInterface) and the
 * pawn's ProduceInput consumes the cached nav request. The Chalkie paths AROUND obstacles on the NavMesh.
 *
 * PERCEPTION (Phase 1): a full UAIPerceptionComponent with ALL engine senses configured up front — Sight
 * (realistic cone: peripheral vision angle, LOS traces, lose-radius hysteresis; the ONLY aggro source today),
 * Hearing (silent until movement/gunshot noise events report in), Damage (aggro on being hit — wired to combat
 * later), Touch (bump-detect), Prediction (BT look-ahead queries), and Team (horde alert broadcasts). Unused
 * senses cost nothing while nothing stimulates them, and having them registered NOW means future features only
 * add *report* calls, not controller surgery. Target selection is affiliation-driven (team attitude), NOT
 * GetPlayerPawn(0) — co-op-safe, per-controller.
 *
 * CROUCH-SNEAK: a sight stimulus from a CROUCHED target (hero's replicated Movement.Crouching tag) is accepted
 * only inside CrouchDetectRange — crouching shrinks the effective detection radius, standing restores it.
 *
 * TICK BRAIN (temp until the Phase-2 BehaviorTree): perceived target -> chase (Run gait) -> lose on stimulus
 * gone + grace -> stop. The BT replaces this Tick with Attack/Chase/Investigate/Patrol branches reading the
 * same perception state via Blackboard.
 */
UCLASS()
class AZ_API AAZ_InfectedAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAZ_InfectedAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// IGenericTeamAgentInterface (via AAIController) — perception affiliation asks THIS.
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// ========================================
	// Perception surface — shared truth for BOTH brains (temp Tick fallback and the BT service).
	// ========================================

	/** Poll the sight-perceived set -> PerceivedTarget / LastKnownTargetLocation / freshness stamp.
	 *  Frame-guarded: safe to call from the controller Tick AND the BT service in the same frame. */
	void UpdatePerception();

	/** The perceived hostile if the last stimulus is within the grace window; nullptr otherwise. */
	APawn* GetFreshPerceivedTarget() const;

	FVector GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }
	float GetStopDistance() const { return StopDistance; }
	AAZ_PawnMoverInfectedCharacter* GetInfectedPawn() const { return InfectedPawn.Get(); }

protected:
	/** Central perception event: every sense funnels through here; updates the target/last-known state. */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// ========================================
	// Sense configs (owned subobjects, registered on the inherited PerceptionComponent)
	// ========================================
	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Touch> TouchConfig;

	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Prediction> PredictionConfig;

	UPROPERTY(VisibleAnywhere, Category = "AZ|AI|Perception")
	TObjectPtr<UAISenseConfig_Team> TeamConfig;

	// ========================================
	// Perception tuning (pushed into the sense configs in the constructor)
	// ========================================

	/** See a standing enemy inside this range (cone + LOS still apply). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SightRadius = 800.f;

	/** Once seen, keep seeing out to this range (hysteresis; escape-by-distance threshold). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LoseSightRadius = 1500.f;

	/** HALF-angle of the vision cone, degrees. 70 = a 140-degree forward cone — sneaking up BEHIND works. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ClampMax = "180"))
	float PeripheralVisionHalfAngleDegrees = 70.f;

	/** A CROUCHED target is only accepted as seen inside this range (crouch-sneak). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CrouchDetectRange = 250.f;

	/** Hearing range for reported noise events (gunshots, sprint footsteps, impacts — reported later). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HearingRange = 1500.f;

	/** Team-sense broadcast radius (a Chalkie alerting nearby Chalkies — future horde behavior). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Perception", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TeamAlertRadius = 2000.f;

	/** Chase survives losing every stimulus for this long (corner-clip grace; longer feels more persistent). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Chase", meta = (ClampMin = "0", ForceUnits = "s"))
	float LoseTargetGraceSeconds = 3.f;

	/** Acceptance radius for the chase move: stop (and stare) once within this distance. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Chase", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StopDistance = 150.f;

	/** The Phase-2 brain: when set, RunBehaviorTree on possess and the temp Tick brain stands down.
	 *  Assigned on the BP subclass (BP_AZ_InfectedAIController) so native code holds no content path. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Brain")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** Tick-driven chase brain — FALLBACK when no BehaviorTreeAsset is set (kept for A/B debugging). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Chase")
	bool bTickBrainEnabled = true;

	/** ~1 Hz Output Log line with speed / distance / perception state (tuning aid). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Debug")
	bool bLogChaseProbe = true;

	// ========================================
	// Perceived-target state (written by OnTargetPerceptionUpdated, read by the Tick brain; the Phase-2
	// Blackboard mirrors exactly these three: TargetActor / LastKnownLocation / freshness)
	// ========================================
	TWeakObjectPtr<APawn> PerceivedTarget;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	double  LastStimulusTimeSeconds = -1000.0;

	/** UpdatePerception frame guard (GFrameCounter of the last poll). */
	uint64 LastPerceptionPollFrame = 0;

	TWeakObjectPtr<AAZ_PawnMoverInfectedCharacter> InfectedPawn;
};
