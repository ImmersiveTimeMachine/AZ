# Next task — distinguish quest and navigation markers

Status: **reviewed and recorded; implementation deferred until the next session.** September 19, 2026. Artur reports that the integrated flow appears to work, but markers are visually indistinguishable. This is preliminary user feedback, not evidence that every save/load and lifecycle acceptance case has passed.

Latest instruction: inspect ProHUD, record the next task and continue later. No gameplay code, Blueprint graphs, asset defaults or level actors were changed during this review. Only read-only inspection receipts, exported reference images and documentation were created.

## Verified cause and available source content

The owned QuestModule has separate `QuestCompassInfo`, `PersonalCompassInfo`, `QuestWorldInfo` and `PersonalWorldInfo` settings, but both pairs currently contain identical values. The integration recipe deliberately seeded both from the same CHALK navigation example. This explains the current compass/world-marker sameness; the styling capability itself is available.

The live compass defaults reference `/Game/AZ/Blueprints/Menu/HUD/Navigation/Art/T_CHALK_CompassTarget`. Both marker/highlight color overrides are zero. Source `HasColorChanged` checks whether any RGBA channel is nonzero: zero is the theme-inheritance sentinel, not an explicit chosen color. A future override must be deliberate and must account for the shared theme fallback.

The native full-map painter currently distinguishes personal markers (open corners) from quest markers (diamonds), and uses personal/tracked/default colors. It does **not** distinguish Story from Side by shape or category color. Its tracked/selected color can override category meaning if we merely add more color branches. The current marker view carries quest/objective IDs, optional/tracked/personal flags, but no quest category or presentation profile key.

Actual reusable content found in `/Game/ProHUDV2_Horror`:

| Source | Reusable capability |
|---|---|
| `Blueprints/Structs/S_CompassMarkerInfo_H` | Texture, scale, marker and highlight colors, distance text, ping and visibility behavior |
| `Blueprints/Structs/S_WorldMarkerInfo_H` | World-marker presentation and label/distance settings |
| `Blueprints/Structs/S_TaskBaseInfo` | Separate icon/text presentation for empty, active, checked and failed task states |
| `Blueprints/Structs/S_TextInfo_H` | Inherited or manual font, scale and color |
| `Textures/MissionNoti/T_MissionIcon_Empty_H`, `Active_H`, `Checked_H`, `Failed_H` | Existing task-state texture assets already referenced by the owned task settings |
| `Textures/Compass/T_DemoIcon1_64x64_H` | Visually inspected outlined double-diamond symbol |
| `Textures/Compass/T_DemoMarker1_64x64_H` | Visually inspected circular exclamation marker; assess small-size readability before reuse |
| `Textures/Compass/Pointers/` and compass ping textures | Direction/ping alternatives, not automatically quest-category symbols |
| `Fonts/F_Roboto`, `F_RobotoCondensed`, `F_Martel`, `F_Trirong` | Existing font families with several weights; availability does not imply approval for CHALK |

Some demo textures are patterns/masks rather than finished semantic icons (`T_DemoMarker_64x64_H` is diagonal stripes). Do not assign textures based on filenames alone. White RGB/alpha-mask exports need inspection against both light and dark backgrounds before selection.

Evidence: `C:/UnrealEngine/Games/AZ/Saved/QuestMarkerStyleReview/source-style-audit.json`, `color-fallback-readback.json`, and the six exported PNG references in that directory. Existing renderer/native source references are listed below.

## Proposed visual meaning — review before choosing exact art

Use **shape plus restrained color**, with the same meaning in compass, world, Map and journal. Do not depend on color alone.

| Meaning | Proposed starting point, not yet approved |
|---|---|
| Story quest | Double diamond; restrained warm accent |
| Side quest | A visibly different circle or single-outline symbol; chalk/cool neutral accent |
| Personal waypoint | Existing open-corner motif; sage accent |
| Tracked/selected | Stronger outline or small halo; preserve its category shape/color |
| Optional objective | Secondary badge/outline and explicit optional text; retain parent quest identity |
| Completed/failed | State glyph/text in the journal/task row; do not confuse lifecycle state with quest category |

Arrival, interaction, possession and delivery are **objective kinds**, while Story/Side are **quest categories**. They already exist in native definitions. Do not create separate ability classes, new gameplay states or C++ subclasses merely for different icons. If useful, add small objective-kind badges after the three primary marker roles are clear.

Keep a common text family and distinguish hierarchy through size/weight. Different fonts for every quest class would undermine the user's requested unified UI direction. Font alternatives belong in the later whole-game mockup comparison, not an unapproved font replacement now.

## Implementation sequence and gates

1. **Freeze the baseline.** Record the current user feedback, saved asset defaults and preview/demo markers. Separate unrelated demonstration targets from actual tracked quest/personal markers. Keep original ProHUD content unchanged. Preserve the working quest, waypoint, save and HUD-lifecycle logic.
   - Gate: exact list of visible marker producers and their current style sources; no hidden shared-default assumption.

2. **Build a compact source comparison.** Show the actual ProHUD candidate icons on bright/dark backgrounds at their intended compass, world and map sizes. Compare three roles together with active/completed/failed task rows. Identify masks/blurred demo symbols before choosing. This is a focused readability comparison, not a redesign of the inventory or all menus.
   - Gate: Artur chooses a clear role-to-symbol mapping; exact colors remain compatible with the later unified design task.

3. **Introduce one presentation definition.** Use one owned, editable style set keyed by semantic role (initially StoryQuest, SideQuest, PersonalWaypoint). Each entry contains icon/shape, normal/highlight color and size; common typography and task-state styles live alongside it. Derive category from the existing quest definition. Visual configuration must not become a second progress store or enter save records as transient widget data.
   - Gate: all display surfaces resolve the same role to the same visual meaning from one configuration source. A designer can edit a category in one place.

4. **Adapt the compass/world bridge first.** Fill the separate existing style structs from the shared presentation definition. Obtain role/category from native quest metadata or a keyed read-only accessor. Keep existing marker delegate signatures where practical; do not broaden this into a rewrite of the accepted compass. Apply changes through the existing keyed upsert path, preserving the separate quest and personal anchor objects.
   - Gate: switching between Story and Side changes appearance; personal waypoint and unrelated markers survive; no duplicate registrations or lingering stale icons.

5. **Bring the full Map into parity.** Extend marker view metadata with category/presentation role and replace the current hardcoded quest-diamond branch with profile-driven shapes/icons. Selection must enhance a marker without erasing its category. Preserve XY calibration, search-area projection, pan/zoom, hit targeting and waypoint persistence. New reflected types require a proper build/restart; plan that boundary before editing.
   - Gate: the same target is recognizable across Map, compass and world; bright/dark backgrounds and zoom do not make roles indistinguishable.

6. **Apply task/journal hierarchy.** Reuse the owned ProHUD state icons/text settings for active, completed and failed objectives. Display Story/Side identity and optional status consistently in tracker and journal. Preserve native snapshot-driven row order/counts and the previously fixed widget lifetime behavior. Do not reintroduce vendor widget-owned quest progression or array-index identity.
   - Gate: quest category, objective kind and objective state remain distinguishable; long text is readable; tracking does not change completion.

7. **User-run acceptance and save.** Check Story ↔ Side switching, a simultaneous personal waypoint, optional/completed/failed rows, load/reopen and HUD recreation. Verify independent marker removal, small compass readability and category recognition without color. Compile/save only owned assets and inspect logs after the user's run. No automated tests or assistant-started PIE without explicit authorization.
   - Gate: functional/readability acceptance. Record remaining save/load cases separately rather than treating “looks like it works” as exhaustive validation.

## Boundary with the later unified design task

Artur explicitly asked to remember: **working systems first; a shared design language for inventory, Map, HUD and all menus comes afterward.** The next session should review this marker-readability task first. Then prepare alternative whole-interface mockups and choose shared fonts, palette, spacing, components and interaction states. Current Field Journal styling is a working layout, not final whole-game design approval.

## Current implementation references

- `C:/UnrealEngine/Games/AZ/Tools/quest_runtime_bindings.py` — identical seeded quest/personal defaults and keyed adapter.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Quests/AZ_QuestTypes.h` — existing Story/Side categories, objective kinds and statuses.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_QuestMapComponent.h` — marker view metadata/delegate boundary.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_MapCanvasWidget.cpp` — current procedural marker shapes and selection colors.
- `C:/UnrealEngine/Games/AZ/Tools/quest_mission_presentation.py` — native snapshot-to-ProHUD presentation; keep its authoring safeguards.
