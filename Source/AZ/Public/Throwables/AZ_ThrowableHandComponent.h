// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_ThrowableHandComponent.generated.h"

class AAZ_PawnMoverHeroCharacter;
class APawn;
class UAnimMontage;
class UAZ_ThrowableDefinition;
class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_ThrowPresentationProfile;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UParticleSystemComponent;
class UParticleSystem;

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
	/** Reconcile a pending selection after the previous ability has fully released its input and tags. */
	void FinishThrowAction();

	/**
	 * Forget the weapon this component holstered, without drawing it back.
	 *
	 * For when the PLAYER picks a weapon while a throwable is in hand: the equipment component is already
	 * going to draw something, deferred behind the throwable's put-away, and the automatic restore would
	 * race it — issuing its own draw in the same millisecond and starting it underneath the put-away
	 * animation instead of after it (measured 2026-09-19: two "switch begin" lines 2 ms apart).
	 *
	 * The player's own choice supersedes the restore. This drops the claim so only one request is made.
	 */
	void AbandonStowedWeapon();

	/**
	 * The throwable is being PUT AWAY, and this clip is how long that takes.
	 *
	 * ★ Called by the throw ability at the moment it starts the cancel clip, because that clip is the only
	 * thing that knows a put-away is happening: it is deliberately cosmetic and outlives the ability, so by
	 * the time readiness clears there is no action left to ask (user call 2026-09-18 made it that way).
	 *
	 * It raises State.Throwable.Stowing for the clip's length, which equipment reads as a committed action.
	 * That is what makes a weapon switch wait: the grenade goes away, THEN the weapon is drawn — instead of
	 * the draw running its full length underneath the grenade's mask and the weapon simply appearing in the
	 * hand (measured 2026-09-20). The prop stays in the hand for the same window, so the clip is not played
	 * on an empty one.
	 *
	 * Timed rather than montage-driven, to match the equipment switch phases beside it, and measured to the
	 * start of the clip's blend-out so the draw begins as the hand drops instead of after a beat of nothing.
	 */
	void BeginPutAway(const UAnimMontage* PutAwayClip);

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
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> HeldFlame;
	UPROPERTY(Transient) TObjectPtr<UParticleSystemComponent> HeldFlameParticles;
	UPROPERTY(Transient) TObjectPtr<UParticleSystem> HeldFlameSource;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> SkeletalProp;

	/** The mesh the props are currently parented to, so a pawn change can be detected and re-attached. */
	TWeakObjectPtr<USkeletalMeshComponent> AttachedTo;

	bool bSuppressed = false;
	bool bActionOwnsBody = false;

	/**
	 * A GRENADE IS A WEAPON SWITCH. Readying one puts whatever is in the hands away first, through the
	 * ordinary holster, and the throw only begins once the hands are empty — otherwise the character throws
	 * a grenade while still holding a rifle, which is what it did (user call 2026-09-19).
	 *
	 * This is the item that was put away, kept so it can be drawn again when the throw is over. Weak on
	 * purpose: the weapon can be dropped, consumed or destroyed while the grenade is in the air, and a
	 * stale strong pointer would re-equip a thing that no longer exists.
	 */
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> StowedForThrow;

	/**
	 * Polls for the holster finishing.
	 *
	 * The equipment component announces changes, but not reliably at the END of the holster phase, and a
	 * missed announcement here would leave the player holding nothing and unable to throw — a silent dead
	 * end. A slow poll cannot deadlock and costs nothing while idle, so the wait is driven from here and
	 * the announcement is only an accelerator.
	 */
	FTimerHandle StowWaitTimer;

	/** Put the equipped weapon away, or report that the hands are already free. True = clear to throw. */
	bool EnsureHandsFreeForThrow();

	/** Draw back whatever the throw put away. No-op when nothing was stowed. */
	void RestoreStowedWeapon();

	/** Start (or keep) the slow poll that re-runs Refresh until the hands are actually free. */
	void ArmStowWait();

	/** Lowers State.Throwable.Stowing when the put-away clip has had its time. */
	FTimerHandle PutAwayTimer;

	/** Drop State.Throwable.Stowing and the timer behind it. Safe to call when neither is set. */
	void EndPutAway();

	/** True while the put-away clip still owns the upper body. */
	bool IsPuttingAway() const;
};
