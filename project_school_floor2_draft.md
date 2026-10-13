---
name: project-school-floor2-draft
description: "2026-10-13 Katimavik school geometry DRAFT in Blender: floors 1-3 in one file (plan v02 metres, classroom block linked) + floor-2-only draft + corridor proposal; user wants draft geometry only, materials later by hand like the classroom"
metadata:
  node_type: memory
  type: project
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-13T23:06:47.334Z
---

**All floors (current)** `Art/School_Katimavik_Floors/v01_draft_20261013/Katimavik_School_Floors123_v01.blend` (+ README, plan/axo/section/interior renders, `school_v01_F{n}_vs_drawing_v02.png`). Builder `scripts/bl_school_floors_v01.py`, numbers `school_layout_v01.py` (reads `C:/UnrealEngine/Games/MoCap/Katimavik_School/CHALK_Katimavik_Floorplans_v02.json`). Collections per floor (geometry / ceilings+slab / work lights), stairs A/B built run by run (1->2, 2->3), gym double height on floor 1, floor 3 has the gym lower roof. Floor Z: F1 -4, F2 0 (hero class), F3 +4.
**In Unreal (2026-10-13):** level `/Game/AZ/Maps/School/L_Katimavik_School_Draft_v01` (5625 StaticMeshActors named like the Blender objects, Outliner folders = Blender collections, 279 movable lights, sun/sky/fog, PlayerStart in the floor-2 corridor); assets `/Game/AZ/Assets/Katimavik/SchoolDraft_v01/{Meshes (116),Materials (M_SD_* + 87 MI)}`; classroom keeps its original UE materials. Source FBX + manifest `unreal_export/`; scripts `bl_export_school_to_unreal_v01.py` + `ue_import_school_draft_v01.py` (stages). Mapping UE = Blender(X,−Y,Z)×100, actor = S·M·S, MakeRotFromXZ — verified on 995 rotated actors.
**Floor 2 only** `Art/School_Katimavik_Floor2/v01_blockout_20261013/` (first version, kept). **Corridor proposal** `Art/Corridor_Katimavik/v01_proposal_20261013/` (prologue dressing; script also runs in floor mode CP_FLOOR/CP_FLIGHTS/CP_STAIR_Z0).
Block coords: X = plan y, Y = 90 − plan x. Deviations: real classroom block 8.4×9.2 (floors 2-3, hall from Y 16.7), corridor 3.2 (north row X 12.5), stair B 4.6 wide dog-leg with janitor/storage (F2-3) or lobby to the north corridor (F1), stair B north door dropped.

**Why:** user (2026-10-13): classroom is fine; build the corridor, then the floor, then "доделай все этажи в таком же стиле"; "сделай драфт только — материалы потом сами, как делали этот класс". Floor 2 is the hero floor (user double-checked).
**How to apply:** keep it a geometry draft; rebuild overwrites the .blend — version up if the user edited it. Render/rebuild with Blender 5.1 (see [[reference-blender-socket-exec]]). Slab rects must stop at the inner faces of exterior walls (coplanar slab/wall faces z-fight). Earlier school planning: docs/design-briefs/katimavik-school-*.md; classroom tool [[reference-ue-to-blender-tool]].
