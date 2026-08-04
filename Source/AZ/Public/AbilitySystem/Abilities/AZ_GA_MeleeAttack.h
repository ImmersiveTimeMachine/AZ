// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"   // our base, NOT UGameplayAbility
#include "AZ_GA_MeleeAttack.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;
class UGameplayEffect;

// Which hand threw it — latched at activation from the InputTag.
UENUM(BlueprintType)
enum class EAZ_MeleeHand : uint8 { Left, Right };

UCLASS()
class AZ_API UAZ_GA_MeleeAttack : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_MeleeAttack();
	
protected:
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** You cannot swing while a reaction owns the body, while caught, while holding someone, or dead.
	 *  Shared by the hero punch and the Chalkie claw — same rail, same rules. */
	virtual void DeclareAbilityTags() override;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	// The selector. Hero: the 4 fist-clip UPROPERTYs. Subclasses (zombie) override to read the
	// avatar's anim-set DA. CHT_Melee replaces both later.
	virtual UAnimMontage* SelectMontage() const;

public:
	/** Reflection read of a montage field off the avatar pawn's AnimSet DataAsset (BP_ChalkieAnimSet
	 *  pattern: pawn has an object property "AnimSet" whose class carries montage fields). Shared by
	 *  the zombie melee + death abilities; goes native with UAZ_ChalkieAnimSet in the batch. */
	static UAnimMontage* FindAnimSetMontage(const AActor* Avatar, FName MontageProperty);

	/** Reflection read of an FAZ_CombatMontage struct variable off the anim set (arch step B: per-clip
	 *  timing authored NEXT TO the clip). Returns false when the variable is absent, wrongly typed, or
	 *  has no montage assigned — callers fall back to the legacy bare-montage field + pawn knob, so
	 *  un-authored variants keep working. Goes native with UAZ_ChalkieAnimSet in the batch. */
	static bool FindAnimSetCombatMontage(const AActor* Avatar, FName StructPropertyName, struct FAZ_CombatMontage& Out);

	/** True if this montage authors a recovery window (a melee-window notify state carrying
	 *  Event.Combat.CancelOpen). Clips WITHOUT one are treated as cancellable throughout: absence of
	 *  data must not make combat more restrictive than it was before the feature existed, and today a
	 *  second fist can already displace the first at any time. Authoring the window is what BUYS the
	 *  commitment, rather than the feature silently imposing it on every clip in the project. */
	static bool MontageHasCancelWindow(const UAnimMontage* Montage);

	/** Trigger time of the Event.Combat.BeatEnd notify authored on this montage, or 0 when absent
	 *  (arch step A: the montage timeline is the beat clock — "events drive, timers guard"). Used by
	 *  GA_HitReact and by the melee bite; 0 means the caller falls back to its own beat source. */
	static float FindBeatEndNotifyTime(const UAnimMontage* Montage);

	/** Reflection read of a float config off any object (BP-variable-tunable under Live Coding — new
	 *  UPROPERTYs need a restart). Falls back to Default when the property doesn't exist. Every call
	 *  site of this is a batch item: promote to a real UPROPERTY / DA_ChalkieConfig field. */
	static float ReadConfigFloat(const UObject* Object, FName PropertyName, float Default);

protected:

	// Montage finished / interrupted / cancelled -> end the ability.
	UFUNCTION()
	void OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);

	// Montage GameplayEvents: WindowBegin/WindowEnd bound the strike phase (socket sweep runs between
	// them), BeatEnd ends the attack from the clip's own timeline.
	UFUNCTION()
	void OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Start/stop the fist socket sweep (authority only; idempotent). Stop also runs on EndAbility so an
	 *  interrupted swing can't leave a detector ticking. */
	void StartHitWindow();
	void StopHitWindow();

	/** Mirror bCancellable onto the ASC as State.Combat.CancelWindow. Absolute count (1/0), because
	 *  loose tags are COUNTED and a cancel-into-attack ends one swing while starting the next — an
	 *  Add/Remove pair would leave the count at 1 after the chain and mark the pawn permanently
	 *  cancellable. Idempotent: safe to call with the same value twice. */
	void SetCancelWindowTag(bool bOpen);

	/** True if some OTHER melee instance on this ASC is mid-swing and is NOT in its recovery window.
	 *  This is what makes chaining controlled rather than free: L and R are separate specs, so retrigger
	 *  alone never governs a cross-hand chain, and without this a second fist could interrupt the first
	 *  at frame 0 — mid-warp, mid-strike — which is the uncontrolled mashing the window exists to
	 *  prevent. Returns false when nothing else is running, so the first punch is always allowed. */
	bool IsOtherMeleeCommitted(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** A physically-touched, pre-filtered victim from the sweep task — apply damage + fight noise. */
	UFUNCTION()
	void OnSweepHit(const FHitResult& Hit);

	// --- Avatar access, pawn-class-agnostic (hero AND infected run this same ability) ---
	USkeletalMeshComponent* GetAvatarMesh() const;
	bool ResolveAvatarIsMoving() const;

	// --- Tunables: assign the 4 fist montages in a BP child (like GA_Shoot's FireMontage*) ---
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_R = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_R = nullptr;

	// RETIRED (socket-sweep windows replaced the single-frame hit event). Kept so BP children with a
	// serialized value still load clean; nothing reads it. Delete at the native-class batch.
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee") FGameplayTag HitWindowEventTag;

	// --- Socket-swept hit detection (see UAZ_AT_MeleeSweep) ---
	/** Sphere swept along the strike socket's path each tick. Fist ~12; claws slightly bigger. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SweepSphereRadius = 12.f;
	/** Strike sockets (bone FNames, not asset paths — fine per project rules). ALL of them are swept
	 *  every window: Hand picks the montage, the animator picks the fist, and only geometry gets to say
	 *  which one connected. See UAZ_AT_MeleeSweep for the measurements that forced this.
	 *
	 *  hand_* is the WRIST. Measured root-relative, the knuckles reach 8-10cm further on every clip in
	 *  the kit (Punch_L wrist 74.9 vs knuckle 83.4; Zombie_Atk_R 76.5 vs 87.0), so sweeping the wrist
	 *  alone under-reaches the visible fist by most of a sphere radius and invites fudge factors in the
	 *  stand-off instead. Sweeping both joints costs one extra sphere trace and makes the detected
	 *  contact match what the player sees connect. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") FName StrikeSocket_L = TEXT("hand_l");
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") FName StrikeSocket_R = TEXT("hand_r");
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") FName KnuckleSocket_L = TEXT("middle_01_l");
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") FName KnuckleSocket_R = TEXT("middle_01_r");

	// --- HIT-STOP (frame-of-contact acknowledgment; see UAZ_AbilitySystemComponent::ApplyHitStop) ---
	/** Attacker's hold on a landed hit. Deliberately SHORTER than the victim's — the attacker recovering
	 *  advantage first is the fighting-game convention that makes a landed hit feel earned. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Feel", meta = (ClampMin = "0", ClampMax = "0.15", ForceUnits = "s"))
	float HitStopAttackerSeconds = 0.05f;
	/** Victim's hold. Longer, and applied AFTER the reaction montage has had a frame to start, so the
	 *  body hangs in a recoil pose rather than freezing in its pre-hit pose. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Feel", meta = (ClampMin = "0", ClampMax = "0.15", ForceUnits = "s"))
	float HitStopVictimSeconds = 0.08f;

	// --- Damage (S1 spine) ---
	/** GE applied to each swept hostile; magnitude rides SetByCaller.Damage. Default = UAZ_GE_Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") TSubclassOf<UGameplayEffect> DamageEffect;
	/** Fists = 10: five punches down a standard 50 HP Chalkie. Weapons override per-ability (BP data). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage", meta = (ClampMin = "0")) float DamageAmount = 10.f;
	/** NO LONGER the damage shape (the socket sweep is): these only size FindWarpTarget's search sweep —
	 *  what's worth lunging at. Damage = the fist's actual path (SweepSphereRadius). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (ClampMin = "0", ForceUnits = "cm")) float MeleeRange = 90.f;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (ClampMin = "0", ForceUnits = "cm")) float MeleeRadius = 40.f;

	/** How long the montage's root motion is allowed to drive the CAPSULE, in seconds. 0 = the whole
	 *  montage (the original behaviour, and correct for in-place punches).
	 *
	 *  Set this on lunge clips whose recovery tail keeps travelling. RTG_RM_Fists_Punch_Heavy2Idle moves
	 *  202cm total but only 131cm of that lands before contact — the remaining 71cm plays out over the
	 *  1.7s recovery at ~42 cm/s. A struck Chalkie only stumbles back at 8-14 cm/s, so that tail walks the
	 *  hero straight through the knockback and makes it look like the reaction never fired. Ending the RM
	 *  bridge at the hit keeps the recovery animating while the capsule stays put, so the victim's stumble
	 *  is the only movement on screen.
	 *
	 *  Only the capsule stops — the montage plays to completion either way. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float RootMotionSeconds = 0.f;

	// --- MOTION WARPING ---
	// A lunging punch clip covers whatever distance the animator authored (RTG_RM_Fists_Punch_Heavy2Idle
	// travels 202cm). Without warping that distance is a constant, so the same punch overshoots a target
	// at 90cm and falls short of one at 250cm. Warping rescales the clip's own root motion to land on the
	// real target. Unlike the Chalkie (whose claw clips are in-place), these clips carry real translation,
	// so SkewWarp takes its scale-and-shear branch — which is where MaxSpeedClampRatio is live.
	//
	// No BT task on the player side, so the ability picks its own target: same forward sweep + cone + team
	// filter as the damage window below, just at a longer reach.

	/** Register a warp target on activation so a montage warp window has something to resolve. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp")
	bool bUseMotionWarping = true;

	/** Must match WarpTargetName on the montage's MotionWarping notify — they rendezvous by FName, and a
	 *  mismatch is a silent no-op rather than an error. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (EditCondition = "bUseMotionWarping"))
	FName WarpTargetName = TEXT("MeleeTarget");

	/** How far to look for something worth lunging at. Beyond this we register nothing and the clip plays
	 *  its authored distance — deliberately: warping the player across a room at an unrelated enemy is
	 *  worse than a whiff. Keep it near the clip's own travel (202cm) so the warp scales rather than
	 *  invents distance. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0", ForceUnits = "cm"))
	float WarpSearchDistance = 280.f;

	/** Where the warp point sits relative to the target, measured back along the target->attacker vector.
	 *  Aiming at their centre would park us inside their capsule; this stops us at punching range.
	 *
	 *  ★ MUST stay inside the fist's MEASURED reach, which is
	 *      hand_forward_extension + SweepSphereRadius + victim_capsule_radius.
	 *  Sampled per clip (AnimPoseExtensions, pelvis-relative; mesh yaw -90 so anim +Y is forward): the
	 *  jabs extend +72..+81cm inside their windows, i.e. 114..123cm of reach against a 30cm capsule. This
	 *  used to be justified against MeleeRange+MeleeRadius, which stopped being the damage shape when the
	 *  socket sweep landed — and a 105 stand-off in front of a 91cm punch is exactly how the heavy attack
	 *  came to be un-landable. Re-measure before raising it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "40", ForceUnits = "cm"))
	float WarpApproachDistance = 100.f;

	/** Slack below which a target is "already close enough" and no warp target is registered at all.
	 *  Without it, a victim standing nearer than WarpApproachDistance puts the warp point BEHIND the
	 *  attacker and translation warping walks them backwards mid-punch. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Warp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0", ForceUnits = "cm"))
	float WarpMinSeparationDeadband = 15.f;

	/** Nearest hostile in front within WarpSearchDistance, or null. Mirrors the hit sweep's filters so the
	 *  thing we lunge at is the thing the damage window would hit. */
	AActor* FindWarpTarget() const;

	// GEs applied to the owner on each activation — e.g. GE_CombatReady to REFRESH the fists-up stance every punch.
	// The GE owns its own duration + refresh-on-reapply stacking; this just re-applies it. Set in the BP ability.
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee") TArray<TSubclassOf<UGameplayEffect>> EffectsOnActivate;

	// --- Runtime state, latched at activation ---
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
	/** Live socket sweep between WindowBegin/WindowEnd; null outside the strike phase. */
	UPROPERTY() class UAZ_AT_MeleeSweep* SweepTask = nullptr;
	/** Generation of THIS activation's DriveRootMotion — EndAbility releases via ReleaseRootMotion so an
	 *  end firing after a reaction (flinch/step-back) has taken over cannot kill the newer move. */
	uint64 RootMotionGen = 0;
	/** Guard for the notify-driven beat (BeatEnd + margin): ends the ability if the event never arrived.
	 *  Member so a retriggered attack RESETS it. Cleared in EndAbility. */
	FTimerHandle BeatWatchdog;
	UPROPERTY(EditDefaultsOnly) EAZ_MeleeHand Hand = EAZ_MeleeHand::Left;
	bool bIsMovingLatched = false;
	/** RECOVERY LATCH. True between the clip's Event.Combat.CancelOpen/CancelClose notify state — the
	 *  phase where startup and the strike are done and the attack may be abandoned. Mirrored onto the ASC
	 *  as State.Combat.CancelWindow so the input layer and movement-intent layer can read it without
	 *  knowing this ability exists. MUST be reset in both ActivateAbility and EndAbility: the instance is
	 *  reused under retrigger, and a stale true would make the next swing cancellable from frame 0. */
	bool bCancellable = false;
	/** THIS activation's montage. Window/cancel events carry their source montage in the payload, and
	 *  anything from a different one is discarded: a cancelled swing's notify states fire their NotifyEnd
	 *  a frame or more AFTER the replacement swing has latched its own state, and an unfiltered
	 *  CancelClose from the outgoing clip would commit the incoming one for its entire montage. */
	TWeakObjectPtr<UAnimMontage> ActiveMontage;
	FGameplayTag ProfileTag;          // current Weapon.* — used by SelectMontage when CHT lands
	int32 ComboIndex = 0;             // later
	
};
