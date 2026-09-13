# Backpack2 — Unreal imports

Destination: `/Game/AZ/Assets/Items/Backpack2`

- `Meshes/Items/SM_Backpack2_*`: seven standalone static meshes. Pivots are baked into the mesh vertices; import translation/rotation are zero and scale is one. Each has one generated convex collision hull.
- `Meshes/Assembly/SKM_Backpack2_Backpack`: **backpack and harness only**, retaining the source skeletal hierarchy, skinning and character-space placement. This is the rigged backpack-only export requested for worn use.
- The other six `Meshes/Assembly/SKM_Backpack2_*` assets preserve aligned/skinned accessory copies for optional assembly use.
- `Rig/SKEL_Backpack2`: separate copy of the original SurvivalMan skeleton. The original skeleton was not used as an import mutation target.

## Standalone pivots

| Item | Origin and orientation |
|---|---|
| Axe | Center of wooden grip; head +Z, broad cutting edge +X |
| Knife | Center of wrapped grip; blade +X, flat sides along +/-Y |
| Bottle | Bottom center; upright +Z |
| Backpack | Center of main bag body, independent of dangling straps |
| Front pouch | Bottom center; upright, flap/button toward +X |
| Bottle holder | Center of carrier cloth |
| Rope | Center of loop; laid in its best-fit XY plane |

## Editable and reimport sources

- `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/Backpack2_GameReady.blend`: standalone item scene plus preserved assembly. Only the axe is initially visible in the item scene; toggle the other objects in the Outliner.
- `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/exports/Items/`: standalone FBXs.
- `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/exports/Assembly/SKM_Backpack2_Backpack.fbx`: rigged backpack-only FBX.
- `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/exports/Assembly/`: all aligned skinned FBXs.

Original Unreal material instances are assigned by full path and verified by fresh getters. Blender PNG preview materials were not imported. Original Unreal assets and the original separated Blender file remain unchanged.

Both sets retain all 14,612 source triangles. Unreal import bounds differ from expected transformed geometry by less than 0.00001 cm. Skinned imports retain all 101 mesh reference bones and their parent hierarchy; round-trip local pose differences are below 0.0021 cm / 0.008 degrees.

The original knife has a truncated/open blade tip; no blade completion was added during this task. Runtime equipment attachment and gameplay behavior have not been changed. No PIE or automated tests were started.

Verification details: `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection/unreal_import_report.json`.
