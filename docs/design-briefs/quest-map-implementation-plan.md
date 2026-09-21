# CHALK — quest and map implementation plan

September 19, 2026. **Full implementation authorized after plan review.** Artur explicitly requested both systems be implemented completely, using the same careful source-reuse method as the successful compass. Implementer: Codex. The confirmed scope is a story campaign with side quests, a full Map page inside the current inventory, quest targets and personal waypoints, and reuse of ProHUD V2 Horror. Artur has accepted the existing compass/markers and asked to stop unrelated checking of them.

Execution state is in [the implementation ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/quest-map-implementation-progress.md). Autosave at important moments and authored save/checkpoint locations are approved; campfires can provide one save-point presentation. No save-anywhere feature is assumed. Layout alternatives and independent foundational source work are proceeding in parallel under the explicit full-implementation authorization.

Read the [source audit and design](C:/UnrealEngine/Games/AZ/docs/design-briefs/quest-map-audit-and-design.md) for verified source behavior, risks, current ownership and official game references. This plan adds execution order, boundaries, outputs and acceptance gates. No gameplay source/assets were changed to create it.

## Working decisions and boundaries

Confirmed by Artur:

- Narrative campaign with side quests; The Last of Us and The Witcher are design references.
- Journal and objective selection belong with the inventory Map page.
- Targets may complete an explicit arrival objective when the player reaches its area.
- Personal map markers are needed.
- Existing vendor functionality should be reused and adapted.
- Campaign progress must survive UI lifetime; the source widget state alone is insufficient.

Recommended defaults for review:

- One tracked quest at a time; several eligible objectives may progress regardless of which is highlighted.
- One independently removable personal waypoint initially. More saved pins can be added after the first complete slice.
- Start with one district/outdoor map layer; include MapId/layer in data from the beginning.
- Exact pins only for known destinations; uncertain information can show a search area.
- The first quest model lives off the pawn/UI, on the protagonist's PlayerState. World facts belong to GameState. Keep one owner for each fact; do not introduce duplicate personal/shared quest stores. A future shared campaign owner remains an extension, not a second store added speculatively.
- First supported predicates: ReachArea, successful Interact, PossessItem and DeliverItem. Counts, required/optional status and failure rules are explicit.
- Versioned save records are included; the user still needs to choose actual checkpoint/autosave/manual-save policy. An implementation must not silently promise save-anywhere or change the game's time/pause behavior when Map opens.

Final review choices: initial map discovery policy, save trigger policy, sample narrative content, and whether the recommended one-waypoint/one-layer defaults fit the intended first slice. Prepare the implementation around these defaults, but settle material gameplay choices before wiring their dependent behavior.

## Proposed code/content boundaries

Names below are proposed destinations, not claims that these classes already exist. Check collisions at module entry.

| Area | Proposed location / responsibility |
|---|---|
| Definitions and progress | `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Quests/` and matching Private folder: typed quest/objective definitions, authority progress component, events and snapshots |
| Map data/transform | `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Navigation/` and matching Private folder: map definition, reversible coordinate transform, target resolution and waypoint data |
| UI adapters | Existing `Source/AZ/.../UI/` / `InventoryUI/`: local presentation snapshots, tracked selection, Map page input and current CommonUI host integration |
| Quest content | `/Game/AZ/Blueprints/Quests/`: definitions, target/area adapters, explicitly named example content |
| Owned ProHUD copies | `/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/`: mission/task widgets and required context-aware helpers; keep original `/Game/ProHUDV2_Horror` unchanged |
| Full Map page | `/Game/AZ/Blueprints/Menu/Map/`: page, marker widgets and map definitions, adapted from useful minimap pieces |
| Art sources | `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuestMap_v01/`: native GIMP sources, approved exports and world-to-art calibration receipt |
| Evidence/ledger | `C:/UnrealEngine/Games/AZ/Saved/QuestPlanning/`, `Saved/MapPlanning/`, then an implementation ledger recording each completed gate |

No inventory migration, second compass solver, vendor GameMode/AHUD replacement, new quest-editor plugin, matchmaking, automatic enemy revelation or route-finding is included. Use existing GAS actions and CommonUI ownership rather than routing state through the Map widget.

## Module 0 — baseline, layout and first playable contract

**Purpose:** make the intended result reviewable before editing active gameplay/UI.

1. Re-read audit receipts and current code; preserve any intervening Claude/user changes.
2. Snapshot current inventory/HUD widget trees, source branch/diff, settings and all packages that will be edited. Maintain an exact copy/modify manifest. Check current PIE state before asset writes.
3. Prepare three native GIMP Map+journal layouts using CHALK's existing fonts/colors. Each shows the same main quest, side quest, selected objective, personal waypoint and search area so the comparison is about layout.
4. Label story/sample content as illustrative. Do not invent committed narrative or mark all map locations discovered.
5. Agree the selected layout, initial district/layer, waypoint count, discovery behavior and save policy. Record decisions alongside this plan.
6. Define the first vertical slice: one main quest with two sequential objectives and one optional objective, one independent side quest, a moving or component target, and a personal waypoint. Use neutral sample names until narrative content is approved.

**Gate:** Artur can inspect the layout and describe how to open Map, choose/track a quest and place a waypoint. Scope defaults and sample semantics are explicit.

**Rollback:** documents/art only at this module; no gameplay rollback.

## Module 1 — stable data, ownership and target contracts

**Depends on:** module 0 decisions.

1. Add data definitions containing stable QuestId, ObjectiveId, stage prerequisites, localized presentation, Story/Side category, target descriptor, required/optional and explicit success/failure outcome policy.
2. Define runtime states separately from the vendor visual states. At minimum distinguish unavailable/available/active and terminal completed/failed/cancelled, and record objective progress independently from UI focus.
3. Add a PlayerState-owned authority progress component for the first protagonist slice. Mirror the existing PS/ASC/PlayerUI binding pattern. Preserve controller-owned CommonUI inventory and the older inventory component; use an explicit adapter to the active inventory.
4. Define target IDs plus MapId/layer and resolution to loaded Actor/SceneComponent. Level instances must retain authored identity across editor reopen/load; cloning an instance must not silently duplicate a target ID. Reject ambiguous/missing required IDs.
5. Define typed, deduplicatable committed gameplay facts with instigator context. Avoid a global player singleton and PlayerController index-zero lookup.
6. Define versioned save snapshot structures now, including quest progress, choices, committed reward/delivery receipts, discovery and local tracking/waypoint preferences. Do not save widgets, transient actor names or array indices.
7. Separate getters/snapshots from mutations. Gameplay state is authority-owned; UI only requests tracking or accepted gameplay actions.

**Gate:** reflected types compile and load; definitions can represent the agreed sample without widget references. Every state has one owner and explicit initialization/restore behavior. Invalid IDs are diagnosed.

**Build boundary:** new reflected C++ types require the project's full build/restart workflow. Do not use a Live Coding patch as evidence that new reflected layouts are installed. No automatic tests are added.

## Module 2 — quest progression and arrival/interact objectives

**Depends on:** module 1.

1. Implement accept/activate/stage advance/complete/fail operations with allowed transitions and idempotence. Tracking is a separate operation.
2. Add a configurable quest-area actor/component. Expose objective reference, 3D volume and eligible instigator policy. The overlap reports an event; the model validates that the objective is active and commits once.
3. On activation/restore, evaluate whether the player is already inside the area. Do not require leaving/re-entering to progress. Do not use compass XY distance for arrival: above/below a destination must respect the actual volume.
4. Add a successful-interaction adapter to the existing target interaction path. A pressed key, ability activation or animation timer is not a successful interaction outcome.
5. Support parallel eligible objectives and required/optional logic. An optional objective's failure cannot accidentally complete or fail its parent unless the definition says so.
6. Replace demo-specific first-actor lookup, ThirdPersonCharacter casts and fixed task-class numbering with explicit references/IDs.
7. Ensure leaving a demonstration area does not reset campaign progress. Cancel scoped delays/subscriptions when a quest or stage ends; presentation timing must not advance stages.

**Gate:** user-run sample can activate a main/side quest, progress the correct objective once and respect order/optional rules. Repeated/duplicate/out-of-order interaction cannot produce a second result. The same eligible untracked objective still progresses.

**Rollback:** disable the new quest providers/definitions; existing gameplay remains functional.

## Module 3 — owned ProHUD quest tracker and compass connection

**Depends on:** module 2. Reuse the already-working compass API.

1. Copy the mission notification, mission and task widgets plus their exact required helper/config closure. Use explicit owning player/config and the existing safe typed-reference-remap workflow. Do not import the vendor AHUD, GameMode or demo character.
2. Use the notification system to display the **tracked quest**. Keep the list of all accepted/completed quests in the progress model, not CurrentMission.
3. Maintain an explicit ObjectiveId→presentation row/index adapter. Reconstruct rows from the current snapshot so counts/descriptions survive switching and HUD recreation.
4. Fix the verified source removal guard, row-state consistency, collection-order assumptions, detached references and stale delayed callbacks in owned copies. Verify the optional-only default and RemoveAll snapshot behavior before retaining those paths.
5. Let gameplay state decide quest success. Source “all task indices removed” is only a presentation-retirement condition.
6. Update compass/world markers by exact target identity. Remove only the previous tracked quest's registrations; personal waypoints and unrelated markers must survive. If two purposes share the same target, reconcile their display ownership rather than blindly deleting the common key.
7. Handle loaded, missing and streamed-out targets deliberately. A missing actor is not world origin and not objective completion. Resolve/rebind when its stable target becomes available.
8. Preserve the current HUD layout/coexistence and quick-select dim policy. Apply approved short task text and state feedback; long descriptions belong in the Map page.

**Gate:** tracking switches quest/destination consistently; description/count survives row reconstruction; invalid/repeated removal cannot lock the tracker; quest changes do not clear independent markers. Source ProHUD packages remain unchanged.

**Rollback:** detach the new tracker host and quest-navigation adapter; leave the accepted compass intact.

## Module 4 — calibrated full Map in the existing inventory

**Depends on:** modules 0/1. Can be authored independently of progression after those contracts are fixed.

1. Bind the current Map entry to a separate CommonUI page within the existing menu. Keep item category grids, currency displays and inventory behavior intact.
2. Make focus, Back, context actions and pointer ownership page-aware; current inventory handlers assume the grid page. Retain the existing pause/time behavior until a separate decision changes it.
3. Build the first Map definition with explicit world/image origin, axes, scale, north, image extents, playable bounds and MapId/layer. Use the current test level only for a calibration proof; final CHALK district artwork must correspond to its playable geometry.
4. Reuse the source corner-calibration conventions and useful marker widgets, while removing the square64–512 full-page limitation. Support rectangular map images/viewports without stretching.
5. Implement one reversible world↔map transform. Handle axis inversion/orientation explicitly; do not invert the source's clamped/absolute-value output. Apply clipping after projection and align north with compass configuration once.
6. Implement fit-to-area, player recenter, free pan and cursor-centered zoom with viewport clamping. While browsing, source player-follow behavior must not pull the map away from the cursor.
7. Show player position/orientation and a few known calibration targets before adding quest UI complexity. Draw only resolved/known targets for the current map/layer.

**Gate:** user checks several known landmarks, corners and a non-square view; markers remain aligned after pan/zoom/recenter. Map input does not trigger throw/fire or inventory-grid actions. Existing inventory/back/focus still works.

**Rollback:** unbind/hide the Map page and restore its previous shell connection; no quest progress depends on the page.

## Module 5 — journal selection, map targets and personal waypoint

**Depends on:** modules 3/4.

1. Populate the Map side panel from quest snapshots: Story, Side quests, Archive; selected quest details and eligible objectives. Reuse appropriate ProHUD visual rows, not their widget-owned progress as the journal database.
2. A Track action sets the shared local tracked quest/objective. Highlight the same target in the list, map and existing compass. Selecting a row without tracking may inspect it without changing progress.
3. Show a search region for uncertain information and a pin for a known location. Nonspatial objectives remain readable without a fabricated pin. Mark knowledge/discovery separately from completion.
4. Add personal waypoint placement and explicit removal. Convert map-local cursor position using the same inverse transform and enforce map/layer/bounds validity. Do not silently invent height or use zero for an unresolved target.
5. Use a local transient anchor for the existing Actor/SceneComponent-based compass interface when necessary. Save waypoint description/coordinates, not its actor pointer. Retain its own identity so quest changes do not delete it.
6. Add filters and shape/color differences for player, tracked goal, other known goals, search area and personal waypoint. Keep the default view restrained; undiscovered actor existence is not permission to reveal it.
7. Provide equivalent controller selection/placement actions. Confirm final shortcuts against actual current bindings rather than copying the vendor Q binding.

**Gate:** selecting/tracking updates all views without changing quest progress. A manual waypoint survives quest switching and is removable independently. Click-to-world remains accurate after arbitrary supported pan/zoom; restore produces one anchor, not duplicates.

## Module 6 — inventory conditions, delivery and narrative event adapters

**Depends on:** modules 1/2. May be completed before module 5 when needed by the sample quest.

1. Implement PossessItem by reading canonical current quantities on activation/load and relevant inventory changes. Explicitly define how equipped/inserted/container items count.
2. For cumulative acquisition, emit/use a post-commit payload containing accepted quantity, item type/source and a deduplication identity. Stack merges, partial pickups and replication callbacks must not be counted as unrelated new acquisitions.
3. Implement DeliverItem at a validated recipient/interaction transaction: item availability, consumption and progress commit succeed together. Decide partial delivery policy explicitly.
4. Add other narrative adapters only for the first approved content: read note, successful interaction, world fact/dialogue outcome. Keep exploration discovery distinct from quest acceptance.
5. Use Gameplay Tags for categories/requirements and derived interaction gates. Keep quest IDs, counters and branch records in typed state; do not grant a long-running ability for every quest.

**Gate:** menus/grid rearrangement never create progress; stack merges and already-owned items behave according to the chosen predicate; rejected/duplicate delivery gives neither double progress nor extra reward.

## Module 7 — save/load, world targets and UI lifetime

**Depends on:** modules 1–6 and an agreed save trigger policy.

1. Extend the actual project save path after verifying native and Blueprint callers. The inspected native SaveGame class is empty; avoid claiming it already persists quests or inventory.
2. Serialize stable quest/objective states, choices, relevant world facts/discovery, reward/delivery receipts, tracked selection and waypoint data with a schema version.
3. Coordinate inventory/world/progress snapshots so reload cannot restore a consumed quest item inconsistently or duplicate rewards. Make commit ordering and recovery behavior explicit.
4. Restore gameplay state first, then reconcile inventory predicates, resolve target providers and publish UI snapshots. Rebuilding a menu must not replay historical rewards or all completion toasts.
5. Handle pawn replacement, PlayerState/controller rebinding, Map/HUD destruction/recreation and target stream-out/reload. Unsubscribe old callbacks; stale callbacks cannot mutate the new context.
6. Preserve unresolved target identity and known location without falsely completing the objective. Rebind exactly once when the target returns.

**Gate:** user-run save/reload and UI recreation retain the same progress and selection without duplicate targets/rewards. World/character scope follows the chosen policy. Save failure is reported; it is not treated as a successful checkpoint.

## Module 8 — final artwork, readability and acceptance

**Depends on:** functional modules above.

1. Finalize the selected native GIMP layout and CHALK map artwork; keep editable originals and export/import receipts. Source-pack assets remain unchanged.
2. Apply existing font/palette conventions; distinguish personal waypoint from quest goal by shape and label as well as color. Avoid tiny low-contrast map icons and crowded default overlays.
3. Verify display scale/aspect ratio, mouse/controller focus, long localized titles, zero/many journal entries and bright/dark map areas through user-run checks.
4. Check active-map-only update work: static targets are event-driven/cached, moving targets update only where visible/needed, and world scans are not per-frame. Keep per-player ownership explicit; full co-op/late-join tests remain separately scoped.
5. Finalize the designer guide: create a quest, add stage/optional objective, assign a world target or search area, select completion predicate, set map calibration and validate an authored save ID.
6. Record actual edited assets/source, builds, saved-package state, user acceptance and remaining limitations. Leave unrelated compass/animation work untouched.

**Gate:** approved sample story and side quest work from activation through navigation and completion, with independent waypoint and correct save/load. Artur accepts the integrated Map/journal appearance and input behavior.

## Execution discipline

- Each module has a recorded entry state, changes, compile/save evidence, user acceptance and rollback scope.
- Use read-only source inspection and appropriate compilation; no new automated tests without an explicit request.
- Do not start PIE/editor tests without asking. Prefer Artur running the scenario, then inspect logs/runtime state when relevant. His acceptance of the existing compass must not trigger repeated unrelated testing.
- Reflected C++ builds use the project full-build/restart workflow; editor-only Blueprint changes use safe native compilation after script return. Recheck PIE before mutations, and save verified owned assets explicitly.
- Avoid repeating the compass integration's broad ownership/reference mistakes: explicit local context, no global widget/player-zero discovery, real initialization completion, and keyed lifecycle cleanup.
- A copied widget, successful build or scrolling map alone does not complete a module whose user-flow gate remains open.

## First action after approval

Begin module 0: reviewable Map+journal layouts and the first neutral sample quest contract, then settle the few material defaults. Do not create gameplay classes or connect a new inventory page before that gate. The accepted source audit need not be repeated except where the project changed.
