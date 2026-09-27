// Copyright Artur. AZ project.

#include "Throwables/AZ_BurningComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AZ_GameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Throwables/AZ_ThrowableDefinition.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

namespace
{
	constexpr float BurnStepSeconds = 0.5f;
	constexpr float MaxBurnDuration = 30.f;
	constexpr float MaxBurnDps = 1000.f;
}

UAZ_BurningComponent::UAZ_BurningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_BurningComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_BurningComponent, bBurning);
	DOREPLIFETIME(UAZ_BurningComponent, VisualDefinition);
}

bool UAZ_BurningComponent::HasLivingVitals() const
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = IsValid(Owner)
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) : nullptr;
	return ASC && ASC->HasAttributeSetForAttribute(UAZ_VitalsAttributeSet::GetHealthAttribute())
		&& ASC->GetNumericAttribute(UAZ_VitalsAttributeSet::GetHealthAttribute()) > 0.f
		&& !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Character_Dead);
}

void UAZ_BurningComponent::Ignite(const UAZ_ThrowableDefinition* InDefinition,
	APawn* Thrower, AActor* FireSource)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !World || !IsValid(InDefinition)
		|| !HasLivingVitals() || !FMath::IsFinite(InDefinition->FireDamagePerSecond)
		|| !FMath::IsFinite(InDefinition->BurnDuration)
		|| InDefinition->FireDamagePerSecond <= 0.f || InDefinition->BurnDuration <= 0.f) return;

	const double Now = World->GetTimeSeconds();
	const bool bWasBurning = bBurning;
	if (!bWasBurning) { LastDamageAt = Now; bNeedsInitialReaction = true; }
	const double RequestedExpiry = Now + FMath::Clamp(InDefinition->BurnDuration, BurnStepSeconds, MaxBurnDuration);
	BurnExpiresAt = bWasBurning ? FMath::Max(BurnExpiresAt, RequestedExpiry) : RequestedExpiry;
	// Keep one timer and at most the strongest single patch's DPS, even if patches overlap.
	DamagePerSecond = bWasBurning
		? FMath::Max(DamagePerSecond, FMath::Clamp(InDefinition->FireDamagePerSecond, 0.f, MaxBurnDps))
		: FMath::Clamp(InDefinition->FireDamagePerSecond, 0.f, MaxBurnDps);
	SourceThrower = Thrower;
	SourceFire = FireSource;
	FireOrigin = IsValid(FireSource) ? FireSource->GetActorLocation() : Owner->GetActorLocation();
	const bool bVisualChanged = VisualDefinition != InDefinition;
	VisualDefinition = InDefinition;
	bBurning = true;
	if (!bWasBurning)
	{
		RefreshVisuals();
		UE_LOG(LogTemp, Log, TEXT("[Burn] ignited %s duration=%.1f"), *GetNameSafe(Owner), InDefinition->BurnDuration);
		World->GetTimerManager().SetTimer(BurnTimer, this, &ThisClass::BurnStep,
			BurnStepSeconds, true, BurnStepSeconds);
	}
	else if (bVisualChanged)
	{
		RefreshVisuals();
	}
}

void UAZ_BurningComponent::BurnStep()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!bBurning || !World || !IsValid(Owner) || !Owner->HasAuthority()) return;
	if (!HasLivingVitals())
	{
		StopBurning();
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double DamageUntil = FMath::Min(Now, BurnExpiresAt);
	const float Damage = DamagePerSecond * static_cast<float>(
		FMath::Clamp(DamageUntil - LastDamageAt, 0.0, static_cast<double>(BurnStepSeconds)));
	LastDamageAt = DamageUntil;
	if (Damage > 0.f)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
		APawn* Thrower = SourceThrower.Get();
		UAbilitySystemComponent* SourceASC = IsValid(Thrower)
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Thrower) : nullptr;
		if (!SourceASC) SourceASC = TargetASC; // Fire outlives its thrower; damage still reaches vitals.
		if (SourceASC && TargetASC)
		{
			const FVector TargetPoint = Owner->GetActorLocation();
			FHitResult FireHit;
			FireHit.TraceStart = FireOrigin;
			FireHit.TraceEnd = TargetPoint;
			FireHit.Location = TargetPoint;
			FireHit.ImpactPoint = TargetPoint;
			FireHit.Normal = (FireOrigin - TargetPoint).GetSafeNormal();
			FireHit.ImpactNormal = FireHit.Normal;
			FireHit.Distance = FVector::Distance(FireOrigin, TargetPoint);
			FireHit.HitObjectHandle = FActorInstanceHandle(Owner);
			FireHit.Component = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
			FireHit.bBlockingHit = true;

			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddInstigator(IsValid(Thrower) ? Thrower : nullptr,
				IsValid(SourceFire.Get()) ? SourceFire.Get() : Owner);
			Context.AddSourceObject(this); // Vitals can distinguish periodic fire from direct hits.
			Context.AddHitResult(FireHit);
			const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
				UAZ_GE_Damage::StaticClass(), 1.f, Context);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, Damage);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
	if (IsValid(this) && (Now >= BurnExpiresAt || !HasLivingVitals())) StopBurning();
}

void UAZ_BurningComponent::StopBurning()
{
	if (!bBurning) return;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BurnTimer);
	bBurning = false;
	DamagePerSecond = 0.f;
	VisualDefinition = nullptr;
	SourceThrower.Reset();
	SourceFire.Reset();
	RefreshVisuals();
}

void UAZ_BurningComponent::OnRep_BurningState()
{
	RefreshVisuals();
}

void UAZ_BurningComponent::RefreshVisuals()
{
	for (UParticleSystemComponent* Particles : BurningParticles) if (IsValid(Particles)) Particles->DestroyComponent();
	BurningParticles.Reset();
	if (BurningEffect)
	{
		BurningEffect->DestroyComponent();
		BurningEffect = nullptr;
	}
	AActor* Owner = GetOwner();
	if (!bBurning || !IsValid(VisualDefinition)
		|| !IsValid(Owner) || !Owner->GetRootComponent()) return;
	if (GetNetMode() == NM_DedicatedServer) return;
	USkeletalMeshComponent* Body = Owner->FindComponentByClass<USkeletalMeshComponent>();
	USceneComponent* AttachTo = Body ? Body : Owner->GetRootComponent();
	const FName TorsoSocket = Body && Body->DoesSocketExist(TEXT("spine_02")) ? FName(TEXT("spine_02")) : NAME_None;
	if (VisualDefinition->BurningTargetEffect) BurningEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		VisualDefinition->BurningTargetEffect, AttachTo, TorsoSocket,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false);
	else if (VisualDefinition->BurningTargetParticles)
	{
		if (BodyParticleSource != VisualDefinition->BurningTargetParticles || !BodyParticleTemplate)
		{
			BodyParticleSource = VisualDefinition->BurningTargetParticles;
			BodyParticleTemplate = DuplicateObject<UParticleSystem>(BodyParticleSource, this);
			if (!BodyParticleTemplate) return;
			// A private body effect follows animation; the shared campfire asset stays unchanged.
			for (UParticleEmitter* Emitter : BodyParticleTemplate->Emitters)
			{
				if (!Emitter) continue;
				const FString Name = Emitter->GetEmitterName().ToString();
				for (UParticleLODLevel* LOD : Emitter->LODLevels)
				{
					if (!LOD || !LOD->RequiredModule) continue;
					LOD->RequiredModule->bUseLocalSpace = true;
					if (Name.Contains(TEXT("Spark")) || Name.Contains(TEXT("Ember"))) LOD->bEnabled = false;
				}
			}
		}
		const auto SpawnBodyFlame = [&](USceneComponent* Parent, FName Socket, FVector Offset, float Scale)
		{
			if (auto* Particles = UGameplayStatics::SpawnEmitterAttached(BodyParticleTemplate, Parent, Socket,
				Offset, FRotator::ZeroRotator, FVector(Scale), EAttachLocation::KeepRelativeOffset, false))
			{
				Particles->SetAbsolute(false, true, true);
				Particles->SetWorldRotation(FRotator::ZeroRotator);
				Particles->SetWorldScale3D(FVector(Scale));
				BurningParticles.Add(Particles);
			}
		};
		if (Body && Body->DoesSocketExist(TEXT("pelvis")))
		{
			SpawnBodyFlame(Body, TEXT("pelvis"), FVector::ZeroVector, .55f);
			if (!TorsoSocket.IsNone()) SpawnBodyFlame(Body, TorsoSocket, FVector::ZeroVector, .45f);
		}
		else SpawnBodyFlame(Owner->GetRootComponent(), NAME_None, FVector(0,0,-35), .65f);
		UE_LOG(LogTemp, Log, TEXT("[Burn] body visuals %s anchors=%d"), *GetNameSafe(Owner), BurningParticles.Num());
	}
}

void UAZ_BurningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BurnTimer);
	bBurning = false;
	RefreshVisuals();
	Super::EndPlay(EndPlayReason);
}
