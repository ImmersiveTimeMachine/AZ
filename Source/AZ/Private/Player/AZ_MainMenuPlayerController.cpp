#include "Player/AZ_MainMenuPlayerController.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/AZ_MenuRouteWidget.h"
#include "UObject/ConstructorHelpers.h"

AAZ_MainMenuPlayerController::AAZ_MainMenuPlayerController()
{
	bFrontEndController = true;
	bShowTitleMenuOnStartup = true;
	bShowMouseCursor = true;
	static ConstructorHelpers::FClassFinder<UAZ_MenuRouteWidget> MenuClass(
		TEXT("/Game/AZ/Blueprints/Menu/FieldNotes/WBP_AZ_MenuRoutes"));
	if (MenuClass.Succeeded()) MenuRoutesWidgetClass = MenuClass.Class;
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MenuContext(
		TEXT("/Game/AZ/Blueprints/Input/FieldNotes/IMC_FN_PauseMenu.IMC_FN_PauseMenu"));
	if (MenuContext.Succeeded()) PauseMenuMappingContext = MenuContext.Object;
	static ConstructorHelpers::FObjectFinder<UInputAction> MenuAction(
		TEXT("/Game/AZ/Blueprints/Input/FieldNotes/IA_FN_PauseMenu.IA_FN_PauseMenu"));
	if (MenuAction.Succeeded()) PauseMenuAction = MenuAction.Object;
}
