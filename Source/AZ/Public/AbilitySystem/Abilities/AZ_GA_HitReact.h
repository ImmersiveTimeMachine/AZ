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

	/**
	 * Grab-escape reactions, one picked at RANDOM per escape. Each carries its own timing because the
	 * clips are shaped differently and a shared beat would be wrong for both:
	 *   AM_Zombie_KB_Chase_2      3.10s, -117cm at 1.40s then walks back to +25cm (a round trip)
	 *   Zombie_Atk_KnockBack_1    5.47s, -163cm arriving at 2.50s then holds (a pure knockback)
	 * Leave empty to fall back to the single-descriptor path (anim-set GrabEscapeReact, else the
	 * variant's own HitReact clip with its beat opened to the whole montage).
	 *
	 * ★ RootMotionSeconds matters more here than anywhere else: once a clip's root stops travelling,
	 * driving it further applies a ~zero delta under OverrideAll, which PINS the pawn instead of moving
	 * it. Set it to where the clip actually settles (2.6 for KnockBack_1), not to the clip length.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Reaction|Grab")
	TArray<FAZ_CombatMontage> GrabEscapeReactions;

	// --- FALLBACK reactions, used ONLY when the avatar has no AnimSet to resolve from (the hero: its
	// pawn carries no AnimSet property, so before these existed the ability activated, resolved nothing,
	// and silently ended — which is why the player ate claws with zero feedback). Chalkies never reach
	// these: their anim-set descriptors resolve first.
	//
	// TWO of them because the pick is DIRECTIONAL: which side of the victim's body the sweep actually
	// struck (impact point in victim local space — NOT the attacker's hand, which reads wrong the moment
	// either body turns mid-swing). If Left/Right look mirrored in PIE, swap the two montage assignments
	// in the BP child — the mapping is data, not code.

	/** Struck on the LEFT side of the body (victim local -Y). Also the pick when no hit result rode the
	 *  event (environment damage). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Reaction|Fallback")
	FAZ_CombatMontage DefaultReactionLeft;

	/** Struck on the RIGHT side of the body (victim local +Y). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Reaction|Fallback")
	FAZ_CombatMontage DefaultReactionRight;

	// --- POSE-SELECTED REACTION (tier 1: single-role motion matching over the knockback pool).
	// The Zombie_01 pack authors its knockbacks by LOCOMOTION STATE (Atk_1..5 / Chase_1..5 / Walk_F_1..6),
	// never by hit direction — i.e. the authored axis IS "what was the body doing when it was hit", which
	// is exactly what a pose search selects on. Before this, one montage per variant DA was hard-picked
	// and ~15 authored reactions never played. When ReactionDatabase is assigned, the descriptor still
	// owns the gameplay TIMING (ActiveSeconds beat, staggerRecover, root motion) and the search only
	// swaps WHICH clip and its entry frame. Null = the single hard-picked montage, unchanged.

	/** Knockback pool (PSD_AZ_ChalkieReactions — montage entries, each SamplingRange-trimmed to its
	 *  impact window so the search picks a CLIP, always near its start). Editor-assigned on the granted
	 *  class (BP-child CDOs need a recompile to see writes — feedback_bp_cdo_write_needs_recompile). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Reaction|MM")
	TObjectPtr<class UPoseSearchDatabase> ReactionDatabase;

	/** Reject an entry frame past this (seconds) — a reaction must start at its impact, not mid-flight.
	 *  0.25 = the 0.2 sampling window + quantization slack; a larger value means the DB trim was lost. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Reaction|MM", meta = (ClampMin = "0"))
	float ReactionEntryMaxTime = 0.25f;

	/** Pick this escape's reaction: a random entry of GrabEscapeReactions, else the fallback below. */
	bool ResolveShoveDescriptor(const AActor* Avatar, FAZ_CombatMontage& Out) const;

	/** Single-descriptor path: the anim set's GrabEscapeReact when authored, else its HitReact descriptor
	 *  with the beat opened to the whole clip (a flinch cuts at the recoil peak; an escape plays out). */
	static bool ResolveShoveFallback(const AActor* Avatar, FAZ_CombatMontage& Out);

	/** LONGEST gate any escape reaction can run for. GA_ChalkieGrab sizes its post-handoff hold from this
	 *  rather than re-picking: a second random draw would disagree with the one that actually played, and
	 *  the hold must never end BEFORE the knockback (it restores pair collision and releases the crowd
	 *  tokens). Erring long is harmless; erring short strands the Chalkie mid-flight. */
	static float GetShoveHoldSeconds(const AActor* Avatar);

	/** Pose-select the reaction clip out of ReactionDatabase (the ONLY PoseSearch call in this class —
	 *  Experimental-API quarantine). On success rewrites Desc.Montage and returns the entry frame; on
	 *  any invalid result logs "[React MM] FALLBACK reason=..." and leaves Desc untouched. */
	bool TrySelectReactionByPose(const AActor* Avatar, FAZ_CombatMontage& InOutDesc, float& OutStartPosition) const;

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
