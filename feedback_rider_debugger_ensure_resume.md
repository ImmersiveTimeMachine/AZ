---
name: feedback-rider-debugger-ensure-resume
description: "★★★ USER RULE 2026-09-27: the editor runs under the Rider debugger (session \"AZ\"); ensures/asserts (often while saving or replacing BPs/assets) PAUSE it and it looks hung - check xdebug status and RESUME (= F5) via Rider MCP, then continue the work."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-27T20:21:26.022Z
---

User (2026-09-27): "Иногда при записи или замене блюпринтов или ассетов может выскочить какой-то ассерт. При этом
нужно нажать F5, продолжение. Проверяй... если вдруг увидишь, что завис по непонятным причинам редактор. Через MCP
райдера продолжи процесс." + "запомни это и запиши как правило".

**Why:** the editor is launched from Rider (run configuration `AZ`) with the debugger attached, so every `ensure`
breaks into the debugger and freezes the editor until someone presses F5. MCP calls then time out and look like a hang
(first seen 2026-09-27: `FAppTime::Get` ensure on the render thread in `UpdateReflectionSceneData` while an agent was
saving PoseSearch databases - engine noise, unrelated to our code).

**How to apply:**
- Whenever an MCP / Python call to the editor times out, stalls, or the editor "does nothing", and routinely after
  batches that save or replace assets: `mcp__rider__xdebug_get_debugger_status(rootFolder="C:\UnrealEngine\Games\AZ")`.
- `state: paused` -> `mcp__rider__xdebug_get_stack` (note the top frames: is it an ensure in engine code or in OUR
  code?) -> `mcp__rider__xdebug_control_session(action=RESUME, sessionId="AZ")` -> re-check it is `running` and
  continue the interrupted step (verify the step's result; the pause may have cut an async operation).
- An ensure in OUR code (Source/AZ) is a real bug signal: resume, but record the stack and report it to the user.
- A true crash (fatal error / access violation, RESUME does not help) is not this case - report, do not loop.
- Tell subagents that drive the editor the same rule (put it in their spec).
Related: [[feedback-editor-close-build-open-loop]] (editor restarts via Rider MCP, authorized by the user 2026-09-27).
