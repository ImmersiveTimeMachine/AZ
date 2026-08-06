// Copyright Artur. AZ project.

#include "Character/Cmc/AZ_CmcInfectedCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/Abilities/AZ_GA_Death.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"
#include "AI/AZ_InfectedAIController.h"
#include "AZ_GameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AAZ_CmcInfectedCharacter::AAZ_CmcInfectedCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No per-frame actor tick: no camera to interp, the AI drive lives on the controller, CMC ticks
	// itself. Keeps a horde of these cheap (v2 convention).
	PrimaryActorTick.bCanEverTick = false;

	// Enemy of the player (team 0). Read by AI perception through the base's team interface.
	DefaultTeamId = 1;

	// Auto-possess by the SAME AI controller the v2 Chalkie uses — the BB/BT brain is reused as-is;
	// only its Mover touchpoints grow CMC branches (P3).
	AIControllerClass = AAZ_InfectedAIController::StaticClass();
	AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;

	// NPC pace: slower body turn than the hero; nav drives orient-to-movement from the base.
	GetCharacterMovement()->RotationRate = FRotator(0.f, 300.f, 0.f);
	// Walk is the Chalkie's resting gait; the AI escalates via SetGait (shamble->chase).
	CurrentGait = EAZ_Gait::Walk;

	// NPC hands/face keep animating for grab-IK correctness even off-screen (v2 convention).
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- GAS: own ASC + shared vitals (S1 damage spine). Minimal replication mode = the NPC standard
	// (GEs don't replicate to simulated proxies; tags/cues do). ---
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	VitalsAttributeSet = CreateDefaultSubobject<UAZ_VitalsAttributeSet>(TEXT("VitalsAttributeSet"));
}

void AAZ_CmcInfectedCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

UAbilitySystemComponent* AAZ_CmcInfectedCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAZ_CmcInfectedCharacter::InitAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Owner = avatar = this pawn (NPC pattern). Re-entrant safe.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority() || bStartupAbilitiesGranted)
	{
		return;
	}
	bStartupAbilitiesGranted = true;

	// Death + HitReact — the two event-triggered abilities every fighting body needs from frame one.
	// Grant the RESOLVED class and patch ITS CDO (doctrine rule 2). Melee/Grab land in P3 with the BT.
	UClass* DeathClass = *DeathAbilityClass ? *DeathAbilityClass : UAZ_GA_Death::StaticClass();
	UAZ_GA_Death::ConfigureTriggerOnCDO(DeathClass);
	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DeathClass, 1, INDEX_NONE, this));

	UClass* HitReactClass = *HitReactAbilityClass ? *HitReactAbilityClass : UAZ_GA_HitReact::StaticClass();
	UAZ_GA_HitReact::ConfigureOnCDO(HitReactClass);
	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(HitReactClass, 1, INDEX_NONE, this));

	UE_LOG(LogTemp, Display, TEXT("[CmcInfected] %s: granted %s + %s"),
		*GetName(), *DeathClass->GetName(), *HitReactClass->GetName());
}

void AAZ_CmcInfectedCharacter::SetStaggeredFor(float Seconds)
{
	if (!AbilitySystemComponent || Seconds <= 0.f)
	{
		return;
	}

	// ONE shared deadline, last-writer-wins, ONLY EVER EXTENDS (v2's counted-tag-trap guard): a second
	// cause pushing the deadline out never lets its clear cut short a longer hold already running.
	const double NewEndTime = GetWorld()->GetTimeSeconds() + Seconds;
	if (NewEndTime <= StaggerHoldEndTime)
	{
		return;
	}
	StaggerHoldEndTime = NewEndTime;

	const FGameplayTag& StaggerTag = FAZ_GameplayTags::Get().State_Combat_Staggered;
	if (!AbilitySystemComponent->HasMatchingGameplayTag(StaggerTag))
	{
		AbilitySystemComponent->AddLooseGameplayTag(StaggerTag);
	}
	GetWorld()->GetTimerManager().SetTimer(StaggerHoldTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		StaggerHoldEndTime = 0.0;
		// Only releases what THIS hold put on: an active reaction ability re-asserts the tag through its
		// own ActivationOwnedTags lifecycle (its removal is counted separately by the loose-tag count).
		AbilitySystemComponent->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Combat_Staggered);
	}), Seconds, false);
}

bool AAZ_CmcInfectedCharacter::IsStaggerReactionPlaying() const
{
	// Tag query, not Montage_IsPlaying (v2 lesson: blend timing set AI pacing by accident).
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Combat_Staggered);
}

void AAZ_CmcInfectedCharacter::BeginCorpse(float RagdollDelay)
{
	if (bCorpse)
	{
		return;
	}
	bCorpse = true;

	// Brain off, collision off, movement off; the death montage keeps playing on the mesh.
	DetachFromControllerPendingDestroy();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (RagdollDelay <= 0.f)
	{
		RagdollCorpse();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(RagdollTimer, this, &AAZ_CmcInfectedCharacter::RagdollCorpse,
			RagdollDelay, false);
	}

	SetLifeSpan(20.f);
}

void AAZ_CmcInfectedCharacter::RagdollCorpse()
{
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Body->SetSimulatePhysics(true);
}
