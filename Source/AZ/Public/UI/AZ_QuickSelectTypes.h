#pragma once

#include "CoreMinimal.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "AZ_QuickSelectTypes.generated.h"

class UTexture2D;
class UAZ_Inv_CommonUI_InventoryItem;

UENUM(BlueprintType)
enum class EAZ_QuickSelectState : uint8
{
	Closed,
	Browsing,
	EditingAssignment
};

/** A disposable presentation snapshot. Item identity and binding stay in inventory/QuickBar. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_QuickSelectEntryView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) EAZ_QuickSlotPosition Position = EAZ_QuickSlotPosition::Center;
	UPROPERTY(BlueprintReadOnly) int32 PositionOrdinal = 0;
	UPROPERTY(BlueprintReadOnly) FGuid ItemId;
	/** Read-only item source for the inventory composite name/icon leaves. */
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UAZ_Inv_CommonUI_InventoryItem> Item = nullptr;
	UPROPERTY(BlueprintReadOnly) FText SlotLabel;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(BlueprintReadOnly) FVector2D IconDimensions = FVector2D(140.f, 62.f);
	UPROPERTY(BlueprintReadOnly) FText AmmoText;
	UPROPERTY(BlueprintReadOnly) FText KeyText;
	UPROPERTY(BlueprintReadOnly) FText StateText;
	UPROPERTY(BlueprintReadOnly) bool bAssigned = false;
	UPROPERTY(BlueprintReadOnly) bool bEmpty = false;
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bHovered = false;
	UPROPERTY(BlueprintReadOnly) bool bEditing = false;
	UPROPERTY(BlueprintReadOnly) bool bEquipped = false;
	UPROPERTY(BlueprintReadOnly) bool bReady = false;
	UPROPERTY(BlueprintReadOnly) bool bPending = false;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_QuickSelectView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EAZ_QuickSelectState State = EAZ_QuickSelectState::Closed;
	UPROPERTY(BlueprintReadOnly) TArray<FAZ_QuickSelectEntryView> Entries;
	UPROPERTY(BlueprintReadOnly) int32 HoveredSlot = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int32 EditingSlot = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) bool bPending = false;
	UPROPERTY(BlueprintReadOnly) FText StatusText;
	UPROPERTY(BlueprintReadOnly) FText HintText;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UAZ_Inv_CommonUI_InventoryItem> FocusItem = nullptr;
	UPROPERTY(BlueprintReadOnly) FText FocusNameText;
	UPROPERTY(BlueprintReadOnly) FText FocusDescriptionText;
	UPROPERTY(BlueprintReadOnly) FText ModeText;
	UPROPERTY(BlueprintReadOnly) FText ModeActionText;
};
