#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "AZ_CraftingPanel.generated.h"

class UAZ_CraftRecipe;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_InventorySwitcherPanel;
class UButton;
class UTextBlock;

/** A focused, temporary sheet over the existing inventory canvas. The inventory menu retains input ownership. */
UCLASS()
class AZ_API UAZ_CraftingPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePanel(UAZ_Inv_CommonUI_InventoryComponent* InInventory,
		UAZ_Inv_CommonUI_InventorySwitcherPanel* InOwner);
	void Refresh();
	void FocusDefaultControl();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UAZ_CraftRecipe* GetSelectedRecipe() const;
	void ChangeRecipe(int32 Direction);
	FText DescribeRequirements(const UAZ_CraftRecipe* Recipe) const;
	int32 CountBackpackItem(const FGameplayTag& ItemType) const;
	static FText ItemTypeLabel(const FGameplayTag& ItemType);
	UButton* MakeButton(const FText& Label);
	UTextBlock* MakeText(const FText& Text, bool bHeading = false);

	UFUNCTION() void PreviousRecipe();
	UFUNCTION() void NextRecipe();
	UFUNCTION() void CraftSelected();
	UFUNCTION() void ClosePanel();

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> Inventory;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventorySwitcherPanel> OwnerPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RequirementsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AvailabilityText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedbackText;
	UPROPERTY(Transient) TObjectPtr<UButton> PreviousButton;
	UPROPERTY(Transient) TObjectPtr<UButton> NextButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CraftButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	int32 RecipeIndex = 0;
	bool bSubmitting = false;
	FText Feedback;
};
