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
	extern IConsoleVariable* CrowdIntensity;             // int32  "az.Crowd.Intensity" (1..5)
	extern IConsoleVariable* GrabForceNext;              // int32  "az.Grab.ForceNext" (1 = next attack becomes a grab, self-clears)
	extern IConsoleVariable* GrabChance;                 // float  "az.Grab.Chance" (<0 = use BT node default)
	extern IConsoleVariable* GrabCooldownSeconds;        // float  "az.Grab.CooldownSeconds" (<0 = node default)
	extern IConsoleVariable* GrabPressesToEscape;        // int32  "az.Grab.PressesToEscape" (<=0 = ability default)
	extern IConsoleVariable* GrabWindowSeconds;          // float  "az.Grab.WindowSeconds" (<=0 = ability default)

	void RegisterAll();
	void UnregisterAll();

	// Thin getters with safe defaults — call sites don't need to null-check.
	int32 GetControlStyle();
	int32 GetStrafeStyle();
	int32 GetAnalogInputStyle();
	float GetReplayMontageErrorThreshold();
	int32 GetCrowdIntensity();
	int32 GetGrabForceNext();
	float GetGrabChance();
	float GetGrabCooldownSeconds();
	int32 GetGrabPressesToEscape();
	float GetGrabWindowSeconds();
}
