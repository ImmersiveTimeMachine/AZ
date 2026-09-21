# CHALK quest, target and map authoring

Implementation guide for the current single-protagonist, same-map campaign slice. Asset compilation and data validation are separate from user-run gameplay acceptance; consult the implementation ledger for the current gate.

## Create a quest

Create an `AZ_QuestDefinition` Data Asset under `/Game/AZ/Blueprints/Quests/`. Give it a unique `QuestId`, title, description and Story/Side category. Each objective has its own unique `ObjectiveId` within that quest, a description, kind and positive required count. Use `PrerequisiteObjectives` to express order; leave independent objectives without prerequisites. The native validator rejects missing IDs, invalid prerequisites and cycles.

- `ReachArea`: a player enters the matching authored three-dimensional objective volume. A compass marker is only a display and does not complete the objective.
- `Interact`: a matching world action successfully commits. Raw input and animation start are not completion.
- `PossessItem`: progress is based on the canonical current inventory quantity for the configured item type. Menu rearrangement is not acquisition.
- `DeliverItem`: the target validates the active objective and available quantity, then consumes items and commits progress as one operation. Use the actual item type in its manifest; do not infer the tag from mesh, color or asset name.

Mark optional objectives with `bOptional`. Required objectives determine completion by default; an all-optional quest requires all its objectives. An optional failure only fails its quest when the explicit failure policy says so. `PrerequisiteQuests` must refer to valid definitions in the authored catalog.

The two assets in `/Game/AZ/Blueprints/Quests/Examples/` are clearly labeled TEST content, not approved narrative. The main example is accept → reach → use, with an optional second area. The side example uses the verified blue sample item and its `Item.Type.Consumable.Buff` tag.

## Place providers

Use `AZ_QuestWorldActor` for a quest offer, an immediate successful-use objective or a delivery recipient. Configure `Action`, `OfferDefinition` or the quest/objective IDs, the interaction prompt and radius. Its `DisplayMesh` is presentation. Its interaction volume and native controller route perform availability, distance and obstruction checks.

Use `AZ_QuestReachArea` for arrival. Configure the quest/objective IDs and the box extent. Height matters: standing on another floor above a marker does not satisfy the area solely through horizontal proximity. Activation and load also reconcile a player who is already within the area.

Each spatial provider carries `AZ_NavigationTargetComponent` with a serialized `TargetId`, `MapId` and `LayerId`. Copy those same identifiers into the objective's target descriptor. The component supports an attachment-relative `LocalOffset`, so a moving actor or its component can be a destination. Do not reference transient actor labels as persistent identity.

Placed targets receive editor identities; ordinary editor duplication regenerates an identity. After duplicating a provider, update the quest descriptor to the intended new instance. Duplicate registered identities are deliberately unresolved rather than silently selecting the first actor. Registration alone does not reveal an unknown quest or advance it.

For nonspatial objectives, leave the target descriptor empty. For authored known coordinates, use `bHasWorldLocation` and an explicit location/map/layer. A positive `SearchRadius` displays an area. Missing or unloaded targets must not be converted to world origin.

## Author map artwork and calibration

The current test map is `/Game/AZ/Blueprints/Menu/Map/DA_AZ_Map_L001`, using `L_001` / `Outdoor`. Its 2048-square texture is generated from the actual editor level; it includes the landscape extension and contains no baked quest/loot/enemy markers.

`WorldOrigin` is the center of the calibration rectangle, not its minimum corner. `WorldSizeCm` contains the full positive X/Y spans, not half-extents. `RotationDegrees` is the world yaw of the map's local X axis. `bFlipU`/`bFlipV` reflect the source artwork's orientation. World-to-map projection remains unclamped; clipping occurs in the view. The inverse returns XY on the authored origin Z plane and does not invent terrain height.

Current measured calibration: center `(-1435, 3620, 0)`, spans `(12400, 12400)` cm, yaw `0`, no flips. Image right is world +X; image up is world −Y, consistent with the existing compass orientation. Do not reuse these numbers for another level. Capture, inspect axes and landmarks, and author a separate definition.

Editable source: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuestMap_v01/sources/map/CHALK_L001_FieldJournal_NATIVE.xcf`. Keep any future paint-over registered to the same geometry; do not crop, stretch or rotate it without updating calibration.

## Map and journal use

The inventory menu's `MapPageClass` references `WBP_AZ_QuestMapPage`. The page uses layout 01 Field Journal: journal on the left, map on the right. Quest state lives on PlayerState, so closing/recreating the page does not erase progress.

Click a quest/objective to inspect it; use **Track selected objective** to change tracking. Inspection does not complete or accept tasks. Story, Side and Archive groups are derived from the quest definitions and runtime status. A personal marker is stored separately from the tracked quest.

Mouse controls: scroll to zoom around the pointer; left-drag to pan; click a goal to select; right-click to place the personal marker. The page has Recenter and Clear personal marker actions. Keyboard map controls currently include arrows, Home, +/− and Enter while the canvas has focus. Input routing and controller parity remain part of final user acceptance.

The existing compass bridge receives only keyed quest/personal registrations. Quest switching must remove only its own registrations; unrelated compass markers remain independently owned.

## Checkpoints and persistence

Place an `AZ_CampaignCheckpoint` and give it a unique, nonempty `CheckpointId`. Use a reachable point above the floor for its actor origin, configure `SaveRadius`, and provide visible art. A campfire is one possible visual representation, not a separate storage system. The current TEST asset uses a readable save-point sign.

Manual saving is performed at a validated save point. Important quest changes request autosave. Busy actions or unsupported context may defer/refuse saving, and failure must be shown to the player. A load request being accepted is not proof of completion: final feedback comes from `OnLoadCompleted` after movement and restored data are reconciled.

Every existing placed CommonUI pickup must have a unique serialized `CampaignPickupId`. Do not generate placed IDs at BeginPlay. The authoring recipe checks actual editor-owned components, preserves existing valid IDs, rejects duplicates and backs up affected packages. Recheck IDs after manually duplicating pickups. Runtime dropped items use their own native identity path.

The current save format includes supported inventory/manifests, placements, equipment/quickbar selection, player attributes/transform, quest progress/tracking, personal waypoint, supported pickups and world facts. It supports one local authoritative protagonist in the currently loaded map with the synchronous Mover backend. It does not claim arbitrary AI-state, multiplayer, cross-map or death-respawn restoration.

## Acceptance route

After asset integration is declared ready, the user starts Play. Check main and side acceptance, ordered/optional progress, journal reopening, tracking changes, independent personal marker, arrival height, successful delivery, checkpoint/autosave feedback and confirmed load. Verify delivered/collected items do not duplicate after loading and existing inventory/weapon inputs remain isolated while Map is open.

No automated tests are added. Do not start Play on the user's behalf without authorization.
