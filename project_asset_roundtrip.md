# Reusable Unreal / Blender mesh workflow

User explicitly requested remembering this workflow for frequent reuse (2026-09-12 Toronto / 2026-09-13 UTC).

Use skill `ue-blender-asset-roundtrip` at:
`C:/Users/Artur/.codex/skills/ue-blender-asset-roundtrip/SKILL.md`

It covers source export or an existing Blender import, source archive, semantic separation, UV/weight/normal preservation, standalone pivots, rigged assembly exports, explicit native FBX import, original material reuse, verification and safe reimports. The skill references the actual Backpack2 scripts/receipts and records discovered failure modes.

User preferences from this work:

- Keep originals intact and place derived Unreal assets in a dedicated project-owned folder.
- Standalone items need useful baked local pivots: axe/knife at the grip; no character-offset item origins.
- Also retain a **backpack-only rigged version** with the source skinning and character placement. This is separate from the loose item's pivot and does not imply every prop needs a rigged copy.
- Preserve original Unreal materials and verify actual assignments, not just intended values printed by scripts.

Completed outputs: `/Game/AZ/Assets/Items/Backpack2`.
`Meshes/Assembly/SKM_Backpack2_Backpack` is the rigged bag/harness only.
`Meshes/Items/SM_Backpack2_*` are the seven standalone meshes with useful pivots.
Art sources, FBXs, manifests and audits: `C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/`.
Editable pivot version: `Backpack2_GameReady.blend` under that root.
Source knife blade remains truncated/open; separation did not model its missing tip.

Read the skill instead of rediscovering the normals encoding, armature/root, unit conversion or Unreal struct-array material assignment issues. Do not run the Backpack2-specific scripts against another asset without adapting their guards and manifests.

## Holster option01 (2026-09-13)

User approved rebuilt rounded canvas holster (earlier flat v01 rejected) and requested export to UE. COMPLETE: `/Game/AZ/Assets/Items/PistolHolster01/Meshes/SM_CHALK_PistolHolster01`, 1 native material + 3 baked4096 textures under sibling Materials/Textures. 155,200 triangles,1UV atlas,1materialslot. Source398,516tri modeling master retained; copy-only thread tessellation/dense surface subdivision reduction. Actual pistol was only a reference and not exported into this mesh. No rigging, gameplay attachment or PIE performed.

Sources/receipts: `C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02/README.md`; `CHALK_Holster01_Rebuilt.blend` is the approved master, `CHALK_Holster01_UnrealExport.blend` the baked copy. Bake/export and guarded UE import scripts are in its scripts directory. Read exports/unreal_import_report.json for authoritative saved results.

New native traps: Blender5.2 CombineRGB/SeparateRGB replaced by Color nodes; reacquire UV-layer handles after Edit Mode; use raw image.save (not save_render) for baked data, BC sRGB/Normal+ORM Non-Color. Query CUDA specifically if device discovery scans crash Intel oneAPI. UE5.8 default texture output can report RGB rather than empty-name alias. DeleteAllMaterialExpressions iterates a shrinking expression list and can skip nodes on retries: delete expressions from a copied list, then verify empty before rebuilding the owned material. Material/UV/bounds getters verified after saves.

FINAL color-transfer correction: Unreal5.8 treats RGBA16 source data as linear regardless of SRGB=true (ImageCore.h GetFormatNeedsGammaSpace; Texture.cpp FTextureSource::GetGammaSpace; ImageWrapperBase.GetRawImage uses default gamma and PNG decoder does not decode sRGB16). An sRGB-encoded16bitBaseColor therefore looked washed out. Export BaseColor as8bit sRGB with neutral Standard/exposure0/gamma1 (no inheritedAgX); keep Normal/ORM16bitlinear. NativeconversionpreservedencodedRGBwithin1/255. Original16bitBC archived. FinalUEpreview matchesdarkcanvas/olivewebbing. FullprecisionUVs andhighprecisiontangents enabledforfinethreadUV islands. Successfulfinalreceipt exports/unreal_import_report.json has5savedassets,155200tris,1UV1material; no heroattachmentyet.
