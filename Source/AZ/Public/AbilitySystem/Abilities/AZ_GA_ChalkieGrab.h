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

	/** Contact distance the grabber CLOSES TO at the catch (cm, center-to-center) — the "attachment
	 *  point" distance, tunable here instead of a physical attach (a live Mover pawn cannot be attached:
	 *  the movement sim stamps its transform every tick). Both actors are rooted + mutually facing for
	 *  the whole hold, so the contact pose keeps itself; nothing to detach on release. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "50"))
	float GrabHoldDistance = 110.f;

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

	/** Single funnel for both outcomes — guards double-resolution (event + safety timer race). */
	void Resolve(bool bPlayerEscaped);
	void PlayExitMontage(FName MontageProperty);
	/** Plays CachedLoopMontage self-looped and arms the replay-on-end binding. */
	void StartLoopMontage();

	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* LoopMontageTask = nullptr;
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* ExitMontageTask = nullptr;
	UPROPERTY() UAnimMontage* CachedLoopMontage = nullptr;
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
