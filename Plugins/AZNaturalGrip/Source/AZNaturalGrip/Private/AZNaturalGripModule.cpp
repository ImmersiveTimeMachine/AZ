// Copyright Artur. AZ project.
// AZ Natural Grip editor module: registers the "AZ Natural Grip" tab and its entry in the Tools menu.

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SAZNaturalGripPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AZNaturalGrip"

namespace
{
	const FName NaturalGripTabName(TEXT("AZNaturalGrip"));
}

class FAZNaturalGripModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(NaturalGripTabName,
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::NomadTab)
					.Label(LOCTEXT("TabLabel", "AZ Natural Grip"))
					[
						SNew(SAZNaturalGripPanel)
					];
			}))
			.SetDisplayName(LOCTEXT("TabTitle", "AZ Natural Grip"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Solve natural hand grasps on held weapons"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAZNaturalGripModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(NaturalGripTabName);
		}
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("AZTools"), LOCTEXT("AZToolsSection", "AZ"));
		Section.AddMenuEntry(TEXT("AZNaturalGrip"),
			LOCTEXT("MenuEntry", "AZ Natural Grip"),
			LOCTEXT("MenuEntryTooltip", "Solve natural hand grasps on held weapons"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(NaturalGripTabName);
			})));
	}
};

IMPLEMENT_MODULE(FAZNaturalGripModule, AZNaturalGrip)

#undef LOCTEXT_NAMESPACE
