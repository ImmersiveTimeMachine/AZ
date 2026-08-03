// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Death.h"

#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetMontage
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "AI/AZ_HordeSubsystem.h"
#include "AI/AZ_InfectedAIController.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"

UAZ_GA_Death::UAZ_GA_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Death originates on the authority (vitals execute server-side); montage replicates via the ASC.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// Death preempts everything and can never be blocked by combat state.
	bServerRespectsRemoteAbilityCancellation = false;
}

void UAZ_GA_Death::ConfigureTriggerOnCDO(UClass* GrantClass)
{
	// Guard by the SPECIFIC tag, not Num()==0 (audit #10): a future second trigger added here would
	// otherwise silently never apply on a CDO already patched by an earlier session.
	UClass* TargetClass = GrantClass ? GrantClass : UAZ_GA_Death::StaticClass();
	UAZ_GA_Death* CDO = Cast<UAZ_GA_Death>(TargetClass->GetDefaultObject());
	const FGameplayTag& DeathTag = FAZ_GameplayTags::Get().Event_Death;
	if (CDO && !CDO->AbilityTriggers.ContainsByPredicate(
		[&DeathTag](const FAbilityTriggerData& T) { return T.TriggerTag == DeathTag; }))
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = DeathTag;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		CDO->AbilityTriggers.Add(Trigger);
	}
}

void UAZ_GA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Dying interrupts whatever else was running (mid-swing death stops the attack + its montage).
	ASC->CancelAbilities(nullptr, nullptr, this);

	// Directional pick from where the killing blow came (payload Instigator = effect causer).
	FName DeathProperty = TEXT("DeathMontage_F");
	const AActor* Killer = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	if (Killer)
	{
		const FVector ToKiller = (Killer->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		const FVector Fwd = Avatar->GetActorForwardVector().GetSafeNormal2D();
		const float FwdDot = FVector::DotProduct(Fwd, ToKiller);
		const float RightDot = FVector::DotProduct(FVector::CrossProduct(FVector::UpVector, Fwd), ToKiller);
		if (FwdDot > 0.707f)        { DeathProperty = TEXT("DeathMontage_F"); }
		else if (FwdDot < -0.707f)  { DeathProperty = TEXT("DeathMontage_B"); }
		else if (RightDot > 0.f)    { DeathProperty = TEXT("DeathMontage_R"); }
		else                        { DeathProperty = TEXT("DeathMontage_L"); }
	}

	UAnimMontage* DeathMontage = UAZ_GA_MeleeAttack::FindAnimSetMontage(Avatar, DeathProperty);
	float RagdollDelay = 0.f;   // no montage (no anim set / hero until his set exists) -> instant ragdoll
	if (DeathMontage)
	{
		// bStopWhenAbilityEnds=false: the ability ends right after setup, the montage plays on and the
		// corpse holds its last frame (auto-blend-out is disabled on the death montages).
		MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
			this, FName("DeathMontage"), DeathMontage, FGameplayTagContainer(),
			/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ false,
			/*AnimRootMotionTranslationScale*/ 1.f);
		if (MontageTask)
		{
			MontageTask->ReadyForActivation();
			RagdollDelay = DeathMontage->GetPlayLength() * RagdollAtMontageFraction;

			// Root-motion death: the montage now sources the ROOT clip (real collapse trajectory) — the
			// layered move makes the CAPSULE follow it, so the body falls WHERE the anim says, not in place.
			if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
			{
				Mover->DriveRootMotion(DeathMontage->GetPlayLength());
			}
		}
	}

	// PACK ALERT: a pack-mate going down is a stimulus — every registered Chalkie within the horde
	// subsystem's AlertRadius converges (urgent investigation) on the KILLER's position, making a kill
	// near the pack a real decision. Must fire BEFORE BeginCorpse (which detaches the controller).
	if (AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Avatar))
	{
		if (AAZ_InfectedAIController* DyingController = Cast<AAZ_InfectedAIController>(Infected->GetController()))
		{
			if (UAZ_HordeSubsystem* Horde = Avatar->GetWorld()->GetSubsystem<UAZ_HordeSubsystem>())
			{
				Horde->NotifyAggro(DyingController, Killer ? Killer->GetActorLocation() : Avatar->GetActorLocation());
			}
		}
		Infected->BeginCorpse(RagdollDelay);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
