#include "InventoryUI/AZ_CraftingPanel.h"

#include "InventoryUI/AZ_CraftRecipe.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Actor.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "AZCrafting"

namespace
{
	const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(221, 214, 196));
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(40, 46, 41));
	const FLinearColor MutedInk = FLinearColor::FromSRGBColor(FColor(89, 99, 85));
}

void UAZ_CraftingPanel::InitializePanel(UAZ_Inv_CommonUI_InventoryComponent* InInventory,
	UAZ_Inv_CommonUI_InventorySwitcherPanel* InOwner)
{
	Inventory = InInventory;
	OwnerPanel = InOwner;
	RecipeIndex = 0;
	Feedback = FText::GetEmpty();
	Refresh();
}

TSharedRef<SWidget> UAZ_CraftingPanel::RebuildWidget()
{
	if (WidgetTree && (!WidgetTree->RootWidget ||
		(WidgetTree->RootWidget->IsA<UCanvasPanel>() && CastChecked<UCanvasPanel>(WidgetTree->RootWidget)->GetChildrenCount() == 0)))
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;

		UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>();
		Scrim->SetBrushColor(FLinearColor(0.035f, 0.04f, 0.035f, 0.83f));
		Scrim->SetPadding(FMargin(0));
		UCanvasPanelSlot* ScrimSlot = Canvas->AddChildToCanvas(Scrim);
		ScrimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ScrimSlot->SetOffsets(FMargin(0));

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrushColor(Paper);
		Card->SetPadding(FMargin(28, 25));
		UScaleBox* CardFit = WidgetTree->ConstructWidget<UScaleBox>();
		CardFit->SetStretch(EStretch::ScaleToFit);
		CardFit->SetStretchDirection(EStretchDirection::DownOnly);
		UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(CardFit);
		CardSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		CardSlot->SetOffsets(FMargin(24));
		CardSlot->SetZOrder(1);
		UScaleBoxSlot* FitSlot = CastChecked<UScaleBoxSlot>(CardFit->AddChild(Card));
		FitSlot->SetHorizontalAlignment(HAlign_Center);
		FitSlot->SetVerticalAlignment(VAlign_Center);

		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
		Width->SetWidthOverride(520.f);
		Card->AddChild(Width);
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
		Width->AddChild(Body);
		auto AddLine = [Body](UTextBlock* Line, float After)
		{
			Body->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 0, 0, After));
		};

		AddLine(MakeText(LOCTEXT("FieldNotes", "CHALK / FIELD NOTES"), true), 18);
		RecipeNameText = MakeText(FText::GetEmpty(), true);
		AddLine(RecipeNameText, 18);
		RequirementsText = MakeText(FText::GetEmpty());
		AddLine(RequirementsText, 14);
		AvailabilityText = MakeText(FText::GetEmpty());
		AddLine(AvailabilityText, 14);
		FeedbackText = MakeText(FText::GetEmpty());
		AddLine(FeedbackText, 20);

		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChildToVerticalBox(Actions);
		auto AddAction = [Actions](UButton* Action)
		{
			auto* ActionSlot = Actions->AddChildToHorizontalBox(Action);
			ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ActionSlot->SetPadding(FMargin(0, 0, 6, 0));
		};
		PreviousButton = MakeButton(LOCTEXT("Previous", "Previous"));
		NextButton = MakeButton(LOCTEXT("Next", "Next"));
		CraftButton = MakeButton(LOCTEXT("Craft", "Craft"));
		CloseButton = MakeButton(LOCTEXT("Back", "Back"));
		AddAction(PreviousButton);
		AddAction(NextButton);
		AddAction(CraftButton);
		AddAction(CloseButton);
	}
	return Super::RebuildWidget();
}

UTextBlock* UAZ_CraftingPanel::MakeText(const FText& Text, bool bHeading)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(Text);
	Label->SetAutoWrapText(true);
	Label->SetFont(FCoreStyle::GetDefaultFontStyle(bHeading ? "Bold" : "Regular", bHeading ? 21 : 17));
	Label->SetColorAndOpacity(bHeading ? Ink : MutedInk);
	return Label;
}

UButton* UAZ_CraftingPanel::MakeButton(const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(Paper);
	UTextBlock* ButtonLabel = MakeText(Label, true);
	ButtonLabel->SetAutoWrapText(false);
	ButtonLabel->SetJustification(ETextJustify::Center);
	USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>();
	ContentSize->SetHeightOverride(36.f);
	UScaleBox* LabelFit = WidgetTree->ConstructWidget<UScaleBox>();
	LabelFit->SetStretch(EStretch::ScaleToFit);
	LabelFit->SetStretchDirection(EStretchDirection::DownOnly);
	LabelFit->AddChild(ButtonLabel);
	ContentSize->AddChild(LabelFit);
	Button->AddChild(ContentSize);
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ContentSize->Slot))
	{
		LabelSlot->SetPadding(FMargin(10, 6));
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	return Button;
}

void UAZ_CraftingPanel::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (PreviousButton) PreviousButton->OnClicked.AddUniqueDynamic(this, &ThisClass::PreviousRecipe);
	if (NextButton) NextButton->OnClicked.AddUniqueDynamic(this, &ThisClass::NextRecipe);
	if (CraftButton) CraftButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CraftSelected);
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClosePanel);
	Refresh();
}

void UAZ_CraftingPanel::NativeDestruct()
{
	if (PreviousButton) PreviousButton->OnClicked.RemoveDynamic(this, &ThisClass::PreviousRecipe);
	if (NextButton) NextButton->OnClicked.RemoveDynamic(this, &ThisClass::NextRecipe);
	if (CraftButton) CraftButton->OnClicked.RemoveDynamic(this, &ThisClass::CraftSelected);
	if (CloseButton) CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::ClosePanel);
	Super::NativeDestruct();
}

FReply UAZ_CraftingPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		ClosePanel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAZ_CraftingPanel::FocusDefaultControl()
{
	if (GetOwningPlayer())
	{
		if (CraftButton && CraftButton->GetIsEnabled()) CraftButton->SetUserFocus(GetOwningPlayer());
		else if (NextButton && NextButton->GetIsEnabled()) NextButton->SetUserFocus(GetOwningPlayer());
		else if (CloseButton) CloseButton->SetUserFocus(GetOwningPlayer());
	}
}

UAZ_CraftRecipe* UAZ_CraftingPanel::GetSelectedRecipe() const
{
	if (!Inventory.IsValid()) return nullptr;
	const auto& Recipes = Inventory->CraftingRecipes;
	if (Recipes.IsValidIndex(RecipeIndex) && IsValid(Recipes[RecipeIndex])) return Recipes[RecipeIndex];
	for (UAZ_CraftRecipe* Recipe : Recipes) if (IsValid(Recipe)) return Recipe;
	return nullptr;
}

void UAZ_CraftingPanel::ChangeRecipe(int32 Direction)
{
	if (!Inventory.IsValid() || Inventory->CraftingRecipes.IsEmpty()) return;
	const auto& Recipes = Inventory->CraftingRecipes;
	for (int32 Offset = 1; Offset <= Recipes.Num(); ++Offset)
	{
		const int32 Candidate = (RecipeIndex + (Direction > 0 ? Offset : -Offset) + Recipes.Num() * 2) % Recipes.Num();
		if (IsValid(Recipes[Candidate]))
		{
			RecipeIndex = Candidate;
			Feedback = FText::GetEmpty();
			Refresh();
			return;
		}
	}
}

int32 UAZ_CraftingPanel::CountBackpackItem(const FGameplayTag& ItemType) const
{
	if (!Inventory.IsValid()) return 0;
	int32 Count = 0;
	for (const FAZ_InventoryGridPlacement& Placement : Inventory->GetPlacements())
	{
		const UAZ_Inv_CommonUI_InventoryItem* Item = Inventory->FindItemById(Placement.ItemId);
		if (IsValid(Item) && Item->GetItemManifest().GetItemTypeTag().MatchesTagExact(ItemType))
			Count += Placement.StackCount;
	}
	return Count;
}

FText UAZ_CraftingPanel::ItemTypeLabel(const FGameplayTag& ItemType)
{
	FString Name = ItemType.GetTagName().ToString();
	int32 Dot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), Dot)) Name.RightChopInline(Dot + 1);
	Name.ReplaceInline(TEXT("_"), TEXT(" "));
	return FText::FromString(Name);
}

FText UAZ_CraftingPanel::DescribeRequirements(const UAZ_CraftRecipe* Recipe) const
{
	if (!Recipe) return LOCTEXT("NoRecipeDetails", "No recipe is assigned to this inventory.");
	FString Lines;
	for (const FAZ_CraftIngredient& Ingredient : Recipe->Ingredients)
	{
		const int32 Available = CountBackpackItem(Ingredient.ItemType);
		Lines += FString::Printf(TEXT("%s  %d / %d%s\n"), *ItemTypeLabel(Ingredient.ItemType).ToString(),
			Available, Ingredient.Quantity, Available >= Ingredient.Quantity ? TEXT("  ready") : TEXT("  missing"));
	}
	for (const FGameplayTag& Tool : Recipe->Tools)
	{
		Lines += FString::Printf(TEXT("%s  %s\n"), *ItemTypeLabel(Tool).ToString(),
			CountBackpackItem(Tool) > 0 ? TEXT("tool available") : TEXT("tool missing"));
	}
	return FText::FromString(Lines);
}

void UAZ_CraftingPanel::Refresh()
{
	UAZ_CraftRecipe* Recipe = GetSelectedRecipe();
	if (RecipeNameText)
		RecipeNameText->SetText(Recipe ? Recipe->DisplayName : LOCTEXT("NoRecipe", "No recipes available"));
	if (RequirementsText) RequirementsText->SetText(DescribeRequirements(Recipe));
	if (FeedbackText) FeedbackText->SetText(Feedback);
	const bool bAuthority = Inventory.IsValid() && IsValid(Inventory->GetOwner()) && Inventory->GetOwner()->HasAuthority();
	const bool bCarryClear = OwnerPanel.IsValid() && !OwnerPanel->HasHoverItem() && !OwnerPanel->HasActivePopUp();
	FString Error;
	const bool bAvailable = Recipe && Inventory.IsValid() && Inventory->CanCraftRecipe(Recipe, Error);
	if (AvailabilityText)
	{
		AvailabilityText->SetText(!bAuthority
			? LOCTEXT("AuthorityOnly", "Crafting is unavailable on a client in this build.")
			: !bCarryClear ? LOCTEXT("FinishCarry", "Finish the current inventory action before crafting.")
			: !Recipe ? LOCTEXT("AssignRecipe", "No recipe has been assigned.")
			: bAvailable ? LOCTEXT("Ready", "Ready to craft")
			: FText::FromString(Error.IsEmpty() ? TEXT("Requirements are not met.") : Error));
	}
	if (CraftButton) CraftButton->SetIsEnabled(bAuthority && bCarryClear && bAvailable && !bSubmitting);
	const bool bMultiple = Inventory.IsValid() && Inventory->CraftingRecipes.Num() > 1;
	if (PreviousButton) PreviousButton->SetIsEnabled(bMultiple);
	if (NextButton) NextButton->SetIsEnabled(bMultiple);
	if (PreviousButton) PreviousButton->SetVisibility(bMultiple ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (NextButton) NextButton->SetVisibility(bMultiple ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// Keep directional gamepad/keyboard focus inside the sheet while it covers the inventory.
	TArray<UButton*> AllButtons = {PreviousButton, NextButton, CraftButton, CloseButton};
	TArray<UButton*> FocusableButtons;
	for (UButton* Button : AllButtons)
	{
		if (!Button) continue;
		for (EUINavigation Direction : {EUINavigation::Left, EUINavigation::Right, EUINavigation::Up, EUINavigation::Down})
			Button->SetNavigationRuleBase(Direction, EUINavigationRule::Stop);
		if (Button->GetIsEnabled()) FocusableButtons.Add(Button);
	}
	for (int32 Index = 0; Index < FocusableButtons.Num(); ++Index)
	{
		if (Index > 0) FocusableButtons[Index]->SetNavigationRuleExplicit(EUINavigation::Left, FocusableButtons[Index - 1]);
		if (Index + 1 < FocusableButtons.Num()) FocusableButtons[Index]->SetNavigationRuleExplicit(EUINavigation::Right, FocusableButtons[Index + 1]);
	}
}

void UAZ_CraftingPanel::PreviousRecipe() { ChangeRecipe(-1); }
void UAZ_CraftingPanel::NextRecipe() { ChangeRecipe(1); }

void UAZ_CraftingPanel::CraftSelected()
{
	if (bSubmitting || !Inventory.IsValid() || !OwnerPanel.IsValid() || OwnerPanel->HasHoverItem() || OwnerPanel->HasActivePopUp()) return;
	UAZ_CraftRecipe* Recipe = GetSelectedRecipe();
	if (!Recipe || !IsValid(Inventory->GetOwner()) || !Inventory->GetOwner()->HasAuthority())
	{
		Feedback = LOCTEXT("NoClientCraft", "Crafting is unavailable on a client in this build.");
		Refresh();
		return;
	}
	FString Error;
	bSubmitting = true;
	Refresh();
	const bool bCrafted = Inventory->TryCraftRecipe(Recipe, FGuid::NewGuid(), Error);
	bSubmitting = false;
	Feedback = bCrafted
		? FText::Format(LOCTEXT("Crafted", "Crafted {0} x {1}"), FText::AsNumber(Recipe->OutputCount), Recipe->DisplayName)
		: FText::FromString(Error.IsEmpty() ? TEXT("Crafting failed. Check your inventory and try again.") : Error);
	Refresh();
	FocusDefaultControl();
}

void UAZ_CraftingPanel::ClosePanel()
{
	if (OwnerPanel.IsValid()) OwnerPanel->CloseCraftingPanel();
}

#undef LOCTEXT_NAMESPACE
