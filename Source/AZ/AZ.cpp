#include "AZ/AZ.h"
#include "AZ_ConsoleVariables.h"

#include <Modules/ModuleManager.h>

DEFINE_LOG_CATEGORY(Log_AZ);

class FAZModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		AZCVars::RegisterAll();
	}

	virtual void ShutdownModule() override
	{
		// Unregistering here (instead of via static FAutoConsoleObject dtors) is what
		// avoids the FConsoleManager::FindConsoleObjectName crash when Live Coding has
		// patched this module: the patch DLL never gets a static-dtor pass that runs
		// against a dying FConsoleManager.
		AZCVars::UnregisterAll();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAZModule, AZ, "AZ");
