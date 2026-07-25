#include "AZ_ConsoleVariables.h"

#include "HAL/IConsoleManager.h"

namespace AZCVars
{
	IConsoleVariable* DDControlStyle             = nullptr;
	IConsoleVariable* DDStrafeStyle              = nullptr;
	IConsoleVariable* DDAnalogInputStyle         = nullptr;
	IConsoleVariable* ReplayMontageErrorThreshold = nullptr;
	IConsoleVariable* CrowdIntensity             = nullptr;
	IConsoleVariable* GrabForceNext              = nullptr;
	IConsoleVariable* GrabChance                 = nullptr;
	IConsoleVariable* GrabCooldownSeconds        = nullptr;
	IConsoleVariable* GrabPressesToEscape        = nullptr;
	IConsoleVariable* GrabWindowSeconds          = nullptr;

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

		CrowdIntensity = CM.RegisterConsoleVariable(
			TEXT("az.Crowd.Intensity"),
			0,
			TEXT("TEST override for Chalkie pack aggression. 0 = off (each crowd uses its own authored level); 1 (wary) .. 5 (frenzy) = force ALL crowds to this level. Per-crowd control is UAZ_HordeSubsystem::SetCrowdIntensity(CrowdId, Level)."),
			ECVF_Default);

		GrabForceNext = CM.RegisterConsoleVariable(
			TEXT("az.Grab.ForceNext"),
			0,
			TEXT("TEST: 1 = the next Chalkie attack opportunity becomes a GRAB (runs the real roll->token->ability path, then self-clears)."),
			ECVF_Default);

		GrabChance = CM.RegisterConsoleVariable(
			TEXT("az.Grab.Chance"),
			-1.f,
			TEXT("Override for the per-attack grab roll chance (0..1). Negative = use the BT node's GrabChance."),
			ECVF_Default);

		GrabCooldownSeconds = CM.RegisterConsoleVariable(
			TEXT("az.Grab.CooldownSeconds"),
			-1.f,
			TEXT("Override for the per-Chalkie grab cooldown. Negative = use the BT node's GrabCooldownSeconds."),
			ECVF_Default);

		GrabPressesToEscape = CM.RegisterConsoleVariable(
			TEXT("az.Grab.PressesToEscape"),
			0,
			TEXT("Override for how many E-presses free the player from a grab. <=0 = use GA_PlayerGrabbed's PressesToEscape."),
			ECVF_Default);

		GrabWindowSeconds = CM.RegisterConsoleVariable(
			TEXT("az.Grab.WindowSeconds"),
			-1.f,
			TEXT("Override for the grab escape window in seconds. <=0 = use GA_PlayerGrabbed's WindowSeconds."),
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
		Drop(CrowdIntensity,              TEXT("az.Crowd.Intensity"));
		Drop(GrabForceNext,               TEXT("az.Grab.ForceNext"));
		Drop(GrabChance,                  TEXT("az.Grab.Chance"));
		Drop(GrabCooldownSeconds,         TEXT("az.Grab.CooldownSeconds"));
		Drop(GrabPressesToEscape,         TEXT("az.Grab.PressesToEscape"));
		Drop(GrabWindowSeconds,           TEXT("az.Grab.WindowSeconds"));
	}

	int32 GetControlStyle()              { return DDControlStyle             ? DDControlStyle->GetInt()             : 0; }
	int32 GetStrafeStyle()               { return DDStrafeStyle              ? DDStrafeStyle->GetInt()              : 0; }
	int32 GetAnalogInputStyle()          { return DDAnalogInputStyle         ? DDAnalogInputStyle->GetInt()         : 0; }
	float GetReplayMontageErrorThreshold() { return ReplayMontageErrorThreshold ? ReplayMontageErrorThreshold->GetFloat() : 0.5f; }
	int32 GetCrowdIntensity()            { return CrowdIntensity             ? CrowdIntensity->GetInt()             : 0; }
	int32 GetGrabForceNext()             { return GrabForceNext              ? GrabForceNext->GetInt()              : 0; }
	float GetGrabChance()                { return GrabChance                 ? GrabChance->GetFloat()               : -1.f; }
	float GetGrabCooldownSeconds()       { return GrabCooldownSeconds        ? GrabCooldownSeconds->GetFloat()      : -1.f; }
	int32 GetGrabPressesToEscape()       { return GrabPressesToEscape        ? GrabPressesToEscape->GetInt()        : 0; }
	float GetGrabWindowSeconds()         { return GrabWindowSeconds          ? GrabWindowSeconds->GetFloat()        : -1.f; }
}
