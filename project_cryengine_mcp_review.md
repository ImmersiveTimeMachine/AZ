---
name: project-cryengine-mcp-review
description: 2026-10-16 independent review of the Codex-built CryEngine MCP (C:/CryEngine/Tools/CryEngineMCP); report path and non-obvious systemic facts
metadata:
  node_type: memory
  type: project
  originSessionId: 0cf39d47-f3b0-4a50-9b24-845e8ec0506f
  modified: 2026-10-16T18:10:21.951Z
---

Artur's side project: a local MCP for CryEngine 5.7.1 Sandbox (C:/CryEngine, project TPS), built by a Codex agent. My full review (snapshot review-snapshot-v01) is at `2026-10-15/realtime-voice-chat/outputs/cryengine-mcp-architecture-review/REVIEW.md`.

Non-obvious facts found (verify before reuse — code was still changing during review):
- Pairing and scene-job journals sit in `%LOCALAPPDATA%`, and the packaged Codex app sees a private shadow (`AppData/Local/Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/CryEngineMCP`). Jobs exist only there; repairs were done by hand by copying endpoint.json into the shadow.
- CGF hard limit `MAX_SUB_MATERIALS 128` (CryHeaders.h:11); the Blender producer puts every selected material on every mesh.
- The engine's shaders are locally modified (April 2026): Illum.ext %SPECULAR_MAP depends on $TEX_Smoothness (in effect no dependency), and in Glass.cfx BumpScale scales the normals.
- Tools/CryEngineMCP is not under git; another agent kept editing it after the "freeze".

2026-10-16 P1 fixes applied (REVIEW.md §7; pre-fix backup next to it). Git was skipped on the user's decision; repo: github.com/ImmersiveTimeMachine/CryEngine_MCP (private, no gh CLI — I did not push).
- Pairing and jobs now live in `Tools/CryEngineMCP/runtime/` (runtime_paths.py), with fallback to legacy AppData.
- client.py now resends an undelivered same-session step and forwards CRYENGINE_MCP_* env vars; the MCP SDK passes only a whitelist to nested servers.
- The producer writes compact per-mesh material slots.
- Checks: `checks/offline_contracts.py` (18/18), `tool_inventory.py`, `verify_install.py`.
- T1 offline: the current producer reproduced all 19 accepted class materials.
- Still pending: native import of the compact bundle (second project); bridge 1.5.2 and adapter 1.4.2 need a Sandbox rebuild/restart.

Blender add-on Tools/BlenderCryEngine (Send to CryEngine) is installed in Blender 5.2. Meshy assets bring packed JPEGs, a glTF normal `uv_map="UVMap"` and a 2048 ORM next to a 4096 base/normal.
- JPEG decoding and explicit first-UV acceptance are fixed.
- The ORM size mismatch is blocked by the "no implicit resampling" rule; I asked the user to decide (recommended: explicit ORM upsample).

2026-10-16 v05 follow-up assessment of the frozen handoff v04 (native 1.11.2 / bridge 2.2.1, 156 tools): `outputs/cryengine-mcp-followup-assessment-v05/results/` (REVIEW, CHECKS-MATRIX, TOOLS-MATRIX).
- On the user's request I fixed P1 (request_id must be `[A-Za-z0-9][A-Za-z0-9._:~-]{0,127}`; otherwise INVALID_REQUEST_ID before sending) and P2 (a failure after the native commit → `NATIVE_COMMITTED_FINALIZATION_FAILED` + `native_effects_committed`).
- The fixes went into `server.py`, `plugin/bridge.py` (2.2.2) and `scene_worker.py`. Backup, patch and hermetic tests (24/24) are in `.../v05/fix/`.
- Bridge 2.2.2 is live since the user's restart at 13:58 (session b6b32100); a read-only check passed.
- Fixed (live since 14:10): `call()` generates a UUID only when `request_id is None`; an empty string → INVALID_REQUEST_ID. Harness 25/25.
- Traps:
  - The author's reliability matrix has 2 cases that fail on v04 itself (D01, S01/S02), so they are not a regression.
  - Windows MAX_PATH breaks that matrix if the output folder name is long.
  - Running checks against the frozen snapshot writes `.pyc` into it, because the nested MCP server does not get `PYTHONDONTWRITEBYTECODE`. Clean up `__pycache__` and re-verify the manifest.
  - The ~230 ms per HTTP request is on the editor side: `Py_Initialize` is called without `PyEval_SaveThread`, so the bridge thread advances only on the 40 ms Qt tick. This is a hypothesis.

**Why:** follow-up requests about this MCP will build on these findings.
**How to apply:** re-hash the live sources against the snapshot manifest before trusting line numbers.
