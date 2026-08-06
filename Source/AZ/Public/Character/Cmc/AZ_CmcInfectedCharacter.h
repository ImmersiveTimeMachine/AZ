// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "AZ_CmcInfectedCharacter.generated.h"

class UAZ_AbilitySystemComponent;
class UAZ_VitalsAttributeSet;
class UDataAsset;
class UGameplayAbility;

/**
 * AAZ_CmcInfectedCharacter — the CMC (v3) Chalkie. [SPIKE: spike/cmc-backport]
 *
 * From-scratch ACharacter NPC, inspired by v2 AZ_PawnMoverInfectedCharacter. Standalone sibling of the
 * CMC hero (shared base, no camera/input): owns its ASC (NPCs have no PlayerState), reuses the SAME
 * AAZ_InfectedAIController + BB/BT — path-following is standard CMC MoveTo here, no NavMover bridge,
 * and the RM-lite curve-follow system has no reason to exist (montage RM is native; locomotion speed
 * is the gait's MaxWalkSpeed, one owner: SetGait).
 *
 * AnimSet: same per-instance DataAsset slot the v2 pawn exposes on its BP child — declared NATIVELY here
 * with the EXACT name "AnimSet" (doctrine rule 5: UAZ_InfectedAnimInstance and FindAnimSetMontage resolve
 * it by reflection name; native beats a BP-child re-declaration and is a step toward task #9).
 *
 * P0 scope: spawnable, AI-possessed, animated (UAZ_InfectedAnimInstance CMC branch), corpse-able. Combat
 * grants beyond Death/HitReact, vitals delegate wiring and BT branches land in P3/P4.
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_CmcInfectedCharacter : public AAZ_CmcCharacterBase
{
	GENERATED_BODY()

public:
	AAZ_CmcInfectedCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;

	// ========================================
	// IAbilitySystemInterface — this pawn owns its ASC.
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========================================
	// IAZ_CombatAvatar — grabber-side seams + reactions + corpse.
	// ========================================
	virtual void SetGrabTarget(AActor* InTarget) override { GrabTarget = InTarget; }
	virtual AActor* GetGrabTarget() const override { return GrabTarget.Get(); }
	virtual void SetStaggeredFor(float Seconds) override;
	virtual bool IsStaggerReactionPlaying() const override;
	virtual void BeginCorpse(float RagdollDelay) override;

	/** Montage->ragdoll hand-off at the fall's impact beat (physics settles the corpse against geometry). */
	void RagdollCorpse();

	// ========================================
	// Anim
	// ========================================

	/** Per-variant anim set (DA_ChalkieAnims_*), EDITOR-ASSIGNED per BP child / level instance. Name is
	 *  an ABI: UAZ_InfectedAnimInstance + FindAnimSetMontage read the property "AnimSet" by reflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Anim")
	TObjectPtr<UDataAsset> AnimSet;

	// ========================================
	// Startup abilities — EDITOR-ASSIGNED BP tuning children; native classes are the fallback.
	// P0 grants Death + HitReact (the proven Configure* patterns); Melee/Grab grants land in P3 with
	// the BT branches that drive them.
	// ========================================
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> DeathAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> MeleeAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> GrabAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> HitReactAbilityClass;

protected:
	void InitAbilitySystem();

	// ========================================
	// GAS — own ASC (NPC pattern).
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|GAS")
	TObjectPtr<UAZ_AbilitySystemComponent> AbilitySystemComponent;

	/** Shared combat vitals (S1 damage spine) — owner-subobject, auto-registered with the ASC. */
	UPROPERTY()
	TObjectPtr<UAZ_VitalsAttributeSet> VitalsAttributeSet;

	bool bStartupAbilitiesGranted = false;

	/** Prey held by the current grab. Weak: the victim can die mid-hold; the anim layer must simply
	 *  stop reaching rather than chase a dangling pointer. */
	TWeakObjectPtr<AActor> GrabTarget;

	// ---- Stagger hold (SetStaggeredFor): ONE shared deadline, last-writer-wins, only ever extends —
	// the counted-tag-trap guard the v2 infected settled on. ----
	FTimerHandle StaggerHoldTimer;
	double StaggerHoldEndTime = 0.0;

	/** Corpse latch (BeginCorpse is idempotent). */
	bool bCorpse = false;
	FTimerHandle RagdollTimer;
};
