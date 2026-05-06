#pragma once

#include "CoreMinimal.h"

class IConsoleVariable;

// AZ project console variables, with module-controlled lifetime.
//
// We deliberately avoid `static TAutoConsoleVariable<>` because its `~FAutoConsoleObject`
// destructor fires after the editor has begun teardown, which crashes
// `FConsoleManager::FindConsoleObjectName` when Live Coding has patched the owning
// module (the patch DLL and the original DLL both run their static dtors).
//
// Registration runs in `FAZModule::StartupModule`; unregistration in `ShutdownModule`.
namespace AZCVars
{
	extern IConsoleVariable* DDControlStyle;             // int32  "DDCvar.ControlStyle"
	extern IConsoleVariable* DDStrafeStyle;              // int32  "DDCvar.StrafeStyle"
	extern IConsoleVariable* DDAnalogInputStyle;         // int32  "DDCvar.AnalogInputStyle"
	extern IConsoleVariable* ReplayMontageErrorThreshold; // float  "GS.replay.MontageErrorThreshold"

	void RegisterAll();
	void UnregisterAll();

	// Thin getters with safe defaults — call sites don't need to null-check.
	int32 GetControlStyle();
	int32 GetStrafeStyle();
	int32 GetAnalogInputStyle();
	float GetReplayMontageErrorThreshold();
}
