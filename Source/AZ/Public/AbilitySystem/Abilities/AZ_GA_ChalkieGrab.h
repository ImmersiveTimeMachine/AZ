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

	/** Contact distance the grabber CLOSES TO at the catch (cm, center-to-center). The paired clips are
	 *  SHARED-ORIGIN — both bodies are authored around one point and their separation is baked into the
	 *  bones — so the paired path wants 0 (both actors at one transform) and the v1 path wants ~110.
	 *  Raising this on the paired path pulls the two halves of the clinch apart. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0"))
	float GrabHoldDistance = 0.f;

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

	/** Event.Grab.OutcomeBegin — the shove section just STARTED. Drives the capsule with the clip's own
	 *  root motion from here, which is the only moment that can be right: the outcome is queued at the
	 *  wrestle loop boundary, so it begins up to a full cycle after it was chosen. */
	UFUNCTION() void OnGrabMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Seconds of root-motion drive for the shove. 0 = the outcome section's own length, which is what
	 *  you want; a positive value cuts the capsule loose earlier than the animation. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0", ForceUnits = "s"))
	float OutcomeRootMotionSeconds = 0.f;

	/** Minimum authored travel before the shove is allowed to drive the capsule. The hero's side of these
	 *  paired clips moves 0.0cm (correct — you plant and shove, they fly), and driving a non-travelling
	 *  clip would PIN the pawn under OverrideAll rather than move it. Measured, not trusted from the
	 *  bEnableRootMotion flag, which is set on clips that do not move. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0", ForceUnits = "cm"))
	float OutcomeMinRootTravel = 5.f;

	/** Recovery beat AFTER the shove animation, during which this Chalkie may not swing. Without it the
	 *  ability ends with the section and the BT can attack on its next tick — shoved 40cm away and
	 *  instantly back on the player, which makes the escape feel like it achieved nothing. Applied
	 *  through the pawn's single stagger owner, so it never fights a hit reaction over the tag. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0", ForceUnits = "s"))
	float PostShoveStaggerSeconds = 1.f;

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

	/** Generation of the shove's root-motion drive. Released in EndAbility through the generation-scoped
	 *  path, so a drive that a knockback or another ability has already superseded is never cancelled. */
	uint64 OutcomeRootMotionGen = 0;
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
	FTimerHandle SafetyTimer;

	/** State.Combat.Grabbing applied as an EXPLICIT loose tag (paired add/remove) — the flinch
	 *  carve-out reads it; ActivationOwnedTags CDO patches don't reach BP-child instances. */
	bool bAppliedGrabbingTag = false;

	/** Pair-collision carve-out active (mutual IgnoreActorWhenMoving for the hold) — paired restore
	 *  in EndAbility so the clinch can go INSIDE capsule contact without permanent ghosting. */
	bool bAppliedMoveIgnore = false;
};
