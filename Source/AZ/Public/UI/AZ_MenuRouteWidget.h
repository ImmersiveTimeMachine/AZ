#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "Styling/SlateTypes.h"
#include "Components/ComboBoxString.h"
#include "UI/AZ_MenuRoutesComponent.h"
#include "AZ_MenuRouteWidget.generated.h"

class UAZ_MenuCommandButton;
class UVerticalBox;
class UTextBlock;
class UBorder;
class UComboBoxString;
class UInputAction;
class UAZ_ActionPrompt;
class USizeBox;
class UCanvasPanelSlot;

/** One lazy-created CommonUI page. Route content is rebuilt only on user/state events. */
UCLASS(Blueprintable)
class AZ_API UAZ_MenuRouteWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UAZ_MenuRouteWidget(const FObjectInitializer& Initializer);
	void SetRouteOwner(UAZ_MenuRoutesComponent* Owner);
	void RefreshRoute();
	void RefreshDisplayCountdown();
	void FocusDefaultControl();
	UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> BackAction;
	UPROPERTY(EditDefaultsOnly, Category="Input") TSubclassOf<UAZ_ActionPrompt> BackPromptClass;
	UPROPERTY(EditDefaultsOnly, Category="Input") TSubclassOf<UAZ_ActionPrompt> OverlayBackPromptClass;
	UPROPERTY(EditDefaultsOnly, Category="Style") FSlateFontInfo BrandFont;
	UPROPERTY(EditDefaultsOnly, Category="Style") FSlateFontInfo TitleBrandFont;
	UPROPERTY(EditDefaultsOnly, Category="Style") FSlateFontInfo HeadingFont;
	UPROPERTY(EditDefaultsOnly, Category="Style") FSlateFontInfo BodyFont;
	UPROPERTY(EditDefaultsOnly, Category="Style") FButtonStyle PaperButtonStyle;
	UPROPERTY(EditDefaultsOnly, Category="Style") FButtonStyle OverlayButtonStyle;
	UPROPERTY(EditDefaultsOnly, Category="Style") FComboBoxStyle PaperComboStyle;
	UPROPERTY(EditDefaultsOnly, Category="Style") FTableRowStyle PaperComboRowStyle;
	UPROPERTY(EditDefaultsOnly, Category="Style") FLinearColor PaperColor = FLinearColor::FromSRGBColor(FColor(221, 214, 196));
	UPROPERTY(EditDefaultsOnly, Category="Style") FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor(40, 46, 41));
	UPROPERTY(EditDefaultsOnly, Category="Style") FLinearColor MutedColor = FLinearColor::FromSRGBColor(FColor(89, 99, 85));
	UPROPERTY(EditDefaultsOnly, Category="Style") FLinearColor OverlayColor = FLinearColor(0.02f, 0.027f, 0.022f, 0.88f);
	UPROPERTY(EditDefaultsOnly, Category="Style") FLinearColor OverlayTextColor = FLinearColor::FromSRGBColor(FColor(239, 232, 207));
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin PaperBrandPlacement = FMargin(56, 26, 420, 100);
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin TitleBrandPlacement = FMargin(110, 145, 620, 180);
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin PaperHeadingPlacement = FMargin(510, 40, 1050, 70);
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin TitleHeadingPlacement = FMargin(126, 345, 640, 50);
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin PaperBodyInsets = FMargin(80, 174, 80, 120);
	UPROPERTY(EditDefaultsOnly, Category="Layout") FMargin TitleBodyInsets = FMargin(110, 450, 80, 110);
	UPROPERTY(EditDefaultsOnly, Category="Layout") float MenuColumnWidth = 520;
	UPROPERTY(EditDefaultsOnly, Category="Layout") float SettingsColumnWidth = 1120;
	UPROPERTY(EditDefaultsOnly, Category="Layout") float ConfirmationColumnWidth = 864;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_MenuRoutesComponent> RouteOwner;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Body;
	UPROPERTY(Transient) TObjectPtr<UBorder> Background;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Brand;
	UPROPERTY(Transient) TObjectPtr<USizeBox> ContentWidth;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> BrandSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> HeadingSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> BodySlot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TObjectPtr<UWidget> DefaultFocus;
	UPROPERTY(Transient) TObjectPtr<UAZ_ActionPrompt> BackPrompt;
	UPROPERTY(Transient) TObjectPtr<UAZ_ActionPrompt> OverlayBackPrompt;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ModeBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ResolutionBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> VSyncBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> QualityBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> FamilyBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> FrameRateBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> MasterVolumeBox;
	TArray<FIntPoint> ResolutionValues;
	TArray<EAZ_GamepadGlyphPreference> FamilyValues;
	TArray<float> FrameRateValues;
	TArray<float> MasterVolumeValues;
	FUIActionBindingHandle BackBinding;
	bool bRefreshing = false;
	bool bOverlay = false;
	int32 LastDisplaySeconds = INDEX_NONE;
	void AddText(const FText& Text, bool bMuted = false);
	UAZ_MenuCommandButton* AddCommand(const FText& Text, EAZ_MenuCommand Command, bool bEnabled = true, bool bDefault = false);
	UComboBoxString* AddChoice(const FText& Label, const TArray<FString>& Choices, int32 Selected);
	void BuildSettings();
	void HandleBack();
	UFUNCTION() void HandleCommand(EAZ_MenuCommand Command);
	UFUNCTION() void HandleSettingChanged(FString Item, ESelectInfo::Type SelectInfo);
	UFUNCTION() UWidget* MakeChoiceLabel(FString Item);
};
