#include "UI/AZ_ActionPrompt.h"

#include "CommonActionWidget.h"
#include "CommonInputSubsystem.h"
#include "CommonTextBlock.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBinding.h"
#include "InputAction.h"

#define LOCTEXT_NAMESPACE "AZActionPrompt"

void UAZ_ActionPrompt::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;
	AttachListeners();
	ApplyConfiguredSource();
	RefreshPresentation();
}

void UAZ_ActionPrompt::NativeDestruct()
{
	bConstructed = false;
	DetachListeners();
	DisconnectHoldProgress();
	// Clear the icon source without unregistering the command's handle.
	if (ActionIcon) ActionIcon->SetEnhancedInputAction(nullptr);
	Super::NativeDestruct();
}

void UAZ_ActionPrompt::AttachListeners()
{
	DetachListeners();
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (!Player) return;
	CommonInput = UCommonInputSubsystem::Get(Player);
	EnhancedInput = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	ActionRouter = Player->GetSubsystem<UCommonUIActionRouterBase>();
	if (CommonInput.IsValid())
	{
		InputChangedHandle = CommonInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	if (EnhancedInput.IsValid())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleMappingsRebuilt);
	}
	if (ActionRouter.IsValid())
	{
		BoundActionsHandle = ActionRouter->OnBoundActionsUpdated().AddUObject(this, &ThisClass::HandleBoundActionsUpdated);
	}
}

void UAZ_ActionPrompt::DetachListeners()
{
	if (CommonInput.IsValid()) CommonInput->OnInputMethodChangedNative.Remove(InputChangedHandle);
	if (EnhancedInput.IsValid()) EnhancedInput->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::HandleMappingsRebuilt);
	if (ActionRouter.IsValid()) ActionRouter->OnBoundActionsUpdated().Remove(BoundActionsHandle);
	InputChangedHandle.Reset(); BoundActionsHandle.Reset();
	CommonInput.Reset(); EnhancedInput.Reset(); ActionRouter.Reset();
}

void UAZ_ActionPrompt::ConfigureAction(UInputAction* Action, FText Description, FName Context)
{
	bBindingMode = false;
	ConfiguredBinding = FUIActionBindingHandle();
	ConfiguredAction = Action;
	ConfiguredDescription = MoveTemp(Description);
	PromptContext = Context;
	ApplyConfiguredSource();
	RefreshPresentation();
}

bool UAZ_ActionPrompt::ConfigureBinding(FUIActionBindingHandle Binding, FText Description, FName Context)
{
	// Keep a registered same-owner source while its activatable node is pending;
	// availability below still requires membership in the active collection.
	if (!Binding.IsValid() || !GetOwningLocalPlayer() || Binding.GetBoundLocalPlayer() != GetOwningLocalPlayer())
	{
		ClearPrompt();
		return false;
	}
	bBindingMode = true;
	ConfiguredBinding = Binding;
	ConfiguredAction = nullptr;
	ConfiguredDescription = MoveTemp(Description);
	PromptContext = Context;
	ApplyConfiguredSource();
	RefreshPresentation();
	return true;
}

void UAZ_ActionPrompt::ClearPrompt()
{
	ConfigureAction(nullptr, FText::GetEmpty(), NAME_None);
}

TSharedPtr<FUIActionBinding> UAZ_ActionPrompt::FindActiveBinding(FUIActionBindingHandle Handle) const
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	const UCommonUIActionRouterBase* Router = Player ? Player->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (!Router || !Handle.IsValid() || Handle.GetBoundLocalPlayer() != Player) return nullptr;
	TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle);
	// Unregister broadcasts after removal from the collection but BEFORE removal
	// from the global registry. IsValid alone is still true in that callback.
	if (!Binding || !Binding->OwningCollection.IsValid() || !Router->GatherActiveBindings().Contains(Handle)) return nullptr;
	return Binding;
}

void UAZ_ActionPrompt::ApplyConfiguredSource()
{
	DisconnectHoldProgress();
	if (!ActionIcon) return;
	const TSharedPtr<FUIActionBinding> Binding = bBindingMode ? FindActiveBinding(ConfiguredBinding) : nullptr;
	if (Binding)
	{
		ActionIcon->SetInputActionBinding(ConfiguredBinding);
		if (bConstructed)
		{
			HeldBinding = Binding;
			HoldProgressHandle = Binding->OnHoldActionProgressed.AddUObject(this, &ThisClass::HandleHoldProgress);
		}
	}
	else
	{
		// Passing an invalid handle to SetInputActionBinding alone leaves its old icon source intact.
		ActionIcon->SetEnhancedInputAction(bBindingMode ? nullptr : ConfiguredAction.Get());
	}
}

void UAZ_ActionPrompt::DisconnectHoldProgress()
{
	if (TSharedPtr<FUIActionBinding> Binding = HeldBinding.Pin())
	{
		Binding->OnHoldActionProgressed.Remove(HoldProgressHandle);
	}
	HeldBinding.Reset();
	HoldProgressHandle.Reset();
	if (ActionIcon) ActionIcon->OnActionComplete();
}

void UAZ_ActionPrompt::HandleHoldProgress(float Percent)
{
	// Forward real existing hold progress. Never synthesize a timer or hold action.
	if (bConstructed && bBindingMode && FindActiveBinding(ConfiguredBinding) && ActionIcon)
	{
		ActionIcon->OnActionProgress(Percent);
	}
}

void UAZ_ActionPrompt::RefreshPresentation()
{
	if (!bConstructed || !ActionIcon || !DescriptionText || !KeyFallbackText) return;
	const UCommonInputSubsystem* Input = CommonInput.Get();
	const ECommonInputType Type = Input ? Input->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;
	EAZ_ActionPromptPresentation Presentation = EAZ_ActionPromptPresentation::Unavailable;
	FText Fallback;
	FKey ActualKey;
	bool bSourceValid = false;
	if (Input && GetOwningLocalPlayer() && Input->ShouldShowInputKeys())
	{
		const UInputAction* Action = ConfiguredAction;
		if (bBindingMode)
		{
			if (TSharedPtr<FUIActionBinding> Binding = FindActiveBinding(ConfiguredBinding))
			{
				Action = Binding->InputAction.Get();
				if (!Action)
				{
					if (const FCommonInputActionDataBase* Legacy = Binding->GetLegacyInputActionData())
					{
						const FCommonInputTypeInfo& Info = Legacy->GetCurrentInputTypeInfo(Input);
						bSourceValid = Info.OverrrideState == EInputActionState::Enabled;
						const FKey Key = Info.GetKey();
						if (bSourceValid && CommonUI::IsKeyValidForInputType(Key, Type)) ActualKey = Key;
					}
				}
			}
			else ApplyConfiguredSource();
		}
		if (Action && CommonUI::IsEnhancedInputSupportEnabled())
		{
			bSourceValid = true;
			ActualKey = CommonUI::GetFirstKeyForInputType(GetOwningLocalPlayer(), Type, Action);
		}
		if (bSourceValid)
		{
			if (!ActualKey.IsValid())
			{
				Presentation = EAZ_ActionPromptPresentation::Unbound;
				Fallback = LOCTEXT("Unbound", "Unbound");
			}
			else if (ActionIcon->GetIcon().DrawAs != ESlateBrushDrawType::NoDrawType)
			{
				Presentation = EAZ_ActionPromptPresentation::Glyph;
			}
			else
			{
				Presentation = EAZ_ActionPromptPresentation::KeyText;
				Fallback = ActualKey.GetDisplayName();
			}
		}
	}
	DescriptionText->SetText(ConfiguredDescription);
	KeyFallbackText->SetText(Fallback);
	ActionIcon->SetVisibility(Presentation == EAZ_ActionPromptPresentation::Glyph
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	KeyFallbackText->SetVisibility(Presentation == EAZ_ActionPromptPresentation::KeyText
		|| (Presentation == EAZ_ActionPromptPresentation::Unbound && bShowUnboundPrompt)
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bVisible = Presentation == EAZ_ActionPromptPresentation::Glyph
		|| Presentation == EAZ_ActionPromptPresentation::KeyText
		|| (Presentation == EAZ_ActionPromptPresentation::Unbound && bShowUnboundPrompt);
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	OnPresentationUpdated(Presentation, Type, PromptContext);
}

void UAZ_ActionPrompt::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshPresentation();
}

void UAZ_ActionPrompt::HandleMappingsRebuilt()
{
	RefreshPresentation();
}

void UAZ_ActionPrompt::HandleBoundActionsUpdated()
{
	if (!bBindingMode) return;
	// Clear the icon/hold listener inside the removal broadcast, while the old
	// handle can still exist in the registry. Reattach if its context reactivates.
	if (!FindActiveBinding(ConfiguredBinding) || !HeldBinding.IsValid()) ApplyConfiguredSource();
	RefreshPresentation();
}

#undef LOCTEXT_NAMESPACE
