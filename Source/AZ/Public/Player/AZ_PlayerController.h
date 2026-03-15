#pragma once

#include "CoreMinimal.h"
#include "Character/AZ_CharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Items/AZ_Inv_InventoryItem.h"
#include "AZ_PlayerController.generated.h"

class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_InventoryComponent;
class UAZ_InventoryHudWidget;
class UInputAction;
class UAbilitySystemComponent;
class UInputMappingContext;
struct FInputActionValue;
struct FInputActionInstance;
struct FGameplayTag;
class UAZ_AbilitySystemComponent;
class UAZ_InputConfig;


UCLASS()
class AZ_API AAZ_PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	UInputAction* MoveAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	class UInputAction* OpenInventoryAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	TObjectPtr<UAZ_InputConfig> InputConfig;
		
	TObjectPtr<UAbilitySystemComponent> GetAbilitySystemComponent() const;
	TObjectPtr<UAZ_AbilitySystemComponent> GetAzAbilitySystemComponent() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Character|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Character|Input")
	TObjectPtr<UInputMappingContext> AlwaysInputMappingContext;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Character|Movement")
	float RunSpeedMultiplayer { 2.0f };

	virtual void SetupInputComponent() override;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY()
	TObjectPtr<UAZ_InventoryHudWidget> HUDWidget;

	UFUNCTION(BlueprintCallable, Category = "AZ|Character|Input")
	void ToggleInventoryMenu();
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Character|Input")
	void ToggleCommonUI_InventoryMenu();

	// Add setter methods
	UFUNCTION()
	FORCEINLINE void SetActivePickUpActor(AActor* NewActor) { Swap(LastActivePickupActor,ActivePickupActor); ActivePickupActor = NewActor; }

protected:
	
	virtual void OnPossess(APawn* aPawn) override;
	
	virtual void BeginPlay() override;

	/** Called for movement input */
	void Move(const FInputActionInstance& Value);
	void Run(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void CreateHUDWidget();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Character|Movement|State")
	TEnumAsByte<ECharacterState> CharacterState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|HUD")
	TSubclassOf<UAZ_InventoryHudWidget> InventoryHudWidgetClass;

	UFUNCTION()
	void HandlePickupPromptToggled(bool bVisible);

	UPROPERTY()
	FString PickupMessage{TEXT("Press E to PickUp")};

public:

	UFUNCTION(BlueprintCallable)
	void PrimaryInteract();

private:

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	TWeakObjectPtr<UAZ_Inv_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> CommonUI_InventoryComponent;

	TWeakObjectPtr<AActor> ActivePickupActor;
	TWeakObjectPtr<AActor> LastActivePickupActor;
	
};
