// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableFireArea.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AZ_GameplayTags.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AISense_Hearing.h"
#include "Throwables/AZ_BurningComponent.h"
#include "Throwables/AZ_ThrowableDefinition.h"
#include "TimerManager.h"
#include "Particles/ParticleSystemComponent.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Obstacle.h"

namespace
{
	constexpr float FireStepSeconds = 0.5f;
	constexpr float MaxFireRadius = 2000.f;
	constexpr float MaxFireLifetime = 60.f;
}

AAZ_ThrowableFireArea::AAZ_ThrowableFireArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // Finite hazard cosmetics must reach viewers outside ordinary actor relevancy.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AAZ_ThrowableFireArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAZ_ThrowableFireArea, Definition);
	DOREPLIFETIME(AAZ_ThrowableFireArea, bActive);
	DOREPLIFETIME(AAZ_ThrowableFireArea, GroundNormal);
}

void AAZ_ThrowableFireArea::Initialize(const UAZ_ThrowableDefinition* InDefinition,
	APawn* InThrower, const FVector& SurfaceNormal)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || bActive || !IsValid(InDefinition)
		|| !FMath::IsFinite(InDefinition->FireRadius) || InDefinition->FireRadius <= 0.f
		|| !FMath::IsFinite(InDefinition->FireDuration) || InDefinition->FireDuration <= 0.f)
	{
		if (HasAuthority()) Destroy();
		return;
	}

	Definition = InDefinition;
	Thrower = InThrower;
	Radius = FMath::Clamp(InDefinition->FireRadius, 1.f, MaxFireRadius);
	ExpiresAt = World->GetTimeSeconds() + FMath::Clamp(InDefinition->FireDuration,
		FireStepSeconds, MaxFireLifetime);
	GroundNormal = SurfaceNormal.ContainsNaN() ? FVector::UpVector
		: SurfaceNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
	bActive = true;
	// High-cost, temporary navigation area: existing paths may replan around the hazard.
	// The level needs DynamicModifiersOnly (or Dynamic) runtime navigation.
	UNavModifierComponent* NavHazard = NewObject<UNavModifierComponent>(this, TEXT("FireNavigationCost"));
	NavHazard->FailsafeExtent = FVector(Radius, Radius, 80.f);
	NavHazard->SetAreaClass(UNavArea_Obstacle::StaticClass());
	AddInstanceComponent(NavHazard);
	NavHazard->RegisterComponent();
	RefreshVisuals();

	if (IsValid(InThrower) && FMath::IsFinite(InDefinition->FireLoudness)
		&& InDefinition->FireLoudness > 0.f)
	{
		UAISense_Hearing::ReportNoiseEvent(World, GetActorLocation(),
			FMath::Clamp(InDefinition->FireLoudness, 0.f, 10.f), InThrower,
			FMath::Max(500.f, Radius * 4.f), InDefinition->FireNoiseTag);
	}
	// Defer initial contact one frame; subsequent scans remain on a fixed half-second cadence.
	World->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireStep,
		FireStepSeconds, true, 0.01f);
}

void AAZ_ThrowableFireArea::FireStep()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !bActive || !IsValid(Definition)) return;
	if (World->GetTimeSeconds() >= ExpiresAt)
	{
		FinishFire();
		return;
	}

	const FVector Origin = GetActorLocation();
	TArray<AActor*> Pawns;
	const TArray<TEnumAsByte<EObjectTypeQuery>> PawnObjects = {
		UEngineTypes::ConvertToObjectType(ECC_Pawn) };
	const TArray<AActor*> Ignored = { this };
	UKismetSystemLibrary::SphereOverlapActors(World, Origin, Radius, PawnObjects,
		APawn::StaticClass(), Ignored, Pawns);

	// A low cylinder around the impact surface prevents fire on the floor above or below.
	const float VerticalHalfHeight = FMath::Clamp(Radius * 0.6f, 100.f, 250.f);
	for (AActor* Target : Pawns)
	{
		if (!IsValid(Target)) continue;
		const FVector TargetPoint = Target->GetActorLocation();
		if (FMath::Abs(TargetPoint.Z - Origin.Z) > VerticalHalfHeight
			|| FVector::DistSquared2D(TargetPoint, Origin) > FMath::Square(Radius)) continue;

		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!ASC || !ASC->HasAttributeSetForAttribute(UAZ_VitalsAttributeSet::GetHealthAttribute())
			|| ASC->GetNumericAttribute(UAZ_VitalsAttributeSet::GetHealthAttribute()) <= 0.f
			|| ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Character_Dead)) continue;

		FCollisionQueryParams Query(SCENE_QUERY_STAT(ThrowFireLOS), false, this);
		Query.AddIgnoredActor(Target);
		FHitResult Obstruction;
		if (World->LineTraceSingleByChannel(Obstruction, Origin + FVector(0.f, 0.f, 20.f),
			TargetPoint, ECC_Visibility, Query)) continue;

		UAZ_BurningComponent* Burn = Target->FindComponentByClass<UAZ_BurningComponent>();
		if (!Burn)
		{
			Burn = NewObject<UAZ_BurningComponent>(Target, UAZ_BurningComponent::StaticClass(),
				TEXT("AZ_BurningComponent"));
			if (!Burn) continue;
			Target->AddInstanceComponent(Burn);
			Burn->RegisterComponent();
		}
		Burn->Ignite(Definition, Thrower.Get(), this);
	}
}

void AAZ_ThrowableFireArea::FinishFire()
{
	if (!HasAuthority() || !bActive) return;
	bActive = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FireTimer);
	if (auto* NavHazard = FindComponentByClass<UNavModifierComponent>()) NavHazard->DestroyComponent();
	RefreshVisuals();
	SetLifeSpan(0.2f);
}

void AAZ_ThrowableFireArea::OnRep_FireVisualState()
{
	RefreshVisuals();
}

void AAZ_ThrowableFireArea::RefreshVisuals()
{
	if (GroundParticles) { GroundParticles->DestroyComponent(); GroundParticles = nullptr; }
	if (IsValid(GroundEffect))
	{
		GroundEffect->DestroyComponent();
		GroundEffect = nullptr;
	}
	if (IsValid(LoopAudio))
	{
		LoopAudio->Stop();
		LoopAudio->DestroyComponent();
		LoopAudio = nullptr;
	}
	if (!bActive || !IsValid(Definition) || !SceneRoot || GetNetMode() == NM_DedicatedServer) return;
	if (Definition->GroundFireEffect)
	{
		GroundEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(Definition->GroundFireEffect,
			SceneRoot, NAME_None, FVector::ZeroVector,
			FRotationMatrix::MakeFromZ(GroundNormal).Rotator(), EAttachLocation::KeepRelativeOffset, false);
	}
	else if (Definition->GroundFireParticles)
	{
		GroundParticles = UGameplayStatics::SpawnEmitterAttached(Definition->GroundFireParticles, SceneRoot,
			NAME_None, FVector::ZeroVector, FRotationMatrix::MakeFromZ(GroundNormal).Rotator(),
			FVector(1.5f, 1.5f, 0.7f), EAttachLocation::KeepRelativeOffset, false);
	}
	if (Definition->FireLoopSound)
	{
		LoopAudio = UGameplayStatics::SpawnSoundAttached(Definition->FireLoopSound, SceneRoot,
			NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f,
			nullptr, nullptr, false);
	}
}

void AAZ_ThrowableFireArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FireTimer);
	bActive = false;
	RefreshVisuals();
	Super::EndPlay(EndPlayReason);
}
