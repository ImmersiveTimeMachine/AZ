#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Items/AZ_Inv_InventoryItem.h"
#include "AZ_PlayerController.generated.h"

class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_InventoryComponent;
class UAZ_InventoryHudWidget;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent;
struct FGameplayTag;
class UAZ_InputConfig;


UCLASS()
class AZ_API AAZ_PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	class UInputAction* OpenInventoryAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Character|Input")
	TObjectPtr<UAZ_InputConfig> InputConfig;

	/** Returns the possessed pawn's ASC (base type). Callers that need AZ-typed
	 *  behavior do an inline Cast<UAZ_AbilitySystemComponent>(...) at the call site. */
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	/** Cross-pawn, persistent IMC pushed at BeginPlay. Holds the input surface that's
	 *  active regardless of the possessed pawn (pause, inventory toggle, menu nav,
	 *  photo mode, scoreboard). Per-pawn IAs live on the pawn's DefaultMappingContext. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Character|Input")
	TObjectPtr<UInputMappingContext> SharedInputMappingContext;

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
	virtual void OnUnPossess() override;
	/** Client-side counterpart of OnPossess — fires when the PC acknowledges a
	 *  server-replicated pawn possession. Required because OnPossess is server-only;
	 *  without this override the client never pushes the pawn's IMC and per-pawn
	 *  IAs (Move/Look on the v2 pawn) don't trigger on remote clients. */
	virtual void AcknowledgePossession(APawn* P) override;

	virtual void BeginPlay() override;

	/** Pushes the pawn's DefaultMappingContext at priority 2. Called from both
	 *  OnPossess (server) and AcknowledgePossession (client). */
	void PushPawnInputMappingContext(APawn* InPawn);
	/** Pops the pawn's DefaultMappingContext. Called from OnUnPossess. */
	void RemovePawnInputMappingContext(APawn* InPawn);

	void CreateHUDWidget();

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
