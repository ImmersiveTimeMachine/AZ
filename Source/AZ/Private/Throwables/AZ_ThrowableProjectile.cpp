// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Net/UnrealNetwork.h"
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

	if (Definition && Definition->bRecoverable && ConvertToPickup(Hit.ImpactPoint))
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
