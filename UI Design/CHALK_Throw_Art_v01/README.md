# CHALK Quiet Sage production art

Art owner: **Codex**. Gameplay/runtime integration: **Claude**. User confirmed this division on September17,2026. This kit derives from approved Quiet Sage03; do not redesign the palette or substitute placeholder boxes for the supplied art.

**Current runtime uses the [sunlight readability revision](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/README.md>):** MI_QS_ArcSunlight/MI_QS_ContactSunlight, charcoal keyline,5.5px total envelope containing3.5px colored pulses, FilamentAlpha.55/PulseAlpha1. The original art below remains the baseline/source reference. Do not reset the active defaults to the earlier3px envelope/.31 alpha while integrating gameplay. The masks and meshes remain shared; six material siblings were added and assigned after user sunlight feedback.

Open [the production board](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/QS_Production_Art_Board.png>) or its editable [GIMP master](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/QS_Production_Art_Board_NATIVE.xcf>). Board scenes are flat dark/bright art swatches, not gameplay screenshots. Range/count are illustrative; the grenade icon is exported from the existing game texture without repainting.

## Sources

- `QS_ContactMasks_NATIVE.xcf`: native vector L corners, broken ellipse and center; packed as R/G/B coverage over black.
- `QS_Blocked_NATIVE.xcf`: native compact cross; runtime supplies localized OBSTRUCTED text.
- `QS_BodyContact_NATIVE.xcf`: compact corners and center for body contact.
- `unreal-art/T_QS_ContactMasks.png`:1024×512 coverage data. R=corners, G=ring, B=center. No baked tint or text.
- `unreal-art/T_QS_BlockedMask.png`, `T_QS_BodyContactMask.png`:256×256 coverage data.
- `sources/quiet_sage_arc.hlsl`, `quiet_sage_marker.hlsl`: editable code embedded in the owned Unreal material graphs. Runtime rendering does not read source files from disk.
- `sources/build_throw_art.py`: native GIMP vector authoring/export; XCF reopen checks preserve paths and vector layers.
- `C:/UnrealEngine/Games/AZ/Tools/throw_quiet_sage_art_setup.py`: owned-asset import/material/mesh authoring and readback. Does not rewire gameplay.

## Unreal asset contract

Destination: `/Game/AZ/Blueprints/Throwables/Art/QuietSageV1/`.

| Style property / use | Asset |
|---|---|
| ArcMesh | `SM_QS_Ribbon` |
| ArcMaterial | `MI_QS_Arc` |
| MarkerMesh | `SM_QS_MarkerPlane` |
| MarkerMaterial | `MI_QS_Contact` |
| Unsafe-launch compact glyph | `MI_QS_Blocked` on marker plane |
| Body-contact compact glyph | `MI_QS_BodyContact` on marker plane |

Master materials are `M_QS_Arc` and `M_QS_Contact`. All11 assets have been created, compiled where applicable, saved and read back. Original materials are preserved. **Codex assigned the four main references and128×64 padded marker canvas to BP_AZ_GA_Throw.PreviewStyle**; these apply on the next fresh Play. Existing cached PIE objects are not patched. Blocked/body variants still require Claude's runtime state wiring.

Arc: flat centered XY ribbon, X is forward, Y is width, +Z normal.32 longitudinal segments/64triangles, UV0.U increasing alongX and UV0.V spanning0–1 acrossY. Nominal bounds100×100×0cm; current renderer scales width and deforms length. No box caps, collision or Nanite. Marker plane has the same coordinate convention and one quad.

Arc material parameters exactly match the newer renderer: `Color`, `FilamentAlpha=.31`, `PulseAlpha=.94`, `FilamentWidth=.4`, `PulseSpacing=40cm`, `PulseDuty=.31`. **Custom Primitive Data float0=segment start distance cm; float1=segment length cm.** Phase is based on `CPD0 + U*CPD1`, preserving continuity across segments. Additional defaults: PulsePhase0cm, Brightness1, Opacity1, ExposureCompensation1. Exposure compensation reduces brightness changes from camera exposure; in-game tonemapping still needs visual review. A static thumbnail without CPD is not a valid pulse preview.

Contact material: `Color`=linear conversion of sage#B5C8B7, `CenterColor`=warm-white#EEEAE0; `MaskTexture`, `CornerAlpha=1`, `RingAlpha=.7`, `CenterAlpha=1`, `Brightness=1`, `Opacity=1`, `ExposureCompensation=1`. Packed texture is linear coverage, sRGB off, lossless RGBA storage with filtered mips/clamp addressing. All world materials keep depth testing and use two-sided translucent unlit rendering. No glow texture, lens effect or second ballistic curve is required.

### Padding and projection

The contact mask has a **128×64 reference-pixel full canvas**, containing **104×30px corner-anchor span** plus stroke and padding. The inner ellipse is60×18px, center10×4.4px. Use the full canvas when sizing the mesh; treating its entire UV range as104×30 shrinks the artwork. Blocked/body masks use32×32px full canvases.

The current renderer converts desired pixel dimensions to world size and then lays the plane on the surface. That does **not** compensate for surface foreshortening. Claude must choose bounded surface projection compensation or an equivalent depth-tested presentation to achieve the reference footprint at the comparison view without stretching excessively at grazing angles. Art cannot fix this through a different texture. Keep hit-point anchoring and correct normal orientation.

### Remaining runtime integration

The four main style references are assigned together in the ability defaults. Preserve them and refresh/recreate cached preview instances when runtime style changes. Do not bind these materials to the old cube/four-bar renderer. Verify CPD0/1 per segment, camera-facing XY strip, bounds and UVs. Keep pulse width3px/filament1.2px as visual calibration targets.

The current runtime does not expose all blocked/body variants, a total-path/end-fade input, or full range/HUD integration. Claude must connect these semantics; no material can infer the correct gameplay hit type or create localized HUD text. Keep first-contact truth when smoothing and changing blockers. Owner-only presentation, no preview collision/shadows, and final controls remain required.

MI_QS_Blocked defaults to warm white. The current ConfigureStyle overwrites every marker's Color with ArcColor, so preserve/apply the state-appropriate tint when adding blocked-variant selection; switching the material alone is insufficient.

Use the already-existing `/Game/FPS_Controller/UI/Textures/T_FragGrenadeIcon.T_FragGrenadeIcon` through the item view for the grenade row. Keep selected grenade/idle/categorization/locomotion requirements in the current completion work order.

## Verification receipts

GIMP sources were saved and reopened; per-document `_layers.json` retains vector/path counts, and `gimp-review-open.json` confirms the board opened cleanly in GIMP. `sources/production-receipt.json` records mask dimensions/channel meanings. Both master materials compiled without errors. Saved mesh readback confirmed ribbon66vertices/64triangles, marker4vertices/2triangles, UV0 spanning0–1 on both axes and Nanite off. Asset Registry confirms BP_AZ_GA_Throw references all four new art assets; affected packages are clean.

Receipts: `C:/UnrealEngine/Games/AZ/Saved/ThrowArtProduction/art-create-receipt.json`, `art-final-readback.json`, `mesh-uv-and-assignment-verify.json`, `style-assignment-receipt.json`. The previous ability asset is backed up as `BP_AZ_GA_Throw.before-art.uasset`; the before style export is retained. Only four art references and two padded canvas dimensions changed in that ability, with no Source, montage or gameplay-logic edits. Native art acceptance is separate from user-run in-game acceptance; no PIE/tests were started.
