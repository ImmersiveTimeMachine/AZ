// Copyright Artur. AZ project.

#include "AI/AZ_HordeSubsystem.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetMontage (pack step-back)
#include "AbilitySystemInterface.h"
#include "AI/AZ_InfectedAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AZ_ConsoleVariables.h"
#include "AZ_GameplayTags.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* RoleName(EAZ_CombatRole Role)
	{
		switch (Role)
		{
		case EAZ_CombatRole::Active:  return TEXT("Active");
		case EAZ_CombatRole::Passive: return TEXT("Passive");
		default:                      return TEXT("None");
		}
	}

	// Lexicographic ranking classes (external design review 2026-07-22: hard constraints must not be
	// magnitude-encoded as score offsets — a sufficiently large arena or a future modifier silently
	// breaks the semantics). Sort ascending by (Class, Utility): class encodes the RULE, utility
	// (distance minus stickiness) only breaks ties WITHIN a class.
	//   Locked    — mid-swing incumbent: may never be rotated out (invariant, not preference).
	//   Eligible  — normal incumbents and off-cooldown challengers: the ordinary contest.
	//   Expired   — incumbent whose turn is up: replaced by ANY Eligible, but still outranks Benched —
	//               so "turn expired" only swaps when a valid replacement exists (a lone attacker,
	//               or an all-benched pack, keeps fighting).
	//   Benched   — inside the post-rotation cooldown window: fills a slot only when nobody else can.
	enum class ERankClass : uint8 { Locked = 0, Eligible = 1, Expired = 2, Benched = 3 };

	// Intensity table — one row per az.Crowd.Intensity level (1..5). Row 3 IS today's hand-tuned
	// baseline; the others scale outward from it. Seam constraints every row must hold (validated
	// against the current BT values, 2026-07-22):
	//   * press-arrival radius (Ring x 0.75) > the Press stop distance (~120cm) — turn clock must run
	//     while actually fighting at every level;
	//   * Ring x drift(3 deg/s = 0.0524 rad/s) < the Ring branch's Walk speed — slots stay catchable
	//     (26cm/s at the widest ring vs ~30-45cm/s walk);
	//   * MinPassive >= 2 brain beats (0.5s) so a demotion actually benches;
	//   * attackers + expected circlers <= NumRingSlots at typical pack sizes.
	struct FIntensityRow
	{
		float AlertRadius;
		int32 MaxAttackers;
		float HoldMin, HoldMax;
		float MinPassive;
		float RingDistance;
		float ShuffleMin, ShuffleMax;
	};
	constexpr FIntensityRow GIntensityTable[5] =
	{
		//        alert  atk  hold      bench  ring   shuffle
		/* 1 */ { 1200.f, 1,  5.f, 9.f, 4.0f,  500.f, 8.f, 14.f },   // wary: single prober, wide slow orbit
		/* 2 */ { 1600.f, 1,  4.f, 8.f, 3.0f,  450.f, 7.f, 13.f },
		/* 3 */ { 2000.f, 2,  4.f, 8.f, 2.0f,  400.f, 6.f, 12.f },   // baseline (= the v3 hand-tuned values)
		/* 4 */ { 2500.f, 2,  3.f, 7.f, 1.0f,  350.f, 5.f, 10.f },
		/* 5 */ { 3000.f, 3,  3.f, 6.f, 0.5f,  300.f, 4.f,  8.f },   // frenzy: three press, tight restless ring
	};
}

bool UAZ_HordeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAZ_HordeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RoleBeatAccumulator += DeltaTime;
	if (RoleBeatAccumulator >= RoleRecomputeIntervalSeconds)
	{
		const float BeatDelta = RoleBeatAccumulator;
		RoleBeatAccumulator = 0.f;

		// Global test override: az.Crowd.Intensity 0 = off (per-crowd levels stand); 1..5 forces EVERY
		// known crowd. Re-applies only when the CVar itself changes, so a scripted per-crowd
		// SetCrowdIntensity between toggles isn't stomped each beat.
		const int32 GlobalCVar = AZCVars::GetCrowdIntensity();
		if (GlobalCVar != LastGlobalIntensityCVar)
		{
			LastGlobalIntensityCVar = GlobalCVar;
			if (GlobalCVar >= 1 && GlobalCVar <= 5)
			{
				for (TPair<FName, int32>& Pair : CrowdLevels)
				{
					SetCrowdIntensity(Pair.Key, GlobalCVar);
				}
				UE_LOG(LogTemp, Display, TEXT("[CrowdBrain] GLOBAL intensity override -> %d (all %d crowds)"),
					GlobalCVar, CrowdLevels.Num());
			}
		}

		AssignCombatRoles();
		UpdateFightRings(BeatDelta);
	}
}

int32 UAZ_HordeSubsystem::GetCrowdIntensity(FName CrowdId) const
{
	const int32* Level = CrowdLevels.Find(CrowdId);
	return Level ? *Level : 3;   // never-seeded crowd behaves as the baseline
}

void UAZ_HordeSubsystem::SetCrowdIntensity(FName CrowdId, int32 Level)
{
	const int32 Clamped = FMath::Clamp(Level, 1, 5);
	if (const int32* Existing = CrowdLevels.Find(CrowdId); Existing && *Existing == Clamped)
	{
		return;
	}
	CrowdLevels.Add(CrowdId, Clamped);   // TMap::Add = insert-or-overwrite (no structural change if present)
	// Nothing to stamp: every intensity-driven knob is resolved on demand from GIntensityTable[Clamped-1]
	// through GetCrowdIntensity, so the change is live from the next beat — new grants use the new row,
	// running holds keep their already-seeded durations, MaxAttackers re-balances via ordinary ranking.
	const FIntensityRow& Row = GIntensityTable[Clamped - 1];
	UE_LOG(LogTemp, Display, TEXT("[CrowdBrain] crowd '%s' intensity -> %d (attackers=%d hold=%.0f-%.0fs bench=%.1fs ring=%.0fcm alert=%.0fcm)"),
		*CrowdId.ToString(), Clamped, Row.MaxAttackers, Row.HoldMin, Row.HoldMax, Row.MinPassive, Row.RingDistance, Row.AlertRadius);
}

TStatId UAZ_HordeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAZ_HordeSubsystem, STATGROUP_Tickables);
}

void UAZ_HordeSubsystem::RegisterInfected(AAZ_InfectedAIController* Controller)
{
	if (Controller)
	{
		Infected.AddUnique(Controller);
		// First member of a crowd seeds its level from the pawn's authored value; later members (and any
		// runtime SetCrowdIntensity) do not stomp it. A never-seen crowd stays absent = treated as 3.
		const FName CrowdId = Controller->GetCrowdId();
		if (!CrowdLevels.Contains(CrowdId))
		{
			CrowdLevels.Add(CrowdId, FMath::Clamp(Controller->GetInitialCrowdIntensity(), 1, 5));
		}
	}
}

void UAZ_HordeSubsystem::UnregisterInfected(AAZ_InfectedAIController* Controller)
{
	if (Controller)
	{
		// Corpses and unpossessed pawns hold no slot and wear no role tag (rule 7 + corpse doctrine).
		ApplyRole(Controller, EAZ_CombatRole::None, nullptr);
		Roles.Remove(Controller);
	}
	Infected.Remove(Controller);
}

void UAZ_HordeSubsystem::NotifyAggro(AAZ_InfectedAIController* Instigator, const FVector& PreyLocation)
{
	const APawn* ScreamerPawn = Instigator ? Instigator->GetPawn() : nullptr;
	if (!ScreamerPawn)
	{
		return;
	}
	const FVector ScreamOrigin = ScreamerPawn->GetActorLocation();
	// The callout carries at the SCREAMER's crowd radius and wakes only that crowd — a scream is a
	// same-pack signal (a louder crowd reaches farther). Cross-crowd cascades are deliberately not a
	// thing yet: crowds stay independent until an encounter explicitly wants them to chain.
	const FName ScreamerCrowd = Instigator->GetCrowdId();
	const float RadiusSq = FMath::Square(GIntensityTable[GetCrowdIntensity(ScreamerCrowd) - 1].AlertRadius);

	for (int32 i = Infected.Num() - 1; i >= 0; --i)
	{
		AAZ_InfectedAIController* Ally = Infected[i].Get();
		if (!Ally)
		{
			Infected.RemoveAtSwap(i);
			continue;
		}
		const APawn* AllyPawn = Ally->GetPawn();
		if (Ally == Instigator || !AllyPawn || Ally->GetCrowdId() != ScreamerCrowd)
		{
			continue;
		}
		if (FVector::DistSquared(AllyPawn->GetActorLocation(), ScreamOrigin) <= RadiusSq)
		{
			Ally->ArmInvestigation(PreyLocation, /*bUrgent*/ true);
		}
	}
}

bool UAZ_HordeSubsystem::RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey)
{
	if (!Attacker || !Prey)
	{
		return false;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const FCombatRoleState* State = Roles.Find(Attacker))
	{
		if (State->Role == EAZ_CombatRole::Active && State->Prey == Prey)
		{
			return true;   // already a designated fighter on this prey (re-entrant swings)
		}
		// Rotation cooldown: a freshly rotated-out Chalkie doesn't reclaim the turn it just handed over,
		// even if a slot is momentarily free — the beat arbitrates its return.
		if (State->EligibleAfterSeconds > Now)
		{
			return false;
		}
	}
	// Inline promotion — don't make a fresh attacker wait out the beat while a slot is open in ITS crowd.
	const FName CrowdId = Attacker->GetCrowdId();
	const int32 MaxAttackers = GIntensityTable[GetCrowdIntensity(CrowdId) - 1].MaxAttackers;
	if (CountActiveOnPrey(Prey, CrowdId) < MaxAttackers)
	{
		ApplyRole(Attacker, EAZ_CombatRole::Active, Prey);
		return true;
	}
	// Slots full: publish the ring role right away so the BT forks onto the ring hold this very tick.
	ApplyRole(Attacker, EAZ_CombatRole::Passive, Prey);
	return false;
}

void UAZ_HordeSubsystem::ReleaseAttackToken(AAZ_InfectedAIController* Attacker)
{
	if (!Attacker)
	{
		return;
	}
	// NOT a demotion — the pause between swings is still Active (rule 7: role = published state, not a
	// per-swing permit). Only clear when the engagement itself is gone (break-off, escape, unpossess).
	const FCombatRoleState* State = Roles.Find(Attacker);
	if (State && State->Role != EAZ_CombatRole::None && Attacker->GetFreshPerceivedTarget() != State->Prey.Get())
	{
		ApplyRole(Attacker, EAZ_CombatRole::None, nullptr);
	}
}

EAZ_CombatRole UAZ_HordeSubsystem::GetCombatRole(const AAZ_InfectedAIController* Controller) const
{
	const FCombatRoleState* State = Roles.Find(const_cast<AAZ_InfectedAIController*>(Controller));
	return State ? State->Role : EAZ_CombatRole::None;
}

bool UAZ_HordeSubsystem::RequestGrabToken(AAZ_InfectedAIController* Grabber, const AActor* Prey)
{
	if (!Grabber || !Prey)
	{
		return false;
	}
	TWeakObjectPtr<AAZ_InfectedAIController>& Holder = GrabTokenByPrey.FindOrAdd(FObjectKey(Prey));
	if (Holder.IsValid() && Holder.Get() != Grabber)
	{
		return false;   // someone else is mid-grab on this prey
	}
	Holder = Grabber;
	return true;
}

void UAZ_HordeSubsystem::ReleaseGrabToken(AAZ_InfectedAIController* Grabber)
{
	if (!Grabber)
	{
		return;
	}
	for (auto It = GrabTokenByPrey.CreateIterator(); It; ++It)
	{
		if (!It->Value.IsValid() || It->Value.Get() == Grabber)
		{
			It.RemoveCurrent();   // also sweeps stale entries whose grabber died holding the token
		}
	}
}

void UAZ_HordeSubsystem::NotifyGrabStarted(AAZ_InfectedAIController* Grabber, const AActor* Prey, float StepBackSeconds)
{
	if (!Prey)
	{
		return;
	}
	for (auto& Pair : Roles)
	{
		AAZ_InfectedAIController* Other = Pair.Key.Get();
		if (!Other || Other == Grabber || Pair.Value.Prey.Get() != Prey)
		{
			continue;
		}
		const AAZ_PawnMoverInfectedCharacter* InfectedCharacter = Cast<AAZ_PawnMoverInfectedCharacter>(Other->GetPawn());
		if (!InfectedCharacter)
		{
			continue;
		}
		UAnimMontage* StepBack = UAZ_GA_MeleeAttack::FindAnimSetMontage(InfectedCharacter, TEXT("GrabEscapeMontage"));
		UAnimInstance* AnimInstance = InfectedCharacter->GetMesh() ? InfectedCharacter->GetMesh()->GetAnimInstance() : nullptr;
		if (!StepBack || !AnimInstance)
		{
			continue;
		}
		Other->StopMovement();
		AnimInstance->Montage_Play(StepBack);
		if (StepBackSeconds > 0.f)
		{
			// Cut the recoil at the beat: blend the montage back out instead of playing the full 5.5s
			// knockback+recover. Weak-captured — a Chalkie dying mid-beat just no-ops the stop.
			TWeakObjectPtr<UAnimInstance> WeakAnim = AnimInstance;
			TWeakObjectPtr<UAnimMontage> WeakMontage = StepBack;
			FTimerHandle ThrowawayHandle;
			GetWorld()->GetTimerManager().SetTimer(ThrowawayHandle, FTimerDelegate::CreateLambda([WeakAnim, WeakMontage]()
			{
				UAnimInstance* Anim = WeakAnim.Get();
				UAnimMontage* Montage = WeakMontage.Get();
				if (Anim && Montage && Anim->Montage_IsPlaying(Montage))
				{
					Anim->Montage_Stop(0.4f, Montage);
				}
			}), StepBackSeconds, false);
		}
	}
}

int32 UAZ_HordeSubsystem::CountActiveOnPrey(const AActor* Prey, FName CrowdId) const
{
	int32 Count = 0;
	for (const auto& Pair : Roles)
	{
		if (Pair.Key.IsValid() && Pair.Value.Role == EAZ_CombatRole::Active
			&& Pair.Value.Prey == Prey && Pair.Value.CrowdId == CrowdId)
		{
			++Count;
		}
	}
	return Count;
}

void UAZ_HordeSubsystem::ReleaseSlot(FCombatRoleState& State)
{
	if (State.SlotIndex != INDEX_NONE)
	{
		// Release from the ring this role actually joined — the (crowd, prey) it held when the slot was
		// granted, carried on the state, so a live CrowdId change can't strand the old slot.
		if (FFightRing* Ring = Rings.Find(FCrowdPreyKey{ State.CrowdId, State.Prey }))
		{
			if (Ring->Slots.IsValidIndex(State.SlotIndex))
			{
				Ring->Slots[State.SlotIndex] = nullptr;
			}
		}
		State.SlotIndex = INDEX_NONE;
	}
	State.bWroteSlot = false;
}

void UAZ_HordeSubsystem::ClearManagedFacing(AAZ_InfectedAIController* Controller, FCombatRoleState& State)
{
	if (State.bFacingOverridden)
	{
		// The override is a single shared slot (ScanAround/ZombieAttack write it too) — bFacingOverridden
		// only proves WE wrote it at some point, not that nobody has written it since. Only clear if the
		// controller's current value still matches what we last set; otherwise a newer owner has it now
		// and clearing would stomp THEIR facing instead of releasing ours.
		if (Controller && Controller->GetFacingOverrideWorld().Equals(State.LastFacingSet))
		{
			Controller->ClearFacingOverride();
		}
		State.bFacingOverridden = false;
	}
}

void UAZ_HordeSubsystem::ApplyRole(AAZ_InfectedAIController* Controller, EAZ_CombatRole NewRole, const AActor* Prey)
{
	if (!Controller)
	{
		return;
	}
	FCombatRoleState& State = Roles.FindOrAdd(Controller);
	if (State.Role == NewRole && State.Prey.Get() == Prey)
	{
		return;   // no transition — no tag churn, no BB writes, no log spam
	}
	const EAZ_CombatRole OldRole = State.Role;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// This Chalkie's crowd row drives every timed/spatial knob of the grant.
	const FIntensityRow& Row = GIntensityTable[GetCrowdIntensity(Controller->GetCrowdId()) - 1];

	// Leaving a role: give back what it held. Slot/facing belong to Passive; a turn timer to Active.
	// ReleaseSlot uses the OLD State.CrowdId (still set from the previous grant) to find the right ring —
	// so it must run BEFORE State.CrowdId is updated below.
	if (OldRole == EAZ_CombatRole::Passive)
	{
		// Don't leave a stale post published — a later Passive assignment on a NEW prey wouldn't get a
		// fresh BB write until this beat's ring pass runs, and a bare bool key elsewhere reading
		// "IsSet" on this vector must not see a leftover position from a fight that's already over.
		if (UBlackboardComponent* BB = Controller->GetBlackboardComponent())
		{
			BB->ClearValue(AZ_ChalkieBBKeys::SlotLocation);
		}
	}
	ReleaseSlot(State);
	ClearManagedFacing(Controller, State);
	if (OldRole == EAZ_CombatRole::Active && NewRole != EAZ_CombatRole::Active)
	{
		// Applies to EVERY Active exit (rotation, distance churn, disengage): a short bench window
		// before the next turn. Harmless on disengage — EligibleAfter only gates re-PROMOTION.
		State.EligibleAfterSeconds = Now + Row.MinPassive;
	}

	State.Role = NewRole;
	State.Prey = Prey;
	State.CrowdId = Controller->GetCrowdId();
	if (NewRole == EAZ_CombatRole::Active)
	{
		State.ActiveSinceSeconds = Now;
		State.ActiveHoldSeconds = FMath::FRandRange(Row.HoldMin, Row.HoldMax);
	}
	else if (NewRole == EAZ_CombatRole::Passive)
	{
		State.NextShuffleSeconds = Now + FMath::FRandRange(Row.ShuffleMin, Row.ShuffleMax);
	}

	// ASC role tags — replicated menace state for the AnimInstance (server-authoritative writer).
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (const APawn* Pawn = Controller->GetPawn())
	{
		if (const IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Pawn))
		{
			if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(AbilityInterface->GetAbilitySystemComponent()))
			{
				ASC->RemoveStateTag(Tags.State_Combat_Engaged_Active);
				ASC->RemoveStateTag(Tags.State_Combat_Engaged_Passive);
				if (NewRole == EAZ_CombatRole::Active)
				{
					ASC->AddStateTag(Tags.State_Combat_Engaged_Active);
				}
				else if (NewRole == EAZ_CombatRole::Passive)
				{
					ASC->AddStateTag(Tags.State_Combat_Engaged_Passive);
				}
			}
		}
	}

	// BB mirror — the ONE key the BT Chase fork observes.
	if (UBlackboardComponent* BB = Controller->GetBlackboardComponent())
	{
		BB->SetValueAsBool(AZ_ChalkieBBKeys::bActiveFighter, NewRole == EAZ_CombatRole::Active);
		BB->SetValueAsFloat(AZ_ChalkieBBKeys::RingDistance, Row.RingDistance);   // legacy key (Ring MoveTo now uses SlotLocation)
	}

	UE_LOG(LogTemp, Display, TEXT("[CrowdBrain] %s [%s] role %s -> %s (prey=%s)"),
		*GetNameSafe(Controller->GetPawn()), *State.CrowdId.ToString(),
		RoleName(OldRole), RoleName(NewRole), *GetNameSafe(Prey));
}

void UAZ_HordeSubsystem::AssignCombatRoles()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 1) Facts: who is engaged, on whom, from how far. Grouped per (crowd, prey) so each crowd runs its
	// own contest — a crowd's attacker budget and ring are never shared with another crowd on the same
	// prey. (Perception owns freshness — rule 9.)
	struct FCandidate
	{
		AAZ_InfectedAIController* Controller = nullptr;
		ERankClass Class = ERankClass::Eligible;
		float Utility = 0.f;   // distance minus incumbent stickiness — tie-break WITHIN a class only
	};
	TMap<FCrowdPreyKey, TArray<FCandidate>> ByCrowdPrey;

	for (int32 i = Infected.Num() - 1; i >= 0; --i)
	{
		AAZ_InfectedAIController* Chalkie = Infected[i].Get();
		if (!Chalkie)
		{
			Infected.RemoveAtSwap(i);
			continue;
		}
		const APawn* ChalkiePawn = Chalkie->GetPawn();
		APawn* Prey = Chalkie->GetFreshPerceivedTarget();
		if (!ChalkiePawn || !Prey)
		{
			// A single-frame freshness flicker must not yank an Active mid-swing: ApplyRole(None) here
			// would clear bActiveFighter, and the Chase Fork's Observer/ValueChange decorator would abort
			// the running ZombieAttack task instantly — the exact mid-bite interruption Locked exists to
			// prevent. The Locked class in the ranking pass below only guards the RANKING path; this early
			// branch bypasses ranking entirely, so it needs its own guard. The melee task owns the exit.
			if (!Chalkie->IsMeleeTaskActive())
			{
				ApplyRole(Chalkie, EAZ_CombatRole::None, nullptr);
			}
			continue;
		}
		const FName CrowdId = Chalkie->GetCrowdId();
		const FIntensityRow& Row = GIntensityTable[GetCrowdIntensity(CrowdId) - 1];
		FCombatRoleState* Current = Roles.Find(Chalkie);
		const bool bWasActiveOnPrey = Current && Current->Role == EAZ_CombatRole::Active && Current->Prey.Get() == Prey;

		FCandidate Candidate;
		Candidate.Controller = Chalkie;
		const float DistToPrey = FVector::Dist2D(ChalkiePawn->GetActorLocation(), Prey->GetActorLocation());
		Candidate.Utility = DistToPrey;
		if (bWasActiveOnPrey)
		{
			Candidate.Utility -= ActiveStickinessCm;   // incumbents don't flicker on a strafe step
			// The turn clock burns FIGHT time, not travel time: a challenger promoted at the ring edge
			// must not spend its whole hold sprinting in and get rotated out on arrival (PIE 2026-07-22:
			// perpetual milling — nobody ever pressed). While still closing, re-seed the clock each beat.
			// No stall risk: a far incumbent still loses its slot to CLOSER Eligibles through the utility
			// sort — freezing only disables TIME-based rotation, not distance-based competition.
			if (DistToPrey > Row.RingDistance * 0.75f)
			{
				Current->ActiveSinceSeconds = Now;
			}
			if (Chalkie->IsMeleeTaskActive())
			{
				Candidate.Class = ERankClass::Locked;
			}
			else if (Now > Current->ActiveSinceSeconds + Current->ActiveHoldSeconds)
			{
				Candidate.Class = ERankClass::Expired;
			}
		}
		else if (Current && Current->EligibleAfterSeconds > Now)
		{
			Candidate.Class = ERankClass::Benched;
		}
		ByCrowdPrey.FindOrAdd(FCrowdPreyKey{ CrowdId, Prey }).Add(Candidate);
	}

	// 2) Decision per (crowd, prey): one lexicographic sort, top MaxAttackers (of THAT crowd) press,
	// everyone else circles. The class order PROVES the invariants: a mid-swing incumbent can never lose
	// its slot; an expired incumbent is replaced exactly when an Eligible challenger exists and otherwise
	// keeps fighting; a benched challenger enters only when nobody better can fill the slot.
	for (auto& Pair : ByCrowdPrey)
	{
		const int32 MaxAttackers = GIntensityTable[GetCrowdIntensity(Pair.Key.CrowdId) - 1].MaxAttackers;
		Pair.Value.Sort([](const FCandidate& A, const FCandidate& B)
		{
			if (A.Class != B.Class)
			{
				return A.Class < B.Class;
			}
			return A.Utility < B.Utility;
		});
		for (int32 i = 0; i < Pair.Value.Num(); ++i)
		{
			ApplyRole(Pair.Value[i].Controller,
				i < MaxAttackers ? EAZ_CombatRole::Active : EAZ_CombatRole::Passive,
				Pair.Key.Prey.Get());
		}
	}
}

FVector UAZ_HordeSubsystem::ComputeSlotWorldLocation(const FFightRing& Ring, int32 SlotIndex, float RingRadius, const FVector& PreyLocation) const
{
	const float AngleDeg = Ring.PhaseDeg + SlotIndex * (360.f / FMath::Max(1, NumRingSlots));
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);
	return PreyLocation + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f) * RingRadius;
}

void UAZ_HordeSubsystem::UpdateFightRings(float BeatDeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	// Drift all rings; drop rings whose prey died or whose pack dispersed (no Passive references them —
	// slot compaction below nulls stale holders, and an all-null ring with a dead prey key is removed).
	for (auto It = Rings.CreateIterator(); It; ++It)
	{
		if (!It.Key().Prey.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		It.Value().PhaseDeg = FMath::Fmod(It.Value().PhaseDeg + RingDriftDegPerSecond * BeatDeltaSeconds, 360.f);
		for (TWeakObjectPtr<AAZ_InfectedAIController>& Holder : It.Value().Slots)
		{
			if (Holder.IsValid())
			{
				// Compact holders whose role state no longer claims THIS (crowd, prey) ring — killed,
				// promoted, left, or re-homed to a different crowd.
				const FCombatRoleState* HolderState = Roles.Find(Holder);
				if (!HolderState || HolderState->Role != EAZ_CombatRole::Passive
					|| HolderState->Prey != It.Key().Prey || HolderState->CrowdId != It.Key().CrowdId)
				{
					Holder = nullptr;
				}
			}
			else
			{
				Holder = nullptr;
			}
		}
	}

	// Publishes SlotLocation with the write-hysteresis + honest bookkeeping every call site needs: never
	// stamp bWroteSlot/LastWrittenSlot as if a write landed when BB_Chalkie doesn't have the key yet (it
	// may not, until the hand-edit lands) — a false-success here would desync bookkeeping from the BB's
	// actual state and corrupt the projection-failure fallback below.
	const auto PublishSlotLocation = [this](AAZ_InfectedAIController* Chalkie, const FVector& Location, FCombatRoleState& State)
	{
		UBlackboardComponent* BB = Chalkie->GetBlackboardComponent();
		if (!BB || BB->GetKeyID(AZ_ChalkieBBKeys::SlotLocation) == FBlackboard::InvalidKey)
		{
			return;
		}
		if (!State.bWroteSlot || FVector::DistSquared2D(Location, State.LastWrittenSlot) > FMath::Square(SlotWriteHysteresisCm))
		{
			BB->SetValueAsVector(AZ_ChalkieBBKeys::SlotLocation, Location);
			State.LastWrittenSlot = Location;
			State.bWroteSlot = true;
		}
	};

	// Slot service per Passive: assign, shuffle, project, publish, face.
	// INVARIANT: nothing below may call ApplyRole or mutate Roles' KEYS (Add/FindOrAdd/Remove) — this is
	// a plain range-for over Roles, and growing/rehashing the map mid-iteration invalidates RolePair.
	// ReleaseSlot/ClearManagedFacing only touch Rings and the controller, precisely so this stays a pure
	// reader+value-writer of Roles.
	for (auto& RolePair : Roles)
	{
		AAZ_InfectedAIController* Chalkie = RolePair.Key.Get();
		FCombatRoleState& State = RolePair.Value;
		if (!Chalkie || State.Role != EAZ_CombatRole::Passive)
		{
			continue;
		}
		const AActor* Prey = State.Prey.Get();
		const APawn* ChalkiePawn = Chalkie->GetPawn();
		if (!Prey || !ChalkiePawn)
		{
			continue;   // next AssignCombatRoles beat retires this state
		}

		// This Passive's own crowd owns the ring it stands in (State.CrowdId was stamped at grant).
		const float RingRadius = GIntensityTable[GetCrowdIntensity(State.CrowdId) - 1].RingDistance;
		FFightRing& Ring = Rings.FindOrAdd(FCrowdPreyKey{ State.CrowdId, State.Prey });
		if (Ring.Slots.Num() != NumRingSlots)
		{
			Ring.Slots.SetNum(NumRingSlots);
		}
		const FVector PreyLocation = Prey->GetActorLocation();

		// Assign: nearest-angle free slot, so a newcomer joins the circle where it already stands
		// instead of crossing the whole ring (and through the fight) to a far slot.
		if (State.SlotIndex == INDEX_NONE || !Ring.Slots.IsValidIndex(State.SlotIndex) || Ring.Slots[State.SlotIndex] != RolePair.Key)
		{
			const FVector ToChalkie = (ChalkiePawn->GetActorLocation() - PreyLocation).GetSafeNormal2D();
			const float MyAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(ToChalkie.Y, ToChalkie.X));
			int32 BestSlot = INDEX_NONE;
			float BestDelta = FLT_MAX;
			for (int32 SlotIdx = 0; SlotIdx < Ring.Slots.Num(); ++SlotIdx)
			{
				if (Ring.Slots[SlotIdx].IsValid())
				{
					continue;
				}
				const float SlotAngle = Ring.PhaseDeg + SlotIdx * (360.f / NumRingSlots);
				const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(MyAngleDeg, SlotAngle));
				if (Delta < BestDelta)
				{
					BestDelta = Delta;
					BestSlot = SlotIdx;
				}
			}
			if (BestSlot == INDEX_NONE)
			{
				// Ring saturated (pack > slots): no post to assign. A brand-new Passive with nothing
				// published yet must not be left with an unset SlotLocation — that reads as the zero
				// vector (world origin) to the BT's Ring MoveTo and sends it deserting toward map center.
				// Publish current ground position instead: hold here until a slot frees up.
				if (!State.bWroteSlot)
				{
					PublishSlotLocation(Chalkie, ChalkiePawn->GetActorLocation(), State);
				}
				continue;   // stand ground this beat, retry next
			}
			State.SlotIndex = BestSlot;
			Ring.Slots[BestSlot] = RolePair.Key;
			State.bWroteSlot = false;   // force a fresh publish for the new post
		}
		// Shuffle: hop to an adjacent free slot on a lazy random cadence — the circling read.
		else if (Now >= State.NextShuffleSeconds)
		{
			const FIntensityRow& Row = GIntensityTable[GetCrowdIntensity(State.CrowdId) - 1];
			State.NextShuffleSeconds = Now + FMath::FRandRange(Row.ShuffleMin, Row.ShuffleMax);
			const int32 Direction = FMath::RandBool() ? 1 : -1;
			const int32 Neighbor = (State.SlotIndex + Direction + NumRingSlots) % NumRingSlots;
			if (!Ring.Slots[Neighbor].IsValid())
			{
				Ring.Slots[State.SlotIndex] = nullptr;
				Ring.Slots[Neighbor] = RolePair.Key;
				State.SlotIndex = Neighbor;
				State.bWroteSlot = false;
			}
		}

		// Where the post is right now (ring follows the prey and drifts).
		FVector SlotWorld = ComputeSlotWorldLocation(Ring, State.SlotIndex, RingRadius, PreyLocation);
		bool bHavePost = true;
		if (NavSys)
		{
			FNavLocation Projected;
			if (NavSys->ProjectPointToNavigation(SlotWorld, Projected, FVector(150.f, 150.f, 300.f)))
			{
				SlotWorld = Projected.Location;
			}
			else if (State.bWroteSlot)
			{
				// Unprojectable slot (over a ledge/wall): keep last written post this beat — the shuffle
				// cadence will walk the observer to healthier ground; do not publish garbage.
				SlotWorld = State.LastWrittenSlot;
			}
			else
			{
				// Fresh assignment, nothing previously published, and this post has no navmesh under it
				// at all: writing the raw point would aim the BT's MoveTo at an unreachable spot for as
				// long as the slot is held. Give it back instead — the assign branch above tries a
				// different slot next beat.
				ReleaseSlot(State);
				bHavePost = false;
			}
		}

		if (bHavePost)
		{
			PublishSlotLocation(Chalkie, SlotWorld, State);
		}

		// Posted observers stare the prey down (menace); travelers face their path (the controller's
		// default when no override) — an override mid-move would moonwalk the run. Gated on bWroteSlot:
		// with no post ever published, LastWrittenSlot is the zero vector, and a pawn that happens to
		// stand near world origin must not read as "posted".
		const bool bPosted = State.bWroteSlot
			&& FVector::DistSquared2D(ChalkiePawn->GetActorLocation(), State.LastWrittenSlot)
				<= FMath::Square(SlotArrivedToleranceCm);
		if (bPosted)
		{
			const FVector Facing = (PreyLocation - ChalkiePawn->GetActorLocation()).GetSafeNormal2D();
			Chalkie->SetFacingOverrideWorld(Facing);
			State.LastFacingSet = Facing;
			State.bFacingOverridden = true;
		}
		else
		{
			ClearManagedFacing(Chalkie, State);
		}
	}
}
