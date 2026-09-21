// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AZ_GameplayTags.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AISense_Hearing.h"
#include "Throwables/AZ_ThrowableDefinition.h"

AAZ_ThrowableProjectile::AAZ_ThrowableProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	// The dedicated additive profile, NOT the pre-existing "Projectile" profile: that one overlaps Pawn, and
	// a thrown object must be BLOCKED by a body so a friendly obstructs the throw even where damage policy
	// later excludes them. Nothing existing was renamed or repointed — see throwable-phase0-status.md §3.
	Collision->SetCollisionProfileName(TEXT("ThrowableProjectile"));
	Collision->SetGenerateOverlapEvents(false);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	// Cosmetic only. Two colliding shapes on one projectile is how a preview and a flight quietly disagree.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bAutoActivate = false;
	Movement->bSweepCollision = true;
	Movement->bShouldBounce = true;
	// InitialSpeed 0 so the component cannot reinterpret the velocity we hand it, and no MaxSpeed clamp:
	// the predictor models neither, so either would make the preview wrong in exactly the cases that matter.
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bRotationFollowsVelocity = true;
	Movement->SetAutoActivate(false);

	// Inert until Activate(). Spawning is not the point of no return; the inventory commit is.
	// Visibility is set on the components rather than through AActor's virtual helpers, which resolve at
	// compile time inside a constructor.
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetVisibility(false);
	Mesh->SetVisibility(false);
}

void AAZ_ThrowableProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAZ_ThrowableProjectile, Thrower);
	DOREPLIFETIME(AAZ_ThrowableProjectile, bActivated);
	DOREPLIFETIME(AAZ_ThrowableProjectile, Definition);
}

void AAZ_ThrowableProjectile::Activate(const FAZ_ThrowLaunchSolution& Solution,
	const UAZ_ThrowableDefinition* InDefinition, APawn* InThrower)
{
	if (bActivated || !HasAuthority() || !InDefinition || !Solution.IsValid())
	{
		return;
	}
	Definition = InDefinition;
	Thrower = InThrower;

	Collision->SetSphereRadius(FMath::Max(1.f, Solution.Radius));
	if (Definition->HeldMesh)
	{
		Mesh->SetStaticMesh(Definition->HeldMesh);
		// The SAME scale the held prop used, so the stone does not change size as it leaves the hand.
		Mesh->SetWorldScale3D(Definition->GetHeldMeshScale());
	}
	// The thrower and whatever it carries must not block the object leaving the hand. The clearance sweep in
	// the solver has already proven the corridor, so this only prevents self-collision on frame one.
	if (Thrower)
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}

	Movement->Bounciness = Definition->Bounciness;
	Movement->Friction = Definition->Friction;
	Movement->MinFrictionFraction = 0.f;
	Movement->BounceVelocityStopSimulatingThreshold = Definition->StopSpeed;
	// Effective gravity must equal what the preview integrated, or the arc the player aimed with is not the
	// arc that flies. The solver resolved world gravity * scale; convert back to a scale for the component.
	const float WorldGravity = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
	Movement->ProjectileGravityScale = FMath::IsNearlyZero(WorldGravity)
		? 0.f : Solution.GravityZ / WorldGravity;
	Movement->bShouldBounce = Definition->ImpactBehavior != EAZ_ThrowImpactBehavior::ImpactAndEmbed;

	Movement->OnProjectileBounce.AddDynamic(this, &AAZ_ThrowableProjectile::HandleBounce);
	Movement->OnProjectileStop.AddDynamic(this, &AAZ_ThrowableProjectile::HandleStop);

	SetActorLocation(Solution.Origin, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetVisibility(true);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetLifeSpan(FMath::Max(1.f, Definition->MaxFlightTime));

	Movement->SetUpdatedComponent(Collision);
	Movement->Velocity = Solution.Velocity;
	Movement->Activate(true);
	bActivated = true;

	// ★ THE FUSE STARTS HERE, AT RELEASE — not on first contact. The grenade goes off wherever it happens
	// to be when the clock runs out: still in the air, mid-bounce, or long since settled (user call
	// 2026-09-19). Because it starts as the grenade leaves the hand it can never expire in the hand, so
	// there is no in-hand detonation case.
	//
	// The timer lives on the PROJECTILE. UAZ_GA_Throw has already ended by now — it ends as the grenade
	// leaves the hand, seconds before this fires — so nothing on the ability side is still around to own it.
	if (Definition->ImpactBehavior == EAZ_ThrowImpactBehavior::FuseAndDetonate && GetWorld())
	{
		// The lifespan set above would otherwise delete a grenade that never settles BEFORE it detonates.
		// A fuse outranks it: the blast is the point of the object.
		SetLifeSpan(0.f);
		GetWorld()->GetTimerManager().SetTimer(FuseTimer, this, &AAZ_ThrowableProjectile::Detonate,
			FMath::Max(0.05f, Definition->FuseSeconds), false);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Throw] launched %s arc=%d origin=(%.0f,%.0f,%.0f) vel=(%.0f,%.0f,%.0f) |v|=%.0f gz=%.0f r=%.1f"),
		*GetNameSafe(Definition), static_cast<int32>(Solution.Arc),
		Solution.Origin.X, Solution.Origin.Y, Solution.Origin.Z,
		Solution.Velocity.X, Solution.Velocity.Y, Solution.Velocity.Z,
		Solution.Velocity.Size(), Solution.GravityZ, Solution.Radius);
}

void AAZ_ThrowableProjectile::SetRecoveryPayload(const FAZ_InventoryPickupRecord& InPayload)
{
	if (!HasAuthority())
	{
		return;
	}
	RecoveryPayload = InPayload;
	bHasRecoveryPayload = true;
}

void AAZ_ThrowableProjectile::HandleBounce(const FHitResult& Hit, const FVector& Velocity)
{
	if (!HasAuthority())
	{
		return;
	}
	++BounceCount;
	ReportImpactNoise(Hit.ImpactPoint, Velocity.Size());
}

void AAZ_ThrowableProjectile::HandleStop(const FHitResult& Hit)
{
	if (!HasAuthority() || bSettled)
	{
		return;
	}
	bSettled = true;
	ReportImpactNoise(Hit.ImpactPoint, 0.f);

	// The object stops being a projectile here. It goes back to being an item through the EXISTING pickup
	// flow — the same manifest spawn and payload handover a dropped item uses — rather than inventing a
	// second item representation, because there must be exactly one owner and a duplicate pickup would be it.
	SetLifeSpan(0.f);
	const FVector RestLocation = GetActorLocation();
	OnSettled.Broadcast(this, RestLocation);

	UE_LOG(LogTemp, Warning, TEXT("[Throw] settled after %d bounce(s) at (%.0f,%.0f,%.0f) surface=(%.0f,%.0f,%.0f) recoverable=%d"),
		BounceCount, RestLocation.X, RestLocation.Y, RestLocation.Z,
		Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z,
		Definition && Definition->bRecoverable ? 1 : 0);

	// A fused throwable is NOT recoverable, whatever the recovery flag says: it is going to destroy itself
	// in a moment and a pickup minted here would outlive the grenade that owned it. It simply lies where it
	// landed until the fuse runs out (user call 2026-09-19: "подобрать её невозможно").
	const bool bFused = Definition && Definition->ImpactBehavior == EAZ_ThrowImpactBehavior::FuseAndDetonate;
	if (!bFused && Definition && Definition->bRecoverable && ConvertToPickup(Hit.ImpactPoint))
	{
		Destroy();
		return;
	}
	// Not recoverable, or the pickup could not be created. Either way the object stays where it landed and
	// still holds the record: silently dropping the item would be worse than leaving it on the floor.
	if (bHasRecoveryPayload)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] settled item not recovered at (%.0f,%.0f,%.0f) def=%s"),
			RestLocation.X, RestLocation.Y, RestLocation.Z, *GetNameSafe(Definition));
	}
}

bool AAZ_ThrowableProjectile::ConvertToPickup(const FVector& RestSurfacePoint)
{
	if (!HasAuthority() || !bHasRecoveryPayload)
	{
		return false;
	}
	// ★ CLEAR of the surface, not on it. SpawnPickupActor uses AdjustIfPossibleButDontSpawnIfColliding, so a
	// pickup placed exactly at the contact point intersects the floor and is silently REFUSED — the item then
	// vanishes with the projectile. The inventory's own drop path spawns at ImpactPoint + 12cm for precisely
	// this reason (AZ_Inv_CommonUI_InventoryComponent, the drop placement search); match it.
	const FVector Base = RestSurfacePoint.IsNearlyZero() ? GetActorLocation() : RestSurfacePoint;
	const FVector SpawnLocation = Base + FVector(0.f, 0.f, PickupSurfaceClearance);
	AActor* Pickup = RecoveryPayload.Manifest.SpawnPickupActor(this, SpawnLocation, FRotator::ZeroRotator);
	if (!IsValid(Pickup))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] pickup spawn REFUSED at (%.0f,%.0f,%.0f) - blocked or no class"),
			SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
		return false;
	}
	auto* Component = Pickup->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
	if (!Component)
	{
		Pickup->Destroy();
		return false;
	}
	// ★ Clear BEFORE the handover completes. A settle callback that re-entered here would otherwise find the
	// record still present and spawn the same stone twice.
	bHasRecoveryPayload = false;
	const FAZ_InventoryPickupRecord Payload = MoveTemp(RecoveryPayload);
	RecoveryPayload = FAZ_InventoryPickupRecord();
	// A thrown unit is always a single item, never a nested container: nothing rides inside a stone.
	Component->SetPickupPayload(Payload, TArray<FAZ_InventoryPickupRecord>());
	UE_LOG(LogTemp, Warning, TEXT("[Throw] recovered as %s at (%.0f,%.0f,%.0f) item=%s"),
		*GetNameSafe(Pickup), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
		*Payload.State.InstanceId.ToString());
	return true;
}

void AAZ_ThrowableProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A blast must never come from an actor that is already gone — level teardown, a destroyed thrower, or
	// the projectile being removed for any other reason all reach here first.
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FuseTimer);
	}

	// A projectile that dies still holding the record never became a pickup, and the item is simply GONE.
	// The usual cause is the lifespan expiring because it never came to rest, which is invisible otherwise.
	if (bHasRecoveryPayload && HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Throw] projectile destroyed STILL HOLDING the item (settled=%d bounces=%d reason=%d) - item lost"),
			bSettled ? 1 : 0, BounceCount, static_cast<int32>(EndPlayReason));
	}
	Super::EndPlay(EndPlayReason);
}

void AAZ_ThrowableProjectile::Detonate()
{
	if (!HasAuthority() || bDetonated || !Definition)
	{
		return;
	}
	bDetonated = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FuseTimer);
	}

	const FVector Origin = GetActorLocation();

	// Ground normal for the scorch decal, and nothing else. A miss is fine — an airburst leaves no mark.
	FVector SurfaceNormal = FVector::UpVector;
	if (UWorld* World = GetWorld())
	{
		FHitResult Ground;
		FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(ThrowDetonationGround), false, this);
		if (World->LineTraceSingleByChannel(Ground, Origin, Origin - FVector(0.f, 0.f, 400.f),
				ECC_Visibility, GroundParams))
		{
			SurfaceNormal = Ground.ImpactNormal;
		}
	}

	ApplyBlast(Origin);

	// Everything that did not see it still has to come. Same instigator trick the impact noise uses: AZ's
	// perception drops a stimulus whose actor is not hostile to the listener, so the THROWER is named here
	// and never the projectile — otherwise the loudest event in the game would be silently unhearable.
	if (Definition->DetonationLoudness > 0.f && Thrower && GetWorld())
	{
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), Origin, Definition->DetonationLoudness,
			Thrower, Definition->DetonationHearingRange, Definition->ImpactNoiseTag);
	}

	MulticastDetonationFX(Origin, SurfaceNormal);

	// The grenade is spent: no pickup, no remains, it simply stops existing. Deferred by a moment so the
	// multicast and the queued impulses are not cut off by the actor going away.
	SetLifeSpan(0.3f);
}

void AAZ_ThrowableProjectile::ApplyBlast(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World || !Definition)
	{
		return;
	}

	// ONE pass. Damage, the reaction and the corpse impulse all need the same overlap, the same
	// line-of-sight answer and the same falloff; computing them apart would triple the cost and let the
	// three disagree about who was in the blast.
	TArray<AActor*> Overlapped;
	const TArray<TEnumAsByte<EObjectTypeQuery>> PawnOnly = { UEngineTypes::ConvertToObjectType(ECC_Pawn) };
	const TArray<AActor*> IgnoreSelf = { this };
	UKismetSystemLibrary::SphereOverlapActors(World, Origin, Definition->DamageOuterRadius,
		PawnOnly, APawn::StaticClass(), IgnoreSelf, Overlapped);

	UAbilitySystemComponent* SourceASC = Thrower
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Thrower) : nullptr;

	int32 Hurt = 0, Blocked = 0, NoAsc = 0;
	PendingImpulseTargets.Reset();

	for (AActor* Target : Overlapped)
	{
		if (!Target)
		{
			continue;
		}
		if (Target == Thrower && !Definition->bDamagesThrower)
		{
			continue;
		}

		const FVector TargetPoint = Target->GetActorLocation();
		const float Distance = FVector::Dist(Origin, TargetPoint);
		const float Falloff = Definition->DetonationFalloff(Distance);
		if (Falloff <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// A wall between the two protects. Without this a grenade kills through geometry, which reads as a
		// bug even to a player who cannot see the radius.
		if (Definition->bRequireLineOfSight)
		{
			FHitResult Obstruction;
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(ThrowDetonationLOS), false, this);
			LosParams.AddIgnoredActor(Target);
			if (World->LineTraceSingleByChannel(Obstruction, Origin, TargetPoint, ECC_Visibility, LosParams))
			{
				++Blocked;
				continue;
			}
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC || !SourceASC)
		{
			++NoAsc;
			continue;
		}

		// ★ DAMAGE IS ALL THAT IS SENT. UAZ_VitalsAttributeSet::PostGameplayEffectExecute already fans out
		// the rest: a survivable hit reaches the infected's own HandleDamaged (scream, damage lock, melee
		// cancel, Event.Combat.HitReact) and the hero's hit-react directly, and a lethal one raises
		// Event.Death with the killer named. Sending a reaction from here as well would give every victim
		// two of them from two senders — the exact thing that code warns against.
		//
		// Which WAY the victim is thrown comes from the hit result carried in the effect context, because
		// that is what GA_HitReact reads to pick a side. So the blast has to put a believable one in: a
		// trace from the centre of the explosion into the body, surface normal pointing back at the blast.
		FHitResult BlastHit;
		BlastHit.TraceStart = Origin;
		BlastHit.TraceEnd = TargetPoint;
		BlastHit.Location = TargetPoint;
		BlastHit.ImpactPoint = TargetPoint;
		BlastHit.Normal = (Origin - TargetPoint).GetSafeNormal();
		BlastHit.ImpactNormal = BlastHit.Normal;
		BlastHit.Distance = Distance;
		BlastHit.HitObjectHandle = FActorInstanceHandle(Target);
		BlastHit.Component = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		BlastHit.bBlockingHit = true;

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		// Causer is the grenade, instigator is the thrower. The vitals set falls back to the instigating
		// ASC's avatar whenever the causer is not a pawn, so the kill is credited to the player and the
		// victim reacts to a body rather than to a projectile.
		Context.AddInstigator(Thrower, this);
		Context.AddSourceObject(this);
		Context.AddHitResult(BlastHit);

		const FGameplayEffectSpecHandle Spec =
			SourceASC->MakeOutgoingSpec(UAZ_GE_Damage::StaticClass(), 1.f, Context);
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage,
			Definition->DetonationDamage * Falloff);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		++Hurt;

		// Whoever this killed is going to ragdoll, and a ragdoll is the only thing here worth pushing. The
		// push cannot happen yet: the death ability owns that hand-over and one of the two infected classes
		// delays it on a timer, so the body is still animated at this instant. Remember it and come back.
		PendingImpulseTargets.Add(Target);
	}

	if (PendingImpulseTargets.Num() > 0 && Definition->CorpseImpulse > 0.f)
	{
		PendingImpulseOrigin = Origin;
		World->GetTimerManager().SetTimer(ImpulseTimer, this,
			&AAZ_ThrowableProjectile::ApplyCorpseImpulses, 0.15f, false);
	}

	// ★ THE NEAREST PAWN, even when it was nowhere near. found=0 on its own cannot tell "the overlap query
	// is broken" apart from "you threw it nowhere near anybody", and those need opposite fixes — measured
	// 2026-09-19, three detonations reported found=0 and the log could not say which it was. Debug-only: the
	// scan is unbounded and exists purely to answer that question.
	FString NearestInfo;
#if !UE_BUILD_SHIPPING
	if (Overlapped.Num() == 0)
	{
		const APawn* Nearest = nullptr;
		float NearestDist = TNumericLimits<float>::Max();
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (*It == Thrower && !Definition->bDamagesThrower)
			{
				continue;
			}
			const float D = FVector::Dist(Origin, It->GetActorLocation());
			if (D < NearestDist)
			{
				NearestDist = D;
				Nearest = *It;
			}
		}
		NearestInfo = Nearest
			? FString::Printf(TEXT(" | nearest pawn %s at %.0fcm (radius %.0f)"),
				*Nearest->GetName(), NearestDist, Definition->DamageOuterRadius)
			: TEXT(" | NO PAWNS IN THE WORLD AT ALL");
	}
#endif

	UE_LOG(LogTemp, Warning,
		TEXT("[Throw] DETONATE %s at (%.0f,%.0f,%.0f) found=%d hurt=%d blockedByCover=%d noAsc=%d dmg=%.0f r=%.0f/%.0f%s"),
		*GetNameSafe(Definition), Origin.X, Origin.Y, Origin.Z,
		Overlapped.Num(), Hurt, Blocked, NoAsc,
		Definition->DetonationDamage, Definition->DamageInnerRadius, Definition->DamageOuterRadius,
		*NearestInfo);
}

void AAZ_ThrowableProjectile::ApplyCorpseImpulses()
{
	if (!Definition)
	{
		return;
	}
	// Only bodies that have actually gone to ragdoll by now. A survivor is still driven by Mover, which owns
	// its capsule — pushing it from outside fights the simulation instead of moving it, and its knockback is
	// the reaction animation the damage already triggered.
	int32 Thrown = 0;
	for (const TWeakObjectPtr<AActor>& Weak : PendingImpulseTargets)
	{
		AActor* Target = Weak.Get();
		if (!Target)
		{
			continue;
		}
		USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
		if (!TargetMesh || !TargetMesh->IsSimulatingPhysics())
		{
			continue;
		}
		const float Distance = FVector::Dist(PendingImpulseOrigin, Target->GetActorLocation());
		const float Falloff = Definition->DetonationFalloff(Distance);
		if (Falloff <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		// Velocity change, not force: corpse mass varies per rig and the throw should read the same for all
		// of them. Radial from the blast, so a body at the edge tips instead of launching.
		TargetMesh->AddRadialImpulse(PendingImpulseOrigin, Definition->DamageOuterRadius,
			Definition->CorpseImpulse * Falloff, ERadialImpulseFalloff::RIF_Linear, true);
		++Thrown;
	}
	PendingImpulseTargets.Reset();
	if (Thrown > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] blast threw %d corpse(s)"), Thrown);
	}
}

void AAZ_ThrowableProjectile::MulticastDetonationFX_Implementation(const FVector& Origin, const FVector& SurfaceNormal)
{
	if (!Definition)
	{
		return;
	}
	if (Definition->DetonationEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Definition->DetonationEffect, Origin);
	}
	if (Definition->DetonationSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Definition->DetonationSound, Origin);
	}
	if (Definition->DetonationDecal)
	{
		// Oriented to the ground it marks, with a random spin so repeated blasts do not stamp the same
		// picture twice.
		FRotator DecalRotation = SurfaceNormal.Rotation();
		DecalRotation.Roll += FMath::FRandRange(0.f, 360.f);
		UGameplayStatics::SpawnDecalAtLocation(this, Definition->DetonationDecal,
			FVector(Definition->DetonationDecalSize), Origin, DecalRotation,
			Definition->DetonationDecalLifetime);
	}
	// Shake everyone inside the blast, fading to nothing at its edge — the same radius the damage used, so
	// the felt size of the explosion matches the dangerous one.
	if (Definition->DetonationShake && Definition->DamageOuterRadius > 0.f)
	{
		UGameplayStatics::PlayWorldCameraShake(this, Definition->DetonationShake, Origin,
			0.f, Definition->DamageOuterRadius);
	}
}

void AAZ_ThrowableProjectile::ReportImpactNoise(const FVector& Location, const float ImpactSpeed)
{
	if (!Definition || Definition->ImpactLoudness <= 0.f || !Thrower || !GetWorld())
	{
		return;
	}
	// A rattling stone must not be a machine gun of stimuli: gate on impact energy, then on a cooldown.
	// A settle (speed 0) always reports, because that is the landing the player actually aimed at.
	if (ImpactSpeed > 0.f && ImpactSpeed < Definition->MinNoiseImpactSpeed)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoiseTime < Definition->ImpactNoiseCooldown)
	{
		return;
	}
	LastNoiseTime = Now;

	// ★ Impact LOCATION, thrower INSTIGATOR. AZ's perception drops any stimulus whose actor is not hostile
	// to the listener (AZ_InfectedAIController::OnPerception), so naming the neutral projectile here would
	// make the noise silently unhearable — which would defeat the entire purpose of throwing a stone.
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), Location, Definition->ImpactLoudness,
		Thrower, Definition->ImpactHearingRange, Definition->ImpactNoiseTag);
}
