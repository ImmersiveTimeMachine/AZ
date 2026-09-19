// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_ThrowableHandComponent.generated.h"

class AAZ_PawnMoverHeroCharacter;
class APawn;
class UAZ_ThrowableDefinition;
class UAZ_ThrowPresentationProfile;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * UAZ_ThrowableHandComponent — the readied throwable, in the character's hand.
 *
 * ★ Owned by the CONTROLLER, not by the throw ability, because the object is in the hand from the moment the
 * item is readied (user call 2026-09-16) and the ability only exists while the player is actually aiming.
 * Putting the prop on the ability meant the grenade popped into existence on RMB-down and vanished on
 * release, which is not how carrying something works.
 *
 * It lives on the controller because that is where the quick bar and the inventory already are, and it
 * re-attaches itself on possession so a pawn swap does not strand the mesh on a dead body.
 *
 * Purely cosmetic: it never collides, it is not the item, and the inventory neither knows nor cares that it
 * exists. The authoritative object is the projectile, and only after the release cue commits.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_ThrowableHandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_ThrowableHandComponent();

	/**
	 * Hide the prop while the real thrown object exists.
	 *
	 * Between the release cue and the end of the action the projectile IS the item; showing a second copy in
	 * the hand would put one object in two places. The throw ability raises this at the cue and drops it when
	 * the action ends, at which point Refresh decides whether another unit is readied.
	 */
	void SetSuppressed(bool bInSuppressed);

	/**
	 * The throw action owns the upper body: stop driving the carry idle.
	 *
	 * ★ Separate from prop suppression. The grenade STAYS VISIBLE in the hand while aiming, but the carry
	 * montage must not be re-played — it shares the upper-body slot with the wind-up, so any inventory event
	 * mid-aim would otherwise restart the idle straight over the preparation.
	 */
	void SetActionOwnsBody(bool bInOwned);

	/** Re-resolve the readied item and show, swap or hide the prop accordingly. Safe to call often. */
	void Refresh();

	// Public to match UActorComponent, which declares both public.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleReadyItemChanged();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** The readied item's throwable definition, or null when nothing throwable is readied. */
	const UAZ_ThrowableDefinition* ResolveReadyThrowable(const UAZ_ThrowPresentationProfile*& OutProfile) const;

	AAZ_PawnMoverHeroCharacter* GetHero() const;

	/**
	 * Start or stop the held-item idle on the upper-body slot.
	 *
	 * ★ Played, not assigned. It must actually advance and loop, and it must stop when the item is no longer
	 * readied — a montage nobody stops outlives the item it was showing.
	 */
	void UpdateCarryMontage(USkeletalMeshComponent* Mesh, const UAZ_ThrowPresentationProfile* Profile);

	/**
	 * Mirror "a throwable is readied" onto the ASC as State.Throwable.Ready.
	 *
	 * The movement-intent layer reads it for combat-ready facing and the walk clamp, and the anim chooser
	 * sees it in its owned-tag snapshot — neither has to know the quick bar exists.
	 */
	void PublishReadyTag(bool bReadied) const;

	/**
	 * Enter the throw action because a throwable is readied.
	 *
	 * ★ Readying IS the entry: the player goes Start -> Loop and stays there until they throw or cancel.
	 * Safe to call on every refresh — the ability blocks itself on its own state tag, so an already-running
	 * one is never restarted.
	 */
	void EnterThrowAction() const;

	/**
	 * Release the input edge the entry latched.
	 *
	 * ★ Mandatory, not tidiness. A spec already marked InputPressed silently swallows the next press, so
	 * without this the grenade can be armed exactly once per session.
	 */
	void LeaveThrowAction() const;

	/** The carry montage currently playing, so it can be stopped exactly once and never left orphaned. */
	UPROPERTY() TObjectPtr<UAnimMontage> ActiveCarryMontage;

	void HideProps();

	UPROPERTY() TObjectPtr<UStaticMeshComponent> StaticProp;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> SkeletalProp;

	/** The mesh the props are currently parented to, so a pawn change can be detected and re-attached. */
	TWeakObjectPtr<USkeletalMeshComponent> AttachedTo;

	bool bSuppressed = false;
	bool bActionOwnsBody = false;
};
