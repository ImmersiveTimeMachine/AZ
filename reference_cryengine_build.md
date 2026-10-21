---
name: reference-cryengine-build
description: "How to configure and build CryEngine 5.7.1 targets (AZ, plugins) from the command line, IncrediBuild (ib_build.py, remote helper DESKTOP-HNBF0TK), plus traps"
metadata:
  node_type: memory
  type: reference
  originSessionId: 8f8d09b9-3f5e-408a-8d76-c4e4c3910763
  modified: 2026-10-17T00:33:39.408Z
---

- **Solution:** `C:/CryEngine/solutions_cmake/win64/CRYENGINE_Win64.sln`, generated for VS 2022 x64, configs Debug, Profile and Release.
- **Reconfigure** after adding a target:
  ```
  "C:/CryEngine/Tools/CMake/Win32/bin/cmake.exe" -S C:/CryEngine -B C:/CryEngine/solutions_cmake/win64
  ```
  It takes about 1–2 minutes; Rider reloads the solution.
- **Build one target:**
  ```
  cmake.exe --build C:/CryEngine/solutions_cmake/win64 --config Profile --target <Target> -- -m -nologo -v:minimal
  ```
  Use dash switches: Git Bash rewrites `/m` into the path `M:/` and MSBuild fails.
- **Where AZ is wired in:** the engine `Tools/CMake/Build.cmake` includes `C:/CryEngine/AZ/Code/CMakeLists.txt`. AZ plugins are added from there with `add_subdirectory(Plugins/<Name>)`.
- **RC trap:** a `CryEngineModule` gets an auto-generated `.rc` that `#include "resource.h"`. A new module needs its own `resource.h` and its source dir in the include dirs, otherwise RC1015.
- **First AZ plugin:** `AZ/Code/Plugins/AZNoesis`. It outputs to `AZ/bin/win_x64` and is referenced in `Game.cryproject` as `bin/win_x64/AZNoesis.dll`. Its `Noesis.dll` is copied next to the exe (`C:/CryEngine/bin/win_x64`).
- **Editing `Game.cryproject` while Sandbox runs is safe:** ProjectManager rewrites the file only on a version upgrade or when it is missing.

Related: [[project-cryengine-dx12]].

## IncrediBuild (2026-10-20)
- ITMPC = IB Coordinator + initiator (32 cores); remote helper **DESKTOP-HNBF0TK** 16 cores (licence 16 floating helper cores). It drops offline at times; then only local cores are used.
- Build through IB: `python -I C:/CryEngine/AZ/tools/ui/ib_build.py <Target> --agent <me> [--paths ...]` (BuildConsole /COMMAND=MSBuild under agent_lock run). Doc: `AZ/docs/implementation/incredibuild-build.md`.
- Measured: one module (unity, ≤45 units) = no gain, IB keeps it on 32 local cores (+~5 s overhead); 7 engine modules / 108 units 49.4 s → 43 s, ~30 tasks remote. `/AVOIDLOCAL=ON` proves the helper works (44/45 remote).
- Traps: a quoted exe path inside /COMMAND="..." breaks after subprocess re-quoting ("Only one filename to build") → put MSBuild dir on PATH, call `MSBuild.exe`. Python CreateProcess resolves a bare exe with the PARENT's PATH, not env=. Git Bash mangles `/NOLOGO`-style switches → call BuildConsole from PowerShell/Python. `xgCoordConsole /EXPORTSTATUS` fails ("Invalid argument to date encode"); helper state is in Logs/CoordinatorCore.log + LicenseService.log.
- Idle `MSBuild /nodemode:1` nodes left by `cmake --build -- -m` block agent_lock for 30 min → ib_build.py passes /nodeReuse:false.
