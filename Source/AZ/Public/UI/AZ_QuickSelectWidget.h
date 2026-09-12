// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/AZ_QuickSelectTypes.h"
#include "AZ_QuickSelectWidget.generated.h"

class UAZ_QuickSelectComponent;
class UAZ_QuickSelectEntryWidget;
class USizeBox;
class UTextBlock;
class UAZ_Inv_CommonUI_CompositeWidget;

/** Temporary input surface. The controller component owns every session and gameplay request. */
UCLASS(Abstract)
class AZ_API UAZ_QuickSelectWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void InitializeSelector(UAZ_QuickSelectComponent* Component);
	void ApplyView(const FAZ_QuickSelectView& View);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect")
	TSubclassOf<UAZ_QuickSelectEntryWidget> EntryWidgetClass;
	/** Optional compact authored card for the intrinsic center slot. */
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect")
	TSubclassOf<UAZ_QuickSelectEntryWidget> CenterEntryWidgetClass;

	/** Positions and sizing are authored in the root Widget Blueprint, not in NativePaint. */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> CenterSlot;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> LeftSlot;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> RightSlot;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> UpSlot;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> DownSlot;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> LeftSlotSecond;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> RightSlotSecond;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> UpSlotSecond;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USizeBox> DownSlotSecond;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HeaderText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UAZ_Inv_CommonUI_CompositeWidget> FocusDetails;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FocusNameText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FocusDescriptionText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(BlueprintReadOnly, Transient, Category="AZ|QuickSelect")
	FAZ_QuickSelectView CurrentView;

private:
	TWeakObjectPtr<UAZ_QuickSelectComponent> Selector;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UAZ_QuickSelectEntryWidget>> EntryWidgets;

	bool CanRouteInput() const;
	USizeBox* FindHost(EAZ_QuickSlotPosition Position, int32 PositionOrdinal) const;
	void UpdateFocusDetails(const FAZ_QuickSelectView& View);
	void HandleSlotHovered(int32 SlotIndex);
	void UpdateHoveredSlotAt(const FVector2D& ScreenPosition);
	FReply HandlePointerPress(const FPointerEvent& Event);
};
