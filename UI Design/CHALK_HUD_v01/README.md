# CHALK HUD — draft 01

First visual iteration, 7 September 2026. Draft only: no Unreal assets or gameplay code changed.

Open **CHALK_HUD_v01.xcf** in GIMP. Toggle one top-level option group at a time. A is initially visible. Text remains native editable text; health fills, tracks, icons, prompts and contrast scrims have named layers. The comparison board is a separate XCF, with the gameplay previews embedded as flattened layers.

## Directions

- **A — Quiet survival (recommended):** health and equipped rifle in one compact bottom-right cluster. An injured player keeps a health cue; weapon information appears on selection, aiming or a change, then can fade. The nearby pickup is an example transient state. No permanent navigation or quest panel.
- **B — Field kit:** circular health at bottom left matches the existing inventory's circular vitals. Bottom right exposes each spare magazine's rounds. Easier to read at a glance, with more screen occupied.
- **C — Raw chalk:** uses the actual distressed ProHUD texture and larger ammunition numerals. Stronger CHALK material identity; the roughness stays out of the small text.
- **A2 — Critical health:** a shorter red health fill plus an explicit LOW HEALTH label. Urgency does not depend on color alone. This is an illustrative 19% state, not a gameplay tuning decision.

PNG previews and A's transparent overlay are included. The background is generated concept art, not a capture of the current game. All numbers in the mockups are illustrative.

## Proposed behavior for iteration

Keep the center clear during exploration. Show E only for the current valid nearby pickup; use the actual item display name and its real state. I opens the existing inventory. The inventory hint fades after onboarding. A's 17 / 30 means rounds in the inserted magazine / its capacity, while 2 MAGS means two distinct spare magazines. B demonstrates why individual counts (30 and 17) can be useful. These are alternatives, not a request to combine everything.

Future HUD settings should support scale, background contrast, persistent visibility and reduced flashing. Those are useful reference ideas from [Naughty Dog's official accessibility breakdown](https://www.naughtydog.com/blog/the_last_of_us_part_ii_accessibility_features_detailed), which documents HUD size, background, color and flashing controls. Our layouts, palette choices and magazine presentation are proposed specifically for CHALK.

Current code has authoritative combat Health/MaxHealth in AZ_VitalsAttributeSet. The inventory's old HeroAttributeSet health is a separate legacy display path. A future HUD must bind the combat owner. No current/max stamina attribute was verified, and infection/mortality in the inventory are authored placeholders rather than verified systems; this draft therefore does not add those meters. Firing/reload were deferred in the September 7 rifle inventory foundation status, so ammo presentation here is a visual design, not proof of a functional firearm loop.

## Existing inventory language used

Live widget-tree reads confirmed Oswald Light headings, Roboto Light body text, sRGB neutral #B9B9B9, warm pale selected text and peach #FFBA8C. The draft uses Oswald Light with slightly firmer Roboto Regular small labels for gameplay readability. The palette retains peach as a selection/detail accent and introduces a muted urgent red for critical health. A and C use the actual inventory rifle icon, recolored through alpha in GIMP.

## Verified reuse map

| Purpose | Unreal asset |
|---|---|
| Linear health/progress renderer | /Game/HQUI_ProgressBars/Widgets/ProgressBarLinear/ProgressBarLinear |
| Circular renderer (already used in inventory) | /Game/HQUI_ProgressBars/Widgets/ProgressBarCircular/ProgressBarCircular |
| Quiet rough bar texture | /Game/HQUI_ProgressBars/Textures/Masks/256x16/T_Mask4_256x16 |
| Raw chalk texture used by C | /Game/ProHUDV2_Horror/Textures/T_PB_Mask_880x16_H |
| Context prompt framework | /Game/ProHUDV2_Horror/Widgets/Content/OptionNotification/WB_OptionNoti_H |
| Pickup notification framework | /Game/ProHUDV2_Horror/Widgets/Content/PickupNotification/WB_PickupNoti_H |
| Current HUD entry | /Game/AZ/Blueprints/Menu/HUD/WBP_AZ_Inventory |
| Current inventory menu | /Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu |

The packs provide presentation components. They do not supply the authoritative health/ammo logic. HQUI exposes percent, target percent, fill/background colors, size, thickness, masks and interpolation. ProHUD's ornate default typography and borders would be restyled to match this draft.

## Provenance and editing

- Sources are in this folder's `sources` directory. GIMP builder uses native text, selections, fills, groups, and imported alpha textures. The rifle icon is the current temporary inventory icon, not final weapon artwork.
- Oswald is from the [Google Fonts source](https://github.com/google/fonts/tree/main/ofl/oswald), with its OFL included. Roboto source files are copied from C:/UnrealEngine/Engine/Content/Slate/Fonts. A task-local GIMP config loads them for batch authoring; the user's global GIMP settings were not changed.
- GIMP's server was not listening on port 9877 during creation. The same GIMP Python API was used through a separate hidden batch instance, then the XCFs were opened in a separate visible GIMP window using the task-local font configuration. This window loads the exact fonts without changing global settings. `gimp_bridge.py` is available for the next live MCP iteration after Tools > MCP > Start MCP Server in that window.
- XCF retains rendered text but does not embed font files. When reopening in a different GIMP session, add the included fonts through GIMP's font folder and refresh the Fonts dialog, or use the included task configuration. Fonts and licenses are included; current preview appearance is already rendered in the XCF.
- Original C:/UnrealEngine/Games/AZ/UI Design/HUD.xcf was preserved.
- The scene was made with built-in image generation; exact prompt is in `sources/background_prompt.txt`.

Next review: choose A, B or C, then refine corner, scale, texture and the amount of magazine detail. Implementation follows approval of the visual direction. No automated tests were added or run, and PIE was not started.
