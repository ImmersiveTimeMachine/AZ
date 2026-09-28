---
name: feedback-livecoding-header-reinstances-animbp
description: "★★ A Live Coding patch that touches a HEADER — even only a comment — reloads the class and re-instances it, which can leave AnimBPs deriving from it in a failed-compile state until a full editor-closed rebuild. Keep LC patches to .cpp only."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-28T00:28:14.658Z
---

**If a change is meant to go in via Live Coding, edit ONLY the .cpp. Never the header — not even a
comment.**

UHT re-runs on any header touch, Live Coding then reports a *class* change, and the log shows:

```
Re-instancing AZ_MoverAnimInstance after reload.
LogBlueprint: Compiling Blueprint '.../AZ_ABP_MoverHero_MHC'
Reload/Re-instancing Complete: 1 package changed, 1 class changed, ...
LogLiveCoding: Warning: Live coding succeeded, data type changes may cause packaging to fail...
```

Every AnimBP deriving from that class is force-recompiled in place, and they can come out broken:
`BlueprintLog: Warning: Blueprint failed to compile: <ABP>` with **no `[Compiler]` error text anywhere in
the log** — that silence is the tell. The graph is fine; the generated class is not.

**Why:** measured 2026-09-19. A patch whose only header change was a comment rewrite still re-instanced
`UAZ_MoverAnimInstance` and put both `AZ_ABP_MoverHero_MHC` and `AZ_ABP_MoverAnimInstance` into a failed
state. The ABP had compiled clean and run in PIE minutes earlier, so the correlation is decisive.

**How to apply:**
- Split the work: body change → .cpp, patch with Live Coding. Comment/doc change to the header → hold it
  until the next editor-closed build, or accept that a rebuild is now required.
- Diagnosing "can't compile ABP": grep the log for `Re-instancing` and `class changed`. If they sit right
  before the failure and there is no `[Compiler]` line, it is this, not the graph. Do not start editing
  graph nodes looking for the fault.
- Recovery is a full rebuild with the editor closed. LC patches do not survive a restart anyway, so
  nothing is lost.

**2026-09-27 (editor hang):** LiveCoding.Compile patches EVERY modified .cpp of the module, including OTHER sessions'
work: my .cpp-only grip patch also took AZ_PlayerState (whose header had changed after the DLL) + an engine NetCore
patch -> "Re-instancing AZ_PlayerState", recompiling BPs incl. the hero ABP -> the editor hung. BEFORE triggering LC:
`git status Source` + compare the mtime of every modified header (mine AND others') with UnrealEditor-AZ.dll; any
header newer than the DLL -> no LC, ask the user for an editor-closed full build. (A Rider launch rebuilds the DLL, so
after the user restarts from Rider the DLL may already contain the source - check its mtime before assuming loss.)

Related: [[feedback_parallel_build_header_edit_corruption]] (same class-layout family, different trigger),
[[feedback_python_gc_crash]] (never compile an AnimBP from Python either),
[[feedback_log_reading_traps]].
