
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
	PrimaryActorTick.bCanEverTick = false;

	DefaultTeamId = 1;

	AIControllerClass = AAZ_InfectedAIController::StaticClass();
	AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCharacterMovement()->RotationRate = FRotator(0.f, 300.f, 0.f);
	CurrentGait = EAZ_Gait::Walk;

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

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

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority() || bStartupAbilitiesGranted)
	{
		return;
	}
	bStartupAbilitiesGranted = true;

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
		AbilitySystemComponent->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Combat_Staggered);
	}), Seconds, false);
}

bool AAZ_CmcInfectedCharacter::IsStaggerReactionPlaying() const
{
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
