// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ZombieAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/AZ_GA_ChalkieGrab.h"
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "AI/AZ_HordeSubsystem.h"
#include "AI/AZ_InfectedAIController.h"   // AZ_ChalkieBBKeys
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "AZ_ConsoleVariables.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"   // IsRootMotionDriven (our own warp lunge isn't a shove)
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

namespace
{
	// Attack gates (constants until the next CLI batch promotes them to UPROPERTYs):
	// don't START a swing beyond ACTUAL claw reach; BREAK OFF the instant the target leaves it —
	// a fleeing player must produce a RUNNING pursuer, not a rooted air-clawing one (bites root the
	// zombie for BiteSeconds via the RM override). Start 180 ~= MeleeRange 170 + margin; break-off
	// 240 = reach + one stride.
	constexpr float AZ_MaxAttackStartDistance = 180.f;
	constexpr float AZ_BreakOffDistance = 240.f;
	// Closing-velocity refinement: a target RUSHING IN can be met early (the swing lands as they
	// arrive); a target already FLEEING fast isn't worth a rooted swing even in nominal reach.
	constexpr float AZ_RushExtendDistance = 230.f;
	constexpr float AZ_RushClosingSpeed = 150.f;      // cm/s toward the zombie
	constexpr float AZ_FleeRecedingSpeed = 200.f;     // cm/s away from the zombie
	// Facing cone: the damage sweep is forward — starting a swing at a target beside/behind us is a
	// guaranteed whiff. Cos(60 deg).
	constexpr float AZ_AttackFacingCos = 0.5f;
	// Moving and fighting are mutually exclusive (user rule): don't START a swing until the body has
	// (nearly) stopped, and BREAK OFF if anything moves us mid-bite — movement always wins.
	constexpr float AZ_MaxSpeedToStartAttack = 80.f;
	constexpr float AZ_MidBiteMoveBreakSpeed = 120.f;

	const AActor* AZ_GetChaseTarget(UBehaviorTreeComponent& OwnerComp)
	{
		const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
		return BB ? Cast<AActor>(BB->GetValueAsObject(AZ_ChalkieBBKeys::TargetActor)) : nullptr;
	}
}

UAZ_BTTask_ZombieAttack::UAZ_BTTask_ZombieAttack()
{
	NodeName = TEXT("Zombie Melee Attack");
	bNotifyTick = true;
	bCreateNodeInstance = true;   // delegate binding + timeout are per-AI state
	AbilityClass = UAZ_GA_ZombieMelee::StaticClass();
	GrabAbilityClass = UAZ_GA_ChalkieGrab::StaticClass();
}

EBTNodeResult::Type UAZ_BTTask_ZombieAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	if (!ASC || !*AbilityClass)
	{
		return EBTNodeResult::Failed;
	}

	// Start gates: reach (velocity-adjusted) + facing cone + engagement token. Any failure -> Failed
	// (the node's ForceSuccess decorator turns it into "skip the swing, keep the chase loop").
	const AActor* Target = AZ_GetChaseTarget(OwnerComp);
	if (Target)
	{
		// PACK HOLD-OFF: while the prey is in a pack-mate's grab, everyone else stands down — no punch
		// spam into the mash (user feedback 2026-07-24: others kept clawing mid-grab). The grabber
		// itself never reaches here (it's latent inside this very task for the whole hold).
		if (const UAbilitySystemComponent* PreyASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			if (PreyASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
			{
				return EBTNodeResult::Failed;
			}
		}

		const FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
		const float Distance = ToTarget.Size2D();
		const FVector DirToTarget = ToTarget.GetSafeNormal2D();

		// Closing speed: + = target moving toward us, - = fleeing.
		const float ClosingSpeed = FVector::DotProduct(Target->GetVelocity(), -DirToTarget);
		const float StartDistance = (ClosingSpeed > AZ_RushClosingSpeed) ? AZ_RushExtendDistance : AZ_MaxAttackStartDistance;
		if (Distance > StartDistance)
		{
			return EBTNodeResult::Failed;
		}
		if (ClosingSpeed < -AZ_FleeRecedingSpeed)
		{
			return EBTNodeResult::Failed;   // already sprinting away — chase, don't claw air
		}
		if (FVector::DotProduct(Pawn->GetActorForwardVector().GetSafeNormal2D(), DirToTarget) < AZ_AttackFacingCos)
		{
			return EBTNodeResult::Failed;   // beside/behind us — the forward sweep would whiff
		}
		if (Pawn->GetVelocity().Size2D() > AZ_MaxSpeedToStartAttack)
		{
			return EBTNodeResult::Failed;   // still moving — settle first, swing next loop
		}
		// Stagger gate (audit rules-finding #3): mid-KnockBack the BT loop retries attacks the moment
		// stumble velocity dips — without this, a new claw cancels the flinch's RM and steals the
		// player's earned stagger window. Backed by State.Combat.Staggered (explicit duration, set where
		// each reaction starts) — no longer inferred from montage playback.
		if (const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Pawn))
		{
			// Covers BOTH stagger-class reactions now (hit-react flinch and the pack step-back off a
			// grab) — a bystander that swings mid-recoil would cancel its own root motion and step
			// straight back into the clinch it just recoiled from.
			if (Infected->IsStaggerReactionPlaying())
			{
				return EBTNodeResult::Failed;   // staggering — no swings until the stumble finishes
			}
		}
		// Engagement token: max 2 simultaneous attackers per prey; the rest hold the ring and retry.
		if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner()))
		{
			if (UAZ_HordeSubsystem* Horde = Pawn->GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
			{
				if (!Horde->RequestAttackToken(Chalkie, Target))
				{
					return EBTNodeResult::Failed;
				}
			}
		}
	}

	// THE GRAB ROLL (rare, mid-engagement, unpredictable — user design 2026-07-24): this swing has
	// already won its slot; a small chance turns it into the grab instead. Gates: per-Chalkie cooldown
	// (from the END of the last grab), prey not already in someone's arms (tag + one-grabber token).
	// az.Grab.ForceNext short-circuits the roll but still runs every REAL gate (seam-trace testability).
	ChosenAbilityClass = AbilityClass;
	bGrabRun = false;
	if (Target && *GrabAbilityClass)
	{
		if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner()))
		{
			const bool bForce = AZCVars::GetGrabForceNext() != 0;
			const float ChanceOverride = AZCVars::GetGrabChance();
			const float Chance = ChanceOverride >= 0.f ? ChanceOverride : GrabChance;
			if (bForce || FMath::FRand() < Chance)
			{
				const float CooldownOverride = AZCVars::GetGrabCooldownSeconds();
				const float Cooldown = CooldownOverride >= 0.f ? CooldownOverride : GrabCooldownSeconds;
				const double Now = Pawn->GetWorld()->GetTimeSeconds();
				const bool bOffCooldown = (Now - Chalkie->LastGrabEndTimeSeconds) >= Cooldown;

				const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
				const bool bTargetFree = TargetASC
					&& !TargetASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed);

				UAZ_HordeSubsystem* Horde = Pawn->GetWorld()->GetSubsystem<UAZ_HordeSubsystem>();
				if (bOffCooldown && bTargetFree && Horde && Horde->RequestGrabToken(Chalkie, Target))
				{
					ChosenAbilityClass = GrabAbilityClass;
					bGrabRun = true;
					if (bForce)
					{
						AZCVars::GrabForceNext->Set(0, ECVF_SetByConsole);   // consume the one-shot
					}
					UE_LOG(LogTemp, Display, TEXT("[Grab] %s rolls a GRAB on %s (force=%d chance=%.2f)"),
						*GetNameSafe(Pawn), *GetNameSafe(Target), bForce ? 1 : 0, Chance);
				}
			}
		}
	}

	// Kill any residual path-following before committing to the swing — a swing starts from stillness.
	if (AAIController* AIOwner = OwnerComp.GetAIOwner())
	{
		AIOwner->StopMovement();
	}

	ElapsedSeconds = 0.f;
	BoundASC = ASC;
	OwningComp = &OwnerComp;

	// BP-first tolerance: the granted spec may be a BP CHILD of the configured class (tuning children
	// win at grant time). Resolve to the CONCRETE granted class so every exact-class lookup below
	// (activate, sync-end guard, cancels) matches what the ASC actually holds.
	for (const FGameplayAbilitySpec& GrantedSpec : ASC->GetActivatableAbilities())
	{
		if (GrantedSpec.Ability && GrantedSpec.Ability->GetClass()->IsChildOf(ChosenAbilityClass))
		{
			ChosenAbilityClass = GrantedSpec.Ability->GetClass();
			break;
		}
	}

	// BEFORE activation: TryActivate can play the montage synchronously, and a MotionWarping notify that
	// opens on frame 0 resolves its target the moment the window becomes relevant. Registering after would
	// race the first frame of the swing.
	RegisterWarpTarget(OwnerComp);

	// Bind AFTER activation (audit #7): a synchronous activate-and-end inside TryActivate would fire
	// the delegate while this node isn't latent yet — engine-internal behavior we shouldn't lean on.
	// The IsActive() recheck below fully covers anything that ended before the bind.
	if (!ASC->TryActivateAbilityByClass(ChosenAbilityClass))
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UAZ_BTTask_ZombieAttack::OnAbilityEnded);
	// Latch WHICH root-motion drive is ours. TryActivate runs the ability synchronously, so the drive the
	// attack queued is the live generation right now; anything that supersedes it later (a knockback, a
	// grab) bumps the counter and the mid-swing move-break stops treating the motion as self-inflicted.
	if (const APawn* MovePawn = Controller ? Controller->GetPawn() : nullptr)
	{
		if (const UAZ_PawnMoverComponent* Mover = MovePawn->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			MeleeRootMotionGen = Mover->GetRootMotionGeneration();
		}
	}
	// Sync-end guard: the ability can activate AND end within TryActivate (missing montage, instant
	// fail). Our delegate fired before the task was latent — FinishLatentTask was ignored — so without
	// this check the node hangs until the timeout. If it's already over, report it directly.
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(ChosenAbilityClass);
	if (!Spec || !Spec->IsActive())
	{
		Cleanup();
		return EBTNodeResult::Succeeded;
	}
	// Latent swing in flight: publish it so the crowd brain's rotation pass never yanks the Press
	// branch (and thereby cancels the ability) mid-bite. Cleared on every exit path via Cleanup.
	if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner()))
	{
		Chalkie->SetMeleeTaskActive(true);
	}
	return EBTNodeResult::InProgress;
}

void UAZ_BTTask_ZombieAttack::OnAbilityEnded(const FAbilityEndedData& EndedData)
{
	const TSubclassOf<UGameplayAbility> RunClass = ChosenAbilityClass ? ChosenAbilityClass : AbilityClass;
	if (!EndedData.AbilityThatEnded || !EndedData.AbilityThatEnded->GetClass()->IsChildOf(RunClass))
	{
		return;
	}
	UBehaviorTreeComponent* Comp = OwningComp.Get();
	Cleanup();
	if (Comp)
	{
		FinishLatentTask(*Comp, EBTNodeResult::Succeeded);
	}
}

void UAZ_BTTask_ZombieAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	ElapsedSeconds += DeltaSeconds;

	// Break-off: the target escaped mid-swing — cancel the whiff and resume the chase immediately
	// instead of clawing the air where they used to be.
	AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const AActor* Target = AZ_GetChaseTarget(OwnerComp);

	// Mutual exclusion, bite side (user rule: moving and fighting never overlap): something is moving
	// us — nav restart, push, external force — so the swing yields to movement immediately instead of
	// playing a claw anim over locomotion. NOT for grab runs: the grab's close-in slide is legitimate
	// fast movement (it cancelled its own grab before this exemption); grabs are protected by the
	// break-off distance below + the ability's safety timer instead.
	//
	// NOR for our own warp lunge. This guard predates Chalkie montages being able to move the capsule at
	// all (the RootMotionMode bug); the moment they could, the attack's own 0.85s warp window closed
	// 110cm in 0.85s ≈ 129 cm/s and tripped this break-off BEFORE the hit window ever opened — the swing
	// cancelled itself. "Something is moving us" was only ever meant to mean something ELSE.
	//
	// Matched by GENERATION, not by a bare "is root motion live": a punch landing mid-bite starts the
	// KNOCKBACK's own drive, and a liveness-only test would exempt that too — leaving a Chalkie that is
	// being hurled backwards still swinging, the exact case this break-off exists to catch. Our
	// generation stops being the live one the moment anything else takes the bridge.
	const UAZ_PawnMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UAZ_PawnMoverComponent>() : nullptr;
	const bool bOwnLungeDriving = Mover && Mover->IsRootMotionDrivenBy(MeleeRootMotionGen);
	if (!bGrabRun && !bOwnLungeDriving && Pawn && Pawn->GetVelocity().Size2D() > AZ_MidBiteMoveBreakSpeed)
	{
		const TSubclassOf<UGameplayAbility> RunClass = ChosenAbilityClass ? ChosenAbilityClass : AbilityClass;
		UAbilitySystemComponent* MovingASC = BoundASC.Get();
		Cleanup();
		if (MovingASC)
		{
			if (FGameplayAbilitySpec* Spec = MovingASC->FindAbilitySpecFromClass(RunClass))
			{
				MovingASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Mid-swing tracking. Motion warping does this from inside the montage now (RegisterWarpTarget put the
	// target under WarpTargetName in follow mode, and the notify window on the clip decides WHEN tracking is
	// allowed), so this per-tick override only runs on the legacy path.
	//
	// Legacy soft-track: keep the body turning toward the target through the swing so the forward damage
	// sweep lands honestly on a strafing target. The override API takes a DIRECTION (unit vector, like
	// ScanAround) — passing the raw location rotated zombies toward a world-origin artifact (audit
	// rules-finding #1).
	if (!bUseMotionWarping && Target && Pawn)
	{
		if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(Controller))
		{
			Chalkie->SetFacingOverrideWorld(
				(Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D());
		}
	}
	if (Pawn && Target && FVector::Dist2D(Target->GetActorLocation(), Pawn->GetActorLocation()) > AZ_BreakOffDistance)
	{
		// UNBIND BEFORE CANCELLING: CancelAbilityHandle fires OnAbilityEnded synchronously — with the
		// delegate still bound it would re-enter and FinishLatentTask(Succeeded) out from under us.
		// (A grab-run tripping this only happens on a physics shove — the cancel path sends
		// Event.GrabRelease from the ability's EndAbility, so the player is freed correctly.)
		const TSubclassOf<UGameplayAbility> RunClass = ChosenAbilityClass ? ChosenAbilityClass : AbilityClass;
		UAbilitySystemComponent* ASC = BoundASC.Get();
		Cleanup();
		if (ASC)
		{
			if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(RunClass))
			{
				ASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// A grab legitimately runs far longer than a swing (hold window + exit montage).
	const float EffectiveTimeout = bGrabRun ? GrabTimeoutSeconds : TimeoutSeconds;
	if (ElapsedSeconds >= EffectiveTimeout)
	{
		// Timeout must CANCEL the ability too (audit #5): finishing without cancelling left an
		// unobserved melee running — RM override fighting nav, and a token-less zombie still visibly
		// attacking (silently breaking the max-2 invariant).
		const TSubclassOf<UGameplayAbility> RunClass = ChosenAbilityClass ? ChosenAbilityClass : AbilityClass;
		UAbilitySystemComponent* TimedOutASC = BoundASC.Get();
		Cleanup();
		if (TimedOutASC)
		{
			if (FGameplayAbilitySpec* Spec = TimedOutASC->FindAbilitySpecFromClass(RunClass))
			{
				TimedOutASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UAZ_BTTask_ZombieAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Higher-priority branch preempted the swing (target lost mid-attack, death) — cancel the ability.
	// UNBIND FIRST: the cancel fires OnAbilityEnded synchronously; with the delegate still bound it
	// would call FinishLatentTask(Succeeded) in the middle of the abort and wedge the tree on this
	// node (the "doesn't escape from attack" symptom with multiple NPCs).
	const TSubclassOf<UGameplayAbility> RunClass = ChosenAbilityClass ? ChosenAbilityClass : AbilityClass;
	UAbilitySystemComponent* ASC = BoundASC.Get();
	Cleanup();
	if (ASC)
	{
		if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(RunClass))
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
	return EBTNodeResult::Aborted;
}

void UAZ_BTTask_ZombieAttack::RegisterWarpTarget(UBehaviorTreeComponent& OwnerComp)
{
	if (!bUseMotionWarping || WarpTargetName.IsNone())
	{
		return;
	}
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const AActor* Target = AZ_GetChaseTarget(OwnerComp);
	if (!Pawn || !Target)
	{
		return;
	}
	UMotionWarpingComponent* Warping = Pawn->FindComponentByClass<UMotionWarpingComponent>();
	USceneComponent* TargetRoot = Target->GetRootComponent();
	if (!Warping || !TargetRoot)
	{
		return;
	}
	// bFollowComponent = true is the whole point: the modifier re-reads the component's transform every
	// frame, so a target that strafes mid-swing is tracked continuously instead of the swing committing to
	// wherever they stood at activation. NAME_None bone = the component's own transform (the capsule),
	// which is what we want to claw at — not a jittering hand/head bone.
	//
	// VectorFromTargetToOwner puts the warp point WarpApproachDistance in front of the target, measured
	// back toward us — so it stays on our side of them however they turn or strafe. Aiming at their centre
	// instead would park the zombie inside their capsule (see WarpApproachDistance).
	//
	// All six trailing args are spelled out on purpose: the two AddOrUpdateWarpTargetFromComponent
	// overloads differ ONLY in their 5th parameter and both default it, so any shorter call is ambiguous.
	Warping->AddOrUpdateWarpTargetFromComponent(WarpTargetName, TargetRoot, NAME_None, /*bFollowComponent*/ true,
		EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner,
		FVector(WarpApproachDistance, 0.f, 0.f), FRotator::ZeroRotator);
}

void UAZ_BTTask_ZombieAttack::ClearWarpTarget()
{
	if (const UBehaviorTreeComponent* Comp = OwningComp.Get())
	{
		if (const AAIController* Controller = Comp->GetAIOwner())
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				if (UMotionWarpingComponent* Warping = Pawn->FindComponentByClass<UMotionWarpingComponent>())
				{
					Warping->RemoveWarpTarget(WarpTargetName);
				}
			}
		}
	}
}

void UAZ_BTTask_ZombieAttack::Cleanup()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	// Drop the warp target on EVERY exit alongside the facing override below. A stale target left
	// registered would silently warp the NEXT swing toward whoever this run was chasing.
	ClearWarpTarget();
	// Release the engagement token + drop the soft-tracking facing override — EVERY task exit
	// (success, break-off, abort, timeout) funnels through here.
	if (UBehaviorTreeComponent* Comp = OwningComp.Get())
	{
		if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(Comp->GetAIOwner()))
		{
			Chalkie->ClearFacingOverride();
			Chalkie->SetMeleeTaskActive(false);   // swing over — rotation may take the turn again
			if (const APawn* Pawn = Chalkie->GetPawn())
			{
				if (UAZ_HordeSubsystem* Horde = Pawn->GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
				{
					Horde->ReleaseAttackToken(Chalkie);
					if (bGrabRun)
					{
						Horde->ReleaseGrabToken(Chalkie);
					}
				}
				// Cooldown counts from the END of the grab (a long hold mustn't eat its own cooldown).
				if (bGrabRun)
				{
					Chalkie->LastGrabEndTimeSeconds = Pawn->GetWorld()->GetTimeSeconds();
				}
			}
		}
	}
	bGrabRun = false;
	MeleeRootMotionGen = 0;
	AbilityEndedHandle.Reset();
	BoundASC = nullptr;
	OwningComp = nullptr;
}

FString UAZ_BTTask_ZombieAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s, latent until it ends (timeout %.1fs)\nTracking: %s"),
		*GetNameSafe(*AbilityClass), TimeoutSeconds,
		bUseMotionWarping
			? *FString::Printf(TEXT("motion warp -> '%s'"), *WarpTargetName.ToString())
			: TEXT("legacy facing override"));
}
