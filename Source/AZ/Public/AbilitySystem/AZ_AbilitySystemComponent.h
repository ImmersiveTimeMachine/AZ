#pragma once

#include <CoreMinimal.h>
#include <AbilitySystemComponent.h>

#include "Abilities/AZ_GameplayAbility.h"
#include "AbilityTasks/AZ_AT_PlayMontageForMeshAndWaitForEvent.h"
#include "AZ_AbilitySystemComponent.generated.h"

class UAZ_GameplayAbility;

USTRUCT()
struct AZ_API FGameplayAbilityLocalAnimMontageForMesh
{
	GENERATED_BODY();

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;
	
	UPROPERTY()
	FGameplayAbilityLocalAnimMontage LocalMontageInfo;

	FGameplayAbilityLocalAnimMontageForMesh() : Mesh(nullptr), LocalMontageInfo()
	{
	}

	explicit FGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh), LocalMontageInfo()
	{
	}

	FGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh, const FGameplayAbilityLocalAnimMontage& InLocalMontageInfo)
		: Mesh(InMesh), LocalMontageInfo(InLocalMontageInfo)
	{
	}
};

/**
* Data about montages that is replicated to simulated clients.
*/
USTRUCT()
struct AZ_API FGameplayAbilityRepAnimMontageForMesh
{
	GENERATED_BODY();

public:
	
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	FGameplayAbilityRepAnimMontage RepMontageInfo;

	FGameplayAbilityRepAnimMontageForMesh() : Mesh(nullptr), RepMontageInfo()
	{
	}

	explicit FGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh), RepMontageInfo()
	{
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/);
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);

UCLASS()
class AZ_API UAZ_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	void AbilityActorInfoSet();
	void AddReplicatedLooseGameplayTag(FGameplayTag GameplayTag);

	FEffectAssetTags EffectAssetTags;
	FAbilitiesGiven AbilitiesGivenDelegate;

	void GrantAbilitiesWithInputTag(const TArray<TSubclassOf<UAZ_GameplayAbility>>& Abilities);

	// ========================================
	// HIT-STOP — the frame-of-contact acknowledgment
	// ========================================
	/**
	 * Briefly collapse this actor's montage play rate, then restore it. Applied to BOTH sides of a
	 * landed hit (attacker slightly shorter than victim, the fighting-game asymmetry that hands the
	 * attacker the advantage back first).
	 *
	 * WHY THIS IS THE FIX, and not more distance tuning: measured, the fist ends ~18cm inside the
	 * victim's 30cm capsule at the current stand-off — which is roughly what shipped melee actually
	 * authors, because the capsule is not the body (the chest surface sits ~18-22cm from centre) and a
	 * fist stopping at the capsule reads as a whiff. The penetration is not the defect. The defect is
	 * that for ~300ms after contact NOTHING on screen changes: the knockback clips peak at 1.34-1.81s
	 * and open at 8-14cm/s, so the victim is still standing there while the follow-through drives
	 * through them. A stop at the contact frame is what the eye reads as impact.
	 *
	 * MOVER NOTE — why rate and not pause or time dilation: root motion reaches the capsule as a
	 * PER-FRAME DELTA (RootMotionFromEverything -> attribute -> FLayeredMove_RootMotionAttribute), not a
	 * time-indexed track. At rate ~0 the clip emits ~zero delta, so the capsule stops by construction
	 * and resumes with no accumulated catch-up to lurch through. Actor CustomTimeDilation would NOT
	 * work: the Mover sim ticks on world time, so the mesh would slow while the capsule kept moving.
	 *
	 * Refreshes rather than stacks — two hits in one frame (a pack landing together) must not multiply
	 * the stop or fight over which one restores the rate.
	 *
	 * @param Seconds     Real-time hold. Kept well under the 0.5s margin on the combat watchdogs so a
	 *                    stop can never outlive the beat it belongs to; clamped to HitStopMaxSeconds.
	 * @param RateDuring  Play rate held during the stop. 0 would halt notify evaluation entirely.
	 */
	void ApplyHitStop(float Seconds, float RateDuring = 0.05f);

	/** Total real seconds this ASC has spent in hit-stop. Combat clocks that are wall-clock (watchdogs,
	 *  cut timers) can add the delta across their own lifetime to stay honest against a slowed montage. */
	double GetTotalHitStopSeconds() const { return TotalHitStopSeconds; }

	bool bStartupAbilitiesGiven = false;
	bool bCharacterAbilitiesGiven = false;
	bool bStartupEffectsApplied = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	/** Returns true if any spec carrying InputTag activated. bBufferIfRefused=false marks the call as the
	 *  buffer's own replay, so a second refusal cannot re-latch and hold the press forever. */
	bool AbilityInputTagHeld(const FGameplayTag& InputTag, bool bBufferIfRefused = true);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ForEachAbility(const FForEachAbility& Delegate);

	/**
	 * INPUT BUFFER for attack presses. Designed alongside the melee recovery gate (see
	 * UAZ_GA_MeleeAttack's constructor comment), built 2026-09-02 after measuring that a click landing in
	 * the committed part of a swing was simply discarded — "I clicked and nothing happened".
	 *
	 * A press whose matching spec carries Ability.Combat.Melee and was REFUSED (mid-commitment, or the other
	 * hand committed) is remembered for this long and replayed the moment the live attack opens its cancel
	 * window (State.Combat.CancelWindow rising) or any ability ends. Only melee presses are buffered, so a
	 * jump or interact pressed mid-swing never fires half a second late. Replays are deferred a tick: both
	 * triggers fire from inside ability code (a notify handler, EndAbility), and re-entering
	 * TryActivateAbility there is how retrigger loops start. 0 disables the buffer.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Input", meta = (ClampMin = "0", ForceUnits = "s"))
	float InputBufferSeconds = 0.5f;

	void OnWeaponEquipped(const FGameplayTag& NewWeaponTag);
	void OnMovementStateChanged(const FGameplayTag& NewMovementStateTag);

	/** State-tag set/clear that REPLICATES: applies the loose tag locally (caller's own queries), and on
	 *  the authority also records it in the replicated loose-tag count map so remote views and late-joiners
	 *  receive it — plain loose tags never replicate (audit P1-12). Member functions because the 5.8 map
	 *  accessor (GetReplicatedLooseTags_Mutable) is protected on UAbilitySystemComponent. Public so equip
	 *  flows (QuickBar) can set orthogonal state tags like Movement.Strafe directly. */
	void AddStateTag(const FGameplayTag& Tag);
	void RemoveStateTag(const FGameplayTag& Tag);

protected:

	/*UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);*/

	FGameplayTag CurrentWeaponTag;
	FGameplayTag CurrentMovementStateTag;

	// --- input buffer state (see InputBufferSeconds) ---
	FGameplayTag BufferedInputTag;
	double BufferedInputTime = -1.0;
	void LatchBufferedInput(const FGameplayTag& InputTag);
	void OnCancelWindowTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnAnyAbilityEnded(const FAbilityEndedData& EndedData);
	void ScheduleBufferedInputReplay();
	void ReplayBufferedInput();

public:

	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support for multiple USkeletalMeshComponents on the AvatarActor.
	//  Only one ability can be animating at a time though?
	// ----------------------------------------------------------------------------------------------------------------	

	// Plays a montage and handles replication and prediction based on passed in ability/activation info
	virtual float PlayMontageForMesh(UGameplayAbility* AnimatingAbility, class USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, bool bReplicateMontage = true);

	// Plays a montage without updating replication/prediction structures. Used by simulated proxies when replication tells them to play a montage.
	virtual float PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	// Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime = -1.0f);

	// Stops all montages currently playing
	virtual void StopAllCurrentMontages(float OverrideBlendOutTime = -1.0f);

	// Stops current montage if it's the one given as the Montage param
	virtual void StopMontageIfCurrentForMesh(USkeletalMeshComponent* InMesh, const UAnimMontage& Montage, float OverrideBlendOutTime = -1.0f);

	// Clear the animating ability that is passed in, if it's still currently animating
	virtual void ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability);

	// Jumps current montage to given section. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName);

	// Sets current montages next section name. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check)
	virtual void CurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName);

	// Sets current montage's play rate
	virtual void CurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, float InPlayRate);

	// Returns true if the passed in ability is the current animating ability
	bool IsAnimatingAbilityForAnyMesh(const UGameplayAbility* InAbility) const;

	// Returns the current animating ability
	UGameplayAbility* GetAnimatingAbilityFromAnyMesh();

	// Returns montages that are currently playing
	TArray<UAnimMontage*> GetCurrentMontages() const;

	// Returns the montage that is playing for the mesh
	UAnimMontage* GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh);

	// Get SectionID of currently playing AnimMontage
	int32 GetCurrentMontageSectionIDForMesh(USkeletalMeshComponent* InMesh);

	// Get SectionName of currently playing AnimMontage
	FName GetCurrentMontageSectionNameForMesh(USkeletalMeshComponent* InMesh);

	// Get length in time of current section
	float GetCurrentMontageSectionLengthForMesh(USkeletalMeshComponent* InMesh);

	// Returns amount of time left in current section
	float GetCurrentMontageSectionTimeLeftForMesh(USkeletalMeshComponent* InMesh);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|CharacterAbilities")
	FGameplayAbilitySpecHandle FindAbilitySpecHandleForClass(TSubclassOf<UGameplayAbility> AbilityClass, UObject* OptionalSourceObject=nullptr);

	// Attempts to activate the given ability handle and batch all RPCs into one. This will only batch all RPCs that happen
	// in one frame. Best case scenario we batch ActivateAbility, SendTargetData, and EndAbility into one RPC instead of three.
	// Worst case we batch ActivateAbility and SendTargetData into one RPC instead of two and call EndAbility later in a separate
	// RPC. If we can't batch SendTargetData or EndAbility with ActivateAbility because they don't happen in the same frame due to
	// latent ability tasks for example, then batching doesn't help and we should just activate normally.
	// Single shots (semi auto fire) combine ActivateAbility, SendTargetData, and EndAbility into one RPC instead of three.
	// Full auto shots combine ActivateAbility and SendTargetData into one RPC instead of two for the first bullet. Each subsequent
	// bullet is one RPC for SendTargetData. We then send one final RPC for the EndAbility when we're done firing.
	UFUNCTION(BlueprintCallable, Category = "AZ|CharacterAbilities")
	virtual bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately);


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// ---- Hit-stop state (see ApplyHitStop) ----
	/** Hard ceiling on a single stop. A stop that outlived its own beat would desync every wall-clock
	 *  combat timer (watchdogs, cut timers) from the montage timeline they guard. */
	static constexpr float HitStopMaxSeconds = 0.15f;
	FTimerHandle HitStopTimer;
	/** Rate to restore. Latched from the montage BEFORE the stop so a rate-scaled attack restores to its
	 *  own rate, not to a hardcoded 1.0. */
	float HitStopRestoreRate = 1.f;
	/** The slowed rate we imposed. Restore only happens if the montage still carries it — anything else
	 *  started at its own authored rate during the window and is not ours to retime. */
	float HitStopRateDuring = 0.05f;
	bool bHitStopActive = false;
	/** World time the live stop ends. Refreshes extend this; TotalHitStopSeconds accrues only the
	 *  extension past it, so overlapping stops are not double-counted. */
	double HitStopEndTime = 0.0;
	double TotalHitStopSeconds = 0.0;

	void EffectApplied(UAbilitySystemComponent* SourceAbilitySystemComponent, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	
	FGameplayAbilitySpecHandle AbilitySpecHandle;

	// Data structure for montages that were instigated locally (everything if server, predictive if client. replicated if simulated proxy)
	// Will be max one element per skeletal mesh on the AvatarActor
	UPROPERTY()
	TArray<FGameplayAbilityLocalAnimMontageForMesh> LocalAnimMontageInfoForMeshes;

	// Data structure for replicating montage info to simulated clients
	// Will be max one element per skeletal mesh on the AvatarActor
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAnimMontageForMesh)
	TArray<FGameplayAbilityRepAnimMontageForMesh> RepAnimMontageInfoForMeshes;

	// Finds the existing FGameplayAbilityLocalAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityLocalAnimMontageForMesh& GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh);

	// Finds the existing FGameplayAbilityRepAnimMontageForMesh for the mesh or creates one if it doesn't exist
	FGameplayAbilityRepAnimMontageForMesh& GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh);

	UFUNCTION()
	virtual void OnRep_ReplicatedAnimMontageForMesh();

	// Returns true if we are ready to handle replicated montage information
	virtual bool IsReadyForReplicatedMontageForMesh();

	// Called when a prediction key that played a montage is rejected
	void OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage);
	
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Copy LocalAnimMontageInfo into RepAnimMontageInfo
	void AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh);
	void AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	// Copy over playing flags for duplicate animation data
	void AnimMontage_UpdateForcedPlayFlagsForMesh(const FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	// RPC function called from CurrentMontageSetNextSectionName, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	void ServerCurrentMontageSetNextSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);
	bool ServerCurrentMontageSetNextSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);

	// RPC function called from CurrentMontageJumpToSection, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageJumpToSectionNameForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	void ServerCurrentMontageJumpToSectionNameForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);
	bool ServerCurrentMontageJumpToSectionNameForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, FName SectionName);

	// RPC function called from CurrentMontageSetPlayRate, replicates to other clients
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerCurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	void ServerCurrentMontageSetPlayRateForMesh_Implementation(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	bool ServerCurrentMontageSetPlayRateForMesh_Validate(USkeletalMeshComponent* InMesh, UAnimMontage* ClientAnimMontage, float InPlayRate);
	
	
};
