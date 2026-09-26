// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_ThrowableDefinition.generated.h"

class AAZ_ThrowableProjectile;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;
class UCameraShakeBase;
class UMaterialInterface;
class UAZ_ThrowPresentationProfile;
class USkeletalMesh;
class UStaticMesh;

/**
 * UAZ_ThrowableDefinition — everything about a throwable that is NOT the animation.
 *
 * Stone, grenade and knife are three of these, not three abilities. UAZ_GA_Throw reads this and never
 * branches on item identity; behaviour differences live in ImpactBehavior and the numbers below.
 */
UCLASS(BlueprintType)
class AZ_API UAZ_ThrowableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Item-specific visual grip adjustment; never changes ballistic release coordinates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Presentation")
	FTransform HeldPropOffset = FTransform::Identity;

	/** Reusable inventory tool needed to prepare this item. Empty means no tool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Preparation")
	FGameplayTag RequiredIgnitionTool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Shatter", meta=(ClampMin="0", ForceUnits="cm/s"))
	float ShatterMinImpactSpeed = 400.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Shatter", meta=(ClampMin="0"))
	float ShatterLoudness = 1.6f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Shatter")
	FName ShatterNoiseTag = TEXT("BottleBreak");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Shatter")
	TObjectPtr<UNiagaraSystem> ShatterEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Shatter")
	TObjectPtr<USoundBase> ShatterSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	bool bIgnitesOnImpact = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire", meta=(ClampMin="1", ForceUnits="cm"))
	float FireRadius = 250.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire", meta=(ClampMin="0.1", ForceUnits="s"))
	float FireDuration = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire", meta=(ClampMin="0"))
	float FireDamagePerSecond = 15.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire", meta=(ClampMin="0.1", ForceUnits="s"))
	float BurnDuration = 3.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UNiagaraSystem> GroundFireEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UNiagaraSystem> BurningTargetEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UNiagaraSystem> HeldIgnitionEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UParticleSystem> GroundFireParticles;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UParticleSystem> BurningTargetParticles;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<UParticleSystem> HeldIgnitionParticles;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	TObjectPtr<USoundBase> FireLoopSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire", meta=(ClampMin="0"))
	float FireLoudness = 2.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Fire")
	FName FireNoiseTag = TEXT("Fire");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Throwable|Detonation")
	FName DetonationNoiseTag = TEXT("Explosion");

	// ---- Presentation ----------------------------------------------------------------------------

	/** Used when no weapon-context override applies (unarmed, or a context with no dedicated family). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Presentation")
	TObjectPtr<UAZ_ThrowPresentationProfile> DefaultProfile;

	/**
	 * Weapon-context overrides, keyed by the equipped weapon's animation-family tag.
	 *
	 * This is how "throw a grenade left-handed while the rifle stays in the right hand" is expressed without
	 * the ability knowing anything about rifles. Empty is legitimate and simply means DefaultProfile always.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Presentation")
	TMap<FGameplayTag, TObjectPtr<UAZ_ThrowPresentationProfile>> ContextProfiles;

	/** Cosmetic mesh while held and in flight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Presentation")
	TObjectPtr<UStaticMesh> HeldMesh;

	/**
	 * Optional in-hand art, used for the HELD PROP ONLY.
	 *
	 * A grenade is carried with its pin and spoon intact and thrown armed, so the hand and the projectile
	 * legitimately show different meshes. When this is empty the held prop falls back to HeldMesh, which is
	 * what a stone or any single-state object wants.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Presentation")
	TObjectPtr<USkeletalMesh> HeldSkeletalMesh;

	/**
	 * Longest-axis size of the held/thrown mesh in real centimetres, converted from the mesh's own bounds.
	 * 0 leaves the mesh at its authored size.
	 *
	 * Exists because the visual and the collision must agree: CollisionRadius below is what the world
	 * actually hits, and art whose authored size happens to be a 3cm debris chunk would otherwise be a
	 * pebble bouncing off a 12cm invisible sphere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Presentation", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HeldMeshSize = 0.f;

	// ---- Flight ----------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight")
	TSubclassOf<AAZ_ThrowableProjectile> ProjectileClass;

	/** Launch speed for each authored arc, cm/s. Names do not determine speed; these do. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float CloseSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float FarSpeed = 1600.f;

	/**
	 * Vertical lift applied to the release origin for each arc, in centimetres.
	 *
	 * This is what lets ONE throw animation serve both arcs: a close toss launches from slightly higher so
	 * the shorter, slower arc still clears the thrower's own body, without changing a frame of the motion.
	 * Lives here beside the speeds because it is per ITEM ballistics tuning, not per animation family - a
	 * stone and a grenade thrown with the same clip can want different lifts.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ForceUnits = "cm"))
	float CloseOriginLift = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ForceUnits = "cm"))
	float FarOriginLift = 0.f;



	/**
	 * Aim distance at or beyond which the Far arc is chosen, with hysteresis so a target hovering at the
	 * boundary cannot make the choice flap. The arc is latched at commit regardless; this only decides which.
	 */
	/**
	 * Aim distance at or below which the throw is fully CLOSE, cm.
	 *
	 * ★ This and FarArcDistance are the two ends of a CONTINUOUS blend, not a switch. Two fixed speeds give
	 * range as v-squared, so 900 and 1600 cm/s land roughly 3x apart at the same aim angle: switching between
	 * them left a band of distances that simply could not be reached, and panning out to close the gap only
	 * flipped the throw back (reported 2026-09-16). Interpolating speed and lift across this range removes
	 * that dead zone, with no charge mechanic and no hidden power boost - the preview still shows exactly
	 * what the parameters do.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CloseArcDistance = 300.f;

	/** Aim distance at or beyond which the throw is fully FAR, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FarArcDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FarArcHysteresis = 150.f;

	/** Scales world gravity for this object. 1 = ordinary ballistic. Preview and runtime share the result. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0.01"))
	float GravityScale = 1.f;

	/** Collision sphere radius, world cm. Drives BOTH the predicted sweep and the runtime root, so the
	 *  preview cannot be quietly thinner than the object that flies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float CollisionRadius = 6.f;

	/** How much of the thrower's own motion is added at release, 0..1. Captured at release, not at aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0", ClampMax = "1"))
	float VelocityInheritance = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Flight", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxFlightTime = 12.f;

	// ---- Impact ----------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact")
	EAZ_ThrowImpactBehavior ImpactBehavior = EAZ_ThrowImpactBehavior::BounceAndSettle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float Bounciness = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float Friction = 0.4f;

	/** Below this speed the object stops and becomes recoverable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StopSpeed = 60.f;

	/**
	 * Loudness reported to AI hearing at a meaningful impact. 0 disables noise entirely.
	 *
	 * ★ The noise is reported at the IMPACT location but with the THROWER as instigator. AZ's hearing
	 * rejects non-hostile stimulus instigators, so reporting a neutral projectile as the source would make
	 * the stone silently inaudible — which is the entire point of a stone.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0"))
	float ImpactLoudness = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ImpactHearingRange = 2500.f;

	/** Minimum gap between reported bounce noises, so a rattling stone is not a machine gun of stimuli. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ForceUnits = "s"))
	float ImpactNoiseCooldown = 0.35f;

	/** Stimulus tag carried by the reported noise. AZ's controllers log it and use it to colour the
	 *  investigation; a neutral tag produces the calm wary-walk investigation a distraction wants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact")
	FName ImpactNoiseTag = FName("ThrowImpact");

	/** Impact speed below which a bounce is not worth reporting at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Impact", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinNoiseImpactSpeed = 250.f;

	// ---- Recovery --------------------------------------------------------------------------------

	/**
	 * Whether the settled object becomes a pickup again.
	 *
	 * There is exactly one owner at any instant: inventory, the flying object, or the recoverable pickup.
	 * A unique knife carries its own GUID and state out and back; a unit split from a stack gets a fresh
	 * world identity, matching the existing drop rule.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Recovery")
	bool bRecoverable = true;

	// ---- Detonation (ImpactBehavior == FuseAndDetonate) -------------------------------------------
	//
	// Everything below is inert for the other behaviours. A grenade is this data on the same asset class a
	// stone uses — the doctrine at the top of this file, kept.

	/**
	 * Seconds from RELEASE to the blast. From release, not from first contact: the grenade goes off wherever
	 * it happens to be when the fuse runs out, which is what makes cooking and bouncing it round a corner
	 * mean anything (user call 2026-09-19, four seconds).
	 *
	 * Because the clock starts as it leaves the hand it can never expire in the hand, so there is no
	 * in-hand detonation case to defend against.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation",
		meta = (ClampMin = "0", ForceUnits = "s"))
	float FuseSeconds = 4.f;

	/** Damage at the centre, before falloff. Fed to the same SetByCaller the firearm uses, so armour and
	 *  resistances in AZ_DamageExecCalc apply exactly as they do to a bullet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation", meta = (ClampMin = "0"))
	float DetonationDamage = 120.f;

	/** Inside this radius the full DetonationDamage lands — no falloff at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float DamageInnerRadius = 200.f;

	/** At and beyond this radius the blast does nothing. Also the radius the overlap query uses, so it
	 *  bounds the cost of the whole detonation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float DamageOuterRadius = 600.f;

	/** Shape of the drop between the two radii. 1 = linear; above 1 concentrates the lethality near the
	 *  centre, which is what makes cover and distance worth playing for. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation", meta = (ClampMin = "0.1"))
	float DamageFalloffExponent = 2.f;

	/**
	 * Impulse applied to a body the blast KILLS, scaled by the same falloff.
	 *
	 * Only to corpses. A living infected is driven by Mover, which owns its capsule; pushing it from outside
	 * fights the simulation instead of moving it. Survivors are thrown by their REACTION animation, not by
	 * physics — see the hit-react event the detonation sends.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation", meta = (ClampMin = "0"))
	float CorpseImpulse = 60000.f;

	/** Does the blast hurt whoever threw it. On by default: a grenade with no back-blast is a free button. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation")
	bool bDamagesThrower = true;

	/**
	 * Whether a wall between the blast and a target protects it.
	 *
	 * On by default and the reason the detonation traces at all. Off turns the blast into a pure radius,
	 * which is cheaper and sometimes what a designer wants for a small charge.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation")
	bool bRequireLineOfSight = true;

	/** How loud the blast is to AI hearing, and how far it carries — separate from the impact noise above,
	 *  because a detonation must pull in everything that did not see it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation", meta = (ClampMin = "0"))
	float DetonationLoudness = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float DetonationHearingRange = 9000.f;

	// ---- Detonation cosmetics ---------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX")
	TObjectPtr<UNiagaraSystem> DetonationEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX")
	TObjectPtr<USoundBase> DetonationSound;

	/** Played on every viewer inside DamageOuterRadius, falling off to nothing at that edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX")
	TSubclassOf<UCameraShakeBase> DetonationShake;

	/** Scorch left on the ground under the blast. Null simply leaves none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX")
	TObjectPtr<UMaterialInterface> DetonationDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float DetonationDecalSize = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throwable|Detonation|FX",
		meta = (ClampMin = "0", ForceUnits = "s"))
	float DetonationDecalLifetime = 60.f;

	/**
	 * Damage multiplier for a target at Distance from the blast: 1 inside the inner radius, 0 at the outer.
	 * One place, so the damage, the corpse impulse and the camera shake cannot disagree about how far the
	 * blast reaches.
	 */
	float DetonationFalloff(const float Distance) const
	{
		if (Distance <= DamageInnerRadius)
		{
			return 1.f;
		}
		if (Distance >= DamageOuterRadius || DamageOuterRadius <= DamageInnerRadius)
		{
			return 0.f;
		}
		const float T = (Distance - DamageInnerRadius) / (DamageOuterRadius - DamageInnerRadius);
		return FMath::Pow(1.f - T, FMath::Max(0.1f, DamageFalloffExponent));
	}

	/** Arc choice for an aim distance, with hysteresis against the arc already displayed. */
	EAZ_ThrowArc SelectArc(float AimDistance, EAZ_ThrowArc Current) const
	{
		const float Threshold = (Current == EAZ_ThrowArc::Far)
			? FarArcDistance - FarArcHysteresis
			: FarArcDistance + FarArcHysteresis;
		return AimDistance >= Threshold ? EAZ_ThrowArc::Far : EAZ_ThrowArc::Close;
	}

	/**
	 * 0 at CloseArcDistance and nearer, 1 at FarArcDistance and beyond, linear between.
	 *
	 * Everything ballistic is driven from this rather than from the arc enum, so what the player gets changes
	 * CONTINUOUSLY with where they point. The enum survives only to choose presentation.
	 */
	float GetAimBlend(float AimDistance) const
	{
		const float Near = FMath::Min(CloseArcDistance, FarArcDistance);
		const float Far = FMath::Max(CloseArcDistance, FarArcDistance);
		return (Far - Near) > KINDA_SMALL_NUMBER
			? FMath::Clamp((AimDistance - Near) / (Far - Near), 0.f, 1.f)
			: (AimDistance >= Far ? 1.f : 0.f);
	}

	float GetSpeedForAim(float AimDistance) const
	{
		return FMath::Lerp(CloseSpeed, FarSpeed, GetAimBlend(AimDistance));
	}

	float GetOriginLiftForAim(float AimDistance) const
	{
		return FMath::Lerp(CloseOriginLift, FarOriginLift, GetAimBlend(AimDistance));
	}

	/** Uniform scale that makes HeldMesh measure HeldMeshSize on its longest axis. Shared by the held prop
	 *  and the thrown projectile so the object does not change size the instant it leaves the hand. */
	FVector GetHeldMeshScale() const;

	/** The same conversion for HeldSkeletalMesh, whose bounds differ from the thrown art's. */
	FVector GetHeldSkeletalMeshScale() const;
};
