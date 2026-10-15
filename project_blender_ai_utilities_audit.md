---
name: project-blender-ai-utilities-audit
description: "AZ AI Utilities 1.5.0 (2026-10-15) — native job engine; AI only writes data (High→Low inventory, inspection findings); bake fix, assemble-for-Unreal, rebuild from inventory, fit metric, Send to Unreal via UE Remote Execution"
metadata:
  node_type: memory
  type: project
  originSessionId: 2fe7c274-bddc-45e1-ab33-fd1c3fc14bd2
  modified: 2026-10-15T19:40:10.371Z
---

2026-10-15: after a 40-job audit Artur said "баги плюс все что ты предлагаешь"; I rebuilt `Tools/blender_codex_uv_task/az_codex_uv_task` as **1.5.0** and installed/reloaded it in Blender 5.2 (1.4.0 archived in `archive/1.4.0`).

- Engine `jobs.py` + `pipelines.py`: every button = live / background-Blender / process steps, timeouts, Стоп (kills tree), `status.json`, `step_NN_*.log`.
- AI only where judgement is needed and only produces data: High→Low (`construction_inventory.json`, must pass `python inventory_common.py <job>`), Inspect (`findings.json`), Fresh UV (unchanged). Native, no AI: Transfer Textures, Собрать для Unreal, Пересобрать из inventory, Repair, Send to Unreal.
- Bake root cause of the glossy-black cabinet: object-space auto cage on non-uniform scale + Blender `use_clear=True` fills alpha=1 so misses were invisible. Fix: world-space proxies, no clear, per-part coverage, push-pull hole fill, ray-based distance, uncovered parts skipped not black. Real locker: top black 85%→0%.
- Собрать для Unreal: Low parts → 1 mesh, one texel-density atlas, per-part cage, one bake; locker 73 parts → 1 mesh/2156 tris/4096 atlas, visible coverage 99.8%, 44 s; imported to UE in 7 s as `/Game/AZ/Assets/BlenderImports/_ToolSmokeTest/SM_AZToolTest_Lockers_v001` (my test asset; Artur keeps it to review — don't delete unasked).
- Inventory `repeat` (count+step, grid up to 3), rebuild from edited inventory in ~3 s, fit metric (silhouette IoU, visible Low→High, High→Low, hidden share).
- Verified in background on real data + engine smoke/Stop in live UI. A real Codex High→Low run with the new prompt: see job notes below if done.

**Why:** the agent previously ran fixed scripts (≈1M tokens, 40–400 s per click, approval/placeholder failures) and bake misses were silent.
**How to apply:** before editing these modules re-check mtimes (Codex edits them too — [[feedback-parallel-editor-edits]]); facts list for docs: README.md/CHANGELOG.md in the tool folder. Unreal Python from scripts → [[reference-ue-remote-execution]].
