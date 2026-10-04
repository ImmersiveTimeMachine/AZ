// Copyright Artur. AZ project.
// ngtest: standalone parity harness for the Natural Grip core (no Unreal). It compiles the plugin's Core sources and
// compares their results with the Python solver's (Tools/wgs/natgrip) on the same inputs.
//   ngtest prims <dump.json>        primitives vs py/dump_prims.py
//   ngtest support <args...>        support (left) hand solver, TestSupport.cpp
//   ngtest trigger <args...>        trigger (right) hand solver, TestTrigger.cpp
//   ngtest geometry <args...>       mesh parts + field bakes, TestGeometry.cpp
//   ngtest jsondiff <ref> <got> [prefix]   result JSON vs a reference (prefix: top-N lists)
// build.bat [all|support|trigger]: NG_WITH_SUPPORT / NG_WITH_TRIGGER select the solver units compiled in.
#include "NGTest.h"

#include "NGJsonDiff.h"

#include <cstdio>
#include <cstring>
#include <exception>

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::printf("usage: ngtest prims <dump.json> | support <args> | trigger <args>\n");
		return 2;
	}
	try
	{
		if (std::strcmp(Argv[1], "prims") == 0 && Argc >= 3)
		{
			return ngtest::RunPrims(Argv[2]);
		}
		if (std::strcmp(Argv[1], "jsondiff") == 0 && Argc >= 4)
		{
			ng::JValue Ref, Got;
			std::string Err;
			if (!ng::LoadJsonFile(Argv[2], Ref, Err) || !ng::LoadJsonFile(Argv[3], Got, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return 2;
			}
			ng::JsonDiffReport R;
			ng::JsonCompare(Ref, Got, R, Argc >= 5 && std::strcmp(Argv[4], "prefix") == 0);
			std::printf("jsondiff %s\n", R.Summary(1e-9).c_str());
			return R.Ok(1e-9) ? 0 : 1;
		}
#if NG_WITH_SUPPORT
		if (std::strcmp(Argv[1], "support") == 0)
		{
			return ngtest::RunSupport(Argc - 2, Argv + 2);
		}
		if (std::strcmp(Argv[1], "fast") == 0)
		{
			return ngtest::RunFast(Argc - 2, Argv + 2);
		}
		if (std::strcmp(Argv[1], "power") == 0)
		{
			return ngtest::RunPower(Argc - 2, Argv + 2);
		}
#endif
#if NG_WITH_TRIGGER
		if (std::strcmp(Argv[1], "trigger") == 0)
		{
			return ngtest::RunTrigger(Argc - 2, Argv + 2);
		}
#endif
#if NG_WITH_GEOMETRY
		if (std::strcmp(Argv[1], "geometry") == 0)
		{
			return ngtest::RunGeometry(Argc - 2, Argv + 2);
		}
#endif
	}
	catch (const std::exception& E)
	{
		std::printf("ERROR %s\n", E.what());
		return 2;
	}
	std::printf("unknown or not compiled-in mode %s\n", Argv[1]);
	return 2;
}
