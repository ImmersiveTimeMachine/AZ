#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "AZ_Inv_CommonUI_HoverItem.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UTextBlock;
class UImage;

UCLASS()
class AZ_API UAZ_Inv_CommonUI_HoverItem : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetImageBrush(const FSlateBrush& Brush) const;
	void UpdateStackCount(const int32 Count);

	FGameplayTag GetItemType() const;

	int32 GetStackCount() const
	{
		return StackCount;
	}

	bool IsStackable() const
	{
		return bIsStackable;
	}

	void SetIsStackable(const bool bStacks);

	int32 GetPreviousGridIndex() const
	{
		return PreviousGridIndex;
	}

	void SetPreviousGridIndex(const int32 Index)
	{
		PreviousGridIndex = Index;
	}

	FIntPoint GetGridDimensions() const
	{
		return GridDimensions;
	}

	void SetGridDimensions(const FIntPoint& Dimensions)
	{
		GridDimensions = Dimensions;
	}

	UAZ_Inv_CommonUI_InventoryItem* GetInventoryItem() const;
	void SetInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_StackCount;

	int32 PreviousGridIndex{0};
	FIntPoint GridDimensions{1, 1};
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InventoryItem;
	bool bIsStackable{false};
	int32 StackCount{0};
};
