#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "Items/AZ_Inv_InventoryItem.h"
#include "Weapon/AZ_WeaponTypes.h"
#include "AZ_PlayerController.generated.h"

class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_InventoryComponent;
class UAZ_Inv_CommonUI_InventoryHudWidget;
class UAZ_PlayerUIComponent;
class UAZ_QuickSelectComponent;
class AAZ_Weapon;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent;
struct FGameplayTag;
class UAZ_InputConfig;
class UAZ_QuickBarComponent;
struct FInputActionInstance;


UCLASS()
class AZ_API AAZ_PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	AAZ_PlayerController();

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

	/** Quick-slot select inputs (AZ_IA_Weapon_0/1/...). Array index = quick-bar slot.
	 *  Native (non-ability) input bound directly to the QuickBar, same lane as Move/Look. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Character|Input")
	TArray<TObjectPtr<const UInputAction>> WeaponSlotActions;

	/** Native selector input; the equipment owner routes its fresh press to authority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Character|Input")
	TObjectPtr<const UInputAction> ChangeFireModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Character|Input")
	TObjectPtr<UInputAction> QuickSelectToggleAction;

	virtual void SetupInputComponent() override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateRotation(float DeltaTime) override;

	/** Authority receipt for one paid shot; only the owning player's control rotation is affected. */
	UFUNCTION(Client, Reliable)
	void Client_ApplyFirearmRecoil(AAZ_Weapon* Weapon, FGuid WeaponItemId, uint32 EquipmentGeneration,
		FGuid ShotId, double AcceptedShotServerTime, int32 AcceptedPreparationKey, const FAZ_FirearmRecoilSettings& Settings);

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_InventoryHudWidget> HUDWidget;

	UFUNCTION(BlueprintCallable, Category = "AZ|Character|Input")
	void ToggleInventoryMenu();
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Character|Input")
	void ToggleCommonUI_InventoryMenu();
	/** Compatibility guard used by firearm abilities: all interactive player UI blocks combat. */
	bool IsInventoryInputCaptured() const { return IsGameplayInputCaptured() || bQuickSelectMouseReleasePending; }
	bool IsGameplayInputCaptured() const { return bInventoryInputCaptured || bQuickSelectInputCaptured; }
	bool IsInventoryMenuOpen() const { return bInventoryInputCaptured; }
	void SetQuickSelectInputCaptured(bool bOpen);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void ToggleQuickSelect();

	// Add setter methods
	UFUNCTION()
	void SetActivePickUpActor(AActor* NewActor);
	void RefreshPickupTarget();

protected:

	virtual void OnPossess(APawn* aPawn) override;
	virtual void OnUnPossess() override;
	/** Client-side counterpart of OnPossess — fires when the PC acknowledges a
	 *  server-replicated pawn possession. Required because OnPossess is server-only;
	 *  without this override the client never pushes the pawn's IMC and per-pawn
	 *  IAs (Move/Look on the v2 pawn) don't trigger on remote clients. */
	virtual void AcknowledgePossession(APawn* P) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_PlayerState() override;

	/** Pushes the pawn's DefaultMappingContext at priority 2. Called from both
	 *  OnPossess (server) and AcknowledgePossession (client). */
	void PushPawnInputMappingContext(APawn* InPawn);
	/** Pops the pawn's DefaultMappingContext. Called from OnUnPossess. */
	void RemovePawnInputMappingContext(APawn* InPawn);

	void CreateHUDWidget();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|HUD")
	TSubclassOf<UAZ_Inv_CommonUI_InventoryHudWidget> InventoryHudWidgetClass;

	UFUNCTION()
	void HandlePickupPromptToggled(bool bVisible);

	UPROPERTY()
	FString PickupMessage{TEXT("Press E to PickUp")};

public:

	UFUNCTION(BlueprintCallable)
	void PrimaryInteract();

private:
	bool CanApplyFirearmRecoil(const AAZ_Weapon* Weapon, const FGuid& WeaponItemId, uint32 EquipmentGeneration) const;
	/** Discard pending kick/return debt without moving a camera now owned by another gameplay state. */
	void ClearFirearmRecoil();
	TWeakObjectPtr<AAZ_Weapon> RecoilWeapon;
	TWeakObjectPtr<APawn> RecoilPawn;
	FGuid RecoilItemId;
	uint32 RecoilEquipmentGeneration = 0;
	FAZ_FirearmRecoilSettings RecoilSettings;
	/** X = pitch, Y = yaw, in degrees. Only actually applied kick becomes return debt. */
	FVector2D PendingRecoil = FVector2D::ZeroVector;
	FVector2D RecoilReturnOffset = FVector2D::ZeroVector;
	double RecoilRecoveryStartTime = 0.0;
	/** Survives runtime cleanup so a repeated receipt cannot restart cancelled recoil. */
	TArray<FGuid> RecentRecoilShotIds;

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	/** Native quick-slot input -> QuickBar->Select(index of the firing action). */
	void OnQuickSlotInput(const FInputActionInstance& Instance);
	void OnChangeFireModeInput();
	UFUNCTION()
	void HandleInventoryMenuToggled(bool bOpen);
	UFUNCTION(Server, Reliable) void Server_SetInventoryInputCaptured(bool bInventoryOpen, bool bQuickSelectOpen);
	void ApplyGameplayInputCapture(bool bWasCaptured);
	bool CanUseInventoryInteraction() const;
	bool bInventoryInputCaptured = false;
	bool bQuickSelectInputCaptured = false;
	bool bQuickSelectMouseReleasePending = false;
	TSet<FGameplayTag> MenuSuppressedInputTags;

	/** Cross-pawn quick-bar (equip/loadout). Owned by the PC so it survives pawn swaps. */
	UPROPERTY(VisibleAnywhere, Category = "AZ|QuickBar")
	TObjectPtr<UAZ_QuickBarComponent> QuickBar;

	/** Shared local presentation bindings for the HUD and inventory panels. */
	UPROPERTY(VisibleAnywhere, Category="AZ|UI")
	TObjectPtr<UAZ_PlayerUIComponent> PlayerUI;

	UPROPERTY(VisibleAnywhere, Category="AZ|QuickSelect")
	TObjectPtr<UAZ_QuickSelectComponent> QuickSelect;

	TWeakObjectPtr<UAZ_Inv_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> CommonUI_InventoryComponent;

	TWeakObjectPtr<AActor> ActivePickupActor;
	TWeakObjectPtr<AActor> LastActivePickupActor;
	
};
