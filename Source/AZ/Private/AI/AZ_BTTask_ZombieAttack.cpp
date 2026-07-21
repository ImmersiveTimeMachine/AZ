// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ZombieAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "AI/AZ_InfectedAIController.h"   // AZ_ChalkieBBKeys
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

namespace
{
	// Range gates (constants until the next CLI batch promotes them to UPROPERTYs):
	// don't START a swing beyond reach; BREAK OFF a swing whose target escaped — either way the
	// task fails, the Chase sequence restarts, and MoveTo resumes the pursuit.
	constexpr float AZ_MaxAttackStartDistance = 220.f;
	constexpr float AZ_BreakOffDistance = 360.f;

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

	// Range gate: no swinging at air. Out of reach -> fail -> the Chase sequence loops back to MoveTo.
	const AActor* Target = AZ_GetChaseTarget(OwnerComp);
	if (Target && FVector::Dist2D(Target->GetActorLocation(), Pawn->GetActorLocation()) > AZ_MaxAttackStartDistance)
	{
		return EBTNodeResult::Failed;
	}

	ElapsedSeconds = 0.f;
	BoundASC = ASC;
	OwningComp = &OwnerComp;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UAZ_BTTask_ZombieAttack::OnAbilityEnded);

	if (!ASC->TryActivateAbilityByClass(AbilityClass))
	{
		Cleanup();
		return EBTNodeResult::Failed;
	}
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
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const AActor* Target = AZ_GetChaseTarget(OwnerComp);
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
		Cleanup();
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
	AbilityEndedHandle.Reset();
	BoundASC = nullptr;
	OwningComp = nullptr;
}

FString UAZ_BTTask_ZombieAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s, latent until it ends (timeout %.1fs)"),
		*GetNameSafe(*AbilityClass), TimeoutSeconds);
}
