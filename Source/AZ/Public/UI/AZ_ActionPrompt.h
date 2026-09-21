#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "CommonUserWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "AZ_ActionPrompt.generated.h"

class UCommonActionWidget;
class UCommonTextBlock;
class UCommonInputSubsystem;
class UCommonUIActionRouterBase;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
struct FUIActionBinding;

UENUM(BlueprintType)
enum class EAZ_ActionPromptPresentation : uint8
{
	Unavailable,
	Unbound,
	KeyText,
	Glyph
};

/** Small view wrapper; displays an existing action/handle and never registers commands. */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class AZ_API UAZ_ActionPrompt : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="AZ|UI|Action Prompt")
	void ConfigureAction(UInputAction* Action, FText Description, FName Context = NAME_None);

	/** Binding must already exist and belong to this widget's owning local player. */
	UFUNCTION(BlueprintCallable, Category="AZ|UI|Action Prompt")
	bool ConfigureBinding(FUIActionBindingHandle Binding, FText Description, FName Context = NAME_None);

	UFUNCTION(BlueprintCallable, Category="AZ|UI|Action Prompt")
	void ClearPrompt();

	UFUNCTION(BlueprintCallable, Category="AZ|UI|Action Prompt")
	void RefreshPresentation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCommonActionWidget> ActionIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCommonTextBlock> DescriptionText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCommonTextBlock> KeyFallbackText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|UI|Action Prompt") bool bShowUnboundPrompt = false;

	/** Styling hook only; do not create commands or overwrite the localized description here. */
	UFUNCTION(BlueprintImplementableEvent, Category="AZ|UI|Action Prompt")
	void OnPresentationUpdated(EAZ_ActionPromptPresentation Presentation, ECommonInputType InputType, FName Context);

private:
	void AttachListeners();
	void DetachListeners();
	void ApplyConfiguredSource();
	TSharedPtr<FUIActionBinding> FindActiveBinding(FUIActionBindingHandle Handle) const;
	void DisconnectHoldProgress();
	void HandleHoldProgress(float Percent);
	void HandleInputMethodChanged(ECommonInputType InputType);
	void HandleBoundActionsUpdated();
	UFUNCTION() void HandleMappingsRebuilt();

	UPROPERTY(Transient) TObjectPtr<UInputAction> ConfiguredAction;
	UPROPERTY(Transient) FText ConfiguredDescription;
	UPROPERTY(Transient) FName PromptContext;
	FUIActionBindingHandle ConfiguredBinding;
	TWeakObjectPtr<UCommonInputSubsystem> CommonInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInput;
	TWeakObjectPtr<UCommonUIActionRouterBase> ActionRouter;
	FDelegateHandle InputChangedHandle;
	FDelegateHandle BoundActionsHandle;
	TWeakPtr<FUIActionBinding> HeldBinding;
	FDelegateHandle HoldProgressHandle;
	bool bBindingMode = false;
	bool bConstructed = false;
};
