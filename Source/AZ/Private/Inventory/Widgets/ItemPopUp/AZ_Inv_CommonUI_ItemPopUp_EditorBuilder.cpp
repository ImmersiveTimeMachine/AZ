// Editor-only console command to build the WBP_AZ_CommonUI_ItemPopUp widget tree.
// Run from console: AZ.BuildItemPopUpWidgetTree
// Safe to delete this file after the widget tree is built.

#if WITH_EDITOR

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

static void BuildItemPopUpWidgetTree()
{
	const TCHAR* BPPath = TEXT("/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/WBP_AZ_CommonUI_ItemPopUp.WBP_AZ_CommonUI_ItemPopUp");
	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, BPPath);

	if (!WBP)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ.BuildItemPopUpWidgetTree: Blueprint not found at %s"), BPPath);
		return;
	}

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ.BuildItemPopUpWidgetTree: WidgetTree is null"));
		return;
	}

	// Clear existing root if present
	if (Tree->RootWidget)
	{
		Tree->RemoveWidget(Tree->RootWidget);
		Tree->RootWidget = nullptr;
	}

	// --- Create widgets ---

	// Root: SizeBox
	USizeBox* SizeBox_Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Root"));
	SizeBox_Root->SetWidthOverride(200.f);
	SizeBox_Root->SetHeightOverride(250.f);
	Tree->RootWidget = SizeBox_Root;

	// VerticalBox inside SizeBox
	UVerticalBox* VBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_0"));
	SizeBox_Root->AddChild(VBox);

	// Load WBP_ContextButton_Horror class (extends AZ_Inv_CommonUI_Button)
	UClass* ButtonClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/InventorySystemPro/ExampleContent/Horror/UI/Widgets/WBP_ContextButton_Horror.WBP_ContextButton_Horror_C")
	);

	if (!ButtonClass)
	{
		// Fallback: use the C++ base class directly
		ButtonClass = FindObject<UClass>(nullptr, TEXT("/Script/AZ.AZ_Inv_CommonUI_Button"));
		UE_LOG(LogTemp, Warning, TEXT("AZ.BuildItemPopUpWidgetTree: WBP_ContextButton_Horror not found, using AZ_Inv_CommonUI_Button"));
	}

	if (ButtonClass)
	{
		// Button_Consume
		UWidget* Button_Consume = Tree->ConstructWidget<UWidget>(ButtonClass, TEXT("Button_Consume"));
		VBox->AddChild(Button_Consume);

		// Button_Drop
		UWidget* Button_Drop = Tree->ConstructWidget<UWidget>(ButtonClass, TEXT("Button_Drop"));
		VBox->AddChild(Button_Drop);

		// Button_Split
		UWidget* Button_Split = Tree->ConstructWidget<UWidget>(ButtonClass, TEXT("Button_Split"));
		VBox->AddChild(Button_Split);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AZ.BuildItemPopUpWidgetTree: No button class found!"));
	}

	// Slider_Split
	USlider* Slider_Split = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_Split"));
	VBox->AddChild(Slider_Split);

	// Text_SplitAmount
	UTextBlock* Text_SplitAmount = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SplitAmount"));
	Text_SplitAmount->SetText(FText::FromString(TEXT("1")));
	VBox->AddChild(Text_SplitAmount);

	// --- Compile and save ---
	WBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WBP);

	UE_LOG(LogTemp, Warning, TEXT("AZ.BuildItemPopUpWidgetTree: Widget tree built and compiled successfully!"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, TEXT("WBP_AZ_CommonUI_ItemPopUp widget tree built!"));
	}
}

static FAutoConsoleCommand BuildItemPopUpWidgetTreeCmd(
	TEXT("AZ.BuildItemPopUpWidgetTree"),
	TEXT("Builds the widget tree for WBP_AZ_CommonUI_ItemPopUp (run once, then save the BP)"),
	FConsoleCommandDelegate::CreateStatic(&BuildItemPopUpWidgetTree)
);

#endif // WITH_EDITOR
