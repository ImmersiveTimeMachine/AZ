# Gamepad support: current audit and next implementation task

Status: design and read-only audit, 2026-09-21. Priority: immediately after completing the current Field Notes interface work. This document does not implement bindings, change assets/config/source, or authorize PIE, input injection, automated tests, or a build.

## Established diagnosis and scope

The user has already verified that Unreal receives the controller: pressing a real controller button switches the UI glyphs to gamepad. Treat detection as established. Do not ask the user to prove connectivity, transport, or which individual action fails again. This task concerns the missing gameplay mappings and complete UI operation coverage.

The configured current hero uses `AZ_IMC_RT_PawnInputs`. A live, read-only asset inspection on September 21 found **38 mappings for 28 distinct actions, with zero gamepad keys**. Movement, look, interaction, attacks, and other current gameplay commands therefore have no gamepad mapping in that context. The interaction prompt obtains the real Interact action from the owning PC's InputConfig and resolves a key for the current device. Interact is mapped only to E, so its gamepad **Unbound** hint is consistent with the mapping data; replacing that hint with a fabricated button would hide the actual problem.

The new Field Notes UI contexts already contain gamepad mappings. That does not establish full inventory operation coverage or runtime focus correctness. Separate three questions: an action has an appropriate mapping; its owning context/binding is active; the focused widget and capture rules route it to the intended operation.

Evidence is the current installed source following the successful 31-source native UI build, current config, and live asset/default queries. PIE was not running during the query, so a possessed runtime pawn, effective per-user remapping profile, and active UI context stack were not sampled. Configured defaults and native ownership were verified; runtime behavior beyond the user's observation remains a manual acceptance step. Historical April input-memory counts and old staged proposals are not the baseline.

## Current configured pawn and context ownership

The loaded `L_001` WorldSettings selects `/Game/AZ/Blueprints/Game/MHC/BP_AZ_GameMode_MHC`, whose defaults are:

- Pawn: `/Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC`.
- Controller: `/Game/AZ/Blueprints/Player/BP_AZ_PlayerController`.
- Pawn context: `/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs`.
- Pawn movement/look actions: `AZ_IA_RT_Move` and `AZ_IA_RT_Look` from that RT folder.

The sampled hero and PC Blueprint EventGraphs contain disconnected lifecycle events, without additional input event consumers. The native PC adds the pawn context on possession/acknowledged possession and removes it on unpossession. The placed Chalkie NPCs are not configured as the automatically possessed player.

| Context | Owner / priority | Mapping rows / actions / gamepad rows | Meaning |
| --- | --- | ---: | --- |
| `AlwaysAllowed/AZ_IMC_AlwaysAllowed` | Local PC / 0 | 14 / 5 / 5 | Inventory and Quick toggles; also legacy actions requiring a consumer audit |
| `InputActions/RT/AZ_IMC_RT_PawnInputs` | Possessed current hero / 2 | 38 / 28 / **0** | Actual current gameplay gap |
| `FieldNotes/IMC_FN_PauseMenu` | Local PC / 3 | 2 / 1 / 1 | Current Pause action |
| `FieldNotes/IMC_FN_MenuRoutes` | Activated menu routes / 100 | 3 / 1 / 2 | Back; standard UI navigation/accept also uses CommonUI/Slate |
| `InventoryMenu/IMC_AZ_InventoryMenu` | Activated inventory / 110 | 17 / 8 / 7 | Inventory operations and tabs |
| `FieldNotes/IMC_FN_Map` | Activated Map page / 120 | 6 / 6 / 6 | Map commands, with parent commands reserved |
| `FieldNotes/IMC_FN_QuickSelect` | Activated Quick Select / 120 | 9 / 9 / 9 | Quick focus/activate/assignment/candidate/cancel |
| First Time Pickup Notification context | Activation in current flow not established | 5 / 1 / 1 | A separate dismissal action exists |
| Inventory Inspection Menu context | Activation in current flow not established | 13 / 5 / 5 | Separate inspect operations exist |
| Legacy `IMC__AZInventoryCharacter` | **Not** this hero's configured context | 37 / 20 / 13 | Do not activate/copy wholesale as a fix |

All listed assets are under `/Game/AZ/Blueprints/Input/`. Sampled contexts have no asset profile overrides. `bEnableUserSettings=True` means the future runtime verification must also inspect the effective local player's mappings, rather than assume asset defaults exclude every possible saved override.

Native ownership references: [PC setup and shared contexts](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:111), [pawn context lifecycle](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:513), [pawn action binding](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:792). Live query receipt: [configured classes](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:18251), [UI references/priorities](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:18279).

## Gameplay action coverage

Every action in the following table is currently missing a gamepad mapping in the configured RT context. The physical gamepad layout is intentionally left undecided here. Existing current functionality should be made reachable through its existing actions and consumers.

| Action / operation | Current keyboard/mouse mapping | Verified current route and required preservation |
| --- | --- | --- |
| Move | WASD and arrow keys | Hero Axis2D handler; preserve partial stick magnitude and camera-relative movement |
| Look | Mouse X/Y | Hero Axis2D handler; requires a pad-specific rate/dead-zone policy |
| Jump | Space | InputConfig Jump tag, granted Jump ability; existing Pressed/Released action triggers |
| Run | Left Shift | InputConfig Run tag and granted Run ability; existing Pressed/Released triggers |
| Crouch | Left Ctrl | InputConfig Crouch tag and granted Crouch ability; existing Pressed/Released triggers |
| Sprint | Q | InputConfig Sprint tag and granted Sprint ability |
| Primary attack | Left mouse | InputConfig PrimaryAttack; equipment/GAS and readied-throw routing |
| Secondary attack | Right mouse | InputConfig SecondaryAttack; equipment/GAS and throw cancellation; existing Pressed/Released triggers |
| Aim | Right mouse | InputConfig Aim; action is non-consuming; shares key with SecondaryAttack |
| Reload | R | InputConfig Reload; preserve fresh-press/reload behavior |
| Heavy strike | R | `AZ_IA_RT_HeavyStrike` is InputConfig `Input.Action.MeleeAttack`; shares key with Reload |
| Interact / pickup / world objective / grab escape | E | InputConfig Interact; PC immediate world interactions, item/GAS interaction, and grabbed-state ability reuse the tag |
| Change fire mode | X | Explicit PC binding to `AZ_IA_RT_ChangeFireMode` |
| Weapon slot 0 and 1 | 0 and 1 | Explicit PC slot bindings to RT actions; Pressed triggers |
| Quick slots 2 through 7 | 2 through 7 | Explicit PC bindings; these six action assets are in AlwaysAllowed but their keys are in the pawn context |

The InputConfig has ten valid action/tag entries and two null entries. Granted movement abilities are Jump, Run, Crouch, and Sprint. Mantle/Hurdle/Climb are granted without separate input tags. The throw ability is also granted without a direct input tag; existing ready/throw/cancel behavior reuses the attack inputs. Do not invent extra traversal/throw bindings solely because those ability classes exist.

Seven additional RT actions are mapped to keyboard/mouse but have **no verified current consumer** in the inspected native bindings, InputConfig, or sampled hero/PC EventGraphs:

| Mapped action | Current key(s) | Required decision before mapping |
| --- | --- | --- |
| TogglePerspective | P | Locate a current consumer or classify explicitly as legacy/unimplemented |
| HoldBreath | Left Shift | Same, and resolve overlap with Run |
| ChangeShoulder | Left Alt | Same |
| WeaponAccessory | L | Same |
| Lethal | G / middle mouse | Same; do not bypass the existing ready/throw flow |
| Melee | thumb mouse / V | Distinct from the currently tagged HeavyStrike action; verify intended role |
| ToggleWeapon | mouse wheel axis, on a Boolean action | Verify consumer and intended cycle semantics |

RT action assets named Walk, Strafe, and SecondaryWeapon also exist but are not mapped in this context. Asset existence is not evidence of a supported operation. No deletion or new behavior is proposed for these unresolved actions.

Equipment-granted abilities must be covered using actual equipped items, including firearm, melee, throwable, and empty-hand states. A legacy `AAZ_Weapon::AddAbilities` implementation can add a fire context at priority 1, but no current native call site was found; this audit does not establish that context as active. Resolve Blueprint/equipment ownership before adding a second mapping path.

The legacy `ActionMappings`/`AxisMappings` in [DefaultInput.ini](C:/UnrealEngine/Games/AZ/Config/DefaultInput.ini:80) include pad Jump and sticks. They do not feed the current hero's Enhanced Input action bindings. Keeping those config lines is not gameplay coverage.

## Existing UI assignments and conflicts to resolve

These are observed assignments, not a proposed new layout:

| Scope | Current gamepad commands |
| --- | --- |
| Shared | Special Left: Inventory; **left stick click: Quick Select**; Special Right: legacy Pause action; D-pad Up/Down: legacy PushToTalk/Discovery actions |
| Current Pause | Special Right: `IA_FN_PauseMenu` |
| Menu routes | Face Right or Special Right: Back; normal focus/accept remains CommonUI/Slate behavior |
| Inventory | Face Right: Back; LT/RT: previous/next tab; Face Bottom: Select; Face Left: **both Rotate and MoveItem**; Face Top: Split |
| Quick Select | D-pad: focus; Face Bottom: activate; Face Left: assignment; LB/RB: previous/next candidate; Face Right: cancel |
| Map context | LB/RB: zoom out/in; right stick click: recenter; Face Left: personal waypoint; Face Top: clear waypoint; left stick click: track |
| Map native canvas | D-pad pan and Face Bottom select at center; no analog-stick pan handler found |
| First pickup notification | Face Top: dismissal |
| Inspection | Face Top: interact; Face Left: reset; left stick Axis2D: rotate; trigger axes: zoom |

Concrete gaps and layout constraints:

1. **Inventory ContextMenu has only RMB/F, with no gamepad key.** The native inventory binds that action to the item context menu. Full gamepad inventory support must make the actual item actions reachable, not merely select categories.
2. **Rotate and MoveItem share Face Left.** This is a data collision needing an explicit state/consumer policy. It is not proof both operations currently execute: no explicit action consumers for Rotate/Move/Split were found in the inspected project native source and sampled grid/slotted-item EventGraphs. Establish the intended operations and implement missing consumers as needed.
3. Left stick click is already the shared Quick toggle and the Map Track command. Do not silently assign Sprint there. Confirm activated Map routing consumes its command before the global toggle and keeps parent Back/tabs reachable.
4. D-pad Up/Down also have legacy shared actions. They have no verified current PC consumer. Confirm references before removing or reusing them; isolate Quick focus from gameplay and legacy actions.
5. Special Right maps both current Pause and a legacy shared Pause action. Current native PC binds the new action. Confirm no remaining Blueprint consumer before cleanup; prevent open/close from the same press crossing contexts.
6. Inventory tab triggers must not leak to future combat trigger actions. Current capture and CommonUI priority/lifecycle ownership should remain authoritative. A high mapping priority alone is not a substitute for correct consuming and focus behavior.
7. Existing keyboard overlaps R (Reload/HeavyStrike), RMB (Aim/SecondaryAttack), and Shift (Run/HoldBreath) require state-aware interpretation. Copying every overlap onto a controller would not produce an intentional layout.

The inventory grid uses mouse position to update tile targeting, a hover-derived `LastHoveredGridIndex` for context menus, and mouse coordinates for popup placement. Its slots are CommonButtonBase-derived, and `bLinkCursorToGamepadFocus=True` is configured, so lack of a bespoke navigation handler does **not** prove navigation is absent. However, controller focus must resolve a stable item/tile target, and move/place/rotate/split/context actions must work without requiring a real mouse movement. Inspect the actual CommonUI focus/analog-cursor behavior before choosing explicit grid focus state or extending the existing cursor route.

Menu routes use ordinary buttons with CommonUI/Slate virtual accept support. A Back-only menu IMC does not itself prove Accept is missing, and adding a duplicate global accept handler could cause double activation. Preserve the framework's existing accept path.

References: [inventory bindings](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_GameInventoryMenu.cpp:137), [grid mouse targeting](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryGrid.cpp:144), [context-menu target](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryGrid.cpp:769), [actual interaction hint fallback](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.cpp:287).

## Implementation constraints for the next task

**Analog movement and look.** The hero preserves the Move Axis2D component magnitudes. Do not normalize partial stick movement to full speed. Choose a pad mapping-specific dead zone/response policy with an explicit axis/sign convention. The current Look handler directly adds mouse delta multiplied by LookRateYaw/Pitch, whose current defaults are 1; it does not multiply by delta time. Feeding a held stick directly into that same treatment risks frame-rate-dependent camera speed. Use pad-specific rate scaling and delta-time handling (the installed engine provides `UInputModifierScaleByDeltaTime`) or a narrowly separated look-rate path, while preserving current mouse delta and inversion behavior. Validate the result at different frame rates and with small stick deflections during user-run acceptance.

**Action edges and values.** The current GAS bridge binds Started to Pressed, Completed and Canceled to Released, and Triggered to Held. Existing jump/run/crouch/secondary action triggers differ from simple held attack actions. Select digital versus trigger-axis mappings and thresholds deliberately; do not add redundant Started/Held dispatch or bypass existing ability tags. Verify release behavior when focus/context changes, when pausing, during possession changes, and on a real disconnect in the later authorized manual pass.

**Capture and handoff.** Preserve the independent inventory/Quick/menu capture ownership, campaign busy lock, existing mouse-release gate, ability releases, and the installed menu-resume Boolean-action barrier. The current barrier rebuilds transient mappings synchronously while capture remains held, preserves authored context ownership, and suppresses an already-held Boolean action until release. It intentionally leaves analog action values available. Future trigger mappings need review against that actual Boolean-versus-axis distinction; do not replace the barrier with an engine change, synthetic key events, or `FlushPressedKeys` assumptions.

**Ownership.** Add pad mappings to the current owned action/context model or a narrowly justified owned split. Do not activate the legacy character context, duplicate the entire mapping stack, change the vendor inventory pack, clear unrelated contexts, or register permanently active UI contexts. Preserve local-player scope and CommonUI activation/deactivation cleanup. Verify the effective profile/input-mode filtering as well as source assets because both user settings and input-mode filtering are enabled.

References: [Move/Look handling](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:833), [paired ability edges](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Input/AZ_EnhancedInputComponent.h:24), [capture and resume barrier](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:621), [PC ability routing](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:786).

## Requested immediate connect/disconnect hints

Assess this from source/API; no new device probes are needed. The current `UAZ_InputPresentationSubsystem` already subscribes to CommonInput method changes, hardware changes, device pairing/connection changes, and local platform-user changes. It resolves an owned, connected gamepad family and calls `SetGamepadInputType`. That changes the glyph **family**, not the active input **method**; it never calls `SetCurrentInputType(Gamepad)` for a connection event. The first real controller input changing hints is consistent with this implementation.

Implement a best-effort event-driven policy in the existing local-player subsystem:

- Keep device presence, device ownership, last active input method, and selected glyph family as separate state. A manual PlayStation/Xbox glyph-family preference must not lock the input method.
- On a reliable event identifying a newly connected gamepad owned by this local player, resolve its family and perform a one-time switch to gamepad hints. Use supported CommonInput APIs, respecting platform support and input-method filtering/thrashing rules.
- Let the next meaningful keyboard/mouse input switch hints normally. Do not force gamepad mode every tick, disable keyboard/mouse switching, or persist the last-active method as a preference.
- On disconnect/reassignment, affect only the owning local player. If another owned connected pad remains, retain a valid gamepad presentation/selection. If the relevant pad is gone and none remains, fall back to keyboard/mouse where the platform supports it. Do not change another local player's hints.
- For multiple controllers, prefer a valid actually used/owned device, then a deterministic owned-device fallback. Do not use global player zero or assume the default input-device ID is a gamepad.
- Some platforms do not provide a reliable gamepad/family descriptor until the first input event. In that case defer classification to real metadata/input; do not label every newly connected device as a controller. Immediate hints are best effort when the platform supplies sufficient identity, not a reason to ask the user to reconfirm this controller's detection.
- Disconnect during fire/aim/throw/menu use must leave no stuck action. First inspect what the installed engine already releases on real disconnect, then add only an owned missing cancellation path if evidence requires it. Do not inject input to simulate this acceptance case.

Source: [subscriptions and ownership](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_InputPresentationSubsystem.cpp:18), [family application](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_InputPresentationSubsystem.cpp:186), [connection/pairing handlers](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_InputPresentationSubsystem.cpp:277). Installed engine CommonInput distinguishes `SetCurrentInputType` from `SetGamepadInputType`; preserve that separation.

## Proposed implementation sequence and acceptance

1. **Freeze the current Field Notes baseline and complete the action/layout matrix.** Use the current hero/PC/context assets above. Identify verified gameplay consumers, equipment-specific grants, and unresolved legacy operations. Produce one intentional layout covering movement, look, locomotion, interaction/grab escape, all existing combat/throw actions, weapon access, Quick, Inventory, Pause, and all UI operations. Resolve conflicts explicitly. This audit chooses no new physical button assignments.
2. **Enable core gameplay actions.** Author owned gamepad mappings, including analog movement and frame-rate-independent stick look. Preserve KBM behavior and existing GAS press/hold/release routing. Verify Interact now resolves a real pad key and its world hint shows that key. Expose only real settings backed by implemented sensitivity/inversion behavior if such settings are included; no decorative controls.
3. **Complete equipment and context-dependent actions.** Cover the actual firearm, melee, throwable, empty-hand, and grabbed states. Ensure reload versus strike and aim versus secondary/throw-cancel resolve intentionally. Provide practical controller access to weapon/quick slots through the agreed layout and existing Quick workflow, without requiring eight distinct physical slot buttons.
4. **Complete controller UI operation coverage.** Repair the ContextMenu mapping gap and resolve Rotate/MoveItem/Select/Split consumers. Ensure item focus, placement, popup targeting, tabs, Map selection/panning/commands, Quick assignment, Pause/Settings/Load/confirmation routes, and Back all work without mouse assistance. Preserve the current Map/Quick/Pause mappings unless the final layout explicitly changes them; update displayed hints from the same actions.
5. **Add event-driven connection hint behavior.** Extend the existing presentation subsystem using the source policy above. Keep per-player ownership and automatic KBM switching; no new campaign save format or transport layer work.
6. **Build/read back, then user-run acceptance.** Use the normal native build only when implementation is authorized and appropriate; read back saved action types, keys, triggers/modifiers, priorities, and asset references. Do not add automated tests. Ask before starting PIE; by default the user performs editor testing and the agent reads logs. Runtime acceptance must inspect the effective local-player contexts/profile, then cover the matrix below.

| Manual acceptance area | Required outcome |
| --- | --- |
| Move/look | Partial movement, diagonals, zero-input stability, correct axes/inversion, stable look rate across frame rates |
| Locomotion | Jump/run/crouch/sprint and existing traversal behave like their KBM routes |
| Interaction | Pickup, quest/world interaction, grabbed escape, and prompt glyph match the real action |
| Equipment | Fire/hold/release, aim, reload, fire mode, melee, readied throw/cancel, weapon and quick-slot access work in applicable states |
| Inventory | Controller-only focus, category tabs, item actions, split, move/place/rotate/cancel, equipment and Back; no hidden mouse prerequisite |
| Map / Quick | All current commands; correct focus ownership; no global toggle/gameplay leakage from a scoped UI command |
| Menus / restore | Pause/resume/settings/confirm/load paths retain input capture correctly; held inputs do not restart attacks or immediately reopen menus |
| Method changes | Real KBM/gamepad switching remains usable; connection/disconnection hints follow the supported per-player policy |
| Lifecycle | Possession/context changes, closing menus while held, and real disconnect while holding an action leave no stuck state or duplicate binding |

Frontend startup/standalone level work remains a separate design task. It is not required to fix the current gameplay mappings and should not delay this priority.

## Audit receipts

- Current RT mapping data: [live query log](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:17791), tagged `GP_MAPPING_AUDIT` on 2026-09-21 at approximately 18:10–18:14 UTC. Subsequent rows contain Shared, Inventory, Quick, Map, Pause, MenuRoutes, notification, inspection, and legacy context snapshots.
- InputConfig and defaults: the same log's `GP_TAG_AUDIT`, `GP_DEFAULT_AUDIT`, `GP_ABILITY_AUDIT`, and `GP_UI_AUDIT` records; [configured chain](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:18251).
- Installed native build baseline: [successful combined build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/build-receipt.json).
- No implementation or runtime success is claimed by this report. Verified mapping gaps, source constraints, and unverified runtime acceptance items are separated above.
