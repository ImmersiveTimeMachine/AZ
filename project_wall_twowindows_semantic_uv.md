---
name: project-wall-twowindows-semantic-uv
description: "2026-10-11 v03 boarded two-window wall (Blender Wall_TwoWindows_SemanticLowPoly.001): geometry cleaned of z-fighting + semantic UV v03, 3 texture sets; UE not reimported"
metadata:
  node_type: memory
  type: project
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-11T04:07:52.837Z
---

User asked for a "logical" UV on the boarded two-window wall open in Blender (not Unreal) for Designer/Painter, then "no flicker, clean everything, make it perfect"; splitting into objects was offered and declined ("если не надо делить").
Live scene `Art/Classroom_AgedPlaster/v20261009_131411/Wall_TwoWindows_AgedPlaster_Comparison_v01.blend` (user saved it with Ctrl+S after v02; v03 edits are unsaved until they save again).
Result folder `Art/Wall_TwoWindows_Boarded/v20261010_SemanticUV/` — **v03 is current** (v02 kept the smeared pier-side texture in the window jambs; v03 keeps the surface owner: sill > reveal > bands > pier; board > frame > mullion > crossbar > residue) (README, FBX/blend v03, uv_templates_v03, scripts, rollback .blends).
- Cleanup: 32 coplanar duplicates deleted, partial overlaps cut along the bigger face outline (no vertex moves), 509 invisible faces deleted (parity AND ray-escape), 2 sliver faces restored after a 128-view hole test; 2559→2170 faces (v03), 0 coplanar pairs, holes only 22 px of silhouette raster noise (ray-verified covered).
- Source-model defects found: pier sides = reveal jambs, sill face = lower band, frame back made of overlapping rectangles at corners, mullion/crossbar ends inside the frame, quad+fan duplicate board fronts.
- UV0_Semantic first (old reprojection UV second); slots CHALK_WallPlaster 4K (1 UV = 3.5068 m), CHALK_WindowFrame / CHALK_Boards 2K (1.7534 m); 1168 px/m; 0 shared pixels per set; vertical faces world-up = +V; wall faces as seamless elevations; ID attribute `ID_Parts`.
**Why:** the old reprojection UV had one island per face; coplanar duplicate boxes flickered.
**How to apply:** UE still places the older `SM_Wall_TwoWindows_Boarded` (AgedPlaster_v01 geometry) — reimporting v03 FBX swaps geometry; ask first. Traps: [[reference-blender-socket-exec]].
- 2026-10-11 follow-up: user asked to rebuild the wall itself as clean concrete blocks separate from the windows (old model kept). Done: collection `ConcreteWall_Blocks_v01`, root empty at X=2.4 (set to 0 to overlay), 13 closed blocks (3 piers; per window LowerBand, Sill, SillNose, UpperBand, TopRecess), 0 coplanar overlaps, 0 T-junctions, UV0_Unique (1168 px/m) + UV1_World_m; files in `Art/Wall_TwoWindows_Boarded/v20261011_ConcreteBlocks/`. Generator `scripts/cw_gen.py` (exact dims taken from the source). Next likely ask: windows/frames/boards as separate clean objects.
- 2026-10-11 AAA pass (user: "максимально правильно, стандарты AAA"): game-ready `SM_CHALK_ConcreteWall_TwoWindows_A` in collection `ConcreteWall_AAA_v02` + `Art/Wall_TwoWindows_Boarded/v20261011_ConcreteWall_AAA/` (FBX with 13 UCX, ue_import_concrete_wall.py, README checklist). Real-world scale ×1.972419 baked (actor scale 1), corner pivot, watertight 198 tris, hard edges ≥30° = UV seams, UV0_Texture + LightMapUV (128, ≥4 texel padding), MikkTSpace. Not imported into UE yet (script ready). UE facts: 8 UV channels max; legacy FbxFactory auto-detects a UV set named "LightMapUV"; Interchange sets lightmap index to the first free channel. Blender trap: from_pydata already creates `sharp_face` (flat) — set use_smooth, don't attributes.new('sharp_face').
