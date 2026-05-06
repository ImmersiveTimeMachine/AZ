#include "AZ_ConsoleVariables.h"

#include "HAL/IConsoleManager.h"

namespace AZCVars
{
	IConsoleVariable* DDControlStyle             = nullptr;
	IConsoleVariable* DDStrafeStyle              = nullptr;
	IConsoleVariable* DDAnalogInputStyle         = nullptr;
	IConsoleVariable* ReplayMontageErrorThreshold = nullptr;

	void RegisterAll()
	{
		IConsoleManager& CM = IConsoleManager::Get();

		DDControlStyle = CM.RegisterConsoleVariable(
			TEXT("DDCvar.ControlStyle"),
			0,
			TEXT("GASP Control Style. 0 = standard third-person, 1 = twin-stick."),
			ECVF_Default);

		DDStrafeStyle = CM.RegisterConsoleVariable(
			TEXT("DDCvar.StrafeStyle"),
			0,
			TEXT("GASP Strafe Style. Selects sprint dot-threshold: 0 = 0.5, 1/2 = -0.1."),
			ECVF_Default);

		DDAnalogInputStyle = CM.RegisterConsoleVariable(
			TEXT("DDCvar.AnalogInputStyle"),
			0,
			TEXT("GASP Analog Input Style. 0 = keyboard (default Run), 1 = analog (deflection-controlled Run/Walk)."),
			ECVF_Default);

		ReplayMontageErrorThreshold = CM.RegisterConsoleVariable(
			TEXT("GS.replay.MontageErrorThreshold"),
			0.5f,
			TEXT("Tolerance level for when montage playback position correction occurs in replays."),
			ECVF_Default);
	}

	void UnregisterAll()
	{
		// Use the NAME-based unregister overload, NOT the IConsoleObject*-based one.
		//
		// `IConsoleManager::UnregisterConsoleObject(IConsoleObject*, bool)` calls
		// `FindConsoleObjectName(InVar)` which, when no map entry matches, dereferences
		// `InVar->GetParentObject()` (ConsoleManager.cpp:3297). If the object was
		// freed by an earlier shutdown phase or invalidated by Live Coding, that
		// vtable call crashes the editor on exit.
		//
		// The name-based overload does an internal `FindConsoleObject(Name)` lookup
		// and is null-safe: missing entries are no-ops.
		IConsoleManager& CM = IConsoleManager::Get();
		auto Drop = [&CM](IConsoleVariable*& CV, const TCHAR* Name)
		{
			CM.UnregisterConsoleObject(Name, /*bKeepState*/ false);
			CV = nullptr;
		};
		Drop(DDControlStyle,              TEXT("DDCvar.ControlStyle"));
		Drop(DDStrafeStyle,               TEXT("DDCvar.StrafeStyle"));
		Drop(DDAnalogInputStyle,          TEXT("DDCvar.AnalogInputStyle"));
		Drop(ReplayMontageErrorThreshold, TEXT("GS.replay.MontageErrorThreshold"));
	}

	int32 GetControlStyle()              { return DDControlStyle             ? DDControlStyle->GetInt()             : 0; }
	int32 GetStrafeStyle()               { return DDStrafeStyle              ? DDStrafeStyle->GetInt()              : 0; }
	int32 GetAnalogInputStyle()          { return DDAnalogInputStyle         ? DDAnalogInputStyle->GetInt()         : 0; }
	float GetReplayMontageErrorThreshold() { return ReplayMontageErrorThreshold ? ReplayMontageErrorThreshold->GetFloat() : 0.5f; }
}
