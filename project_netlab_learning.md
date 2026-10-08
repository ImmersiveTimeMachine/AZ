---
name: project-netlab-learning
description: "NetLab (C:\\Projects\\NetLab) — Artur's learning project: UE-style replication over WinSock UDP (prediction, reconciliation, interpolation, RPC via C++26 reflection). ★★★ Artur writes the code himself; I only scaffold + explain + review."
metadata:
  node_type: memory
  type: project
  originSessionId: 69252bd1-e0bc-40bf-ad8d-f297854e552c
  modified: 2026-10-08T03:31:13.492Z
---

Started 2026-10-07. Separate repo `C:\Projects\NetLab`, outside AZ. Two console apps (server.exe, client.exe) + libs
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

**★★★ RULE: code goes in CHAT, Artur transfers it into the files himself.** (2026-10-07)
Quote: "сам я навряд ли буду писать… будем обсуждать… что мы будем выдавать в этом окне, я его буду просто
переносить. Но не надо за меня писать код полностью в самом файле."
**Why:** learning by moving every piece through his own hands; a file filled by me teaches nothing.
**How to apply:** I create only the skeleton on disk (CMake, presets, folders, placeholder files with a task comment +
UE analog, run configs, docs). Implementation = small pieces in chat, each with the why + UE analog + what to observe;
he pastes them. Never write the implementation into the project files. I MAY read his files to review/debug and build
via CLion MCP to check. Same rule lives in NetLab/AGENTS.md.
