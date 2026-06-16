// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "AZ_QuickBarComponent.generated.h"

struct FGameplayAbilitySpecHandle;
class UAZ_AbilitySystemComponent;
class UAZ_GameplayAbility;
// One quick-slot: a profile tag + the abilities that profile grants.
  // Phase 1 inline data; mirrors what AbilityGrantFragment carries in the real path.
  USTRUCT(BlueprintType)
struct FAZ_QuickSlot
  {
  	GENERATED_BODY()
  	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") FGameplayTag WeaponTag;            // Weapon.Fist, ...
  	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<TSubclassOf<UAZ_GameplayAbility>> WeaponAbilities; //{BP_GA_Punch_L, BP_GA_Punch_R}
  };


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AZ_API UAZ_QuickBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAZ_QuickBarComponent();
	
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void Select(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CycleNext();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CyclePrev();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<FAZ_QuickSlot> Slots;
	int32 ActiveSlotIndex = -1;                 // -1 = empty hands (Weapon.None)

	UAZ_AbilitySystemComponent* GetASC() const;
	void EquipSlot(int32 SlotIndex);
	void UnequipActive();

	UPROPERTY() TArray<FGameplayAbilitySpecHandle> GrantedHandles;   // so we clear only OUR abilities

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	
};
