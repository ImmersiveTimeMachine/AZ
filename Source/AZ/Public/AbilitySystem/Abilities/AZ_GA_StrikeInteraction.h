// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AZ_GA_StrikeInteraction.generated.h"

class UPoseSearchDatabase;

/**
 * One way this key can strike: a pair database (one PSIA per victim variant) and OUR half of that pair.
 * A key with several variants (the heavy: punch or kick) shuffles them by Weight on every press and takes
 * the first whose alignment is valid — random where more than one fits the current gap, the right one
 * where only one does (the kick pairs from ~180-300cm, the punch from ~125-245). Two PSIAs in ONE database
 * would be picked by search cost instead: the same arrangement, the same strike, every time.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_StrikeVariant
{
	GENERATED_BODY()

	/** The pair database this variant searches (PSD_AZ_Strike, PSD_AZ_Strike_Kick, ...). */
	UPROPERTY(EditAnywhere, Category = "AZ|Strike")
	TObjectPtr<UPoseSearchDatabase> Database;

	/** Our half — must be the PSIA's Attacker item (validated per search). */
	UPROPERTY(EditAnywhere, Category = "AZ|Strike")
	TObjectPtr<UAnimMontage> Montage;

	/** Sockets the hit window sweeps for THIS clip (a kick: foot_r, ball_r). Empty = the ability's fists.
	 *  The first entry is also the contact-probe limb. */
	UPROPERTY(EditAnywhere, Category = "AZ|Strike")
	TArray<FName> StrikeSockets;

	/** Relative pick weight when more than one variant fits. */
	UPROPERTY(EditAnywhere, Category = "AZ|Strike", meta = (ClampMin = "0"))
	float Weight = 1.f;

	bool IsSet() const { return Database != nullptr && Montage != nullptr; }
};

/** Everything one successful strike search resolves — handed from TryStrikeSearch to PlayPairedStrike. */
struct FAZ_StrikePair
{
	/** Our half (the variant's montage) and the sockets its hit window sweeps. */
	UAnimMontage* HeroMontage = nullptr;
	TArray<FName> StrikeSockets;
	FName ProbeSocket;
	UAnimMontage* VictimMontage = nullptr;
	/** Entry frame on the SHARED timeline (both halves start here). */
	float StartTime = 0.f;
	/** Rate the search wanted — logged only; both halves play at 1 so the authored contact frame holds. */
	float WantedPlayRate = 1.f;
	/** Contact = the start of the victim montage's React section. */
	float ContactTime = 0.f;
	/** How long the close-in has: entry -> contact. */
	float CloseSeconds = 0.f;
	/** Planar displacement the victim must cover by contact, and the facing it must hold there. */
	FVector VictimDisplacement = FVector::ZeroVector;
	FVector VictimFacing = FVector::ZeroVector;
};

/**
 * THE HEAVY STRIKE AS A PAIRED ANIMATION (2026-09-03; plan: memory project_psia_heavy_strike_plan).
 *
 * A melee attack on the fist rail — UAZ_GA_MeleeAttack's input rail, chain gate, hit sweep, damage,
 * hit-stop, cancel window and root-motion drive are all inherited — whose ALIGNMENT comes from a
 * PoseSearch Interaction pair instead of motion warping. One MotionMatchMulti over StrikeDatabase with
 * (self = Attacker, the Chalkie in front = Victim) picks the entry frame and where the VICTIM must stand;
 * the hero anchors (attacker item weights 1/1 in the PSIA), so the player is never moved and the Chalkie
 * is placed on his forward axis. Both halves then play ONE timeline: our trimmed heavy (StrikeMontage,
 * contact = the right hook at 0.50) and the victim's [walk-in | knockback] montage (GA_HitReact,
 * Event.Strike.Victim), so the fist meets the chest on the authored frame and the reaction is the
 * authored knockback — chosen BEFORE contact by the Chalkie's arrangement, not by a cost contest after it.
 *
 * Order of operations is the catch's (project_grab_grapple_design, 2026-09-02): ROOT THE VICTIM FIRST
 * (zero-velocity override), search only once it is still (next-tick retries), close it in with a layered
 * velocity move that ends AT CONTACT (the React section start), start both montages the same frame.
 * No motion warping on the pair: the PSIA owns the geometry, and two writers is how the catch went wrong.
 *
 * Anything short of a valid pair falls back to the parent's warped heavy (Super::ActivateAbility) and
 * logs "[Strike] FALLBACK reason=..." — the key must always throw SOMETHING. Set every PunchIdle/Move/
 * Lunge slot on the BP child to the warped heavy so the fallback is the heavy at any range.
 */
UCLASS()
class AZ_API UAZ_GA_StrikeInteraction : public UAZ_GA_MeleeAttack
{
	GENERATED_BODY()

public:
	UAZ_GA_StrikeInteraction();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The strike interaction database (PSD_AZ_Strike: one PSIA per victim variant, SamplingRange-trimmed
	 *  to the wind-up). Editor-assigned on the GRANTED class (no /Game/ paths in C++; BP-child CDOs don't
	 *  inherit runtime patches). Null = always the warped heavy. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI")
	TObjectPtr<UPoseSearchDatabase> StrikeDatabase;

	/** Our half: the TRIMMED heavy (AM_Fists_Punch_Heavy_Strike — Heavy2Idle [0, 0.9], right hook lands
	 *  at 0.50). Must be the PSIA's Attacker item; validated on every search. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI")
	TObjectPtr<UAnimMontage> StrikeMontage;

	/** MORE THAN ONE WAY TO STRIKE (the heavy: punch or kick). When set, these replace the single
	 *  StrikeDatabase/StrikeMontage pair above: shuffled by Weight on every press, first valid alignment
	 *  wins, warped fallback only when none fits. Leave empty for a single-variant key (the jabs). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI")
	TArray<FAZ_StrikeVariant> StrikeVariants;

	/** Reject an entry frame past this (s): the pair must start inside the wind-up. 0.20 = the DB entry's
	 *  SamplingRange (0.15) + indexing quantization slack; anything above means the trim was lost. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI", meta = (ClampMin = "0", ForceUnits = "s"))
	float StrikeEntryMaxTime = 0.2f;

	/** Farthest the close-in may move the victim (cm). Past it the pair reads as a teleport, so the warped
	 *  heavy plays instead. With the F_5 pair authored at ~184cm root-to-root at entry, this makes the
	 *  strike live from ~125 to ~245cm. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxCloseInDistance = 60.f;

	/** Section on the VICTIM montage whose start is the contact frame: the close-in ends there and the
	 *  victim's root-motion drive begins there (GA_HitReact::StrikePairReactSection — same name). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI")
	FName ReactSectionName = TEXT("React");

	/** Victim chest bone for the contact probe log ("[Strike] contact fist->chest"). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Strike|PSI")
	FName VictimChestSocket = TEXT("spine_03");

private:
	/** Phase 2: re-checks the victim's planar speed each sim tick (<= 4) after the rooting move, then
	 *  BeginStrike — the search aligns the pair to where the victim IS, and a walking Chalkie moved
	 *  20-30cm between a synchronous search and its rooting reaching the sim on the catch. */
	void TryBeginStrikeWhenStill(int32 Attempt);
	/** The search, then either the pair or the fallback. */
	void BeginStrike();
	/** THE driver — the only function touching the Experimental PoseSearch interaction API. Any invalid
	 *  result logs "[Strike] search FALLBACK reason=..." and returns false. */
	bool TryStrikeSearch(AActor* Target, const FAZ_StrikeVariant& Variant, FAZ_StrikePair& Out);
	/** The variants to try this press, in weighted-random order (see FAZ_StrikeVariant). */
	TArray<FAZ_StrikeVariant> ShuffledVariants() const;
	/** The parent's fallback (no pair) throws the SAME thing the pair would have — this press's drawn
	 *  variant — so a kick is a kick whether or not a Chalkie is there, at the same odds. */
	virtual UAnimMontage* SelectMontage() const override;
	/** The active variant's sockets while a pair plays, else the fists. */
	virtual TArray<FName> GetStrikeSockets() const override;
	/** Close-in on the victim, our montage at the entry frame, the victim's half by event, probes. */
	void PlayPairedStrike(const FAZ_StrikePair& Pair);
	/** The parent's warped heavy, from wherever the pair path gave up. */
	void FallbackToWarpedHeavy(const TCHAR* Reason);

	TWeakObjectPtr<AActor> StrikeTarget;
	/** This press's variants, in the order BeginStrike tries them. */
	TArray<FAZ_StrikeVariant> PendingVariants;
	/** Sockets of the pair being played (empty outside a pair -> the parent's fists). */
	TArray<FName> ActiveStrikeSockets;
	/** The victim took the pair (State.Combat.StruckPair went up on its ASC). */
	bool bPairLive = false;
	/** Set by the contact probe: from here the hit is real and an interrupted swing keeps the reaction. */
	bool bContactReached = false;
	float PairContactTime = 0.f;
	FTimerHandle ProbeMidTimer;
	FTimerHandle ContactProbeTimer;
};
