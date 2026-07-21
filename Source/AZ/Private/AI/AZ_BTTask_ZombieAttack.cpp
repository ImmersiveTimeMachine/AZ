// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ZombieAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

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

	ElapsedSeconds = 0.f;
	BoundASC = ASC;
	OwningComp = &OwnerComp;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UAZ_BTTask_ZombieAttack::OnAbilityEnded);

	if (!ASC->TryActivateAbilityByClass(AbilityClass))
	{
		Cleanup();
		return EBTNodeResult::Failed;
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
	if (ElapsedSeconds >= TimeoutSeconds)
	{
		Cleanup();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UAZ_BTTask_ZombieAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Higher-priority branch preempted the swing (target lost mid-attack, death) — cancel the ability.
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
	Cleanup();
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
