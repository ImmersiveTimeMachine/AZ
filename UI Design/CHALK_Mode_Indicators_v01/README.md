# CHALK — Fight / Explore HUD proposals

Artur selected **01 — Equipment row**. The mockup pass used native GIMP only; the selected layout is now being implemented in the game HUD. Runtime status is recorded in `C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-mode-indicator-status.md`. No editor play session or automated tests were started by the agent.

Open `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Mode_Indicators_v01/CHALK_Mode_Comparison_NATIVE.xcf` to compare all three options. The comparison also has native editable text and vector layers, not flattened HUD panels.

| Option | Editable GIMP document | Intent |
|---|---|---|
| 01 — Equipment row | `CHALK_Mode_01_NATIVE.xcf` | Larger fist/walking symbol and mode name, closest to the existing weapon row. Recommended starting point for this request. |
| 02 — Compact status | `CHALK_Mode_02_NATIVE.xcf` | Small symbol and name directly above health; least additional footprint. |
| 03 — Two-symbol strip | `CHALK_Mode_03_NATIVE.xcf` | Both symbols remain visible. The active symbol is bright and underlined, with a current-mode label. |

The individual documents open on a two-state review with enlarged and game-size HUD samples. Hide `90 / TWO-STATE REVIEW` and show exactly one of `20 / FULL FRAME / FIGHT` or `21 / FULL FRAME / EXPLORE` for placement on the previous 1920 × 1080 Montréal composition. The background character's weapon pose is illustrative and unchanged; the cropped review panels avoid it.

## Visual contract

- HUD describes **the current committed mode**: FIGHT uses the fist; EXPLORE uses the walking person. This is separate from quick-select slot 0, which describes its destination action.
- These proposals show unarmed Fight and Explore. The existing firearm silhouette, item name, ammo and magazine count return when a firearm is equipped. No invented ammo or resource readings appear in the unarmed states.
- Health stays on the approved baseline at the same size and illustrative 72% value in every sample.
- Warm white `#EEEAE0`, restrained existing peach `#FFBA8C`, Oswald Light and Roboto Regular match the approved CHALK HUD and inventory. Explore does not imply safety, invisibility or immunity from attacks. Red remains available for actual damage/critical-health feedback.
- No permanent input hint is included in the gameplay HUD. Review annotations explain the 0 key outside the gameplay examples.
- The fist and walking geometry reuse the existing quick-select symbols. This keeps one visual vocabulary across the HUD and selector.

## Editing and provenance

All HUD symbols, rules, heart and health-bar contours are native GIMP vector layers with retained editable paths. Text is native GIMP text. Each current state, symbol and health group is independently toggleable. Background imagery alone is raster and is reused from the approved Montréal concept plate. Existing masters and Unreal assets are preserved.

Authoring uses GIMP 3.2's native Python batch interface through `sources/build_mode_mockups.py`. The image-generation skill was consulted, but its existing-vector-art exception applies: no new AI-generated HUD imagery or scene was needed. The artwork specification is the visual contract above and the three variant definitions in that script.

Reused sources:

- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/sources/build_native_hud.py`
- `C:/UnrealEngine/Games/AZ/Tools/quick_select_fist_art.py`
- `C:/UnrealEngine/Games/AZ/Tools/quick_select_explore_art.py`
- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01/sources/montreal_concept_plate.png`

PNG files are review exports. The four `_NATIVE.xcf` files are the editable sources. Saved XCFs are reopened to check native layer types and retained paths; receipts are in `saved-file-receipt.json` and each document's `_layers.json` file. This document records a design proposal, not runtime implementation or gameplay validation.
