---
name: reference-cryengine-build
description: "How to configure and build CryEngine 5.7.1 targets (AZ, plugins) from the command line, plus the traps hit on 2026-10-16"
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
