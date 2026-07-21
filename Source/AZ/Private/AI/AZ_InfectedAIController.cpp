// Copyright Artur. AZ project.

#include "AI/AZ_InfectedAIController.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"   // AddStateTag/RemoveStateTag (replicated phase tags)
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
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Prediction.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Team.h"
#include "AI/AZ_HordeSubsystem.h"
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

	// Default to the infected faction OUTRIGHT — never depend on adoption timing. OnPossess still adopts
	// the pawn's team when the pawn has one, but if possession beats the pawn's own team init (placed-pawn
	// PostInitializeComponents possession), this keeps us on team 1 instead of NoTeam (which made every
	// other Chalkie read as Hostile → horde in-fighting).
	SetGenericTeamId(FGenericTeamId(1));
}

void AAZ_InfectedAIController::BeginPlay()
{
	Super::BeginPlay();

	ApplyPerceptionTuning();

	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AAZ_InfectedAIController::OnTargetPerceptionUpdated);
	}
}

void AAZ_InfectedAIController::ApplyPerceptionTuning()
{
	// The constructor configures senses from NATIVE defaults (it runs before Blueprint property overrides
	// deserialize). Re-pushing here makes the top-level tuning floats the single source of truth — edit them
	// on BP_AZ_InfectedAIController and they actually take effect.
	UAIPerceptionComponent* Perception = GetPerceptionComponent();
	if (!Perception)
	{
		return;
	}
	if (SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionHalfAngleDegrees;
		Perception->ConfigureSense(*SightConfig);
	}
	if (HearingConfig)
	{
		HearingConfig->HearingRange = HearingRange;
		Perception->ConfigureSense(*HearingConfig);
	}
	Perception->RequestStimuliListenerUpdate();
}

void AAZ_InfectedAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InfectedPawn = Cast<AAZ_PawnMoverInfectedCharacter>(InPawn);

	// Adopt the pawn's faction so perception affiliation (and GetTeamAttitudeTowards) answers for THIS body.
	// NoTeam guard: placed-pawn possession can run before the pawn's own team init — never adopt "no team"
	// over our constructor default (team 1), or every other Chalkie reads as Hostile and the horde fights itself.
	if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		if (PawnTeam->GetGenericTeamId() != FGenericTeamId::NoTeam)
		{
			SetGenericTeamId(PawnTeam->GetGenericTeamId());
		}
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

			// Pacing seed — the single source for every BT Wait bound to these keys. Constants for now;
			// becomes a per-variant read from DA_ChalkieConfig in the config-DA batch. RandomDeviation
			// on the nodes jitters around these bases per execution.
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::WaitChaseBreather, 0.5f);
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::WaitSearchPoint, 1.0f);
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::WaitSearchSettle, 1.5f);
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::WaitHomeArrive, 5.0f);
			BB->SetValueAsFloat(AZ_ChalkieBBKeys::WaitHomeWander, 8.0f);
		}
	}

	// Join the pack registry (server-only by construction — AI controllers don't exist on clients).
	if (UAZ_HordeSubsystem* Horde = GetWorld() ? GetWorld()->GetSubsystem<UAZ_HordeSubsystem>() : nullptr)
	{
		Horde->RegisterInfected(this);
	}

	// Seed the initial phase tag on the pawn's ASC (SetPhase early-outs on no-change, so publish directly).
	// OnPossess is server-only; AddStateTag replicates the tag to client-side views of this Chalkie.
	if (AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get())
	{
		if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(P->GetAbilitySystemComponent()))
		{
			ASC->AddStateTag(FAZ_GameplayTags::Get().State_Infected_Dormant);
		}
	}
	CurrentPhase = EAZ_InfectedPhase::Dormant;
}

void AAZ_InfectedAIController::OnUnPossess()
{
	if (UAZ_HordeSubsystem* Horde = GetWorld() ? GetWorld()->GetSubsystem<UAZ_HordeSubsystem>() : nullptr)
	{
		Horde->UnregisterInfected(this);
	}
	if (AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get())
	{
		// Don't leave a stale intent latched on the pawn after we let go of it.
		P->SetMoveIntentWorld(FVector::ZeroVector);
		P->SetDesiredFacingWorld(FVector::ZeroVector);
	}
	InfectedPawn = nullptr;
	PerceivedTarget = nullptr;
	AlertCandidate = nullptr;
	FacingOverrideWorld = FVector::ZeroVector;
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
	// location, damage direction). Target PROMOTION stays in the Tick poll (sight) — noises make a Chalkie
	// INVESTIGATE, not laser-lock.
	if (!Actor || GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}
	LastKnownTargetLocation = Stimulus.StimulusLocation;

	// TLOU noise-investigation: a HEARD hostile noise while not already chasing arms the Investigate branch
	// directly — sprint footsteps, gunshots, thrown objects (whatever reports to UAISense_Hearing) pull the
	// Chalkie to the SOUND location. Sight handles its own promotion; hearing only ever points, never locks.
	// Heard-only = CALM arm (wary Walk-gait investigation) — escalation memory promotes repeats to urgent.
	if (Stimulus.WasSuccessfullySensed()
		&& Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>()
		&& GetFreshPerceivedTarget() == nullptr)
	{
		ArmInvestigation(Stimulus.StimulusLocation, /*bUrgent*/ false);
	}
}

void AAZ_InfectedAIController::ArmInvestigation(const FVector& Location, bool bUrgent)
{
	const double NowSeconds = FPlatformTime::Seconds();

	// Episode counting: a burst of stimuli (running footsteps re-arm every noise interval) is ONE
	// investigation. A new EPISODE needs a quiet gap; the escalation window lapsing resets the chain.
	if (NowSeconds - LastInvestigationTimeSeconds > EscalationWindowSeconds)
	{
		RecentInvestigationCount = 1;
	}
	else if (NowSeconds - LastInvestigationTimeSeconds > InvestigationEpisodeGapSeconds)
	{
		++RecentInvestigationCount;
	}
	else
	{
		RecentInvestigationCount = FMath::Max(RecentInvestigationCount, 1);
	}
	LastInvestigationTimeSeconds = NowSeconds;

	LastKnownTargetLocation = Location;
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsVector(AZ_ChalkieBBKeys::LastKnownLocation, Location);
		BB->SetValueAsBool(AZ_ChalkieBBKeys::bInvestigateUrgent, bUrgent || IsInvestigationEscalated());
	}
}

bool AAZ_InfectedAIController::IsInvestigationEscalated() const
{
	return RecentInvestigationCount >= EscalationThreshold
		&& (FPlatformTime::Seconds() - LastInvestigationTimeSeconds) <= EscalationWindowSeconds;
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

	const double NowSeconds = FPlatformTime::Seconds();

	// Calm-down + stale-clear: once the grace window fully expires, forget the target ENTIRELY. This both
	// re-arms the alert beat for the next encounter and fixes a subtle bug — a stale PerceivedTarget kept the
	// "already my target" crouch exemption forever, so crouch-sneak stopped working after the first chase.
	if (PerceivedTarget.IsValid() && (NowSeconds - LastStimulusTimeSeconds) > LoseTargetGraceSeconds)
	{
		PerceivedTarget = nullptr;
	}

	// Proximity retention (TLOU close-range rule): a target we're ALREADY on can't be "lost" at arm's
	// length by slipping out of the sight cone — within InstantDetectRange the Chalkie hears/feels them.
	// Without this, a player strafing around an attacking zombie exits the 70-degree cone, the grace
	// expires mid-approach, and the brain absurdly drops to Investigate AT the player's own location.
	// Only refreshes an existing engagement (never acquires) and only while still within grace.
	if (!Best)
	{
		if (const APawn* Retained = PerceivedTarget.Get())
		{
			const float RetainDistSq = FVector::DistSquared2D(InfectedCharacter->GetActorLocation(), Retained->GetActorLocation());
			if (RetainDistSq <= FMath::Square(InstantDetectRange))
			{
				LastKnownTargetLocation = Retained->GetActorLocation();
				LastStimulusTimeSeconds = NowSeconds;
			}
		}
	}

	if (Best)
	{
		if (PerceivedTarget.Get() == Best)
		{
			// Already aggressive on this target: keep the chase fresh.
			LastKnownTargetLocation = Best->GetActorLocation();
			LastStimulusTimeSeconds = NowSeconds;
		}
		else
		{
			// NEW target: TLOU-style reaction beat. Close range = instant ("it's right there!"); farther out the
			// Chalkie freezes and faces the stimulus for AlertDelaySeconds before committing to the chase.
			const float DistSq = FVector::DistSquared2D(InfectedCharacter->GetActorLocation(), Best->GetActorLocation());
			const bool bInstant = DistSq <= FMath::Square(InstantDetectRange);

			if (AlertCandidate.Get() != Best)
			{
				AlertCandidate = Best;
				AlertStartTimeSeconds = NowSeconds;
			}
			if (bInstant || (NowSeconds - AlertStartTimeSeconds) >= AlertDelaySeconds)
			{
				// Commit: alerted -> aggressive.
				PerceivedTarget = Best;
				LastKnownTargetLocation = Best->GetActorLocation();
				LastStimulusTimeSeconds = NowSeconds;
				AlertCandidate = nullptr;
			}
		}
	}
	else if (APawn* LostGlimpse = AlertCandidate.Get())
	{
		// Saw SOMETHING for under the alert delay, then lost it — the TLOU "huh?" moment. Arm the Investigate
		// branch with the glimpse location so the Chalkie walks over to check instead of shrugging it off.
		// A glimpse is a CALM arm — it isn't sure, so it walks over warily.
		ArmInvestigation(LostGlimpse->GetActorLocation(), /*bUrgent*/ false);
		AlertCandidate = nullptr;
	}

	// TEMP DIAGNOSTIC (blindness hunt 2026-07-21, remove after): every decision input, ~1.5Hz per zombie.
	// Throttle is PER INSTANCE (a shared static let the first-ticking zombie hog the log and hid the
	// broken one entirely — the diag bug that delayed the diagnosis).
	{
		static TMap<const void*, double> LastDiagTimes;
		double& LastDiagTime = LastDiagTimes.FindOrAdd(this, 0.0);
		if (NowSeconds - LastDiagTime > 0.7)
		{
			LastDiagTime = NowSeconds;
			const APawn* Hero = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
			float HeroDist = -1.f; bool bHeroCrouchTag = false; ETeamAttitude::Type HeroAttitude = ETeamAttitude::Neutral;
			if (Hero)
			{
				HeroDist = FVector::Dist2D(InfectedCharacter->GetActorLocation(), Hero->GetActorLocation());
				if (const IGameplayTagAssetInterface* HeroTags = Cast<IGameplayTagAssetInterface>(Hero))
				{
					bHeroCrouchTag = HeroTags->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Movement_Crouching);
				}
				HeroAttitude = GetTeamAttitudeTowards(*Hero);
			}
			UE_LOG(LogTemp, Display, TEXT("[ChalkieDiag] %s seen=%d best=%s tgt=%s fresh=%d cand=%s phase=%d heroDist=%.0f heroCrouchTag=%d heroAttitude=%d"),
				*GetNameSafe(InfectedCharacter), Seen.Num(), *GetNameSafe(Best), *GetNameSafe(PerceivedTarget.Get()),
				GetFreshPerceivedTarget() != nullptr, *GetNameSafe(AlertCandidate.Get()), static_cast<int32>(CurrentPhase),
				HeroDist, bHeroCrouchTag, static_cast<int32>(HeroAttitude));
		}
	}

	// Resolve + publish the phase: aggressive = committed target; alerted = reaction beat OR an armed
	// investigation (LastKnownLocation pending); dormant = nothing going on. SetPhase mirrors to BB + ASC.
	EAZ_InfectedPhase Phase = EAZ_InfectedPhase::Dormant;
	if (GetFreshPerceivedTarget() != nullptr)
	{
		Phase = EAZ_InfectedPhase::Aggressive;
	}
	else if (AlertCandidate.IsValid()
		|| (GetBlackboardComponent() && GetBlackboardComponent()->IsVectorValueSet(AZ_ChalkieBBKeys::LastKnownLocation)))
	{
		Phase = EAZ_InfectedPhase::Alerted;
	}
	SetPhase(Phase);
}

void AAZ_InfectedAIController::SetPhase(EAZ_InfectedPhase NewPhase)
{
	if (NewPhase == CurrentPhase)
	{
		return;
	}
	const EAZ_InfectedPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	// ASC = the replicated truth (client Chalkie AnimInstances read these tags; the controller is server-only).
	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
	auto PhaseTag = [&AZTags](EAZ_InfectedPhase Phase) -> const FGameplayTag&
	{
		switch (Phase)
		{
		case EAZ_InfectedPhase::Aggressive: return AZTags.State_Infected_Aggressive;
		case EAZ_InfectedPhase::Alerted:    return AZTags.State_Infected_Alerted;
		default:                            return AZTags.State_Infected_Dormant;
		}
	};
	if (AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get())
	{
		if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(P->GetAbilitySystemComponent()))
		{
			ASC->RemoveStateTag(PhaseTag(OldPhase));
			ASC->AddStateTag(PhaseTag(NewPhase));
		}
	}

	// BB mirrors (BT-local conveniences for decorators/branches).
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsBool(AZ_ChalkieBBKeys::bAlerted, NewPhase == EAZ_InfectedPhase::Alerted);
		BB->SetValueAsBool(AZ_ChalkieBBKeys::bAggressive, NewPhase == EAZ_InfectedPhase::Aggressive);
	}

	// Pack callout: confirmed prey wakes nearby infected — they get an URGENT investigation at the prey's
	// position (Investigate branch, run gait). Their own sight promotes it to a chase if they get eyes on.
	// Fires on every (re-)entry into Aggressive, so a chase that flickers re-calls with a FRESH location.
	if (NewPhase == EAZ_InfectedPhase::Aggressive)
	{
		if (UAZ_HordeSubsystem* Horde = GetWorld() ? GetWorld()->GetSubsystem<UAZ_HordeSubsystem>() : nullptr)
		{
			const APawn* Prey = GetFreshPerceivedTarget();
			Horde->NotifyAggro(this, Prey ? Prey->GetActorLocation() : LastKnownTargetLocation);
		}
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
	// stopped INSIDE StopDistance with a live target (caught up), stare at them. While ALERTED (noticed
	// something, chase not yet committed), snap toward the stimulus — the TLOU "head turn" telegraph that gives
	// the player a beat to react.
	const bool bArrived = bTargetFresh && Distance <= StopDistance && GetMoveStatus() == EPathFollowingStatus::Idle;
	FVector DesiredFacing = FVector::ZeroVector;
	if (!FacingOverrideWorld.IsNearlyZero())
	{
		// A BT task (ScanAround's look pulses) owns facing right now — without this hand-off the per-frame
		// rewrite below would stomp the task's facing the very next tick.
		DesiredFacing = FacingOverrideWorld;
	}
	else if (bArrived)
	{
		DesiredFacing = ToTarget.GetSafeNormal();
	}
	else if (!bTargetFresh)
	{
		if (const APawn* Alert = AlertCandidate.Get())
		{
			FVector ToAlert = Alert->GetActorLocation() - InfectedCharacter->GetActorLocation();
			ToAlert.Z = 0.f;
			DesiredFacing = ToAlert.GetSafeNormal();
		}
	}
	InfectedCharacter->SetDesiredFacingWorld(DesiredFacing);

	// Keep the controller's control rotation aligned with the BODY facing so the AnimInstance's AimingRotation
	// (read from the controller) yields ~zero rotation offset (no AO fighting the path direction).
	SetControlRotation(InfectedCharacter->GetActorRotation());
}
