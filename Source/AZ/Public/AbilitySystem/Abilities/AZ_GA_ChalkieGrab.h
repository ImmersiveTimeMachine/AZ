// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_ChalkieGrab.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * The TLOU-style GRAB, grabber side. Fired INSTEAD of the melee swing by UAZ_BTTask_ZombieAttack's
 * random roll (no telegraph — blends from the fight straight into the hold). Flow:
 *   1. Read the prey off the controller's blackboard, verify it isn't already grabbed.
 *   2. Fire Event.Grabbed at the prey's ASC (triggers GA_PlayerGrabbed on ITS avatar — no cross-actor
 *      montage), then verify the grab actually took (State.Grabbed present) before committing the hold.
 *   3. Play GrabLoopMontage self-looped (Montage_SetNextSection to itself) and wait for the player's
 *      verdict: Event.GrabEscaped -> GrabEscapeMontage stagger; Event.GrabTimeout -> damage chunk +
 *      GrabEndMontage. A safety timer treats a silent player as escaped so nobody wedges.
 *   4. Any OTHER end (BT abort, death, flinchless cancel) sends Event.GrabRelease so the player is
 *      never left frozen.
 * The BT task stays latent for the whole thing (grab = "a longer attack" to the tree); its
 * SetMeleeTaskActive lock keeps the crowd brain from rotating the grabber out mid-hold.
 */
UCLASS()
class AZ_API UAZ_GA_ChalkieGrab : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_ChalkieGrab();

	/** Idempotent CDO patch: adds State.Combat.Grabbing to ActivationOwnedTags (native tags register
	 *  after CDO ctors — same constraint as GA_Death's trigger). Call before GiveAbility with the class
	 *  you actually GRANT — a BP tuning child's CDO does NOT inherit runtime patches made to the native
	 *  CDO, so the patch must land on the granted class itself. Null = patch the native class. */
	static void ConfigureCDO(UClass* GrantClass = nullptr);

protected:
	/** Committing to a grab cancels this Chalkie's own swing, and a corpse cannot grab. State.Combat.
	 *  Grabbing stays an EXPLICIT loose tag (see bAppliedGrabbingTag) — one owner per fact. */
	virtual void DeclareAbilityTags() override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Damage chunk applied to the player when the escape window elapses (the attack "lands"). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0"))
	float FailDamageAmount = 35.f;

	/** Safety ceiling on the hold: if the player-side ability never reports back, free everyone. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "1"))
	float MaxHoldSeconds = 15.f;

	/** GE for the fail chunk; magnitude rides SetByCaller.Damage. Default = UAZ_GE_Damage (S1 spine). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// --- PAIRED MONTAGE (the shared-origin NAAT grab). When PairedGrabMontage is assigned this ability
	// runs one sectioned montage for the whole grab and drives the outcome by SECTION, and the player's
	// ability follows it through UAnimInstance::MontageSync_Follow. Leave it unset to fall back to the
	// v1 loop/exit montage pair. Section names must match the hero montage character-for-character —
	// that is the sole condition under which the engine mirrors section jumps to the follower.

	/** Chalkie-side sectioned grab montage. Editor-assigned (no /Game/ paths in C++); unset = v1 path. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Paired")
	TSoftObjectPtr<UAnimMontage> PairedGrabMontage;

	/** Entry section — the catch. Its montage link should lead to WrestleSection. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Paired")
	FName CatchSection = FName("Catch");

	/** The hold. Self-linked in the montage so it loops until an outcome is queued onto it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Paired")
	FName WrestleSection = FName("Wrestle");

	/** Player-wins sections, one picked at random (the human shoves or kicks the Chalkie off). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Paired")
	TArray<FName> EscapeSections = { FName("Push"), FName("Kick") };

	/** Player-loses section (the window elapsed and the bite lands). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Paired")
	FName FailSection = FName("Munch");

	/** Contact distance the grabber CLOSES TO at the catch (cm, center-to-center).
	 *
	 *  ★ 92, AND 0 IS WRONG — DO NOT "RESTORE THE DOCTRINE" HERE. Tested in PIE 2026-08-04: at 0 the two
	 *  bodies stand BACK TO BACK. The reasoning that says 0 ("the clips are shared-origin, so put both
	 *  actors at one transform") only holds if the actors share a ROTATION as well as a position, and this
	 *  feature deliberately does not: the hero look-ats the grabber, so the two are ~180 degrees OPPOSED
	 *  (the shared-yaw variant was tried 2026-08-01 and read as "same line, not face to face").
	 *
	 *  Each NAAT clip bakes its body roughly 30cm IN FRONT of the actor origin (measured pelvises: hero
	 *  (-0.2,-15.5,88.3), Chalkie (-2.0,14.1,90.9), ~29.6cm apart; the mesh's -90 yaw maps anim +Y onto
	 *  actor forward). Two opposed actors at ONE origin therefore push their bodies along opposite
	 *  facings — apart, backs together. Separating the origins by ~2x that offset lands each body in
	 *  front of the other, which is why the hand-tuned 92 is right and is not a fudge factor.
	 *
	 *  The v1 (unpaired) path wants ~110. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0"))
	float GrabHoldDistance = 92.f;

	/** Seconds for the close-in slide at the catch (short = a snatch/lunge feel). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0.05"))
	float GrabCloseSeconds = 0.15f;

	/** PACK STEP-BACK beat: every other engaged Chalkie recoils (its variant KB montage) for this long
	 *  when the grab lands, then blends back out. 0 = plays the whole knockback clip (5.5-7.5s). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0"))
	float PackStepBackSeconds = 1.5f;

private:
	UFUNCTION() void OnEscaped(FGameplayEventData Payload);
	UFUNCTION() void OnTimeout(FGameplayEventData Payload);
	UFUNCTION() void OnExitMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);
	/** Unbreakable hold: if the loop montage ends for ANY reason while unresolved, replay it —
	 *  nothing but the two grab anims may show during the hold (user rule 2026-07-24). */
	UFUNCTION() void OnLoopMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Event.Grab.OutcomeBegin — the outcome section just STARTED, i.e. the one frame on which both bodies
	 *  leave the hold pose together. On an ESCAPE this is where the two halves DIVERGE: the hero keeps
	 *  playing its authored shove/kick, and we hand our own body to GA_HitReact to play the variant
	 *  knockback. It has to be here and not at Resolve, because the outcome is queued at the wrestle loop
	 *  boundary and begins up to a full cycle after it was chosen. */
	UFUNCTION() void OnGrabMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Hand our half of the escape to GA_HitReact (Event.Combat.GrabShoved) and release the hero from the
	 *  montage sync. Ordering inside is load-bearing — see the implementation. */
	void HandOffShoveToReaction();

	/** Tell the victim its outcome has started: swap to the pulled-back camera framing, drop the struggle
	 *  rumble, and — on an escape — stop following us and play escape montage EscapeMontages[index]
	 *  itself. The index rides EventMagnitude and is the ONE piece of coupling between the two sides'
	 *  ordered arrays (ours is EscapeSections). Sent on both outcomes. */
	void NotifyHeroOutcomeBegan();

	/** Event.Grab.Shove, forwarded by the victim from a notify on ITS escape montage — the frame the
	 *  shove connects. Launches the knockback. */
	UFUNCTION() void OnHeroShove(FGameplayEventData Payload);

	/** Single funnel for both outcomes — guards double-resolution (event + safety timer race). */
	void Resolve(bool bPlayerEscaped);
	void PlayExitMontage(FName MontageProperty);
	/** Plays CachedLoopMontage self-looped and arms the replay-on-end binding. */
	void StartLoopMontage();

	/** True once the paired montage resolved and loaded — selects section-driven flow over the v1 pair. */
	bool IsPaired() const { return CachedPairedMontage != nullptr; }
	/** Our own anim instance, or null (both sides' montage control funnels through here). */
	class UAnimInstance* GetOwnAnimInstance() const;
	/** Publishes exactly one State.Grab.* stage tag, removing whichever was there before. */
	void SetGrabStage(const FGameplayTag& NewStage);

	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* LoopMontageTask = nullptr;
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* ExitMontageTask = nullptr;
	UPROPERTY() UAnimMontage* CachedLoopMontage = nullptr;
	UPROPERTY() UAnimMontage* CachedPairedMontage = nullptr;

	/** The stage tag currently on our ASC (paired removal — see SetGrabStage). */
	FGameplayTag ActiveStageTag;

	/** Fires when the catch section ends, flipping the stage tag Catching -> Wrestling. */
	FTimerHandle StageTimer;
	UPROPERTY() UAbilityTask_WaitGameplayEvent* WaitEscapedTask = nullptr;
	UPROPERTY() UAbilityTask_WaitGameplayEvent* WaitTimeoutTask = nullptr;

	TWeakObjectPtr<AActor> GrabTarget;
	bool bResolved = false;
	/** Which way it resolved — the outcome section is queued at Resolve but does not BEGIN until the
	 *  wrestle loop boundary, and only the escape branch diverges when it does. */
	bool bResolvedEscaped = false;
	/** The hero now owns its own exit (it was told to unfollow and is playing its outcome section under
	 *  its own steam). Suppresses Event.GrabRelease in EndAbility: that signal ENDS the victim's ability
	 *  outright, so sending it here would drop State.Grabbed and hand movement back mid-animation —
	 *  roughly 2.3s early, in the same synchronous stack as the mash press that started all this.
	 *  Abnormal exits leave this false and still release, which is what keeps a dying grabber from
	 *  freezing the player. */
	bool bOutcomeHandedToHero = false;
	FTimerHandle SafetyTimer;
	/** Keeps this ability alive across the shove after the handoff, so the BT task stays latent and the
	 *  crowd/grab tokens release on the same schedule as before — and so the pair-collision carve-out is
	 *  restored once the bodies are APART. Restoring it at handoff time would fire with the capsules
	 *  coincident (shared-origin hold), where the depenetration direction is arbitrary. */
	FTimerHandle HandoffTimer;
	/** Guards the hero's contact notify: fires the knockback anyway if the event never arrives, so an
	 *  unauthored montage degrades instead of leaving the grab waiting on a signal that never comes. */
	FTimerHandle ShoveWatchdog;
	/** Which entry of EscapeSections this resolve picked — sent to the hero so both sides play the same
	 *  outcome without the montage sync having to mirror it. */
	int32 ChosenEscapeIndex = 0;
	UPROPERTY() class UAbilityTask_WaitGameplayEvent* WaitShoveTask = nullptr;

	/** State.Combat.Grabbing applied as an EXPLICIT loose tag (paired add/remove) — the flinch
	 *  carve-out reads it; ActivationOwnedTags CDO patches don't reach BP-child instances. */
	bool bAppliedGrabbingTag = false;

	/** Pair-collision carve-out active (mutual IgnoreActorWhenMoving for the hold) — paired restore
	 *  in EndAbility so the clinch can go INSIDE capsule contact without permanent ghosting. */
	bool bAppliedMoveIgnore = false;
};
