---
name: AZ project CVar lifetime rule — module-managed register / name-based unregister
description: For UE Mover/Live-Coding projects, console variables must be registered in FAZModule::StartupModule and unregistered by NAME in ShutdownModule — never as static FAutoConsoleVariable, never via the IConsoleObject*-based UnregisterConsoleObject overload
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
When adding a console variable in the AZ project, do **not** use `static TAutoConsoleVariable<T>` and do **not** unregister via the `IConsoleObject*` overload. Use `IConsoleManager::Get().RegisterConsoleVariable(Name, Default, Help, Flags)` in `FAZModule::StartupModule` and `IConsoleManager::Get().UnregisterConsoleObject(Name, false)` (name overload) in `ShutdownModule`. The full pattern lives in `Source/AZ/Public/AZ_ConsoleVariables.h` and `Source/AZ/Private/AZ_ConsoleVariables.cpp` — add new CVars there.

**Why:**

Two distinct editor-shutdown crashes blocked us before this rule landed:

1. **Static-FAutoConsoleObject dtor crash.** `static TAutoConsoleVariable<T>` is a `FAutoConsoleObject` whose destructor runs after `FConsoleManager` has begun teardown. With Live Coding patches in play, the patch DLL and the original DLL both run static dtors and crash at `FConsoleManager::FindConsoleObjectName` (`ConsoleManager.cpp:3297`). Eliminating the static dtor (by manual register/unregister in module lifecycle) fixes this.

2. **Stale IConsoleObject* dtor crash.** Even with module-managed lifetime, calling `UnregisterConsoleObject(IConsoleObject*, bool)` with a cached pointer crashes if the object was already freed by an earlier shutdown phase or invalidated by Live Coding. The pointer overload internally calls `FindConsoleObjectName(InVar)` which dereferences `InVar->GetParentObject()` (engine line 3297) — a vtable call on freed memory. The **name-based** overload `UnregisterConsoleObject(const TCHAR*, bool)` does an internal `FindConsoleObject(Name)` and is null-safe: missing entries are no-ops.

**How to apply:**

- New CVar → declare `extern IConsoleVariable*` + a getter in `AZ_ConsoleVariables.h`, define + register in `AZ_ConsoleVariables.cpp::RegisterAll`, add a name-based `Drop(...)` line to `UnregisterAll`.
- Call sites use the inline getter (`AZCVars::GetXxx()`) — never reach for the cached `IConsoleVariable*` directly except in the unregister path.
- Never write `static TAutoConsoleVariable<T>` in any AZ module file — it bypasses `AZCVars::UnregisterAll` and reintroduces both crashes. If you see one in a code review (yours or generated), move it.
- Never use the `IConsoleObject*` overload of `UnregisterConsoleObject`. Engine code uses it internally, but our project rule is name-based for safety.
- This is a project rule, not an engine bug we can fix. Engine modules + plugins still have their own static `FAutoConsoleObject` instances — Live-Coded sessions can still crash on close from *those*. The mitigation in that case is to do clean rebuilds (skill `cpp-build-livecoding`) when you'll close cleanly, not Live Coding.

**Reference commit:** `feature/rootmotion` — the file pair (`AZ_ConsoleVariables.h/.cpp`) was created and `FAZModule` was introduced in `Source/AZ/AZ.cpp` to call `RegisterAll`/`UnregisterAll` on module lifecycle.
