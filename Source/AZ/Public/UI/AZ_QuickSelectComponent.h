#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/AZ_QuickSelectTypes.h"
#include "InputCoreTypes.h"
#include "AZ_QuickSelectComponent.generated.h"

class AAZ_PlayerController;
class APawn;
class UAZ_QuickSelectWidget;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;
class UAbilitySystemComponent;
class UInputAction;
struct FOnAttributeChangeData;

/** Local UI session only. Assignment and activation use the existing authoritative owners. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuickSelectComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuickSelectComponent();
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect") TSubclassOf<UAZ_QuickSelectWidget> WidgetClass;
	UFUNCTION(BlueprintPure, Category="AZ|QuickSelect") bool IsOpen() const { return View.State != EAZ_QuickSelectState::Closed; }
	UFUNCTION(BlueprintPure, Category="AZ|QuickSelect") FAZ_QuickSelectView GetView() const { return View; }
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void Toggle();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void Close(bool bRestoreGameplayInput = true);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void Cancel();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void HoverSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void ToggleAssignment();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void CycleCandidate(int32 Direction);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void ActivateHovered();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickSelect") void ActivateSlot(int32 SlotIndex);
	bool IsToggleKey(FKey Key) const;
	bool IsInventoryKey(FKey Key) const;
	int32 FindSlotForKey(FKey Key) const;
	/** Called by existing controller lifecycle/tick; checks pause only, never polls inventory. */
	void RefreshBindings();
	void CheckPause();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient) TObjectPtr<UAZ_QuickSelectWidget> Widget;
	UPROPERTY(Transient) FAZ_QuickSelectView View;
	TWeakObjectPtr<AAZ_PlayerController> Controller;
	TWeakObjectPtr<UAZ_QuickBarComponent> QuickBar;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> Inventory;
	TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> Equipment;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TMap<FGameplayTag, FDelegateHandle> GateHandles;
	FDelegateHandle HealthHandle;
	FDelegateHandle ApplicationActivationHandle;
	TArray<FGuid> Candidates;
	FGuid CandidateId;
	FGuid AssignmentRequestId;
	FGuid ActivationRequestId;
	int64 EditingRevision = 0;
	int64 AcknowledgedRevision = INDEX_NONE;
	bool bEndingPlay = false;
	bool bPreviousCursorVisible = false;
	bool bCloseAfterActivation = false;
	bool CanUseSelector() const;
	void Open();
	void Unbind();
	void RebuildView();
	void RebuildCandidates(bool bChooseInitial);
	void FinishAssignment();
	void ReportStatus(const FText& Text);
	TArray<FKey> KeysForAction(const UInputAction* Action) const;
	FText ShortcutText(int32 SlotIndex) const;
	void HandleGate(FGameplayTag Tag, int32 Count);
	void HandleHealth(const FOnAttributeChangeData& Change);
	void HandleApplicationActivation(bool bActive);
	UFUNCTION() void HandleSourcesChanged();
	UFUNCTION() void HandleRequestResult(const FAZ_QuickBarRequestResult& Result);
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
};
