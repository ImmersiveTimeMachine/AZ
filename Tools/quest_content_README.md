# Quest example content recipe

`C:/UnrealEngine/Games/AZ/Tools/quest_content_setup.py` is inert when imported. It authors only neutral TEST definitions/fixtures under `/Game/AZ/Blueprints/Quests/Examples` and assigns missing persistent IDs to actual placed inventory pickups. No story canon is introduced; no quest auto-accepts at BeginPlay.

## Stages run by root

1. `specification()` is file-only. Its side-quest source is verified in `Saved/QuestMapImplementation/reopened-editor-audit.json`: BLUE potion class, actual `Item.Type.Consumable.Buff`, count 1. The red bottle has a different, surprising tag; appearance is not used as type evidence.
2. In an Idle editor, `prepare_assets()` creates two Quest DataAssets and seven native-derived BPs. Existing unowned destinations are refused. Compile the seven BPs with the dedicated native tool after the script returns.
3. `configure_assets()` fills every objective and validates both definitions and the complete catalog. Struct arrays are read back by native export. Compile configured BPs and explicitly save the nine assets. The recipe never calls compilation or asset saving internally.
4. `inspect_pickup_ids()` reads real GUID values, not wrapper summaries. `assign_pickup_ids()` rejects duplicates before writing, backs up clean touched packages, changes only empty CampaignPickupId fields, then reads them back. Explicitly save the listed map/actor packages before continuing. It preserves every valid pre-existing ID.
5. `placement_plan(anchor=(x,y), ground_z=z)` only proposes explicit coordinates. Review actual terrain, existing actors and pickup mesh offset; the supplied Z offsets are not measurements. Keep checkpoint/interaction origins above the floor for LOS. Root may edit each coordinate before `place_examples(plan, geometry_reviewed=True)`.
6. Placement creates only missing tag-owned TEST actors, never moves existing actors, and does not touch CompassPreview actors. Reruns find exact ownership tags rather than trusting actor labels. Foreign/duplicate target IDs or class mismatches stop the script. Save explicitly and let the user run Play checks.

Receipts/backups are under `Saved/QuestMapImplementation/ContentAuthoring`. A partial editor failure is for inspection/recovery, not broad deletion. Existing dirty touched packages must be saved before backup; the script will not silently overwrite their disk baseline. Actor enumeration uses the engine's editor-only, non-template/nontransient filtered enumeration. GUID candidates are actor-owned Native/SCS components; generated/instance components are reported as skipped for explicit review.

## Fixture behavior

- Main offer: press E to accept `TEST.Main.Route`. Reach the first volume, optionally visit the second volume, then press E at the test panel. Required completion closes the main quest and cancels any unfinished optional objective, matching current runtime policy.
- Side offer: press E to accept `TEST.Side.Inventory`. Already-owned matching items qualify. Possess one matching item, then deliver one at the recipient. The side quest is independent of main completion. Possession is nonspatial and has no fabricated target pin; delivery has a real stable target.
- One new copy of the verified blue pickup is optional sample supply within the fixture plan. Original placed supplies are not moved or consumed by authoring. Quantity/type matching uses canonical inventory, not actor appearance; future child tags may also satisfy the predicate.
- Save point: manual E at the authored checkpoint; no save-anywhere action. Existing important-quest autosave remains runtime-owned. Every placed pickup requires a unique saved CampaignPickupId before a meaningful save/load check.
- Derived interaction BPs have the native DisplayMesh component but no new visual art is invented here. Root should assign an approved visible TEST prop/indicator to those owned templates before user-facing evaluation. Reach volumes need no runtime solid geometry.

## Authoring actual quests later

Create a new owned QuestDefinition with a permanent QuestId, localized title/description, Story/Side category and stable ObjectiveIds. Keep IDs unchanged once saves exist. Author prerequisites as an acyclic graph; use the native definition/catalog validators. Decide optional/failure policy explicitly.

Use QuestWorldActor OfferQuest for acceptance, InteractObjective for a successful use, and DeliverItems for the existing atomic delivery path. Use QuestReachArea for 3D arrival. Match each objective descriptor to the placed NavigationTarget's TargetId, MapId and LayerId; cloning a provider must not silently duplicate a target ID. Set checkpoint IDs on placed instances, not a shared BP default.

Set inventory objective ItemType to a verified registered tag and define count semantics. Current PossessItem latches completion once the threshold is met; delivery revalidates availability and consumes the full outstanding amount. Equipped/contained/reserved items are unavailable for delivery. Do not trigger progress from UI selection, map pins, presses, montage starts or callback frequency.

Do not rename TEST records into production story assets after saving progress. Author separate approved quest IDs/content. Native player/world state owns progress; ProHUD is presentation. Manual map waypoints remain independent from objective state.

This subtask wrote files only. No editor recipe stage, build, PIE or test was executed, and no level actor was placed by the authoring agent.
