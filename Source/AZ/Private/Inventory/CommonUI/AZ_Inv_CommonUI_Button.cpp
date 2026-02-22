// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_Button.h"

#include "CommonTextBlock.h"

void UAZ_Inv_CommonUI_Button::SetupButton()
{
	// Blueprint: VariableGet ButtonText -> CallFunction SetButtonText
	SetButtonText(ButtonText);
}

void UAZ_Inv_CommonUI_Button::SetButtonText(const FText& InButtonText)
{
	// Safety check for the Mandatory Reference
	if (TextRef)
	{
		TextRef->SetText(InButtonText);
	}
	else
	{
		// Helpful log for debugging your CHALK UI hierarchy
		UE_LOG(LogTemp, Warning, TEXT("[%s] TextRef is null! Check BindWidget name in Blueprint."), *GetName());
	}
}

void UAZ_Inv_CommonUI_Button::SetButtonVisbility(bool bIsVisible)
{
	// Blueprint logic: If bIsVisible is True -> Set Visible, Else -> Set Collapsed
	ESlateVisibility NewVisibility = bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
    
	// Call the native SetVisibility function on this widget
	SetVisibility(NewVisibility);
}

void UAZ_Inv_CommonUI_Button::NativePreConstruct()
{
	Super::NativePreConstruct();
}
