
#pragma once

#include "CoreMinimal.h"
#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "AZ_CmcInfectedCharacter.generated.h"

class UAZ_AbilitySystemComponent;
class UAZ_VitalsAttributeSet;
class UDataAsset;
class UGameplayAbility;

UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_CmcInfectedCharacter : public AAZ_CmcCharacterBase
{
	GENERATED_BODY()

public:
	AAZ_CmcInfectedCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void SetGrabTarget(AActor* InTarget) override { GrabTarget = InTarget; }
	virtual AActor* GetGrabTarget() const override { return GrabTarget.Get(); }
	virtual void SetStaggeredFor(float Seconds) override;
	virtual bool IsStaggerReactionPlaying() const override;
	virtual void BeginCorpse(float RagdollDelay) override;

	void RagdollCorpse();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Anim")
	TObjectPtr<UDataAsset> AnimSet;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|GAS")
	TObjectPtr<UAZ_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAZ_VitalsAttributeSet> VitalsAttributeSet;

	bool bStartupAbilitiesGranted = false;

	TWeakObjectPtr<AActor> GrabTarget;

	FTimerHandle StaggerHoldTimer;
	double StaggerHoldEndTime = 0.0;

	bool bCorpse = false;
	FTimerHandle RagdollTimer;
};
