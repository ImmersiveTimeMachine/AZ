# CHALK option 01 holster rebuild

Approved design: C:/UnrealEngine/Games/AZ/UI Design/CHALK_Pistol_Holster_v01/concepts/01_Thigh_Canvas_Holster.png.

This version replaces the rejected flat-blockout construction with a modeled fabric pocket, rim/lining, shaped seams, folded webbing and metal hardware. Only option 01 is in scope. Existing backpack and character files are preserved.

The actual game pistol is exported read-only as a sizing and appearance reference. Pistol geometry stays in a separate reference collection and is excluded from the holster-only export collection.

## Current file

`C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02/CHALK_Holster01_Rebuilt.blend`

- Collections 01-04 contain the editable holster geometry, webbing, seams and hardware.
- `REFERENCE / actual AZ pistol / toggle` contains the real game pistol at its original scale. Hide this collection to inspect the empty cavity.
- Studio helpers and the thigh envelope are hidden in the working viewport.
- The approved option01 image and original pistol base color are packed into the file.
- All holster components follow `HOLSTER_ROOT / thigh attachment`.

Native renders: `Holster01_Rebuilt_Preview.png` and `Holster01_Rebuilt_Empty.png` in this directory. These are renders of the saved mesh, not new image-generated proposals.

The editable modeling master keeps its procedural Blender materials. The prior model remains in its original v01 directory.

## Unreal export completed

Saved asset: `/Game/AZ/Assets/Items/PistolHolster01/Meshes/SM_CHALK_PistolHolster01`.

The derived static mesh uses one material, one normalized UV atlas, and three 4096-square baked textures (BaseColor, tangent Normal, and ORM). Its 155,200-triangle export copy reduces thread tessellation and one dense surface subdivision while retaining the detailed 398,516-triangle modeling master. Measured overall bounds differ by less than 0.015 mm; the maximum measured local surface change was 0.945 mm.

The native Unreal material uses BaseColor, Normal (green flipped from Blender's +Y convention), and packed ORM (R ambient occlusion, G roughness, B metallic). The mesh and four material/texture assets were saved and their real getters rechecked. No reference pistol, studio helpers or thigh dummy were exported. Rigging and hero attachment are not implemented by this asset import.

- `CHALK_Holster01_UnrealExport.blend`: baked and consolidated export copy.
- `exports/SM_CHALK_PistolHolster01.fbx`: single static mesh source.
- `textures/T_CHALK_PistolHolster01_*.png`: three 4096-square maps; BaseColor is 8-bit sRGB, Normal and ORM are 16-bit linear data. The original 16-bit sRGB BaseColor is archived under textures/source_16bit because Unreal treats 16-bit source images as linear.
- `exports/export_manifest.json`: source hashes, UV/geometry/bake evidence.
- `exports/unreal_import_report.json`: saved Unreal paths and verification.

Reimport through `scripts/import_holster_unreal.py` only after reviewing changed export files. Existing owned assets require the explicit `holster_replace_owned=True` retry setting. The original model and existing Unreal pistol remain unchanged.

The mesh uses full-precision UVs and high-precision tangents to preserve the small atlas islands used by the stitching. Full float source normals and the normal map remain editable in the export master.
