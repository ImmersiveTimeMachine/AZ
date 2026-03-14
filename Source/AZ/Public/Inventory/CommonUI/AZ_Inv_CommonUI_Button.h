// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "AZ_Inv_CommonUI_Button.generated.h"

class UCommonTextBlock;
/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_Button : public UCommonButtonBase
{
	GENERATED_BODY()
	
public:
	// ---------------------------------------------------
	// MANDATORY WIDGET BINDINGS
	// ---------------------------------------------------
	// Matches "Inventory System Pro - Mandatory References" in your screenshot.
    
	/** The text block component used to display the button's label */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Inventory System Pro")
	TObjectPtr<UCommonTextBlock> TextRef;
	
	FCommonButtonBaseClicked& OnButtonClicked() { return OnButtonBaseClicked; }

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory Button")
	void SetupButton();

	/** * Updates the internal text reference.
	 * Blueprint: SetButtonText
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory Button")
	void SetButtonText(const FText& InButtonText);
	
	/** * Toggles button visibility.
	 * @param bIsVisible - If true, sets visibility to Visible; otherwise, sets to Collapsed.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory Button")
	void SetButtonVisbility(bool bIsVisible);
		
	// ---------------------------------------------------
	// CONFIGURATION
	// ---------------------------------------------------
	// Matches "Inventory Button" category.
    
	/** The text to display on the button (Editable in Designer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Inventory Button")
	FText ButtonText;

	// ---------------------------------------------------
	// RUNTIME DATA
	// ---------------------------------------------------
	// Matches "Runtime Variables (Do Not Edit)" category.
    
	/** Reference to the parent or owner menu */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "AZ|Runtime Variables")
	TObjectPtr<UUserWidget> InventoryMenu; 
	// Note: Replace UUserWidget with UAZ_Inv_InventoryMenu if you have that C++ class defined.

protected:
	// Optional: Override PreConstruct to auto-assign the text
	virtual void NativePreConstruct() override;
};
