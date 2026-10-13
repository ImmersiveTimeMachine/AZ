---
name: project-window-glass-variants
description: "Cracked/broken window glass 2026-10-13: Painter project with 4 variant groups + parametric Substance Designer graph window_glass_cracked (v13) for generating any number of cracked windows"
metadata:
  node_type: memory
  type: project
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-13T17:50:45.675Z
---

Classroom window glass (CHALK, post-apocalyptic) as of 2026-10-13. User liked the look ("очень неплохо").

**Painter** `Art/Painter_MCP/Projects/Classroom_WindowGlass_v01.spp` (user's glass plane, 3 panes): groups `VARIANT Long crack` (one crack across the whole window), `VARIANT Single crack`, `VARIANT Meshy style broken`, `VARIANT Procedural broken` — toggle visibility. Exports `Art/Painter_MCP/Exports/Classroom_WindowGlass_v08_*` and `_v09_longcrack`; Blender `Plane` has one material per variant (`WindowGlass_Painter_v08_*`, `_v09_longcrack`). Scripts in `Tools/painter_mcp` (gen_glass_*.py, gen_broken_glass_masks_v03/v04_meshystyle.py, glass_finish_v01.py, painter_window_glass_v08/v09_longcrack.py).

**Designer** `Art/WindowGlass_Designer/v13_cracked_glass_20261013/window_glass_cracked_v13.sbs/.sbsar` + README: one pane per 0..1 texture; Crack type Clean/Single/Star/Broken, impact, rays, long straight cracks, hole made of whole fallen shards, dirt; seed = new window. Batch via `Tools/substance/sbs_cook_render.py ... --set id@value`.

**Why:** user wants to generate many different cracked windows for the school blocks ([[reference-ue-to-blender-tool]]).
**How to apply:** new glass looks → change params/seed of the Designer graph first; Painter MCP rules in [[reference-painter-mcp]]; Designer node traps were added to the skill's engine-rules.md (Transformation offset in source space, polar node convention, flood fill quirks). User rule: a given model (Meshy) is a style REFERENCE, never stitched in pieces; one pane = one continuous sheet.
