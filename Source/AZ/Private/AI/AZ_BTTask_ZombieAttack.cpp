// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ZombieAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "AI/AZ_HordeSubsystem.h"
#include "AI/AZ_InfectedAIController.h"   // AZ_ChalkieBBKeys
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/SkeletalMeshComponent.h"
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
		// player's earned stagger window. (Proper State.Staggered tag lands with the batch.)
		if (const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Pawn))
		{
			UAnimMontage* Flinch = UAZ_GA_MeleeAttack::FindAnimSetMontage(Infected, TEXT("HitReactMontage"));
			const UAnimInstance* AnimInstance = Infected->GetMesh() ? Infected->GetMesh()->GetAnimInstance() : nullptr;
			if (Flinch && AnimInstance && AnimInstance->Montage_IsPlaying(Flinch))
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

	// Kill any residual path-following before committing to the swing — a swing starts from stillness.
	if (AAIController* AIOwner = OwnerComp.GetAIOwner())
	{
		AIOwner->StopMovement();
	}

	ElapsedSeconds = 0.f;
	BoundASC = ASC;
	OwningComp = &OwnerComp;

	// Bind AFTER activation (audit #7): a synchronous activate-and-end inside TryActivate would fire
	// the delegate while this node isn't latent yet — engine-internal behavior we shouldn't lean on.
	// The IsActive() recheck below fully covers anything that ended before the bind.
	if (!ASC->TryActivateAbilityByClass(AbilityClass))
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UAZ_BTTask_ZombieAttack::OnAbilityEnded);
	// Sync-end guard: the ability can activate AND end within TryActivate (missing montage, instant
	// fail). Our delegate fired before the task was latent — FinishLatentTask was ignored — so without
	// this check the node hangs until the timeout. If it's already over, report it directly.
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || !Spec->IsActive())
	{
		Cleanup();
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::InProgress;
}

void UAZ_BTTask_ZombieAttack::OnAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (!EndedData.AbilityThatEnded || !EndedData.AbilityThatEnded->GetClass()->IsChildOf(AbilityClass))
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
	// playing a claw anim over locomotion.
	if (Pawn && Pawn->GetVelocity().Size2D() > AZ_MidBiteMoveBreakSpeed)
	{
		UAbilitySystemComponent* MovingASC = BoundASC.Get();
		Cleanup();
		if (MovingASC)
		{
			if (FGameplayAbilitySpec* Spec = MovingASC->FindAbilitySpecFromClass(AbilityClass))
			{
				MovingASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Soft tracking (MotionWarping's stand-in until the warp primitive lands): keep the body turning
	// toward the target through the swing so the forward damage sweep lands honestly on a strafing
	// target. The override API takes a DIRECTION (unit vector, like ScanAround) — passing the raw
	// location rotated zombies toward a world-origin artifact (audit rules-finding #1).
	if (Target && Pawn)
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
		UAbilitySystemComponent* ASC = BoundASC.Get();
		Cleanup();
		if (ASC)
		{
			if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
			{
				ASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (ElapsedSeconds >= TimeoutSeconds)
	{
		// Timeout must CANCEL the ability too (audit #5): finishing without cancelling left an
		// unobserved melee running — RM override fighting nav, and a token-less zombie still visibly
		// attacking (silently breaking the max-2 invariant).
		UAbilitySystemComponent* TimedOutASC = BoundASC.Get();
		Cleanup();
		if (TimedOutASC)
		{
			if (FGameplayAbilitySpec* Spec = TimedOutASC->FindAbilitySpecFromClass(AbilityClass))
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
	UAbilitySystemComponent* ASC = BoundASC.Get();
	Cleanup();
	if (ASC)
	{
		if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
	return EBTNodeResult::Aborted;
}

void UAZ_BTTask_ZombieAttack::Cleanup()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	// Release the engagement token + drop the soft-tracking facing override — EVERY task exit
	// (success, break-off, abort, timeout) funnels through here.
	if (UBehaviorTreeComponent* Comp = OwningComp.Get())
	{
		if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(Comp->GetAIOwner()))
		{
			Chalkie->ClearFacingOverride();
			if (const APawn* Pawn = Chalkie->GetPawn())
			{
				if (UAZ_HordeSubsystem* Horde = Pawn->GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
				{
					Horde->ReleaseAttackToken(Chalkie);
				}
			}
		}
	}
	AbilityEndedHandle.Reset();
	BoundASC = nullptr;
	OwningComp = nullptr;
}

FString UAZ_BTTask_ZombieAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s, latent until it ends (timeout %.1fs)"),
		*GetNameSafe(*AbilityClass), TimeoutSeconds);
}
