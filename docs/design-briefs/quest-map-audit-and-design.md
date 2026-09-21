# CHALK — quests, map and objective navigation

September 19, 2026. **Analysis and proposed design; gameplay implementation is not authorized by this document.** Artur confirmed a story campaign with side quests, asked to inspect the existing ProHUD V2 Horror implementation first, and wants the inventory Map entry to show objectives and support personal waypoints. He reports the integrated compass/markers work well and asked to stop further compass testing.

## Result of the source audit

ProHUD provides a useful mission/task presentation system, an executable six-task demonstration, and a calibrated texture minimap. Reuse these foundations. A persistent campaign journal and an interactive inventory map still need to be connected and extended for CHALK.

| Area | Verified existing functionality | What CHALK needs to add or adapt |
|---|---|---|
| Mission display | One displayed mission, heading, several selected task rows, one highlighted task, optional labels, completion/failure feedback | A collection of main/side quests, persistent progress, tracked quest selection, current data supplied to the display |
| Arrival objectives | Demo actors complete tasks through Box begin-overlap; stage activation and the next marker are wired in the demo | Configurable objective volumes, actual player context, stable objective IDs, active-stage validation, repeat/restore safety |
| Navigation | Current AZ compass and world markers; source minimap marker support for Actor/SceneComponent | One tracked-target selection shared by quest panel, full map and existing compass; independent personal waypoint |
| Inventory Map entry | Current WBPMapButton_1 and existing CommonUI menu shell | Map page, map content, button binding and page-aware input/focus/back routing |
| Map foundation | Texture map, world/image corner calibration, player/target XY projection, zoom, pointer and distant-marker presentation | Rectangular full map, free pan, cursor-centered zoom, selection, click-to-world inverse transform, discovery policy |
| Save/progress | Item identities and authority patterns exist; native save class is currently empty | Versioned quest/map progress snapshots and integration with inventory/world saves; exact save trigger policy remains a design choice |

Source receipts are read-only exports. No ProHUD/AZ gameplay assets or code were changed during this audit, and no Play session or automated test was started.

## What the shipped mission demonstration actually does

The source mission trigger creates six task records. It activates task 1 first, then tasks 2/3, then tasks 4/5/6. Some are optional. Entering an enabled task actor's Box overlap executes task removal with a completion/failure presentation outcome and can activate the next stage. Thus **reaching a destination can complete a ReachArea objective**, as Artur expected. The marker itself is only its display; it does not evaluate the completion rule.

The demonstration's Q binding switches the displayed current task, receives its target object and creates a world marker for that target. That demo flow does not itself add a compass marker. Its task targets are MaterialBillboard components. Several helpers find the first actor of a particular demo class and cast overlaps to the vendor demo character; those dependencies must become explicit CHALK references/context.

The source exit area removes the mission and all world markers, then resets demo DoOnce gates. This is a demonstration reset, not suitable campaign persistence. Several delayed stage transitions are not cancelled by that exit path. These are static integration risks, not gameplay failures reproduced in CHALK.

Relevant source assets:

- `/Game/ProHUDV2_Horror/Widgets/Content/MissionNotification/WB_MissionNotification_H`
- `/Game/ProHUDV2_Horror/Widgets/Content/MissionNotification/WB_Mission_H`
- `/Game/ProHUDV2_Horror/Widgets/Content/MissionNotification/WB_Task_H`
- `/Game/ProHUDV2_Horror/Blueprints/Structs/S_Task_H`
- `/Game/ProHUDV2_Horror/DemoContent/Blueprints/DemoActor/Mission/BP_DemoMissionObject_H`
- `/Game/ProHUDV2_Horror/DemoContent/Blueprints/DemoActor/Mission/BP_DemoTaskObject1_H` through `BP_DemoTaskObject6_H`
- `/Game/ProHUDV2_Horror/DemoContent/Blueprints/DemoActor/Mission/BP_MissionArea_H`

## Presentation state is not campaign progress

`S_Task_H` contains a target UObject, object name, description, font/style information and an optional flag. The displayed mission uses task-array indices and row-widget references. Its public operations include create/remove mission, select active task indices, switch task, remove task, and update/get task description. The native schema confirms visual task states Inactive/Active/Completed/Failed and removal outcomes Completed/Failed/Just Remove. Raw enum ordinals must be read from their actual types during implementation rather than inferred from a name or schema order.

A new mission replaces the current displayed mission. That is useful for a **tracked quest panel**. It is not a store of every accepted/completed quest. Its automatic retirement checks whether all original task indices have been removed, including optional tasks and regardless of completion/failure outcome. That check must not decide campaign success.

The owned presentation adapter needs a narrow hardening pass before reuse:

1. Invalid or repeated task removal must restore switching permission instead of leaving it disabled.
2. Current descriptions/counts must survive task switching and row reconstruction; regenerate from the quest snapshot or update the authoritative presentation model.
3. Row-state snapshots and visible highlighted state must agree.
4. Map an explicit objective ID to its row; do not depend on matching Map-key order and an index array.
5. Cancel old presentation delays and clear detached widget references. A delayed fade must not decide which quest is active or remove a newer marker.
6. Remove only markers owned by the relevant quest/objective. Never call global RemoveAllWorldMarkers when one quest changes.

The optional-only local default and RemoveAll collection-snapshot behavior remain narrow source questions for implementation preflight; neither is needed to design the overall campaign model.

## Proposed player experience

Use the inventory's **Map** entry as one page containing both the map and a quest journal panel. Preserve the existing inventory design, typography and CommonUI shell.

- A collapsible side panel lists **Story**, **Side quests**, and an **Archive** containing completed/failed entries. Main and side quests use the same runtime rules.
- Selecting a quest shows its description and current objectives. **Track** chooses the quest shown in the compact HUD tracker and the target highlighted on the map/compass.
- Multiple eligible objectives may progress at once. Selecting a different row changes guidance, not which world actions are allowed to count.
- A quest with an exact known destination shows a pin. A clue with an approximate location shows a search area. A purely informational step can remain in the journal without inventing coordinates.
- A personal waypoint has a separate visual identity. Placing/removing it never accepts, advances, fails or completes a quest.
- Proposed initial clutter policy: one tracked quest, its chosen destination emphasized, other active destinations muted on the open map, and one independent personal waypoint. The maximum personal-waypoint count is tunable and can expand later.
- The compact quest panel shows a short immediate action and brief progress feedback. Long story descriptions stay in the Map/journal page.

This is a recommended UI arrangement, not an approved final art layout. Create a reviewable layout before editing the active inventory widget tree.

## Lessons from the requested game references

These are specific design lessons, not claims that CHALK should copy either game's entire quest architecture.

- **The Witcher 3: configurable information density.** CD Projekt's official next-gen notes describe a default map filter that hides some icons and optional hiding of minimap/objectives while exploring. For CHALK, use a restrained default filter with explicit access to other known objectives; avoid filling the map with every undiscovered location. [Official update notes](https://www.thewitcher.com/ro/en/news/47105/next-gen-update-list-of-changes).
- **The Last of Us Part II: authored story pacing with room to explore.** In the developer interview, larger layouts support moments when tension drops and the player decides what to explore. For CHALK, use authored district segments and meaningful optional tasks around the campaign route. This recommendation follows the user's campaign choice; it does not require an open-world simulation. [Developer interview](https://blog.playstation.com/archive/2019/10/09/neil-druckmann-discusses-new-the-last-of-us-part-ii-details/).
- **Optional guidance.** The official accessibility page describes optional assistance that points toward story progression. For CHALK, make navigation emphasis adjustable and keep target direction distinct from a promised traversable route. Route/pathfinding assistance would be a separate feature. [Official accessibility description](https://www.playstation.com/en-ca/games/the-last-of-us-part-ii/accessibility/).

The latest CHALK synopsis establishes a teacher who sheltered afflicted parents/neighbours, with compassion and intermittent lucidity central. Help, supplies, investigation and reaching safety are plausible objective families. Particular quests, choices and story consequences still require authored narrative decisions; no new story canon is introduced here.

## Quest model and GAS integration

Keep a small gameplay model independent of widget lifetime. Proposed data:

- **Quest definition:** stable QuestId, localized title/description, Story/Side category, availability prerequisites, stages/objectives and explicit outcomes.
- **Objective definition:** stable ObjectiveId, kind, target descriptor, required count or condition, required/optional flag, activation prerequisites, and display policy.
- **Runtime progress:** current stage, objective state/counters, completion/failure facts, resolved branch choices and committed reward/delivery receipts.
- **Navigation target descriptor:** stable world target ID or authored location/search area, plus MapId and optional layer. Resolve it to an Actor/SceneComponent for the existing compass bridge when available.
- **Local presentation preferences:** tracked quest/objective, map filters/view position and personal waypoint. These do not own quest success.

Use authored stable IDs, not localized captions, row pointers, array positions, transient actor names or new IDs generated on every BeginPlay. PlayerState and GameState are candidate lifetime owners for personal and shared campaign state respectively; neither automatically saves to disk. Establish the initial shared/personal boundary before coding the first component. Existing controller-owned CommonUI inventory does not need to be migrated for this feature.

| Initial objective kind | Completion input | Important rule |
|---|---|---|
| ReachArea | Enter an authored 3D objective volume | Validate actor/player and active objective; count once; also evaluate already-inside when activated/restored. Being above/below the destination must not complete via XY distance alone. |
| Interact | Successful result from the relevant world interaction | An input press, ability start or timer is not proof that interaction succeeded. |
| PossessItem | Recount current canonical inventory quantity | Recount once on activation/load and on relevant inventory changes. Do not count UI notifications as acquisitions. |
| DeliverItem | Validated inventory/destination transaction | Consume and progress atomically, once. Distinguish possession, cumulative acquisition and delivery. |

Confirmed AZ event caveats: OnInventoryChanged also fires for grid changes/menu opening/replication; OnItemAdded misses stack merges. Use quantity recounting for possession goals and a typed post-commit result for cumulative acquisition/delivery. Existing GAS interaction completion is not a generic target-success receipt. A small adapter is needed; rewriting inventory is not.

Gameplay Tags can classify item/event/interaction requirements and expose derived gameplay gates. IDs, counts, progress and save records remain typed data. There is no need for a permanent ability per quest. Actions can continue through GAS and emit validated outcomes to the quest model.

Required-objective success, optional failure, abandoned tasks and branch outcomes are explicit model rules. Disabling tracking or hiding the HUD never changes progress. A replayed overlap/receipt cannot award the same progress or reward twice.

## Full map foundation and extensions

Useful source assets include `WB_Minimap_H`, `WB_MinimapMarker_H`, `WB_MinimapDistantMarker_H`, `BP_Minimap_BottomLeft_H`, `BP_Minimap_TopRight_H`, and editor utility `EUW_MinimapCalibrator_H`, all under `/Game/ProHUDV2_Horror`.

The source projection uses two world-corner anchors and corresponding image coordinates, transforms horizontal XY and discards Z. It supports player following, zoom, marker icons and off-map indicators. The widget enforces a square size clamped to64–512 and derives a square canvas from texture width. Its calibrator adjusts an editor image; it is not a runtime interactive-map controller. The inspected widget CDO has no assigned map texture/corner actors; demo instances/settings may supply them. Demo textures do not represent the CHALK level.

Build an AZ-owned full-map view around the verified calibration convention and useful presentation pieces:

1. **Map definition:** MapId, map artwork, world/image calibration, orientation/north, playable bounds, layer metadata and discovered places. Validate nonzero spans and support rectangular aspect ratios.
2. **One coordinate transform:** world→map and its inverse use the same explicit origin/axes/scale. Keep clipping at the presentation boundary; clamped/absolute-value source helpers cannot be inverted blindly. Align map north with the existing compass configuration exactly once.
3. **Independent camera:** drag pans; scroll zooms around the cursor; recenter is explicit. Automatic player following stops while the user browses. Fit/clamp the map to its viewport without stretching.
4. **Selection and waypoint:** click selects a known pin; an explicit place-waypoint action converts map-local input through the inverse transform. Keep it within valid map bounds/layer. Reuse one local transient anchor for compass compatibility; save waypoint data, not that actor pointer. Unknown height/layer must not silently become world origin.
5. **Marker layers:** player, tracked objective, other known objectives/places, search areas and personal waypoint. Discovery determines whether a target may be shown; an Actor's mere existence is not discovery.
6. **Art:** author a CHALK district map from actual playable geometry, with readable streets/buildings and restrained chalk/peach/sage markings matching the HUD. First prove alignment on the current test level; final district artwork follows the approved level layout. No live SceneCapture rendering is required by this design.
7. **Input/accessibility:** use the current CommonUI activation/focus/back flow. While the map handles drag/click, inventory-grid and gameplay throw/fire actions must not receive those inputs. Offer controller focus/selection equivalents and distinguish marker types by shape as well as color. Final shortcuts must be checked against current bindings.

Start with one outdoor layer. Retain MapId/layer/target-height metadata so a later interior floor view can be added without redefining quest identities. Do not imply the current vendor XY minimap already supports floors or a navigable route.

## Shared progress and persistence

The current native save class is empty, and inspected quest/map widgets contain runtime object references. Saving is a real part of the new integration, not something supplied by hiding/showing the menu.

Save versioned quest/objective states, choices, discovery, tracked selection and waypoint descriptions using stable IDs. Coordinate the snapshot with inventory/world state so loading cannot duplicate a reward or restore a delivered item inconsistently. Restore gameplay state, reconcile inventory predicates, resolve loaded targets, then reconstruct map/HUD views without replaying all historical notifications.

A streamed-out target is unresolved, not completed or moved to zero. It can retain an authored/known map location while its runtime Actor reference is absent. Rebind when it registers again. Choose autosave/checkpoint/manual triggers with the campaign/save design; do not promise save-anywhere in this phase.

## Implementation order with separate review gates

| Step | Deliverable | Gate before expanding |
|---|---|---|
| 0 | Confirm scope defaults; review Map+journal layout and simple sample content | User can understand where main/side quests, track action and personal waypoint appear. No gameplay edits yet. |
| 1 | Stable quest/objective/target and map definitions; ownership and save snapshot contract | Activation, progress, tracking and persistence have distinct owners; definitions do not depend on widgets. |
| 2 | One main quest and one side quest driven by ReachArea/Interact | Repeated/out-of-order overlap is safe; an already-inside active target works; untracked eligible objectives still progress. |
| 3 | Owned ProHUD tracked-mission presentation and existing compass adapter | Switching quests/rows preserves progress/text; only the relevant quest's markers change; unrelated/personal markers survive. |
| 4 | Full-map page/calibration in current inventory shell | Player and known targets align at several landmarks; rectangular sizing, north, pan/zoom and recenter are consistent. |
| 5 | Quest selection on map and independent personal waypoint | Same selected destination on map/compass; click-to-world round trip remains accurate after pan/zoom; no quest progress from pin placement. |
| 6 | Inventory possession/delivery integration and discovery/search areas | Stack merges, partial delivery, menu reopen and replayed events cannot produce false progress. |
| 7 | Save/load and target/HUD lifecycle | Reopen/recreate UI, restart/load and resolve unloaded targets without lost progress, duplicate rewards or origin markers. |
| 8 | Approved CHALK artwork and final UX/accessibility pass | Readability across light/dark map areas, mouse/controller input and existing inventory/gameplay coexistence. |

At each implementation stage use source/asset readback and appropriate native compilation. Artur controls Play validation. No automated tests or agent-started PIE are authorized by this analysis. The user has already accepted the compass/markers; do not restart unrelated compass work.

## Decisions to settle before implementation

Confirmed: campaign plus side quests; Map inside inventory; quest targets and personal map markers; reuse ProHUD; existing compass already accepted.

Recommended initial defaults: one tracked quest, one independent personal waypoint, one outdoor district layer, explicit ReachArea/Interact/PossessItem/DeliverItem rules, and only known locations shown. Remaining material choices are map/discovery behavior, save trigger policy, shared-versus-personal campaign progress, and the first authored sample quest. These do not block the current analysis.

## Evidence

- [Mission/task widget audit](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/mission-widget-audit.md)
- [Executable demo objective audit](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/demo-objective-audit.md)
- [AZ ownership, inventory events and save audit](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/az-quest-integration-notes.md)
- [Fresh mission/task graph export](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/vendor-task-graphs.json)
- [Mission facade and settings export](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/vendor-mission-facade.json)
- [Native task schemas/defaults](C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/mission-types-defaults.json)
- [Minimap reuse and full-map gaps](C:/UnrealEngine/Games/AZ/Saved/MapPlanning/minimap-reuse-audit.md)
- [Fresh inventory and minimap graph/tree export](C:/UnrealEngine/Games/AZ/Saved/MapPlanning/map-source-graphs.json)
- [Minimap widget default/calibration schema](C:/UnrealEngine/Games/AZ/Saved/MapPlanning/minimap-defaults.json)

Raw protected enum-name maps/local-variable defaults were not accessible through the inspected Python/property tools. Do not claim these narrow values were verified. Display labels and struct fields above come from the native property schema and graph behavior.
