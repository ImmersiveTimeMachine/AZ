// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "Throwables/AZ_ThrowPreviewComponent.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_GA_Throw.generated.h"

class AAZ_ThrowableProjectile;
class UAZ_ThrowableDefinition;
class UAZ_ThrowPresentationProfile;
class UAZ_AT_PlayMontageAndWaitForEvent;

/**
 * UAZ_GA_Throw — the ONE throw action. Stone, grenade and knife are data, not subclasses.
 *
 * Controls (user, 2026-09-18): **readying the grenade enters the action** — Start into Loop, and the player
 * stays there — **press RMB to throw, press LMB to cancel.** Cancelling un-readies the item; while it stays
 * readied the hand component would re-enter this action immediately and the cancel would be invisible.
 *
 * ★ There is NO hold and NO release trigger. An earlier revision aimed while RMB was held and threw on the
 * release, which cannot survive entry-by-readying: WaitInputRelease with bTestAlreadyReleased fires at once
 * for an ability nobody pressed, and the grenade threw itself about a second later. The commit is an
 * explicit press, routed from the controller into RequestThrow().
 *
 * ★ Aiming does NOT plant the body. Start/Loop/Cancel play on the masked upper-body slot and the player
 * walks in the combat stance throughout; only the committed release raises Ability.State.Throwing, and that
 * is what stops the legs under the full-body release clip.
 *
 * Phase flow:
 *
 *   Preparing  Start montage. Unit reserved, nothing spent, preview already live.
 *              An RMB release here latches ONE pending commit and branches at the measured ready seam.
 *   Aiming     Loop repeats indefinitely. Preview updates; arc candidate re-selected with hysteresis.
 *   Windup     Release montage playing, aim intent and arc FROZEN. Nothing spent yet.
 *   Released   The validated cue fired: unit spent, projectile activated. No refund past here.
 *   Recovering Authored tail. Interruption cleans up presentation only.
 *   Cancelling Nothing spent; reservation released.
 */
UCLASS()
class AZ_API UAZ_GA_Throw : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_Throw();

	/**
	 * LMB cancel, routed from the player controller before ordinary ASC dispatch.
	 *
	 * ★ Also DISARMS the pending release. The player is still holding RMB at this moment, and that button
	 * will come up eventually: without disarming, the resulting release would throw the item the player just
	 * cancelled. After a cancel, a fresh RMB press is required.
	 */
	void RequestCancel();

	/**
	 * Commit the throw. Routed in from the controller on an explicit press.
	 *
	 * ★ A PRESS, not the release of a held button. Readying a grenade enters this action and the player
	 * stays in it, so there is no hold to release. A press during Start latches one intent and is consumed
	 * at the ready seam, so an early click still throws instead of being swallowed.
	 */
	void RequestThrow();

	/** True while this action owns throw input, so the controller knows to consume LMB/RMB. */
	bool IsThrowContextActive() const { return Phase != EAZ_ThrowPhase::None; }

	EAZ_ThrowPhase GetPhase() const { return Phase; }
	FGuid GetSourceItemId() const { return SourceItemId; }
	/** Includes the synchronous inventory notification inside the release transaction. */
	bool HasCommittedRelease() const;

	virtual void DeclareAbilityTags() override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Tag carried by the release notify on the project-owned montages. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw")
	FGameplayTag ReleaseEventTag;

	/** Preview horizon and solve rate. Bounded so an throw into open sky costs a fixed amount. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float PreviewHorizon = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw", meta = (ClampMin = "1"))
	float PreviewFrequency = 20.f;

	/**
	 * Aim-ray length, as a multiple of the definition's FarArcDistance.
	 *
	 * Must stay above 1 + hysteresis/FarArcDistance, because "the ray hit nothing" returns this range and an
	 * unobstructed aim into open space has to read as FAR. Returning exactly FarArcDistance there sits below
	 * the Close->Far threshold, so a Close selection could never be promoted again.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw", meta = (ClampMin = "1.5"))
	float AimRangeFactor = 3.f;

	/**
	 * How long a different arc has to stay selected before it is accepted.
	 *
	 * Distance hysteresis alone cannot settle this: the aim distance is BIMODAL, not noisy. Tilting slightly
	 * moves the aim point between nearby ground and open sky, so the raw signal jumps 650 -> 2700 -> 660 and
	 * a distance band either side of the threshold is never crossed slowly (measured 2026-09-16: eight arc
	 * flips in a single aim). A dwell is what turns that into one decision.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw", meta = (ClampMin = "0", ForceUnits = "s"))
	float ArcDwellTime = 0.25f;

	/**
	 * Quiet Sage / mockup03. Carried here rather than on the pawn so the hero Blueprint needs no change and
	 * no new reflected pawn member: the ability creates ONE preview component on first use and keeps it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throw|Preview")
	FAZ_ThrowPreviewStyle PreviewStyle;

private:
	UFUNCTION()
	void OnReleaseCue(FGameplayTag EventTag, FGameplayEventData EventData);

	UFUNCTION()
	void OnPresentationEnded(FGameplayTag EventTag, FGameplayEventData EventData);

	UFUNCTION()
	void OnPresentationInterrupted(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Start -> ready seam reached. Branches to the release if a commit is pending, else into the Loop. */
	void OnReadySeam();

	/**
	 * Detach and end the montage task currently held.
	 *
	 * ★ NOT just EndTask(). The task's OnDestroy only unbinds its montage callbacks when it stops the
	 * montage itself, which it never does here (bStopWhenAbilityEnds is false), and the broadcast gate
	 * (UAbilityTask::ShouldBroadcastAbilityTaskDelegates) asks only whether the ABILITY is still active —
	 * not whether the task has finished. Starting the next montage interrupts the previous one, so an
	 * ended-but-still-bound task would fire OnInterrupted and cancel the throw on the Start -> Loop seam.
	 */
	void ReleasePresentation();
	/** Fade only this profile's preparation clips; cross-group montages do not stop each other. */
	void StopPreparationMontages(float BlendOutTime = -1.f) const;

	/**
	 * Clear the held task's delegates WITHOUT ending it, so the ability's own teardown still reaches the
	 * task with AbilityEnded=true and stops the montage. Ending it here instead would leave a looping aim
	 * montage playing forever after the action is gone.
	 */
	void DetachPresentation();


	/**
	 * Show or hide the readied object in the character's hand.
	 *
	 * The prop itself belongs to UAZ_ThrowableHandComponent on the controller, because the object is in the
	 * hand from the moment the item is READIED and this ability only exists while the player is aiming. All
	 * the action does is hide it between the release cue and the end of the throw, when the projectile is
	 * the item and a copy in the hand would put one object in two places.
	 */
	void SuppressHandProp(bool bSuppressed) const;

	/** Tell the hand component the action owns the upper body, so its carry idle yields the shared slot. */
	void SetHandActionOwnership(bool bOwned) const;

	/**
	 * Raise or clear Ability.State.Throwing — "the release is committed and the body is planted".
	 *
	 * ★ This, not Ability.State.ThrowPreparing, is what locks movement. Aiming is an upper-body presentation
	 * the player walks around in; only the full-body release clip needs the legs to stop.
	 */
	void SetThrowCommittedTag(bool bCommitted) const;

	class UAZ_ThrowableHandComponent* FindHandComponent() const;

	void EnterAiming();
	void EnterWindup();
	/** Authority: spend the unit, then activate the prepared projectile. Never the other way round. */
	void PerformRelease();
	void TickPreview();
	void FinishAndRelease(bool bCancelled);
	/** The owner's preview component, created once on first aim and reused for every later throw. */
	UAZ_ThrowPreviewComponent* GetPreview();
	void HidePreview();

	UPROPERTY() TObjectPtr<const UAZ_ThrowableDefinition> Definition;
	UPROPERTY() TObjectPtr<const UAZ_ThrowPresentationProfile> Profile;
	UPROPERTY() TObjectPtr<UAZ_AT_PlayMontageAndWaitForEvent> PresentationTask;
	UPROPERTY() TObjectPtr<UAZ_ThrowPreviewComponent> Preview;

	EAZ_ThrowPhase Phase = EAZ_ThrowPhase::None;

	/** Provisional while aiming, frozen at Windup. Never re-selected after that: swapping animation or origin
	 *  mid-commit to salvage a blocked solution is exactly the dishonesty the plan forbids. */
	EAZ_ThrowArc Arc = EAZ_ThrowArc::Far;

	/** Accepted aim intent. Follows the camera while aiming; frozen when Windup begins so the throw goes
	 *  where the player committed rather than where the camera drifted during the wind-up. */
	FRotator AcceptedAim = FRotator::ZeroRotator;

	/** Accepted aim DISTANCE, frozen alongside the rotation. It drives speed and origin lift continuously,
	 *  so it has to be part of what the commit locks - otherwise the throw would be solved with a distance
	 *  the player was no longer pointing at. */
	float AcceptedAimDistance = 0.f;

	/** Identity of this action, threaded through the inventory reservation and every validation. */
	FGuid ThrowActionId;

	/** The reserved source item. */
	FGuid SourceItemId;

	/** One pending commit from a release that arrived during Start. Exactly one, cleared on every exit, and
	 *  never carried into another item or action. */
	bool bPendingCommit = false;

	/** Set by RequestCancel. A still-held RMB coming up after this must not throw. */
	bool bCancelRequested = false;
	/** Selection changes during teardown must not start another cancel or recursively end this action. */
	bool bEndingThrow = false;

	/** Last reported solution status, so a refusal is logged on the EDGE and not twenty times a second. */
	EAZ_ThrowSolutionStatus LastPreviewStatus = EAZ_ThrowSolutionStatus::Valid;

	/** Arc the aim currently wants, and since when. Accepted only once it has held for ArcDwellTime. */
	EAZ_ThrowArc PendingArc = EAZ_ThrowArc::Far;
	double PendingArcSince = 0.0;

	FTimerHandle ReadySeamTimer;
	FTimerHandle PreviewTimer;
};
