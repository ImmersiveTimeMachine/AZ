# Field Notes — approved style and asset migration contract

2026-09-20. Artur selected **02 / Field Notes** and requested a detailed implementation plan first. This is the bounded visual contract supporting that plan. It records file/source evidence only; no new mockups, gameplay/source changes, asset edits, editor calls or tests were made for this review.

## Approved source and scope

Authoritative visual family: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_UnifiedUI_v01/02_FIELD_NOTES/CHALK_AllMenus_NATIVE.xcf`. Its twelve 1920×1080 exports are Inventory, Map, Quick, HUD, Pause, Settings, Title, Save_Load, Item_Actions, Confirmation, Loading and Indicators. The saved master was reopened during production: **314 native text layers, 727 vector layers/paths, 12 screen groups, 33 reused raster layers**. Receipt: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_UnifiedUI_v01/sources/manifest-02.json`.

Exact authoring source: `C:/UnrealEngine/Games/AZ/Tools/unified_ui_mockups_gimp.py`; state/type legend: `C:/UnrealEngine/Games/AZ/Tools/unified_ui_indicator_sheet.py`. Inventory and Indicators exports were visually inspected again for this contract.

The selected visual direction is **light paper menus, dark gameplay HUD/quick-select surfaces, Roboto typography, thin rules and a neutral selection margin**. It supersedes old Oswald/warm-dark styling where this family changes it. It preserves the three-column inventory and fixed eight-cell selector. It does not authorize copying demonstration values into data bindings, changing inventory capacity, changing quest/save rules, introducing crafting, or assuming every MenuSystemPro prototype is already an active CHALK screen.

## Colour tokens

Hex values are **sRGB**. Numeric triples below are **linear RGB**, alpha1 unless separately stated. Use `FLinearColor::FromSRGBColor`, or the equivalent piecewise conversion: `c/12.92` below0.04045, otherwise `((c+0.055)/1.055)^2.4`, with c=byte/255. Never enter sRGB byte fractions directly as a linear Unreal tint.

| Token | sRGB | Linear RGB |
|---|---|---|
| Menu background | #DDD6C4 | 0.723055, 0.672443, 0.552011 |
| Menu panel | #EAE3D1 | 0.822786, 0.768151, 0.637597 |
| Menu selected/inset | #C9C4B3 | 0.584078, 0.552011, 0.450786 |
| Menu text | #282E29 | 0.021219, 0.027321, 0.022174 |
| Menu muted | #596355 | 0.099899, 0.124772, 0.090842 |
| Menu edge/rule | #949B88 | 0.296138, 0.327778, 0.246201 |
| Menu story | #8E573E | 0.270498, 0.095307, 0.048172 |
| Menu side quest | #466477 | 0.061246, 0.127438, 0.184475 |
| Menu personal marker | #526D48 | 0.084376, 0.152926, 0.064803 |
| Danger/error | #913F34 | 0.283149, 0.049707, 0.034340 |
| HUD/Quick panel | #282E28 | 0.021219, 0.027321, 0.021219 |
| HUD/Quick selected/inset | #3C4237 | 0.045186, 0.054480, 0.038204 |
| HUD/Quick text | #F0E7D1 | 0.871367, 0.799103, 0.637597 |
| HUD/Quick muted | #C2C6B5 | 0.539479, 0.564712, 0.462077 |
| HUD/Quick edge | #788371 | 0.187821, 0.226966, 0.165132 |
| HUD/Quick story | #E0B08B | 0.745404, 0.434154, 0.258183 |
| HUD/Quick side quest | #B8CCD5 | 0.479320, 0.603827, 0.665387 |
| HUD/Quick personal marker | #BACDA9 | 0.491021, 0.610496, 0.396755 |
| Map paper reference | #C2C7B5 | 0.539479, 0.571125, 0.462077 |
| Map structure reference | #ACB49D | 0.412543, 0.456411, 0.337164 |
| Map route reference | #DADBCB | 0.701102, 0.708376, 0.597202 |

The last three colours style the **illustrative** map drawing; they are tonal targets for the real calibrated artwork, not instructions to invent roads or buildings. `hudpalette()` overrides eight colours and leaves danger inherited as #913F34. Its contrast on actual dark critical-health HUD backgrounds is not demonstrated by the paper Indicators sheet; include that case in the first runtime style review rather than silently claiming it is proven.

Rules and opacity from the source:

- Paper panels use a bottom rule1px; selected panels use a2px bottom rule, selected fill and a4px neutral left margin. Category colour remains on the category glyph/text. Do not replace all category colours with one highlight colour.
- Main menu-paper surfaces cover the entire screen opaquely. Do not leave dark header/footer text over the scene border.
- HUD compass/task backing: #07100E at58%; equipment backing60%. Quick selector adds #06100C scene dim18%; card fill uses dark HUD panel at78%. Normal health track uses edge at60%.
- Grid lines0.8px at60%; active inventory tab underline170×3px. These are GIMP reference pixels, not a reason to change global DPI settings.

## Typography and layout

**Headings/bold: Roboto Bold. Body: Roboto Regular.** Oswald is not the selected family. Source font files already exist at `C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01/sources/fonts/Roboto-Bold.ttf` and `Roboto-Regular.ttf`.

An owned Regular font already exists: `/Game/AZ/Blueprints/Menu/Map/Art/Fonts/FF_CHALK_RobotoRegular_Font`. Its receipt defines the sole typeface as **Default**, not Regular/Bold. Reuse it with the correct typeface. No owned Menu Roboto-Bold asset was found in the file inventory. Import the existing Bold TTF into an owned FontFace/Font, or create one owned composite with explicitly named Regular/Bold entries; validate the actual names before assigning them. `/Engine/EngineFonts/Roboto` may remain an unchanged dependency where its exact required face is verified.

Reference sizes are GIMP pixel sizes at1920×1080. Existing authoring recipes translate these to Slate points with×0.75; use that as the initial conversion, then compare rendered geometry at the current project DPI. Do not assume that copying the number unchanged gives the same glyph size.

| Use | Reference size |
|---|---|
| CHALK page brand / page title | 45 /29 Bold |
| Main section/item title | 28–34 Bold |
| Journal heading / selected quest | 30 /27 Bold |
| Body/objective text | 19–24 Regular |
| Metadata / hints | 15–18 Regular |
| Compass letters | 21 Bold |
| HUD inserted ammo / capacity | 57 /27 Bold |
| Quick header / shortcut / item label / ammo | 16 /15 /14 /16 Bold |
| Quick focused name / description / hints | 30 Bold /18 Regular /15 Regular |

Preserve these structural references; adapt anchors/size boxes rather than stretching the full UI bitmap:

| Screen area | Approved reference geometry |
|---|---|
| Shared shell | Side margin56; header ruleY105; footer ruleY1014 |
| Inventory left column | (56,150),405×810; portrait(95,206),316×444 |
| Inventory centre | StartsX510; reference grid(528,300),800×400; details belowY740 |
| Inventory right column | (1400,150),464×810; capabilities/details remain present |
| Inventory categories | Equippables, Consumables, Craftables, Map; startX510,214px pitch |
| Map/journal | Journal(56,153),447×787; map(538,153),1326×787; approximately25%/75% |
| Quick selector | Centre(1368,468); **eight104×72 cells**, slots0–7; offsets0(-116,0),1(-232,0),2(0,-84),3(0,-168),4(116,0),5(232,0),6(0,84),7(0,168) |
| Quick details | HeaderY226; focused nameY706; descriptionY750; hintY792 |
| Quick clearances | Existing V5 minimum12px card gap; central arrows retain80×48 bounds and24px nearest-card clearance |

The ten-by-five grid and M16 four-by-two placement in the mockup are **sample presentation**, not authority to alter actual grid capacity or item `GridSize` fragments. Keep real footprints/counts and make the three-column surface contain them. The concept's simplified compass tick positions are not a new calibration: preserve the implemented600px window,1440px strip draw width and existing bearing/math contract while restyling it.

## Semantic glyph/state contract

- Story: two nested open diamonds, inner radius0.57×outer. Side quest: open circle plus centre point. Personal marker: four open corners. Map/HUD/journal use the same families with context-appropriate palette. Generic size13px radius; typical map pins17–19, compass8–10; stroke1.8px, tracked2.5px.
- Tracking adds a **neutral** short tick beside the category symbol; it does not remove its colour or change a side-quest circle into a story diamond.
- Active objective: open square26×26 plus6px centre point. Completed: check. Failed: cross plus danger colour. Optional is the text modifier `(optional)`, not a mutually exclusive progress state or a personal-marker symbol.
- HUD names the **current** Explore/Fight mode. Slot0 shows its inverse **destination**: current Explore→FIGHT/fist action; current Fight→EXPLORE/walking action. Slot0 must not receive an equipped-item badge merely because the player is in Fight.
- Focus: steady neutral frame. Equipped: independent steady lower notch16×3px, at local(44,69) in a104×72 card. Assignment: **border-only** pulse; no card/content movement, fill pulse, icon fade or in-card assignment caption. Focus and equipped can coexist.
- Current native assignment implementation already has a one-second35–100% opacity pulse on `HighlightBorder`; preserve it. The mockup does not request new timing or input semantics. Source: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_QuickSelectEntryWidget.cpp`.
- Critical health uses low fill and a warning shape/text as well as colour. Keep the existing authoritative critical threshold (currently `CriticalHealthFraction=.25`) rather than deriving it from the illustrative16/100 example. Retain existing “LOW HEALTH”/“NO HEALTH” meaning.

## Reusable artwork and actual gaps

| Reuse without new generation | Required treatment/addition |
|---|---|
| Existing rifle/ammo item icons and manifests; pistol, magazine, grenade artwork | Change presentation tint where appropriate; preserve alpha/aspect and item identity. Do not create replacement inventory items. Source images are referenced in `Tools/unified_ui_mockups_gimp.py:ICONS`. |
| `/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Heart`, `T_HUD_Fists`, `T_HUD_Explore`, `T_HUD_Magazine`, `T_HUD_Pistol` | Re-tint existing brushes for light/dark contexts. Existing health update/HQUI renderer is reusable; the paper reference's straight7px meter may need a solid/native rectangular fill instead of the old irregular health mask. |
| Current inventory portrait (exported as `UI Design/CHALK_UnifiedUI_v01/sources/hero_inventory_current.png`) | Preserve the active inventory portrait resource; do not reimport the reference PNG as a new character. |
| `/Game/AZ/Blueprints/Menu/HUD/Navigation/Art/T_CHALK_CompassPointer` | Existing white downward triangle is reusable with new tint. |
| Native compass strip source at `UI Design/CHALK_HUD_v03/unreal-art/Compass/CHALK_Compass_Strip_NATIVE.xcf` | **Re-export an owned Roboto-Bold strip**: current2880×128 texture bakes Oswald Light. Preserve360° period, tick positions and shader phase-0.00275. |
| Existing generic marker/widget infrastructure | **New matching role masks** for double diamond, circle+point and open corners. Existing `T_CHALK_CompassTarget` was inspected: it is one hollow diamond with a filled centre, not the approved double diamond. Export white-alpha native GIMP shapes into an owned style folder; let runtime tint them. |
| Existing ProHUD completion-state APIs and candidate icon assets | Export/match active-square, check, cross and warning-triangle masks to the approved geometry; vendor checked texture alone is not proven an exact shape match by this audit. Simple geometry may remain native Slate/UMG instead of textures. |
| Calibrated `/Game/AZ/Blueprints/Menu/Map/Art/T_CHALK_Map_L001` and `/Game/AZ/Blueprints/Menu/Map/DA_AZ_Map_L001` | Derive a **paper-toned real map** from the retained actual capture; preserve2048², origin(-1435,3620,0),12400²cm span, orientation and all geometry. Keep markers out of the map texture. Never substitute the illustrative street grid. |
| Native UMG rectangles/rules and existing button widgets | Use brushes/borders for paper panels, selection margins, focus frames and equipped notches. No flattened menu screenshot or generated paper background is required. |

Proposed new asset namespace: `/Game/AZ/Blueprints/Menu/Style/FieldNotes/` for shared fonts, colour/style data and role/status masks. This is a proposed destination, not a claim those assets exist. Preserve original pack assets and old source XCFs. New native GIMP exports should retain editable text/vector layers and source/size receipts.

## Current owners and migration touchpoints

| Owned surface | Existing authoring/evidence to reuse |
|---|---|
| Three-column inventory | `/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu`, `AZ_WBP_GameInventorySwitcher`, `AZ_WBPCharacterVitalsPanel`, existing grid/composite/details/skills children. Preserve their bindings and source-portrait reference. Read-back baseline: `Saved/MapPlanning/map-source-graphs.json`; do not rebuild the old inventory tree from a screenshot. |
| HUD, health, weapon/ammo, prompts | `/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD`; `Tools/hud_phase1_assets.py`, `hud_magazine_widget.py`, `hud_mode_indicator_setup.py`, `hud_reticle_assets.py`. Change style properties only; never rerun the old cleanup/creation stages. Native HUD health colours are editable; preserve PS Vitals data and committed weapon/magazine readings. |
| Eight-cell Quick Select | `/Game/AZ/Blueprints/Menu/HUD/QuickSelect/` root/Entry/Fists/ItemDetails/NameLeaf/FocusDetails/FocusNameLeaf/DescriptionLeaf. `Tools/quick_select_v5_widgets.py` and `Saved/QuickSelectV5/v5-verify.json`, `saved-file-receipt.json` record the fixed layout. Do not rerun V2–V4 migrations. |
| Compass/world markers | Owned `/Game/AZ/Blueprints/Menu/HUD/Navigation/` module/leaves and Art. `Tools/compass_style_setup.py` gives actual tint/font/texture fields and current layout; `compass_art_export.py` gives exact strip contract. Preserve owner/context, registration and material scrolling. |
| Quest HUD | Owned `/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/`: module, MissionNotification, Mission, Task and owned helpers. `Tools/quest_prohud_setup.py`, `quest_hud_host_setup.py`; latest presentation records under `Saved/QuestPlanning/ProHUDAuthoring/`. Keep gameplay snapshots separate from row styling. |
| Full map/journal | `/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage`, `WBP_AZ_QuestJournalEntry`, `WBP_AZ_MapCanvas`. `Tools/quest_map_page_setup.py`; `Saved/MapImplementation/page-styled.json` currently records Oswald title/Roboto body and old dark palette; `Saved/QuestMapImplementation/font-import.json`, `map-art-configured.json` establish current owned art. |
| Ancillary menus | Title/Pause/Settings/Load/Confirmation/Loading studies correspond to found MenuSystemPro concepts. The artwork/README does **not** prove their active entry classes, routing or actions. Resolve actual active classes, duplicate any pack classes needing edits into AZ ownership, then style those routes. Do not present prototype options as already connected functionality. |

Small presentation-code work is required where styles are currently baked into runtime code: `AZ_QuestJournalEntry.cpp` hardcodes chalk/peach/button colours; `AZ_QuestMapPage.cpp` creates Regular13/sage group headings; `AZ_MapCanvasWidget` has editable font/colours but renders all quest pins with one diamond. `FAZ_QuestMapMarkerView` currently exposes tracked/personal/optional flags, not Story/Side category. Carry the existing quest definition category through presentation so all views choose the correct glyph; this is a styling/data-view change, not a new quest mechanic. Asset-only recolouring cannot cover these paths.

## Staged asset migration and gates

1. **Freeze current bindings and identities.** Record active owned classes, template overrides, fonts/brushes, widget trees, palette values and source-pack hashes. Include actual menu routing in the main plan's preflight. Back up only touched assets. Gate: exact owned edit manifest; existing inventory/selector geometry and quest/save owners recorded.
2. **Shared Field Notes resources.** Add the two-context colour/style source, owned Bold font/composite, role/status masks and Roboto compass strip; retain native masters. Gate: dimensions, typeface names, linear colours/alpha and icon silhouettes read back; original pack files unchanged.
3. **Inventory first.** Apply light surfaces, Roboto hierarchy and neutral focus/selection rules to the current three-column tree and actual item-action widgets. Gate: portrait, panels, grid, categories and live fragment bindings remain; real counts/footprints unchanged; focus/disabled actions visible on paper.
4. **HUD and Quick Select.** Apply dark tokens to existing HUD/compass/selector, all item-composite leaves and mode views. Add separate equipped notch only if not already represented. Gate: V5 geometry unchanged; border-only assignment pulse and inverse slot0 semantics survive; health warning and world-space markers remain readable over bright/dark gameplay.
5. **Map/journal/quest HUD.** Migrate paper page and native-generated rows, apply category glyphs consistently, assign the paper-toned calibrated map, retain dark tokens for in-world/HUD views. Gate: current target/category/optional/completion/tracking remain distinguishable; selecting a quest does not recolour every category identically; calibration and independent personal marker unchanged.
6. **Ancillary active routes.** Restyle owned copies of confirmed title/pause/settings/save-load/confirmation/loading pages with the same typography, surfaces and controls. Gate: correct local ownership/focus/back routes and truthful enabled/disabled actions; no duplicate menu stacks or accidental rewiring of campaign behavior.
7. **Integration acceptance.** Compile through the native tool outside Python; save explicit owned packages and re-read persisted properties. Use user-authorized runtime checks for mouse/controller focus, long text, aspect/DPI, transitions and HUD recreation. Compare actual screens against the twelve approved references. Gate: Field Notes appearance without data/input regressions; no automated tests or unsolicited PIE.

Mockup readings such as72/100,124 CAD, skills4/6/5/3, infection0%, low mortality, weight, bonuses, example task names,48m, sample inventory quantities and loading62% remain illustrative. Style existing placeholder panels without silently inventing their gameplay backing or removing their structure. Preserve current category rules, assignment/activation controls, checkpoint policy and quest/inventory/save state. Full implementation of the selected look is not permission to add unrelated mechanics.
