# Quiet Sage — sunlight readability revision

The user's September17 gameplay capture showed that the first pale sage treatment disappeared against sunlit surfaces. This revision preserves the palette, pulse spacing, first-contact shapes and depth occlusion, while adding a charcoal keyline and slightly stronger strokes.

**Latest user-requested tuning:** line envelope is now **7.5px** (previously5.5), and **Brightness=1.5** on the active arc and contact instances. StrokeFill remains3.5/5.5, so the wider envelope scales both sage core and outline together (core~4.77px, filament~1.91px at1080p). Earlier5.5px numbers below describe the original daylight calibration. Width is already exposed in BP_AZ_GA_Throw → Class Defaults → Preview Style → Pulse Width Pixels. Marker width/height have separate controls. No trajectory distance/height/velocity changes.

**Applied and saved:** BP_AZ_GA_Throw now uses MI_QS_ArcSunlight and MI_QS_ContactSunlight, PulseWidthPixels5.5, FilamentAlpha.55 and PulseAlpha1. These five fields were verified against the full prior style; all others, including the newer55-degree marker tilt, are preserved. No Source/gameplay/montage changes or C++ build. Next fresh Play uses the revision; a user-run sunlight/close-marker capture is still needed for final visual acceptance.

**Subsequent runtime fix:** a live capture showed the ability was still initializing the old numeric values despite correct CDO data, and55-degree marker tilt buried its lower half. The [compiled readability fix](C:/UnrealEngine/Games/AZ/docs/design-briefs/throw-preview-live-readability-fix.md) now consumes coherent class-default art settings and keeps the marker parallel to the hit surface with bounded height compensation. Legacy tilt remains serialized but is no longer used. This supersedes the earlier assumption that assigning the Sunlight materials alone completed the runtime handoff.

[Native comparison PNG](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/QS_Sunlight_Readability.png>) · [Editable GIMP master](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/QS_Sunlight_Readability_NATIVE.xcf>). These are art swatches, not claimed gameplay captures. The top row shows the prior treatment; bottom shows the revised pavement/foliage/dark comparison.

## Assets and settings

Six new material assets are saved under `/Game/AZ/Blueprints/Throwables/Art/QuietSageV1/` so existing masks/meshes remain shared:

- `M_QS_ArcSunlight`, `MI_QS_ArcSunlight`
- `M_QS_ContactSunlight`, `MI_QS_ContactSunlight`
- `MI_QS_BlockedSunlight`, `MI_QS_BodyContactSunlight`

Both master materials compiled without errors. Earlier materials remain available as a baseline. Existing `SM_QS_Ribbon`, `SM_QS_MarkerPlane` and three mask textures are unchanged.

Arc:5.5px full ribbon envelope at1080p, with3.5px sage pulses and1.4px colored filament. `StrokeFill=3.5/5.5` defines the colored portion; surrounding coverage supplies an approximately1px charcoal edge. `FilamentAlpha=.55`, `PulseAlpha=1`, `FilamentWidth=.4`, `OutlineAlpha=.8`, `OutlineColor=#18251D` converted to linear. Main sage/white colors and CPD0/1 remain unchanged. Do not interpret5.5px as a5.5px solid colored cable or reset the envelope to3px, which would shrink the whole treatment.

Contact: reuses R/G/B vector-derived masks. Eight surrounding texture taps dilate their union for a1-reference-pixel charcoal keyline. Original corner/ellipse/center colors remain; ring alpha increases to.85. `MaskCanvasWidth/Height=128/64` for the contact material and32/32 for compact blocked/body variants. The centers stay open; no opaque ground disk, glow or through-wall rendering is added.

The user's close-marker screenshot also shows low contrast, so the revision covers both arc and marker. A surface decal may be considered later for conformity to uneven ground, but changing primitive type alone does not provide contrast. The current depth-tested plane remains in this art-only revision; particle effects are not needed to depict the deterministic predicted path. Runtime first-hit/projection issues remain separate from art legibility.

Unchanged controls, trajectory physics, release origin, camera, montage/movement and gameplay logic. The current newer MarkerMaxTiltDegrees field is preserved when updating ability defaults.

## Applying and verifying

Authoring: `C:/UnrealEngine/Games/AZ/Tools/throw_quiet_sage_sunlight_setup.py`. `create()` creates the six sibling materials; `apply()` assigns the new ArcMaterial/MarkerMaterial and changes only PulseWidthPixels, FilamentAlpha and PulseAlpha in the ability's PreviewStyle, after PIE stops and the target package is clean. It preserves all remaining live style fields. Do not rerun the historical baseline assignment to undo this revision.

Receipts live in `C:/UnrealEngine/Games/AZ/Saved/ThrowArtProduction/Sunlight/`. `create-receipt.json` confirms creation/compilation; `apply-receipt.json` is produced only when assignment has actually succeeded. Final in-game sunlight acceptance needs a new user-run capture. No C++ build or agent-started PIE/tests are required for the art change.
