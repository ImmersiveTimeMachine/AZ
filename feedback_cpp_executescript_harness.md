---
name: feedback_cpp_executescript_harness
description: "How mcp execute_script script_type=cpp actually works in AZ — it writes your code VERBATIM to Source/AZ/Generated/UnrealClaude/<@Description>.cpp and Live-Coding-compiles it with NO entry call, so code must run from a file-scope static initializer. ASCII-only @Description (non-ASCII corrupts the filename and poisons the cached makefile). Lets you reach private/protected UObject members without adding UFUNCTIONs or restarting."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

`mcp__unrealclaude__unreal_execute_script` with `script_type=cpp` is a live one-off C++ runner — far more powerful than I assumed. Verified by reading the plugin (`ScriptExecutionManager::ExecuteCpp`, 2026-06-14).

**How it works:** It writes `script_content` **verbatim** (no wrapping/template) to `C:\UnrealEngine\Games\AZ\Source\AZ\Generated\UnrealClaude\<sanitized-@Description>_NNN.cpp`, then triggers `LiveCoding.Compile` on the whole AZ module. There is **NO entry-point call** — nothing invokes your code. So your code MUST run from a **file-scope static initializer** (a `static` object whose constructor does the work; Live Coding runs it when it patches the new TU in).

**Why it's worth it:** the constructor runs as normal editor game-thread code with full access — `LoadObject`, reflection (`FindPropertyByName`/`FScriptArrayHelper` reach PRIVATE members like `UPoseSearchDatabase::DatabaseAnimationAssets`), direct access to protected fields C++-side (`UAnimSequence::Notifies`), and calling AZ statics (`UAZ_PoseSearchUtils::*`). This is how to read/mutate things Python can't (Python reported `Notifies` / `DatabaseAnimationAssets` as protected/unreadable). **No new UFUNCTION, no editor restart** — unlike adding methods to a util class.

**Template (use this shape):**
```cpp
// @Description: AZ_ascii_only_name_no_punctuation
#include "CoreMinimal.h"
#include "UObject/UnrealType.h"   // FArrayProperty / FObjectProperty / FScriptArrayHelper
// ... other engine + AZ headers ...
namespace {                       // anonymous ns = internal linkage = no cross-run dup-symbol clashes
  struct FRun { FRun() { /* work here; UE_LOG(LogTemp, Warning, TEXT("AZCPP ...")) */ } };
  static FRun GRun;
}
```
Filter output with a unique tag (e.g. `AZCPP`) via `unreal_get_output_log filter=AZCPP`.

**TRAPS (all hit on 2026-06-14):**
1. **ASCII-only @Description.** A non-ASCII char (em-dash `—`) became the filename; UBT recorded it in a different encoding than NTFS wrote → `C1083 Cannot open source file` that **persists in the cached makefile and fails EVERY later compile**. Fix: `rm` the phantom `.cpp` AND `rm Intermediate/Build/Win64/x64/AZEditor/Development/Makefile.bin`, then write a FRESH script (a *changed source* — see #2).
2. **Console `LiveCoding.Compile` alone won't re-run after only deletions.** Live Coding compiles only on a *changed/added source file*; deleting files is not a trigger. To force a clean rebuild after fixing the dir, submit a new `cpp` script (new file).
3. **Anonymous namespace is mandatory** if you run multiple cpp scripts in a session — each generated `.cpp` stays in the module; same-named file-scope symbols across TUs = duplicate-symbol link errors. Anon ns gives each its own.
4. **Clean up after.** `rm Source/AZ/Generated/UnrealClaude/*.cpp` when done — else they recompile and their static initializers RE-RUN on the next editor-start build. (A plugin `cleanup_scripts` tool also exists.)

Read compile result from `C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt` (`Result: Succeeded` / `error C####`); `.voltbl` warnings in LiveCodingConsole.log are non-fatal. Saving from the static init is risky — prefer marking dirty in cpp and saving via Python `EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False)` (note: `save_asset(path,...)` returned False for PSD/anim here — use `save_loaded_asset`). See skill `cpp-build-livecoding`, [[feedback_python_gc_crash]], [[feedback_posesearch_branchin_db_sync]].
