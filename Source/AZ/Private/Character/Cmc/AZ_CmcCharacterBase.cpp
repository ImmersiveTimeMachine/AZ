// Copyright Artur. AZ project.

#include "Character/Cmc/AZ_CmcCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContextualAnimSceneActorComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"

AAZ_CmcCharacterBase::AAZ_CmcCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// --- Capsule / mesh conventions carried over from the v2 pawns so the same skeletal meshes align
	// and floor queries behave identically (capsule 25/90, mesh at -92 with the -90 yaw the whole anim
	// stack assumes). BP children assign the actual mesh + ABP assets (no /Game/ paths in C++). ---
	GetCapsuleComponent()->InitCapsuleSize(25.f, 90.f);
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	// A nav-relevant capsule carves a hole in the navmesh under the pawn — the infected's own path
	// queries then start "off-navmesh" inside its own carve (found the hard way in the v2 Phase-0 smoke).
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// Pawns are never floors: a chaser slides along the hero's capsule instead of climbing on top.
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Body rotation policy: orient-to-movement (GASP explore semantics). Controller rotation never
	// drives the body directly; children tune RotationRate.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Native crouch is the stance owner (the v2 generation hand-built this as a Mover mode).
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// CAS readiness — see header. Both components are engine-self-wiring; zero per-frame cost while idle.
	MotionWarpingComponent  = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	ContextualAnimComponent = CreateDefaultSubobject<UContextualAnimSceneActorComponent>(TEXT("ContextualAnim"));
}

void AAZ_CmcCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Resolved here (not the ctor) so a BP-child override of the EditDefaultsOnly values is honored.
	TeamId = FGenericTeamId(DefaultTeamId);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	SetGait(CurrentGait);
}

void AAZ_CmcCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_CmcCharacterBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TagToCheck);
}

bool AAZ_CmcCharacterBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAllMatchingGameplayTags(TagContainer);
}

bool AAZ_CmcCharacterBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAnyMatchingGameplayTags(TagContainer);
}

void AAZ_CmcCharacterBase::SetJumpPressed(bool bPressed)
{
	// GAS gated the request (UAZ_GA_PawnJump); CMC executes. This is the "legacy CMC pawn" leg the
	// IAZ_JumpRequester contract always documented.
	if (bPressed)
	{
		Jump();
	}
	else
	{
		StopJumping();
	}
}

void AAZ_CmcCharacterBase::SetGait(EAZ_Gait NewGait)
{
	CurrentGait = NewGait;
	float Speed = RunSpeed;
	switch (NewGait)
	{
	case EAZ_Gait::Walk:   Speed = WalkSpeed;   break;
	case EAZ_Gait::Run:    Speed = RunSpeed;    break;
	case EAZ_Gait::Sprint: Speed = SprintSpeed; break;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}
