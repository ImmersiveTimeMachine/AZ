# Throw preview: live settings and floor clipping

September17,2026. User screenshots remained faint after the Sunlight materials were assigned. This pass inspected the actual user-run PIE components before making further art changes.

## Confirmed causes

1. **Class defaults and live ability disagreed.** BP_AZ_GA_Throw CDO had the intended5.5px envelope/.55 filament alpha/1 pulse alpha, but the live ability and preview retained3px/.31/.94. Both referenced the new Sunlight materials. Their StrokeFill=.636 therefore produced only~1.91px colored pulses and~.764px filament at1080p reference scale, instead of3.5/1.4px. The live MID was explicitly overwritten with those old alpha values.
2. **The marker tilted into the floor.** Captured plane centerZ1.5cm, scale1.112499×.556249 on the100cm mesh and55-degree tilt placed its lower edge21.28cm below the floor. Lower corner anchors were~9.18cm underground and the lower ellipse~4.91cm. Depth testing correctly hid those fragments.

Engine-source review explains the class-default discrepancy: BlueprintGeneratedClass's post-construction optimization caches only properties differing from native defaults. Direct nested CDO edits can leave formerly-native-equal numeric fields absent from that cache while existing object-reference entries still initialize correctly. CDO readback alone was insufficient evidence of effective runtime values. No shader alpha/pin-order failure was established; broad AA/fog/TSR remain secondary checks only if needed after these fixes.

Evidence: `C:/UnrealEngine/Games/AZ/Saved/ThrowArtProduction/LiveReadability/live-before.json`, `ability-style-comparison.json`, `geometry-review.md`, `shader-review.md`, `renderer-review.md` and the archived user screenshot.

## Changes

- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Throw.cpp`: GetPreview reads the EditDefaultsOnly style from the ability's class defaults; new previews use it, and a cached preview refreshes only when the complete style differs. This deliberately fixes art consumption without unsafe Blueprint recompilation or altering the ability's action state. Added width/alpha diagnostics.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowPreviewComponent.cpp`: marker remains parallel to the real hit plane at its existing normal offset. In-plane height compensation is bounded to4× at grazing angles. Contact position, width and depth occlusion remain intact. No floating center or disabled depth test.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Throwables/AZ_ThrowPreviewComponent.h`: documented MarkerMaxTiltDegrees as a retained legacy field; no reflected field/type/default/metadata/layout change.

No new art, trajectory, input, inventory or montage changes in this pass. The marker material and its mask continue to work on the existing plane; changing to particles or a decal was not required for these two defects.

## Which material to edit

| Purpose | Current instance |
|---|---|
| Marker | `/Game/AZ/Blueprints/Throwables/Art/QuietSageV1/MI_QS_ContactSunlight.MI_QS_ContactSunlight` |
| Arc | `/Game/AZ/Blueprints/Throwables/Art/QuietSageV1/MI_QS_ArcSunlight.MI_QS_ArcSunlight` |

`/Game/AZ/Blueprints/Throwables/MI_AZ_ThrowPreview_Marker` is the old unused instance. It was preserved, not deleted; editing it cannot change this preview.

Marker palette, opacity, outline and brightness are material-owned. Arc Color/FilamentAlpha/PulseAlpha/FilamentWidth/PulseSpacing/PulseDuty are driven by the ability's PreviewStyle and will override corresponding MI defaults. Change those in the ability's class-default PreviewStyle; the new cached-style comparison propagates them. Other artist parameters such as OutlineAlpha, OutlineColor, Brightness and StrokeFill remain in the active arc material instance. Keep the5.5px envelope/StrokeFill3.5÷5.5 contract together.

## Validation

Static reviewers confirmed both fixes against source and captured runtime values. Normal CLI build correctly refused the active Live Coding session; Live Coding then compiled successfully: **UBT Result:Succeeded,14.87s**, patch creation for UnrealEditor-AZ.dll successful at **05:07:39UTC**, loaded in editorPID45240. No agent-started PIE or tests, and no automated tests added. Editor was idle at post-build inspection, so actual post-fix gameplay appearance remains for the user's next Play. A normal build is needed before a later editor restart to retain the C++ changes outside this Live Coding session.

Next user capture should establish correct live5.5/.55/1 values and a complete marker above the local hit plane. Further anti-aliasing/fog tuning, if needed, should follow that evidence rather than another blind palette revision.
