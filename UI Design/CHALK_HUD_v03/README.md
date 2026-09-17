# CHALK HUD — v03, native GIMP layers and compass

The editable source is **CHALK_HUD_v03_NATIVE.xcf**, saved in normal play with the new compass visible. **CHALK_Quick_Select_v03_NATIVE.xcf** is the same layered composition saved with quick select open. PNGs are previews/exports, not editing sources.

## Layers the user can toggle

| Group | Contents |
|---|---|
| 00 / GENERATED SCENE | The existing generated Montreal scene as one image layer |
| 10 / CORE HUD | Separate native weapon silhouette, heart, health track/fill, magazine shapes and editable labels |
| 20 / QUICK SELECT | Separate category groups, card fills, borders, selection edge, icons, counts, labels, directions and scene dim |
| 30 / COMPASS | Scale, cardinal labels, heading pointer, tracked target and optional second waypoint |

Expand an icon group to toggle its individual shapes. Text uses GIMP TextLayer, so spelling, font, size and color remain editable. Geometry uses GIMP 3.2 VectorLayer with paths, so it can be recolored or reshaped. Associated paths are retained in the Paths dock; select the path belonging to the named layer to edit its control points. Soft contrast fills are painted through GIMP's feathered selection tools on dedicated layers. The generated scene is not altered or flattened with the UI.

The rifle image was converted to an editable silhouette using GIMP's native Selection to Path procedure at source resolution, then the resulting path was scaled into each HUD location. The health bar is redrawn as a subtle irregular vector shape matching A; the pack texture is no longer baked into that bar.

## Compass proposal

Use a slim top-center heading strip, neutral direction labels, a small fixed camera-forward pointer and one warm peach tracked-objective marker. The draft shows a sample 045-degree heading and a target 126 meters away. Those values are illustrative. The objective marker moves with bearing; its distance label is separately toggleable. The objective name is included as hidden native text for exploration. A second player waypoint is a separate hidden group, ready for comparison.

Keep the compass optional in settings. Give the tracked objective priority and show extra markers selectively; marker prioritization is our proposed game behavior, not a verified stock feature. Avoid filling the compass with every loot object or automatically revealing unknown enemies. The initial draft uses one objective to preserve the clear HUD the user requested. During quick select, the compass is quieter so the selection remains the focus.

The top compass provides bearing; a separate world marker can be considered on request/near arrival if elevation or entrances are ambiguous. A compass marker should not imply that the world target itself has been revealed. Target visibility and discovery are gameplay decisions for the later implementation.

## Verified assets already installed

- /Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_Compass_H
- /Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_CompassMarker_H
- /Game/ProHUDV2_Horror/Widgets/Content/WB_HUD_Container_H
- /Game/ProHUDV2_Horror/Materials/M_Compass_H
- /Game/ProHUDV2_Horror/Blueprints/Libraries/BPi_HUDManagerV2_H
- /Game/ProHUDV2_Horror/Blueprints/Structs/S_CompassMarkerInfo_H

The compass already supports heading offset/orientation, tint and masks, background and center-pointer styling, marker appearance and fade. Its live heading path uses camera yaw. The material exposes Offset, Orientation and CompassMask. Clean, no-text and direction-only compass strip textures are present in the pack.

AddOrUpdateCompassMarker_H accepts a marker object plus the info struct; RemoveCompassMarker_H uses the same object. The marker location function handles Actors and SceneComponents. The info struct supplies icon scale/color/highlight, optional distance, units/stepping, ping and visibility controls. No marker-priority field was observed. Normal UMG layout can make the stock compass narrower; no separate compass width/height field was verified.

The separate /Game/ProHUDV2_Horror/Widgets/Content/WorldMarker/WB_WorldMarker_H supports name/distance and offscreen arrow/clamping. Those world-marker features should not be presumed to be compass features.

## Scope and verification

This is a GIMP mockup. No Unreal assets/code or input settings were changed, and no PIE/tests were started. Original images and prior iterations remain unchanged. The build uses GIMP's Python API through its batch interface because MCP port 9877 is still off; it is native GIMP authoring rather than external image composition.

The saved master was reopened in GIMP batch for a read-only layer audit: **28 native text layers, 101 native vector layers with 101 retained paths, 3 dedicated GIMP feathered-fill paint layers, and 1 generated scene image**, organized into 32 groups. The actual saved-file readback is recorded in saved_layer_readback.json. Normal and quick-select PNG exports were visually checked for spacing and clipping. Fonts and the original scene come from C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01/sources; the same task-local GIMP configuration loads the exact fonts.
