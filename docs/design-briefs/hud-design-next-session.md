# CHALK HUD — approved design and next-session handoff

Saved 2026-09-07 after the user said the latest design looks good and explicitly deferred implementation to the next session. Next session: inspect the existing project/assets and develop the implementation plan before beginning the agreed implementation work. Do not restart visual exploration or ask the user to choose A/B/C again.

## Approved direction

- Direction **A / Quiet survival**: compact health and equipped-weapon/ammunition cluster at bottom right, simple and clear, suited to CHALK's contemporary civilian survival-horror setting.
- No permanent **I / Inventory** hint during normal play. Keep the inventory action itself; contextual/onboarding/help prompts are separate.
- Temporary quick-access selector inspired by the user's The Last of Us reference. Current draft uses a cross with care above, utility below, long gun left and sidearm right. Exact slot capacity, item categories and bindings remain planning details.
- Slim top-center compass with heading and target markers. Current draft shows one tracked objective with distance; a second waypoint and objective-name label are separate hidden layers.
- Future RPG elements include consumables and crafted items. Keep recipes, ingredients, equipment stats and progression primarily in inventory/crafting. Put crafted items into functional quick-access categories.
- Preserve the inventory's Oswald/Roboto family and restrained warm neutral/peach styling. Use critical-health red with a non-color cue. The mockup's counts, sidearm/consumable types and effect text are illustrative.

## Authoring rule — explicit user requirement

Use native GIMP tools for every text/shape that can be authored that way. Text stays native editable text. Shapes stay native vector layers with editable paths whenever possible; soft painted effects get dedicated GIMP layers. Each component must be named and independently toggleable, organized into sensible groups. Generated artwork stays on image layers. Do not flatten the HUD into the background.

## Authoritative visual files

- Editable master: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/CHALK_HUD_v03_NATIVE.xcf
- Quick-select-open version: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/CHALK_Quick_Select_v03_NATIVE.xcf
- Normal preview: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/01_HUD_Compass.png
- Quick-select preview: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/03_Quick_Select_Compass.png
- Detailed design/reuse notes: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/README.md
- Authoring rules/index: C:/UnrealEngine/Games/AZ/UI Design/README.md
- User's selector reference: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v02/sources/user_quick_select_reference.png

Saved XCF was reopened and verified: 28 native text layers, 101 native vector layers with 101 retained paths, 3 GIMP feathered-fill layers, 1 generated background image, 32 groups. v01/v02 comparison boards contain flattened review panels; v03 native XCF is the editing source. The original C:/UnrealEngine/Games/AZ/UI Design/HUD.xcf and earlier iterations were preserved.

## Existing assets already inspected — revalidate live when planning

ProHUDV2_Horror provides the compass/marker and notification presentation. HQUI_ProgressBars provides configurable progress renderers. The inventory already uses HQUI circular bars. Reuse and restyle existing components where they fit; their demo appearance is not the approved design.

| Need | Verified asset/API |
|---|---|
| Current HUD entry | /Game/AZ/Blueprints/Menu/HUD/WBP_AZ_Inventory |
| CommonUI inventory | /Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu |
| Health/progress | /Game/HQUI_ProgressBars/Widgets/ProgressBarLinear/ProgressBarLinear; ProgressBarCircular sibling |
| Compass | /Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_Compass_H |
| Compass target | /Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_CompassMarker_H |
| Compass styling | /Game/ProHUDV2_Horror/Widgets/Content/WB_HUD_Container_H |
| Compass material | /Game/ProHUDV2_Horror/Materials/M_Compass_H — Offset, Orientation, CompassMask |
| Target registration | /Game/ProHUDV2_Horror/Blueprints/Libraries/BPi_HUDManagerV2_H — AddOrUpdateCompassMarker_H, RemoveCompassMarker_H, ShowCompass_H/HideCompass_H |
| Target data | /Game/ProHUDV2_Horror/Blueprints/Structs/S_CompassMarkerInfo_H — icon, color, highlight, distance, ping, visibility |
| Interaction | /Game/ProHUDV2_Horror/Widgets/Content/OptionNotification/WB_OptionNoti_H |
| Pickup toast | /Game/ProHUDV2_Horror/Widgets/Content/PickupNotification/WB_PickupNoti_H |

Compass currently uses camera yaw. Target location supports Actors or SceneComponents; generic UObject resolves to zero. No marker-priority field was observed: priority/discovery are proposed CHALK behavior. World-marker offscreen arrows/clamping belong to WB_WorldMarker_H and must not be assumed to be compass features. Clean, no-text and direction-only compass strips are installed. Size the stock compass through normal UMG layout; no dedicated width/height setting was verified.

## Important ownership facts for the plan

- Authoritative combat health: C:/UnrealEngine/Games/AZ/Source/AZ/Public/AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h. Do not accidentally bind the new HUD to legacy HeroAttributeSet.Health used by old UI.
- Weapon selection: C:/UnrealEngine/Games/AZ/Source/AZ/Public/Inventory/AZ_QuickBarComponent.h. Equipment owns grants/transitions; the selector should present existing owners, not create a second inventory/equipment state store.
- Ammo uses individual magazine identities. A's `17 / 30` means inserted rounds/capacity; `2 MAGS` means separate spare magazines, not a pooled reserve-round total.
- Consumable fragments exist in C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h. This does not prove a complete crafting/quick-use system. Current/max stamina was not verified; infection/mortality inventory meters appeared to be placeholders. Do not create fake gameplay bindings for visual examples.
- Start the new session by reading current C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-inventory-foundation-status.md and live code/assets, since other work may advance weapon behavior between sessions.

## Next-session agenda — a plan to develop, not a finished implementation specification

1. Open the approved master/preview and inspect the current HUD, CommonUI input stack, inventory/equipment signals, vitals, and both packs' widgets/interfaces.
2. Map each visible element to its authoritative data, events, reusable widget/material, owner and asset assignments. Identify actual missing consumable/crafting/target systems separately from presentation.
3. Write a phased implementation plan: HUD container/core vitals and weapon data; compass/targets; quick access; contextual prompts/effects; inventory/crafting integration as supported. Include asset duplication/restyling strategy, lifecycle/input capture, resolution/scale behavior and manual acceptance criteria.
4. Resolve only meaningful open decisions: selector bindings and hold/toggle behavior, number of quick slots, equip-versus-use flow, compass visibility/target rules, and whether selector time behavior should change in solo play. Current recommendation is select/equip first, deliberate use separately; no automatic consumption on opening/closing.
5. Implement in the agreed order during the next session. No Unreal HUD implementation was done in this design session.

Project constraints remain: no automated tests unless the user explicitly asks; ask before PIE/editor tests and normally let the user test, then read logs. Preserve unrelated ongoing edits. Do not commit or change editor lifecycle merely to resume this task.

## GIMP operational note

GIMP 3.2.4 is installed. MCP plugin exists but port 9877 was off; native GIMP batch authoring was used. The current GUI received the XCF via filename forwarding. Fonts are provided through C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01/sources/batch.gimprc, without changing global settings. Native vector creation uses Gimp.VectorLayer + Gimp.Path. Selection-to-Path array arguments require cfg.set_core_object_array('drawables', [layer]), not set_property. Exact authoring source and layer audits are retained under the v03 folder. Read current GIMP state before editing; the user may have unsaved changes.
