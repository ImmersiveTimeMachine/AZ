---
name: project-netlab-learning
description: "NetLab (C:\\Projects\\NetLab) — Artur's learning project: UE-style replication over WinSock UDP (prediction, reconciliation, interpolation, RPC via C++26 reflection). ★★★ I write all code into files with teaching comments; Artur studies; replication = the focus."
metadata:
  node_type: memory
  type: project
  originSessionId: 69252bd1-e0bc-40bf-ad8d-f297854e552c
  modified: 2026-10-08T03:31:13.492Z
---

Started 2026-10-07. Separate repo `C:\Projects\NetLab`, outside AZ. GitHub: git@github.com:ImmersiveTimeMachine/ReplicationLearning.git (branch main, SSH; local git identity copied from AZ). Two console apps (server.exe, client.exe) + libs
nl_core (platform utils) → nl_net (serialization/transport/reliability/replication) → nl_game (protocol, shared sim).
Terminal arena: own letter = AutonomousProxy (predicted + reconciled), others = SimulatedProxy (interpolated),
RPC names mirror CMC (ServerMove / ClientAdjustPosition / ClientAckGoodMove / MulticastChat). Roadmap of 10 stages
in NetLab/docs/ROADMAP.md.

Toolchain: GCC 16 (WinLibs, UCRT, POSIX) with `-std=c++26 -freflection` — the only compiler with C++26 reflection
(P2996 + P3394 annotations + P3096 param reflection + P1306 template for) as of 2026-10. MSVC has none; CLion
2026.2 highlights reflection only with GCC ≥ 16.1. CLion built-in MCP server → I drive builds/runs through it.
Installed 2026-10-07: `C:\Toolchains\winlibs-gcc16\mingw64` = WinLibs GCC 16.2.0 r2 (sha256 verified); reflection
smoke test passed (annotations, template for, parameters_of, splice call). TRAP: std::print on MinGW needs
`-lstdc++exp` (linker: undefined std::__open_terminal) — already in cmake/NetLabTarget.cmake. Its gdb 18.1 prints a
PYTHONHOME warning (pretty-printers) → CLion should use bundled GDB. Skeleton builds with `cmake --preset debug`.

**★★★ RULE (revised 2026-10-08): I write ALL code straight into the project files, with rich teaching comments;
Artur studies it.** History: first "code in chat, he transfers" → then "писать здесь в окне потом копипейст никакого
смысла". Emphasis: "Мне сами принципы репликации важны намного, чем сокет" — infrastructure (socket, packets, acks,
reliable channel, simulator, console) can be thorough but is secondary; replication stages get the depth.
**How to apply:** per step: write code in files (comments explain WHY + UE analog with real file path), build + run to
verify, then in chat a SHORT explanation of the idea + what to look at / what experiment to try. No code dumps in chat.

TRAP: CLion's clangd 23 flags every std::print/std::format with string args as "call to consteval function ... not a
constant expression" (libstdc++ 16 <format>). False positive — GCC builds clean. FIXED by project `.clangd` → `Diagnostics: Suppress: [invalid_consteval_call]` (verified on/off via get_file_problems). Root cause = CLion feeds clangd GCC 16's predefined macros; exact macro not isolated — don't chase it again.

CLion setup (2026-10-08): CLion 2026.2.3.1 (installed over the "CLion 2025.2" folder). Its default "Debug" profile
already uses the WinLibs GCC 16 toolchain → builds into cmake-build-debug (preset profiles exist but are disabled).
CLion MCP = global `clion` entry, http://127.0.0.1:64362/stream, answers initialize without a token
("CLion MCP Server"). Port 64462 is a different restricted endpoint — ignore it.

STATE 2026-10-08 (commit 34c2b8d, pushed): ALL stages 0-12 done, 52 tests green (10/10 stable), 0 warnings,
verified live in CLion (shoot+OnRep, chat, reliable test 100/100 on bad net, push/speedhack corrections).
Guide: docs/USER_GUIDE.md (Mermaid diagrams, experiments table, UE mapping, CMC vs NPP/Mover). Logic lives in
libs nl_server/nl_client (exes = main.cpp only) so tests run server+clients in-process on virtual time.
GCC 16 reflection traps: never capture std::meta::info in lambdas or use it in runtime expressions — bind to
constexpr first; pass member pointers as template params (InvokeWith<ptr>); WriteRpc uses decltype(&[:Fn:]) only.
Live-test helper: scratchpad sendkey.exe <pid> <vkhex>[:holdMs] (AttachConsole + WriteConsoleInput).
After adding files CLion needs "Reload CMake Project" (else "Cannot resolve symbol" in new files).
LAUNCH (2026-10-08, commit 954e8a7): CLion starts programs HIDDEN -> AllocConsole window invisible + 3x5 font; resizing
it breaks it to 1 row. Fix: ShowWindow when we allocated; resize only when GetConsoleProcessList()==1 (own console).
Reliable way = run_all.cmd (ASCII+CRLF; `start "t" /D dir exe args` directly — `cmd /c "mode con ... & exe"` silently
never started the exe; `timeout` fails with redirected stdin -> use ping). User confirmed "все работает" via this.
Processes started by my Bash/PowerShell via `start` persist after the tool call; PowerShell capturing 2>&1 of
run_all hangs (children inherit pipes) — don't capture output when launching windows.
