# Field Notes — current integration status

## Inventory corrections built and saved — user verification pending

**Execution ownership handed to Claude:** the user explicitly asks Claude to handle all remaining gamepad work, including movement/gameplay, menus, inventory operation consistency, adaptive world prompts, and the moving map pointer. The [final consolidated execution assignment](C:/UnrealEngine/Games/AZ/docs/design-briefs/gamepad-claude-execution-handoff.md) is the handoff. Root's five source fixes and subsequent tab/focus asset corrections are already built/saved. Runtime acceptance is incomplete and belongs in Claude's follow-up, not a reason to wait for root to implement the map cursor. Root is not making overlapping gamepad/UI implementation changes for this handoff. No message was sent to an external Claude session; the user receives the file to pass along.

**Waypoint clarification — controller pointer missing:** the user explicitly confirms mouse placement works: move the mouse and put a marker at its pointer. The controller has no movable equivalent. The required fix is a visible stick-controlled map pointer and placement at its position through the existing Place action. No extra placement mode or confirmation/cancel flow was requested; earlier draft-mode proposals are superseded. Source confirms current pad PlaceWaypointAtCenter targets ViewCenter, pinned to 0.5 at fit zoom and limited to interior regions after zoom. Preserve working mouse behavior and map calibration. This controller-cursor work is specified in Claude's handoff and has not been implemented. The Boolean-only Map-context validator and CommonUI analog preprocessing must be handled deliberately; a D-pad-only workaround does not meet the stick requirement.

**Follow-up initial-focus correction:** user reported that controller navigation did not start after opening inventory. The log recorded CommonUI's failed desired-focus resolution; read-only inspection confirmed the menu returned no desired target. The existing post-activation SetUserFocus(Menu) also targeted the container. Corrected only owned asset defaults using Unreal's built-in focus forwarding: Menu → embedded InventorySwitcherPanel → Button_Equippable. Applied to Menu CDO, Switcher CDO and embedded panel; auto-restore and input mappings unchanged. Both widgets compiled/saved and all three names read back, with no dirty packages. [Latest focus completion receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/InitialFocus/saved.json) supersedes the earlier widget hashes. No additional native build needed. User now checks that opening inventory immediately allows stick/D-pad navigation, then continues carry/tab acceptance. This is an initial/default focus policy, separate from persistent active-category selection.

**Latest verified state:** normal AZEditor build **Succeeded**, 18 actions, 26.47 seconds, including the five correction source files. The user reopened Unreal (PID52200). Root confirmed Idle, backed up and applied the exact policies on four tab instances, native-compiled both InventorySwitcher and InventoryMenu, saved both, and read back all four policies. Dirty content and maps were empty. [Build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/build-receipt.json) and [asset completion receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/TabSelectionAssetRun/saved.json) record the results. No agent-started PIE or tests. User verification is now the only remaining correction gate; do not rerun completed authoring.

The user has now checked the integrated UI and reported: (1) personal and quest markers look the same on the compass and in the world, (2) a held inventory item disappears visually when moved with the controller, and (3) the active category does not consistently stay highlighted with either mouse or controller. Do not treat the implementation as fully accepted. Preserve the completed integration baseline; correct these bounded issues rather than reauthoring the UI.

**Latest clarification supersedes the initial marker report:** the user confirms the personal marker works and is visible on both the map and compass. It is faint relative to other markers; improve its readability later. No marker identity/shape defect is currently confirmed. The requested runtime-marker inspection is no longer required for this correction and was not performed. Continue with the held-item and tab-selection defects; defer marker art changes as requested.

Later marker-art requirement: reserve red for future enemy indicators. For the personal marker, evaluate brighter green and suitable existing ProHUD symbols (the user mentioned an exclamation mark as an example), with better visibility than the current muted sage. Neither the exact glyph nor a color value is selected yet. This does not authorize automatic enemy detection/marking or a navigation-logic rewrite as part of the current inventory correction.

Confirmed so far: all four live inventory tab templates have selection disabled; the native code tries to select them anyway, and its deselection/late inner-switcher callback also needs reconciliation with the outer page. The carried item is currently hosted only as a mouse software cursor, which CommonUI hides in gamepad mode. Placement also depends on a mouse-cached tile rather than the clicked/focused tile. Beginning a move leaves the item in canonical inventory data; visual disappearance is not evidence of item deletion. Marker category settings and distinct texture references are present in the saved assets, consistent with the user's corrected observation.

The user requests a separate detailed English execution assignment for Claude to implement full gamepad gameplay support. This includes adaptive world interaction prompts using the same action/device presentation system as menus. Root retains ownership of the two current inventory regressions; serialize any later inventory/UI input edits so Claude does not overwrite these fixes.

The completed correction scope is five source files in [build inputs](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/build-inputs.json), plus two saved owned widgets. Switcher and carry proposals have separate pre-apply backups/receipts. The additional GridSlot.cpp correction keeps programmatic cell feedback independent of CommonUI click-to-toggle behavior. Root has finished these source/asset writes. The [Claude execution assignment](C:/UnrealEngine/Games/AZ/docs/design-briefs/gamepad-claude-execution-handoff.md) must consume this completed baseline; runtime acceptance still remains explicit, and any new user-reported failure requires renewed ownership coordination before overlapping edits.

Correction acceptance: use the controller to pick up/move/place a grenade, cancel a second move, and switch between controller and mouse while carrying. Confirm the icon stays visible and placement follows the intended target. Then switch categories by mouse and Q/E/controller triggers, include Map/back and a fast reversal, and confirm exactly the current section stays selected after hover moves away. Check that a refused tab change during carrying preserves the current section. No stale cell highlight should remain after placement.

## Completed integration baseline

The native code and owned UI assets are integrated, compiled and saved. This is readiness for gameplay acceptance, not a claim that every runtime path has been tested.

- Inventory: Field Notes styling, linear vitals, corrected layout, previous/next controls and Q/E four-section navigation, adaptive Back hint.
- Map: retained projection/calibration and markers, adaptive section hints, six scoped controller commands and keyboard/mouse guidance.
- Journal: category/state glyphs and colors, compact objective presentation, expandable selected-task details.
- Quick Select: nine scoped controller commands, adaptive card shortcuts, preserved equipped notch and assignment presentation. The old key label remains bound but is visually and accessibly retired in the owned templates.
- Pause/settings/title/checkpoint routes: wired through the existing player controller, with actual graphics/audio/UI preferences and display rollback. Title-on-startup remains off; current Main Menu is available from Pause.
- World prompts: real interaction-action lookup; explicit captions for six owned class templates and eight L_001 instances. Legacy strings remain intact, actor transforms and unrelated properties were verified unchanged.
- Controller configuration: Windows loads owned keyboard, Xbox, PS4 and PS5 data. The user confirms glyph switching on gamepad input. **Do not repeat hardware recognition checks.**

Native build: AZEditor succeeded after compiler corrections, 242 actions, 74.95 seconds. Thirty initial files plus AZ.Build.cs are recorded in [the build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/build-receipt.json). No source prepare/apply or new build is needed without a new source change. Codex did not start PIE or automated tests.

Asset evidence: [integration checkpoint](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/asset-integration-checkpoint.json), [prompt host receipts](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/Module7/PromptHosts), [caption audit](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/WorldInteractionPromptNativeProposal/AssetRun/caption-audit.json). The audit covers 27 declared owned Blueprint packages and loaded L_001 instances, with no remaining caption gaps. Unloaded maps outside that scope are not claimed checked.

## Persistence and authoring notes

The inventory save RPC timed out while the editor stopped at an engine ensure during thumbnail rendering. The same operation subsequently completed; both widgets were saved and verified. Do not repeat or recover that finished authoring stage. Pending cell36 is closed.

L_001 was successfully saved after all eight instance captions were authored; the editor log, later package timestamp and clean state confirm this. A subsequent redundant SaveMap attempt failed because Windows reported the file was in use. It did not replace the successfully saved file. Do not force-delete/move the map or close unknown file handles to work around this.

Editor previews of Inventory, Map and Quick Select were inspected for static layout. Inventory grids, live values, Quick cards and actual key glyphs require a player context; empty designer placeholders and simultaneously visible device groups are not runtime evidence. Final live visual/input acceptance is still needed.

The final read-only progress-bar diagnostic found no evidence that the owned plain-bar settings cause a runtime defect. The vendor already defaults to shader-disabled bars and empty optional masks. Its SetBlendMask function dereferences the retainer effect material without a validity check; the observed warnings belong to editor thumbnail objects, including the latest inventory preview. Health is intentionally hidden by the native vitals panel until a valid player vitals view arrives, so the empty designer health row is expected. Keep vendor assets and current masks unchanged; verify actual health display/update in the user-run pass. This conclusion does not claim runtime acceptance.

## One combined acceptance pass

User performs Play testing after this integration stage:

1. Inventory: mouse tabs and Q/E in both directions, wrapping through Map; Back returns correctly; dragging/context menus retain their previous behavior; health appears from live player data and updates when reopened.
2. Map/journal: task selection/tracking, category/state indicators, selected-task details, personal marker and return to Inventory.
3. Quick Select: open/close, selection, assignment/cancel, current item/mode and correct shortcut hints.
4. Pause/settings: Resume, Inventory/Map handoff, settings Apply/Cancel and controller appearance. Load only after making a checkpoint, with the existing confirmation.
5. World hints: pickup/quest/checkpoint captions; no duplicate hardcoded E. Unbound on gamepad currently reflects missing gameplay bindings, described below.

## Next priority: complete gamepad action support

User reports the controller is recognized and UI glyphs switch correctly. The read-only audit confirms the active RT pawn context has 38 mappings and **zero Gamepad keys**; Interact is E-only. Legacy DefaultInput.ini axes do not supply the current Enhanced Input handlers. UI mappings exist separately.

Next work is the complete gameplay/UI binding matrix: movement, camera, abilities, interactions, equipment and menu conflicts. The existing Quick toggle on LeftThumbstick must be considered when choosing Sprint and other controls. Prefer immediate hint switching on connection/disconnection when supported, while retaining normal keyboard/mouse use. Hardware recognition is accepted as user-verified and must not be retested. The [action/context audit and implementation sequence](C:/UnrealEngine/Games/AZ/docs/design-briefs/gamepad-support-next-task.md) are complete, covering 28 mapped gameplay actions, unresolved legacy consumers, analog look, UI coverage and binding conflicts. No new gameplay bindings were applied during this UI pass; the final controller layout is not yet selected.

## Startup menu: design only

Use the current Main Menu layout, with The Last of Us / Witcher as visual references. Background artwork comes later. Keep map names L_001, L_002, L_003. Future New Game starts episode1; Continue should restore saved episode and location/progress. **Do not create a frontend level or implement episode travel now.** See [agreed design notes](C:/UnrealEngine/Games/AZ/docs/design-briefs/frontend-menu-design-notes.md).
