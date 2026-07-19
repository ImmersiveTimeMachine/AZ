// Copyright Artur. AZ project.

#include "AI/AZ_InfectedAIController.h"

#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait
#include "AZ_GameplayTags.h"                 // Movement.Crouching (crouch-sneak detection range)
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Character/AZ_PawnMoverComponent.h"    // TEMP: speed probe reads GetVelocity
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "Navigation/PathFollowingComponent.h"   // EPathFollowingStatus
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Prediction.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Team.h"
#include "Perception/AISenseConfig_Touch.h"

AAZ_InfectedAIController::AAZ_InfectedAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// ========================================
	// Perception: ALL senses registered up front. Unused ones are inert until something REPORTS to them
	// (noise / damage / touch / team events) — future features add report calls, not controller changes.
	// ========================================
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent")));

	// --- Sight: the only aggro source today. Realistic forward cone + engine LOS traces + lose-radius
	// hysteresis. Affiliation: enemies + neutrals (neutrals kept ON as a safety net until every actor has a
	// team; the Tick/handler still hard-filter to Hostile via GetTeamAttitudeTowards). Friendlies OFF so a
	// horde doesn't perceive-spam itself; ally alerts are the Team sense's job.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionHalfAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies   = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals  = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;
	SightConfig->SetMaxAge(5.f);
	GetPerceptionComponent()->ConfigureSense(*SightConfig);

	// --- Hearing: reacts to UAISense_Hearing::ReportNoiseEvent — gunshots, sprint footsteps, obstacle
	// impacts (the obstacle-reaction system's AI-noise decision reports here). Nothing reports yet.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies   = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals  = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	HearingConfig->SetMaxAge(8.f);
	GetPerceptionComponent()->ConfigureSense(*HearingConfig);

	// --- Damage: aggro-on-hit. Combat (Phase 4) calls UAISense_Damage::ReportDamageEvent from the GAS
	// damage pipeline; a shot Chalkie then knows WHO hit it and from WHERE even without sight.
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(10.f);
	GetPerceptionComponent()->ConfigureSense(*DamageConfig);

	// --- Touch: bump-detect (player brushing a dormant Chalkie in the dark wakes it — Phase 3 flavor).
	TouchConfig = CreateDefaultSubobject<UAISenseConfig_Touch>(TEXT("TouchConfig"));
	TouchConfig->SetMaxAge(5.f);
	GetPerceptionComponent()->ConfigureSense(*TouchConfig);

	// --- Prediction: "where will the target BE" queries (BT chase look-ahead / cutoff behavior, Phase 2+).
	PredictionConfig = CreateDefaultSubobject<UAISenseConfig_Prediction>(TEXT("PredictionConfig"));
	PredictionConfig->SetMaxAge(5.f);
	GetPerceptionComponent()->ConfigureSense(*PredictionConfig);

	// --- Team: ally alert broadcasts (a Chalkie that spots prey screams it to the horde via
	// FAITeamStimulusEvent within TeamAlertRadius — Phase 3/5). Broadcast range lives on the EVENT.
	TeamConfig = CreateDefaultSubobject<UAISenseConfig_Team>(TEXT("TeamConfig"));
	TeamConfig->SetMaxAge(10.f);
	GetPerceptionComponent()->ConfigureSense(*TeamConfig);

	GetPerceptionComponent()->SetDominantSense(*SightConfig->GetSenseImplementation());
}

void AAZ_InfectedAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AAZ_InfectedAIController::OnTargetPerceptionUpdated);
	}
}

void AAZ_InfectedAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InfectedPawn = Cast<AAZ_PawnMoverInfectedCharacter>(InPawn);

	// Adopt the pawn's faction so perception affiliation (and GetTeamAttitudeTowards) answers for THIS body.
	if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		SetGenericTeamId(PawnTeam->GetGenericTeamId());
	}

	// Phase-2 brain: run the BehaviorTree (uses the BT's own BlackboardAsset). The temp Tick brain stands
	// down automatically (it checks BrainComponent->IsRunning). Seed the static keys the tree navigates by.
	if (BehaviorTreeAsset && InPawn)
	{
		RunBehaviorTree(BehaviorTreeAsset);
		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			BB->SetValueAsVector(AZ_ChalkieBBKeys::HomeLocation, InPawn->GetActorLocation());
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::AttackRange, StopDistance);
		}
	}
}

void AAZ_InfectedAIController::OnUnPossess()
{
	if (AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get())
	{
		// Don't leave a stale intent latched on the pawn after we let go of it.
		P->SetMoveIntentWorld(FVector::ZeroVector);
		P->SetDesiredFacingWorld(FVector::ZeroVector);
	}
	InfectedPawn = nullptr;
	PerceivedTarget = nullptr;
	Super::OnUnPossess();
}

ETeamAttitude::Type AAZ_InfectedAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Resolve the other side's team: the actor itself, else its controller (players carry team on the pawn).
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);
	if (!OtherTeamAgent)
	{
		if (const APawn* OtherPawn = Cast<const APawn>(&Other))
		{
			OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController());
		}
	}

	const FGenericTeamId OtherTeam = OtherTeamAgent ? OtherTeamAgent->GetGenericTeamId() : FGenericTeamId::NoTeam;
	if (OtherTeam == FGenericTeamId::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}
	return OtherTeam == GetGenericTeamId() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void AAZ_InfectedAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Event-driven bookkeeping: record WHERE hostiles were last stimulated (sight-lost location, heard-noise
	// location, damage direction). The Phase-2 Blackboard's LastKnownLocation / Investigate branch reads this.
	// Target PROMOTION stays in the Tick poll (sight) — hearing/damage/touch promote via the BT later, so a
	// noise makes a Chalkie investigate, not laser-lock.
	if (!Actor || GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}
	LastKnownTargetLocation = Stimulus.StimulusLocation;
}

void AAZ_InfectedAIController::UpdatePerception()
{
	// Frame guard: the controller Tick AND the BT service both call this; poll once per frame.
	if (LastPerceptionPollFrame == GFrameCounter)
	{
		return;
	}
	LastPerceptionPollFrame = GFrameCounter;

	AAZ_PawnMoverInfectedCharacter* InfectedCharacter = InfectedPawn.Get();
	UAIPerceptionComponent* Perception = GetPerceptionComponent();
	if (!InfectedCharacter || !Perception)
	{
		return;
	}

	// --- Target selection: POLL the sight-perceived set (cone + LOS + radius already applied by the sense).
	// Polling (not events) so the crouch rule re-evaluates as DISTANCE changes: a crouched hostile beyond
	// CrouchDetectRange stays unnoticed even though the sense "sees" it; once already OUR target, crouching
	// in plain sight no longer helps.
	TArray<AActor*> Seen;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);

	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AActor* SeenActor : Seen)
	{
		APawn* SeenPawn = Cast<APawn>(SeenActor);
		if (!SeenPawn || GetTeamAttitudeTowards(*SeenActor) != ETeamAttitude::Hostile)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(InfectedCharacter->GetActorLocation(), SeenPawn->GetActorLocation());
		if (PerceivedTarget.Get() != SeenPawn)
		{
			// Crouch-sneak: a crouched NEW target only registers inside CrouchDetectRange.
			const IGameplayTagAssetInterface* TargetTags = Cast<IGameplayTagAssetInterface>(SeenActor);
			const bool bCrouched =
				TargetTags && TargetTags->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Movement_Crouching);
			if (bCrouched && DistSq > FMath::Square(CrouchDetectRange))
			{
				continue;
			}
		}
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = SeenPawn;
		}
	}

	if (Best)
	{
		PerceivedTarget = Best;
		LastKnownTargetLocation = Best->GetActorLocation();
		LastStimulusTimeSeconds = FPlatformTime::Seconds();
	}
}

APawn* AAZ_InfectedAIController::GetFreshPerceivedTarget() const
{
	APawn* Target = PerceivedTarget.Get();
	const bool bFresh = Target && (FPlatformTime::Seconds() - LastStimulusTimeSeconds) < LoseTargetGraceSeconds;
	return bFresh ? Target : nullptr;
}

void AAZ_InfectedAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AAZ_PawnMoverInfectedCharacter* InfectedCharacter = InfectedPawn.Get();
	if (!InfectedCharacter)
	{
		return;
	}

	UpdatePerception();

	const double NowSeconds = FPlatformTime::Seconds();
	APawn* Target = PerceivedTarget.Get();
	const bool bTargetFresh = (GetFreshPerceivedTarget() != nullptr);
	const bool bChasing = GetMoveStatus() != EPathFollowingStatus::Idle;

	// Phase-2 brain active? The BT (service + branch tasks) owns decisions; this Tick keeps only the shared
	// plumbing below (perception poll above, probe log, arrived-stare facing, control-rotation sync).
	const bool bBTActive = BrainComponent && BrainComponent->IsRunning();

	float Distance = 0.f;
	FVector ToTarget = FVector::ZeroVector;
	if (Target)
	{
		ToTarget = Target->GetActorLocation() - InfectedCharacter->GetActorLocation();
		ToTarget.Z = 0.f;
		Distance = ToTarget.Size();
	}

	// TEMP speed probe (tuning aid): ~1 Hz, Output Log.
	if (bLogChaseProbe)
	{
		static double LastSpeedLogTime = 0.0;
		if (NowSeconds - LastSpeedLogTime > 1.0)
		{
			LastSpeedLogTime = NowSeconds;
			const UAZ_PawnMoverComponent* Mover = InfectedCharacter->GetMoverComponent();
			const float SpeedXY = Mover ? Mover->GetVelocity().Size2D() : -1.f;
			UE_LOG(LogTemp, Display, TEXT("[Chalkie] SpeedXY=%.0f cm/s | Dist=%.0f | %s | %s | Mode=%s"),
				SpeedXY, Distance,
				bChasing ? TEXT("CHASING") : TEXT("idle"),
				bTargetFresh ? TEXT("target-FRESH") : (Target ? TEXT("target-stale") : TEXT("no-target")),
				Mover ? *Mover->GetMovementModeName().ToString() : TEXT("?"));
		}
	}

	// --- Fallback chase brain: only when NO BehaviorTree is running (A/B debugging, missing asset) ---
	if (!bBTActive && bTickBrainEnabled)
	{
		if (bChasing)
		{
			if (!bTargetFresh)
			{
				// Lost them — outrun past LoseSightRadius, hidden past the grace window, or sneaked from the
				// cone: give up where we stand. (The BT Investigates LastKnownTargetLocation instead.)
				StopMovement();
				InfectedCharacter->SetDesiredFacingWorld(FVector::ZeroVector);
				InfectedCharacter->SetGait(EAZ_Gait::Walk);
				PerceivedTarget = nullptr;
				return;
			}
			// Keep chasing: the path OBSERVES the goal actor and repaths as the player moves.
			InfectedCharacter->SetGait(EAZ_Gait::Run);
		}
		else if (bTargetFresh && Distance > StopDistance)
		{
			// Perceived a hostile IN VIEW: nav-path to them (goes AROUND obstacles).
			MoveToActor(Target, StopDistance);
			InfectedCharacter->SetGait(EAZ_Gait::Run);
		}
	}

	// Facing: while pathing, face the PATH direction (zero desired facing -> ProduceInput faces the move), so the
	// Chalkie looks where it walks when rounding corners instead of staring through walls at the target. When
	// stopped INSIDE StopDistance with a live target (caught up), stare at them.
	const bool bArrived = bTargetFresh && Distance <= StopDistance && GetMoveStatus() == EPathFollowingStatus::Idle;
	InfectedCharacter->SetDesiredFacingWorld(bArrived ? ToTarget.GetSafeNormal() : FVector::ZeroVector);

	// Keep the controller's control rotation aligned with the BODY facing so the AnimInstance's AimingRotation
	// (read from the controller) yields ~zero rotation offset (no AO fighting the path direction).
	SetControlRotation(InfectedCharacter->GetActorRotation());
}
