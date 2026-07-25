// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_PlayerGrabbed.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;
class UAZ_AT_WaitInputPressWithTags;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UCameraShakeBase;
class UCurveFloat;

/**
 * The TLOU-style GRAB, victim side. Event-triggered by Event.Grabbed (same mechanism as
 * Event.Death -> GA_Death) so the montage plays on the player's OWN avatar. While active:
 *  - ActivationOwnedTags applies State.Grabbed: movement zeroed (ProduceInput gate), camera frozen
 *    (OnLookTriggered gate), and every non-opted-in ability blocked (UAZ_GameplayAbility base gate).
 *  - Plays the looping hero struggle montage (FullBody slot).
 *  - THE MASH: E (Interact) presses reach this ability because its granted spec carries the
 *    Input.Action.Interact dynamic tag — each press re-arms a WaitInputPressWithTags task.
 *    PressesToEscape presses inside WindowSeconds = escape; window first = the Chalkie's attack lands.
 * Verdicts go back to the grabber by gameplay event (Event.GrabEscaped / Event.GrabTimeout — the
 * CHALKIE applies the fail damage so kills credit the attacker); Event.GrabRelease from a dying or
 * cancelled grabber frees the player silently. This ability owns the ONE clock and the meter UI.
 */
UCLASS()
class AZ_API UAZ_GA_PlayerGrabbed : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_PlayerGrabbed();

	/** Idempotent CDO patch: Event.Grabbed trigger + State.Grabbed ActivationOwnedTags (native tags
	 *  register after CDO ctors). Call before GiveAbility WITH the class you grant (BP child CDOs do
	 *  not inherit runtime native-CDO patches; null = native). The grant site also adds the
	 *  Input.Action.Interact dynamic tag to the spec so E-presses route here. */
	static void ConfigureCDO(UClass* GrantClass = nullptr);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** E-presses needed to break free (az.Grab.PressesToEscape overrides for testing). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "1"))
	int32 PressesToEscape = 8;

	/** Escape window in seconds — timer first = the Chalkie's attack lands (az.Grab.WindowSeconds overrides). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "1"))
	float WindowSeconds = 7.f;

	/** Looping hero struggle poses (FullBody slot) — a POOL; one entry is picked per grab. Assign in
	 *  BP_GA_PlayerGrabbed. Selection: bRandomStruggleMontage picks randomly each grab, otherwise
	 *  StruggleMontageIndex picks a fixed entry (array index from 0; 0 = first). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab")
	TArray<TSoftObjectPtr<UAnimMontage>> StruggleMontages;

	/** True = pick a random pool entry per grab (overrides StruggleMontageIndex). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab")
	bool bRandomStruggleMontage = false;

	/** Fixed pick when not random: array index from 0 (clamped to the pool; 0 = first by default). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab", meta = (ClampMin = "0", EditCondition = "!bRandomStruggleMontage"))
	int32 StruggleMontageIndex = 0;

	// --- Camera feel (local viewer only; the boom pull-in itself lives in the pawn's CameraGrabbed mode) ---

	/** Continuous struggle rumble for the whole hold (bSingleInstance: per-press restarts retune it). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Camera")
	TSubclassOf<UCameraShakeBase> RumbleShakeClass;

	/** Short jolt on each E-press — the physical "wrench" feedback. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Camera")
	TSubclassOf<UCameraShakeBase> JoltShakeClass;

	/** Shake intensity over the hold: X = normalized hold time 0..1 (elapsed/window), Y = scale applied
	 *  to both shakes. Unset = constant 1. Authored asset lets the shake RAMP as the window runs out. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Grab|Camera")
	TSoftObjectPtr<UCurveFloat> ShakeIntensityCurve;

private:
	UFUNCTION() void OnMashPress(float TimeWaited);
	UFUNCTION() void OnGrabberReleased(FGameplayEventData Payload);
	/** Unbreakable hold: replay the struggle loop if it ends while the grab is unresolved — nothing
	 *  but the two grab anims may show during the hold (user rule 2026-07-24). */
	UFUNCTION() void OnStruggleMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData);

	void StartMashTask();
	/** Plays CachedStruggleMontage self-looped and arms the replay-on-end binding. */
	void StartStruggleMontage();
	/** Single funnel: escape/timeout/release all end here. bNotifyGrabber=false for Event.GrabRelease. */
	void FinishGrab(bool bEscaped, bool bNotifyGrabber);
	void UpdateStruggleHUD() const;

	class APlayerController* GetOwnerPlayerController() const;
	/** Curve-scaled shake intensity for the current hold progress (1 when no curve). */
	float ComputeShakeScale() const;

	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
	UPROPERTY() UAZ_AT_WaitInputPressWithTags* MashTask = nullptr;
	UPROPERTY() UAbilityTask_WaitGameplayEvent* WaitReleaseTask = nullptr;
	UPROPERTY() UAnimMontage* CachedStruggleMontage = nullptr;

	TWeakObjectPtr<const AActor> Grabber;
	int32 Presses = 0;
	int32 PressesNeededEffective = 8;
	float WindowSecondsEffective = 7.f;
	bool bResolved = false;
	FTimerHandle WindowTimer;
	double HoldStartTimeSeconds = 0.0;   // for the shake-intensity curve's normalized hold progress

	/** State.Grabbed is applied as an EXPLICIT loose tag (paired add/remove), NOT via
	 *  ActivationOwnedTags: runtime CDO patches to that container do not survive into instances of
	 *  BP tuning children (proven 2026-07-24 — instance activated with an empty container). */
	bool bAppliedGrabbedTag = false;
};
