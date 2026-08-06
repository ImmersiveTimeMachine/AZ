// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AZ_CombatAvatar.generated.h"

class USkeletalMeshComponent;

UINTERFACE(MinimalAPI)
class UAZ_CombatAvatar : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAZ_CombatAvatar — the GA-facing seam every fighting body implements.
 *
 * Exists to kill the concrete-cast disease (CMC spike doctrine rule 3): three pawn generations each grew
 * their own class, and the abilities that drive them (grab, melee, hit-react, death) accumulated casts to
 * whichever generation was current — v1 references still litter 15 files. Abilities talk to THIS instead;
 * a pawn implements the subset it supports and inherits safe no-ops for the rest.
 *
 * Method semantics are the v2 pawns' (AZ_PawnMoverHeroCharacter / AZ_PawnMoverInfectedCharacter) — the
 * names and lifetimes match their concrete methods on purpose, so implementing the interface there is a
 * pure `override` stamp, not a behavior change. Defaults are no-ops: calling a victim-side method on a
 * grabber (or vice versa) does nothing rather than requiring every caller to know the pawn's role.
 */
class AZ_API IAZ_CombatAvatar
{
	GENERATED_BODY()

public:
	/** The mesh combat systems act on (IK targets, socket reads, montage mesh). */
	virtual USkeletalMeshComponent* GetCombatMesh() const { return nullptr; }

	// ---- Grab, VICTIM side (the hero while State.Grabbed) ----

	/** Body faces this actor while grabbed; also drives the cinematic grab camera. Null = clear on exit. */
	virtual void SetGrabFacingTarget(const AActor* Target) {}
	virtual const AActor* GetGrabFacingTarget() const { return nullptr; }

	/** Swap the hold framing for the pulled-back outcome framing (escape/bite payoff shot). */
	virtual void SetGrabOutcomeFraming(bool bInOutcomeFraming) {}

	/** Let go with the hands at the shove's CONTACT frame (IK would stretch after the partner flies off). */
	virtual void SetGrabIKReleased(bool bInReleased) {}
	virtual bool IsGrabIKReleased() const { return false; }

	// ---- Grab, GRABBER side (the Chalkie while State.Combat.Grabbing) ----

	/** The prey currently held (anim layer reads it for hand-IK). Null = clear on every exit. */
	virtual void SetGrabTarget(AActor* InTarget) {}
	virtual AActor* GetGrabTarget() const { return nullptr; }

	// ---- Reactions / death ----

	/** Non-ability stagger causes funnel through here — one shared deadline, last-writer-wins, only ever
	 *  extends (the counted-tag trap guard the v2 infected settled on). */
	virtual void SetStaggeredFor(float Seconds) {}

	/** True while a stagger-class reaction owns the body (BT refuses to swing on this). */
	virtual bool IsStaggerReactionPlaying() const { return false; }

	/** Corpse-ification after GA_Death starts the death montage: brain off, collision off, ragdoll at
	 *  RagdollDelay seconds (0 = instantly). Idempotent. */
	virtual void BeginCorpse(float RagdollDelay) {}
};
