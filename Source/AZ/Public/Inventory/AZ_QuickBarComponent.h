#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "AZ_QuickBarComponent.generated.h"

class UAZ_GameplayAbility;
class UGameplayEffect;
class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;

/** Intrinsic profiles keep their authored data; physical slots bind an owned item identity. */
USTRUCT(BlueprintType)
struct FAZ_QuickSlot
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") FGameplayTag WeaponTag;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<TSubclassOf<UAZ_GameplayAbility>> WeaponAbilities;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") bool bStrafeOnEquip = false;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<TSubclassOf<UGameplayEffect>> EffectsOnEquip;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") bool bInventoryBacked = false;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar", meta=(EditCondition="bInventoryBacked")) FGameplayTag InventoryItemType;
};

/** Selection UI only. The equipment component owns all grants and weapon transitions. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuickBarComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuickBarComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void Select(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CycleNext();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CyclePrev();
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") int32 GetActiveSlotIndex() const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") UAZ_Inv_CommonUI_InventoryItem* GetBoundItem(int32 SlotIndex) const;
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") bool BindItemToSlot(int32 SlotIndex, UAZ_Inv_CommonUI_InventoryItem* Item);
	const FAZ_QuickSlot* GetSlotDefinition(int32 SlotIndex) const;
	void BindSelectedItem(UAZ_Inv_CommonUI_InventoryItem* Item);

protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<FAZ_QuickSlot> Slots;

private:
	UPROPERTY(Replicated) TArray<FGuid> SlotItemIds;
	UAZ_Inv_CommonUI_InventoryComponent* GetInventory() const;
	UAZ_Inv_CommonUI_EquipmentComponent* GetEquipment() const;
	bool CanBindItem(int32 SlotIndex, const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	void SelectInternal(int32 SlotIndex);
	void Cycle(int32 Direction);
	UFUNCTION() void OnInventoryChanged();
	UFUNCTION(Server, Reliable) void Server_Select(int32 SlotIndex);
	UFUNCTION(Server, Reliable) void Server_BindItem(int32 SlotIndex, FGuid ItemId);
};
