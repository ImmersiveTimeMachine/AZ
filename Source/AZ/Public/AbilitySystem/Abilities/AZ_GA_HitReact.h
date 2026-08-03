// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "Animation/AZ_CombatMontage.h"
#include "AZ_GA_HitReact.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;

/**
 * THE stagger-class reaction ability (arch step A) — one class, two triggers:
 *   Event.Combat.HitReact  — survivable hit (HandleDamaged; payload Instigator = causer)
 *   Event.Combat.StepBack  — pack recoil beat (horde; payload OptionalObject = montage,
 *                            EventMagnitude = caller beat override, 0 = clip default)
 *
 * Owns exactly three things: the reaction MONTAGE (task), the capsule ROOT MOTION
 * (generation-scoped DriveRootMotion), and the State.Combat.Staggered TAG (ActivationOwnedTags —
 * drops automatically when the ability ends, which is what gates BT attacks). Everything else a hit
 * causes — damage-lock, scream, melee self-cancel — deliberately stays in HandleDamaged: those fire
 * even for a GRABBING Chalkie, whose ActivationBlockedTags armor only suppresses the flinch MOTION.
 *
 * Clocking ("events drive, timers guard"):
 *   - The beat ends via the Event.Combat.BeatEnd notify AUTHORED ON THE MONTAGE at the measured
 *     recoil peak — the animation timeline is the clock, so rate scale / interrupts / hitstop
 *     propagate for free. On BeatEnd the montage is stopped with the descriptor's blend; the task's
 *     completion callback then ends the ability, so the Staggered gate naturally spans beat + blend.
 *   - A cut TIMER runs only when the notify does not own the beat (un-authored variant fallback, or a
 *     caller-shortened StepBack beat) — same visible behaviour, explicitly second-class.
 *   - A WATCHDOG (gate + margin) ends the ability if every event path failed (e.g. the montage was
 *     killed the same frame its notify would have fired). Guards, never the mechanism.
 *
 * Death: GA_Death's CancelAbilities reaches this like any ability — montage stops, RM releases, tag
 * drops, same frame. Re-trigger: InstancedPerActor + retrigger, so a second punch mid-stagger
 * restarts the reaction (stun-lock accepted — the pack is the difficulty, per the fight rulebook).
 *
 * Trigger/tag wiring lives in ConfigureOnCDO (native tags register after CDO ctors — GA_Death pattern).
 */
UCLASS()
class AZ_API UAZ_GA_HitReact : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_HitReact();

	/** Idempotent CDO patch: both GameplayEvent triggers + Staggered owned-tag + Grabbing blocked-tag.
	 *  Call before GiveAbility with the class actually granted (BP child CDOs don't inherit runtime
	 *  patches to the native CDO). Null = patch the native class. */
	static void ConfigureOnCDO(UClass* GrantClass = nullptr);

protected:
	/** GRAB ARMOR (rule 8) + the melee cancel. Damage-lock and scream are NOT here: they live in
	 *  HandleDamaged and must fire even for a grabbed Chalkie, whose armor only suppresses the flinch
	 *  MOTION.
	 *
	 *  State.Combat.Staggered is deliberately NOT declared as an ActivationOwnedTags entry — this
	 *  ability applies it by hand (SetLooseGameplayTagCount in Activate/End). Two mechanisms writing one
	 *  counted tag would fight: the explicit clear sets an absolute 0 while GAS decrements its own +1.
	 *  One owner, and it is the explicit pair. */
	virtual void DeclareAbilityTags() override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** BeatEnd notify arrived from the montage timeline — stop the montage at the beat. */
	UFUNCTION()
	void OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Montage finished / interrupted / cancelled — the reaction is over. */
	UFUNCTION()
	void OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Stop the reaction montage with the descriptor blend (idempotent — BeatEnd and the fallback cut
	 *  timer can both reach this; whichever is second finds nothing playing). */
	void StopMontageAtBeat();

	// --- Runtime state, latched per activation (InstancedPerActor + retrigger resets these) ---
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
	FAZ_CombatMontage Desc;
	uint64 RootMotionGen = 0;
	FTimerHandle BeatCutTimer;   // fallback cut — armed only when the notify doesn't own the beat
	FTimerHandle Watchdog;       // gate + margin: ends the ability if every event path failed
	/** Post-animation hold (descriptor's StaggerRecoverSeconds): the ability — and therefore the
	 *  Staggered tag, and therefore the AI's chase block — outlives the clip by this much. */
	FTimerHandle RecoverTimer;
	bool bRecovering = false;
	/** TEMP diagnostic ([HitReact], strip with the other combat diags): where the victim stood when the
	 *  reaction started, so EndAbility can report how far the knockback ACTUALLY moved the capsule. */
	FVector ReactStartLocation = FVector::ZeroVector;
};
