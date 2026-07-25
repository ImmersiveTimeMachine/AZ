---
name: feedback_editor_close_build_open_loop
description: "RETIRED BY USER 2026-07-24 — do NOT close/reopen the editor programmatically; the USER drives the editor lifecycle. Root cause of retirement: programmatic quit reliably CRASHES on exit (SAssetShortcut::~SAssetShortcut, Persona asset-family bar) whenever asset-editor tabs are open — happened on BOTH automated cycles. Mechanics kept for reference."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-24T17:47:31.772Z
---

# Editor close→build→reopen automation — RETIRED

**Why:** the user retracted the automation after the second cycle ("ok forget about close open loop UE
Editor crashed"). Both automated quits crashed identically at EXIT in
`SAssetShortcut::~SAssetShortcut` (Persona asset-family shortcut bar, AssetFamilyShortcutBar.cpp:195,
null deref) while destroying open asset-editor windows (first: an anim asset tab; second: the
BP_GA_ChalkieGrab window). Crash is AFTER save-all — no data loss — but it spawns the crash reporter
and reads as instability.

**How to apply:** for editor-closed builds, ASK the user to close the editor and to reopen it after
`Result: Succeeded`. Never call `quit_editor` / launch UnrealEditor.exe yourself.

**Reference mechanics (verified working, use only if the user explicitly re-authorizes):**
- Save-all: python `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`.
- Quit: python `unreal.SystemLibrary.quit_editor()` (console `QUIT_EDITOR` via the bridge NO-OPS).
- Build: standard CLI Build.bat; relaunch: `Start-Process UnrealEditor.exe <uproject>`; bridge back in
  ~20-60s (poll `unreal_status` for `connected:true` — the :3000 HTTP server answers even without the editor).
- Rider attach (fine ON REQUEST after a user reopen): `mcp__rider__attach_to_process(pid,
  rootFolder="C:/UnrealEngine/Games/AZ")` — engine root path is rejected; verify via
  `xdebug_get_debugger_status`.

Related: [[feedback_cpp_executescript_harness]] (bridge CRLF gotcha, unprefixed HTTP tool names).
