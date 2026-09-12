// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/AZ_QuickSelectTypes.h"
#include "AZ_QuickSelectEntryWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UAZ_Inv_CommonUI_CompositeWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FAZ_QuickSelectSlotHovered, int32);

/** Designer-authored card for a configured slot; it owns no assignment or equipment state. */
UCLASS(Abstract)
class AZ_API UAZ_QuickSelectEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void ApplyEntryView(const FAZ_QuickSelectEntryView& View);
	int32 GetSlotIndex() const { return EntryView.SlotIndex; }
	FAZ_QuickSelectSlotHovered OnSlotHovered;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Transient, Category="AZ|QuickSelect")
	FAZ_QuickSelectEntryView EntryView;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Icon;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CategoryText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StateText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyMarkText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> HighlightBorder;
	/** The same fragment-assimilation composite used by inventory, with compact name/icon leaves. */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UAZ_Inv_CommonUI_CompositeWidget> ItemDetails;

	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect|Style")
	FLinearColor IdleColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect|Style")
	FLinearColor HoveredColor = FLinearColor(.87f, .61f, .42f, .16f);
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect|Style")
	FLinearColor EditingColor = FLinearColor(.87f, .61f, .42f, .28f);
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect|Style")
	FLinearColor EquippedColor = FLinearColor(.85f, .82f, .75f, .09f);
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickSelect|Style", meta=(ClampMin="0", ClampMax="1"))
	float UnavailableOpacity = .5f;

	/** Optional BP styling hooks may animate widgets, but must not submit gameplay requests. */
	UFUNCTION(BlueprintImplementableEvent, Category="AZ|QuickSelect")
	void OnEntryViewChanged(const FAZ_QuickSelectEntryView& View);
};
