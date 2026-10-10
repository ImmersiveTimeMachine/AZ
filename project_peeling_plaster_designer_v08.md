---
name: project-peeling-plaster-designer-v08
description: "CHALK peeling-plaster Substance Designer material v08 (2026-10-09) — offline SBS generator workflow, plate-quantised peel mechanism, status in progress"
metadata:
  node_type: memory
  type: project
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-09T23:37:34.669Z
---

Task brief: `docs/task-briefs/substance-designer-plaster-task-and-status-2026-10-09.txt`.
Work folder: `Art/Classroom_AgedPlaster_Designer/v08_flakes_20261009/` (build_v08.py → CHALK_PeelingPlaster_v08.sbs/.sbsar, README.md, iter/ sheets). Tools: `Tools/substance/sbsgen.py` (writes .sbs from stock Adobe nodes), `sbs_cook_render.py`, `compare_sheet.py`.
Mechanism = Adobe old_painted_planks: Histogram Scan of (Clouds/Perlin field + Cells4 per-plate/chip random), quantised by Cells4 [Image input] small cells; edge style map mixes rounded cone lobes vs sharp polygon shards inside a flat-topped edge zone; cracks = Edge Detect of plate ids on the same cells.
Status 2026-10-09: user said "начинает выглядеть неплохо"; NOT accepted; not yet opened/assigned on the wall mesh in Designer (user does the clicks); wall preview mesh with 2 m-tile UVs: `Art/Designer_Wall_Preview/v02_20261009_uv2m/rear_wall1_designer_uv2m.fbx`.
Substance plugin for UE: Artur copied Fab "Substance in UE5" 5.8.1 (v72) into `Plugins/Marketplace/Substance` (gitignored); built from source with the source engine 2026-10-09 19:37 (Result: Succeeded, only C4996 warning); it pulls in USDImporter. Known 5.8 issues (user reports): forced texture compression; packaged games use SSE2 CPU engine unless SubstanceEngine.Build.cs non-editor branch is patched — plan is editor-only use + baked textures.
**Why:** prior v01–v07 own graphs and concrete_006 adaptation were rejected (wrong flake structure / horizontal scratches).
**How to apply:** continue from build_v08.py; next = user review in Designer, then 1K/2K/4K delivery, Blender/Unreal transfer, knowledge skills. Engine rules: [[reference-substance-designer-engine-rules]]. User taste: [[feedback-peeling-edges-varied]].
