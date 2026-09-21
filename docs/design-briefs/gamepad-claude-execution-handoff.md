# Claude Opus 5 — implement complete gamepad support for current AZ gameplay

Prepared September 21, 2026. Project: `C:/UnrealEngine/Games/AZ/AZ.uproject`.

**Final user handoff:** Claude owns the next complete controller implementation, including gameplay, consistent menu navigation, inventory operations, shared adaptive world/menu hints, and the stick-controlled map pointer. Root has completed and saved the inventory correction baseline described below and is not making further overlapping implementation changes for this handoff. Controller runtime acceptance remains incomplete; Claude owns investigating and finishing any remaining gaps against that baseline. Do not wait for a new root implementation of the map pointer: it is part of this assignment.

**The map requirement in the user's terms:** a mouse can move around the map, and a marker is placed where that pointer is. Give the gamepad that same ability with a visible pointer moved by its stick. Preserve the mouse route; use the existing placement action at the controller pointer. Do not add a mandatory enter-placement/confirm dialog workflow, and do not substitute map panning, the player's position, or a fixed centre point for a movable pointer. See the required Map subsection in Module 6 for verified code constraints and acceptance.

## Objective and authority

Implement controller access to the current playable hero's movement, camera, locomotion, interaction, combat, equipment, Quick Select, world prompts, and remaining UI operations. The outcome is a playable game with truthful adaptive hints, not merely an input asset containing controller keys. Preserve existing keyboard/mouse behavior, gameplay rules, and the approved Field Notes interface.

This is the next implementation priority; the initial Field Notes correction baseline is already built and saved, with runtime follow-ups handed to you. The user has already established that Unreal recognizes the controller and switches menu glyphs when a real controller button is pressed. **Do not repeat hardware recognition, connection/transport, driver, Steam Input, or controller-model diagnosis. Do not ask the user to demonstrate recognition again.** The current gamepad `Unbound` interaction hint is explained by the missing gameplay binding, not by unrecognized hardware.

The user specifically requires world-prompt parity: pickup's E hint must become the actual mapped gamepad glyph, just as menu hints do. Cover all existing item, interaction, quest, checkpoint/save, grab-escape, and other world-action prompts. The displayed action and the action actually executed must agree.

The physical layout below is an **engineering recommendation, not a user-approved choice**. Present it clearly as the starting proposal when executing this assignment. Routine implementation choices are yours; do not repeatedly stop for permission on reversible work already authorized by the execution request. Honor any later user layout preference, and document a coordinated change if current source makes a proposed assignment unsuitable.

This handoff itself does not implement or test anything. It gives the next implementing agent a concrete assignment. Its underlying [read-only action/context audit](C:/UnrealEngine/Games/AZ/docs/design-briefs/gamepad-support-next-task.md) contains additional receipts; do not restart that audit as the entire task.

## Module 0 — consume the completed UI fixes and establish write ownership

**September21 integration release:** root's inventory corrections are now applied, built and saved. Consume [the five-file build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/build-receipt.json) and [the tab asset completion receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/TabSelectionAssetRun/saved.json). They cover InventoryGrid h/cpp, InventorySwitcherPanel h/cpp, GridSlot.cpp, and the two owned inventory widgets. Root has finished these edits; user runtime verification remains pending. Preserve this baseline, do not apply old proposal files again, and coordinate before overlapping edits if a new acceptance failure reopens root's work.

**Later focus follow-up:** preserve the [latest owned-widget focus defaults](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/AcceptanceFixes/InitialFocus/saved.json), which supersede the widget hashes in the earlier receipt. Menu's DesiredFocusWidget is InventorySwitcherPanel; both the Switcher CDO and embedded instance forward to Button_Equippable. The existing engine NativeGetDesiredFocusTarget fallback and UUserWidget focus-forwarding chain are intentional: absence of a custom native override no longer means no configured desired focus. This also forwards subsequent SetUserFocus(Menu) calls. Auto-restore remains unchanged; default focus and active category selection are separate. User navigation acceptance is pending, so do not claim this was runtime-verified or remove it based on an older audit.

The root task owns **inventory carry behavior and tab/navigation regression corrections**, now released at the boundary above. Those changes take precedence over older snapshots in this handoff. Do not edit their source, widget assets, mapping assets, or authoring scripts concurrently if root is correcting a new acceptance failure. This includes overlapping PlayerController input/focus/capture code and the inventory/Map/Quick widget hierarchy when the fix owner is changing it. The user subsequently confirmed that the personal marker works; only its low contrast remains, deferred to later art refinement. Do not reopen marker logic as a prerequisite to controller support.

Before implementation, read the latest [Field Notes status](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-current-status.md), current source, and the completed fix receipt/diff. Consume that finished baseline. While another agent still owns those files, do only independent read-only preparation. Do not create an alternate implementation in a stale checkout and overwrite its fixes later. If work is handed over mid-session, transfer exact changed-file ownership and re-read those files before editing.

The preceding UI build succeeded with 31 source files represented in the [combined native build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/build-receipt.json). This is historical baseline evidence, not permission to replay `Saved/FieldNotesImplementation/*Proposal` or regenerate the old batch. Current installed files and newer completed regression fixes are authoritative.

Deliverables for this module:

- A bounded changed-file/asset list against the released current baseline, including hashes or a diff sufficient to preserve unrelated work.
- An action-to-operation matrix using the facts below, with the proposed physical layout and any justified deviations.
- A small list of unresolved legacy actions. They must not silently become invented features or expand the task into a gameplay redesign.

Gate: the active UI fix owner has completed or explicitly handed over the overlapping work, and your baseline contains it. Subsequent UI operation work is serialized in Module 6, after core gameplay mapping; do not run an independent inventory rewrite in parallel.

## Current architecture to reuse

The current `L_001` WorldSettings uses `/Game/AZ/Blueprints/Game/MHC/BP_AZ_GameMode_MHC`. Its default pawn is `/Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC`, and its PC is `/Game/AZ/Blueprints/Player/BP_AZ_PlayerController`.

The hero's `DefaultMappingContext` is `/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs`. Live read-only inspection on September 21 found **38 mapping rows, 28 distinct actions, and zero gamepad mappings**. There is no need to rediscover this diagnosis. Read current values immediately before changing them so newer work is retained.

| Context asset | Current owner and priority | Current controller coverage |
| --- | --- | --- |
| `/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed` | Local PC, priority 0 | Inventory/View, Quick/L3; also legacy Pause/PushToTalk/Discovery mappings |
| `/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs` | Current hero through PC possession lifecycle, priority 2 | None at audit time; main implementation target |
| `/Game/AZ/Blueprints/Input/FieldNotes/IMC_FN_PauseMenu` | Local PC, priority 3 | Menu/Start and Escape |
| `/Game/AZ/Blueprints/Input/FieldNotes/IMC_FN_MenuRoutes` | Activated menu routes, priority 100 | Back on Face Right and Menu/Start; ordinary UI accept/navigation also uses CommonUI/Slate |
| `/Game/AZ/Blueprints/Input/InventoryMenu/IMC_AZ_InventoryMenu` | Activated inventory, priority 110 | Back, tabs, select, rotate/move, split; consume newer root fixes before changing |
| `/Game/AZ/Blueprints/Input/FieldNotes/IMC_FN_Map` | Activated Map page, priority 120 | Six scoped commands |
| `/Game/AZ/Blueprints/Input/FieldNotes/IMC_FN_QuickSelect` | Activated Quick Select, priority 120 | Nine scoped commands |

Do not activate the old `IMC__AZInventoryCharacter` to obtain its gamepad mappings. The current pawn does not use it. Legacy `ActionMappings` and `AxisMappings` in `DefaultInput.ini` do not feed the current Enhanced Input handlers. Do not add a parallel legacy input route.

The PC's `/Game/AZ/Blueprints/Input/AZ_InputConfig` has ten valid ability action/tag entries, plus two null entries. The PC binds their Started/Completed/Canceled/Triggered edges to its existing GAS routing. The hero separately binds Move and Look. Quick slots, fire mode, Inventory, Quick, and Pause have explicit PC bindings.

`bEnableUserSettings=True`, input-mode filtering, and CommonUI Enhanced Input support are enabled. Asset mappings and the effective local-player mappings are distinct evidence. Preserve a user's existing remap data; do not delete settings/save files to make a new default appear. If an old saved profile masks a new default, implement or explain the narrow migration/reset behavior rather than silently destroying preferences.

Source entry points, all current rather than staged:

| Concern | Source |
| --- | --- |
| Possession/context ownership, action dispatch, capture, held-key protection | [AZ_PlayerController.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp), [header](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Player/AZ_PlayerController.h) |
| Move, Look, Mover input and gait resolution | [AZ_PawnMoverHeroCharacter.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp), [AZ_PawnMovementMode_Walking.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMovementMode_Walking.cpp) |
| GAS input edge bridge | [AZ_EnhancedInputComponent.h](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Input/AZ_EnhancedInputComponent.h), [AZ_AbilitySystemComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AZ_AbilitySystemComponent.cpp) |
| Crouch toggle | [AZ_GA_Crouch.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Crouch.cpp) |
| Equipment ownership and granted ability input tags | [AZ_Inv_CommonUI_EquipmentComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp) |
| Fire/aim/reload | [AZ_GA_FirearmFire.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmFire.cpp), [AZ_GA_FirearmAim.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmAim.cpp), [AZ_GA_FirearmReload.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmReload.cpp) |
| Readied throw and grab escape | [AZ_GA_Throw.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Throw.cpp), [AZ_GA_PlayerGrabbed.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.cpp) |
| Quick selection and authoritative item/mode requests | [AZ_QuickSelectComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_QuickSelectComponent.cpp), [AZ_QuickBarComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Inventory/AZ_QuickBarComponent.cpp) |
| Real world-action hints | [AZ_Inv_CommonUI_InventoryHudWidget.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.cpp), [AZ_ActionPrompt.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_ActionPrompt.cpp) |
| Input method/family and local UI preferences | [AZ_InputPresentationSubsystem.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_InputPresentationSubsystem.cpp), [AZ_UIPreferencesSaveGame.h](C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_UIPreferencesSaveGame.h) |
| Pause/settings and menu lifetime | [AZ_MenuRoutesComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_MenuRoutesComponent.cpp), [AZ_MenuRouteWidget.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_MenuRouteWidget.cpp) |

Line numbers in old reports may move when root completes fixes. Locate the named function and inspect its current body; do not apply an old whole-file replacement.

## Recommended physical layout — provisional engineering choice

Names below describe physical positions, not the user's hardware model. Xbox/PlayStation labels are illustrative equivalents; the actual glyph comes from CommonInput and the user's family preference.

| Operation | Recommended physical control / Unreal key | Existing action or route | Behavior |
| --- | --- | --- | --- |
| Move | Left stick / `Gamepad_Left2D` | `AZ_IA_RT_Move` | Analog Axis2D; camera-relative, magnitude retained |
| Look | Right stick / `Gamepad_Right2D` | `AZ_IA_RT_Look` with mapping-specific rate treatment, or narrowly separate pad-rate action if justified | Analog yaw/pitch rate, time-scaled exactly once |
| Jump / existing contextual traversal request | Face Bottom: A / Cross / `Gamepad_FaceButton_Bottom` | `AZ_IA_RT_Jump` | One fresh press; existing release behavior retained |
| Crouch/stand | Face Right: B / Circle / `Gamepad_FaceButton_Right` | `AZ_IA_RT_Crouch` | Toggle on each fresh press, not hold-to-crouch |
| Interact / pickup / quest / checkpoint / grab escape | Face Left: X / Square / `Gamepad_FaceButton_Left` | `AZ_IA_RT_Interact` | Same current action/tag and existing per-context press/hold/mash rules |
| Reload | Face Top: Y / Triangle / `Gamepad_FaceButton_Top` | `AZ_IA_RT_Reload` | Fresh press through existing equipment ability |
| Run | LB / L1 / `Gamepad_LeftShoulder` | `AZ_IA_RT_Run` | Hold request; release clears Run according to existing ability |
| Sprint | RB / R1 / `Gamepad_RightShoulder` | `AZ_IA_RT_Sprint` | Hold request, exploration-only; do not make L3 Sprint |
| Primary attack / firearm fire / release readied throwable | RT / R2 / `Gamepad_RightTrigger` | `AZ_IA_RT_PrimaryAttack` | Boolean action; firearm cadence and throw outcome remain ability-owned |
| Aim / existing secondary action / cancel readied throwable | LT / L2 / `Gamepad_LeftTrigger` | `AZ_IA_RT_Aim` and `AZ_IA_RT_SecondaryAttack` | Preserve existing shared physical-button semantics and equipment/throw arbitration |
| Heavy strike | R3 / `Gamepad_RightThumbstick` | `AZ_IA_RT_HeavyStrike` → `Input.Action.MeleeAttack` | Existing tagged action, distinct from the unresolved `AZ_IA_RT_Melee` asset |
| Change fire mode | D-pad Right / `Gamepad_DPad_Right` | `AZ_IA_RT_ChangeFireMode` | Fresh press; only a supported active firearm changes mode |
| Explore/Fight mode | D-pad Left / `Gamepad_DPad_Left` | `AZ_IA_RT_Weapon_0`, through existing slot 0 `ActivateSlot` | Same existing authoritative mode action; also reachable in Quick Select |
| Quick Select open/close | **L3 / `Gamepad_LeftThumbstick`** | `/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSelect` | Preserve current toggle and scoped Quick handling |
| Inventory/Map shell | **View/Back/Select / `Gamepad_Special_Left`** | `/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_Inventory` | Preserve current open/back ownership |
| Pause/menu | **Menu/Start/Options / `Gamepad_Special_Right`** | `/Game/AZ/Blueprints/Input/FieldNotes/IA_FN_PauseMenu` | Preserve menu route and same-press protection |
| Item slots 1–7 | L3 → D-pad focus → Face Bottom activate | Existing Quick Select → `ActivateSlot` → QuickBar request | All seven physical item cells reachable; no need for seven simultaneous direct buttons |
| D-pad Up/Down in gameplay | No new assignment proposed | Existing legacy shared PushToTalk/Discovery mappings require reference classification | Do not silently repurpose an unverified consumer |

The recommendation separates Reload and HeavyStrike despite their existing shared R keyboard mapping. That avoids introducing a new controller ambiguity while retaining both actions. It keeps Run and Sprint independent rather than turning stick magnitude into an automatic Run/Sprint state machine. Default stick movement remains the current Walk gait; LB requests Run, RB requests Sprint when permitted.

For triggers, the table recommends digital gamepad trigger keys feeding the existing Boolean actions. Do not also map trigger axes to those same actions and create duplicate edges. If the installed platform's threshold behavior requires analog trigger keys with an explicit threshold, use one documented mapping path, preserve the Boolean action contract, and audit menu-resume suppression for that actual key/value combination. Do not globally alter existing action triggers to accommodate one device without proving KBM parity.

LT deliberately mirrors the current RMB relationship between Aim and SecondaryAttack. Aim is presently non-consuming. Verify actual equipment grants and keep both required consumers reachable; do not remove SecondaryAttack merely because firearm Aim is native. While a throwable is readied both routes cancel the same throw idempotently. Do not issue a second unrelated weapon action from that press.

Quick Select is eight total cells: mode 0 plus seven manually assigned item cells 1–7. Do not resurrect older nine-cell or auto-assignment configurations. Empty slots stay empty until explicitly assigned. Slot 0 describes the destination Explore/Fight action, not a separate inventory item. Do not implement a new weapon-cycle mechanic to satisfy controller access when the current selector already supplies it.

## Module 1 — author the missing gameplay mappings

Extend the current owned RT context while retaining its keyboard/mouse rows, action identities, existing triggers/modifiers, mapping priority, and registration lifecycle. Prefer mapping existing actions over adding another PC polling/input path. Move and Look are already assigned on the current pawn. InputConfig is already wired; do not bind its gameplay actions again in a HUD widget.

Current supported coverage that must be delivered:

- Move and Look.
- Jump, Run, Crouch, Sprint.
- PrimaryAttack, SecondaryAttack, Aim, Reload, HeavyStrike/`Input.Action.MeleeAttack`.
- Interact, including the existing world/item/quest/checkpoint/grab contexts.
- ChangeFireMode.
- Mode/slot 0 and all item slots 1–7 through direct mode access and the existing Quick Select workflow. The PC's `WeaponSlotActions` contains RT Weapon_0/Weapon_1 and AlwaysAllowed QuickSlot_2 through QuickSlot_7; the latter keys are currently mapped in the RT context despite the actions' folder name.
- Inventory, Quick Select, and Pause through their already authored shared/scoped contexts.

Use the actual existing action asset names under `/Game/AZ/Blueprints/Input/InputActions/RT/`; QuickSlot_2…7 are under `/Game/AZ/Blueprints/Input/AlwaysAllowed/`. New native helpers or an extra pad Look-rate action are allowed only when they solve a concrete gap cleanly and preserve current ownership.

Not-currently-proven operations must be classified without feature invention:

| Existing asset / key | Current evidence and required disposition |
| --- | --- |
| TogglePerspective / P | No verified current consumer in native bindings, InputConfig, or inspected hero/PC EventGraphs |
| HoldBreath / Left Shift | Same; shares Shift with real Run |
| ChangeShoulder / Left Alt | Same |
| WeaponAccessory / L | Same |
| Lethal / G or middle mouse | Same; existing throwable functionality uses readiness and attack routes instead |
| Melee / thumb mouse or V | Same; do not confuse it with the tagged HeavyStrike action |
| ToggleWeapon / mouse wheel on Boolean action | Same; do not assume a supported cycle operation |
| Walk, Strafe, SecondaryWeapon action assets | Present as assets but absent from the configured RT mapping context |

For these, perform only a focused reference/consumer check needed to close the matrix. If a real reachable consumer exists, integrate it into the final layout with explicit conflict handling. Otherwise retain the legacy asset and record it as dormant/unimplemented. Do not implement camera modes, breath mechanics, accessories, or a new throw system just to make every old filename have a button.

Deliver: saved owned mapping changes, an exact before/after mapping table, and current action value types/trigger/modifier readback. Gate: each supported operation has one intended execution route and controller access; no unsupported action is falsely listed as working; existing keyboard rows and unrelated contexts are preserved.

## Module 2 — make analog movement and camera correct

### Movement magnitude and gait

`OnMoveTriggered` currently reads Axis2D X=right/left, Y=forward/back, clamps components, and writes cached pawn intent. `ProduceInput_Implementation` rotates this by camera yaw, applies the existing movement-capability constraints, and sends deterministic Mover input. Preserve that path, obstacle gating, grabbed/body-busy behavior, crouch clearance, and camera-relative directions.

Author a controller-specific radial dead zone and response with inspectable defaults. A starting recommendation is inner dead zone 0.15, outer threshold 0.98, linear magnitude response; these are tunable engineering starting values, not measured controller calibration. Check existing mapping/action/global modifiers before adding another dead zone. A square per-axis dead zone stacked with a radial one can distort diagonals and remove useful low-range input.

Do not normalize every nonzero stick value to unit length. Preserve magnitude after dead-zone remapping, and cap the final planar magnitude appropriately so diagonals do not exceed the intended envelope. Verify that no downstream helper accidentally destroys partial input. Returning the stick to neutral and receiving Completed/Canceled or losing possession must clear cached movement; keep existing menu capture/reset behavior. A dead-zone change must not leave the last nonzero intent latched.

Run and Sprint are real separate GAS requests. The current hero maps `Movement.Sprinting` to Sprint only when not strafing/aiming; otherwise `Movement.Running` maps to Run unless a throwable is readied; otherwise gait is Walk. Crouch remains its own stance request. The current accepted Sprint behavior is hold/release and exploration-only, described in [mover-q-sprint.md](C:/UnrealEngine/Games/AZ/docs/design-briefs/mover-q-sprint.md). The authored Run/Sprint abilities are `/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_AZ_GA_Run` and `BP_AZ_GA_Sprint`; inspect their current release tasks before altering any action trigger. Reuse their lifecycle and effects. Do not replace them with raw speed writes or new boolean gait state.

When LB and RB are held together, preserve the existing tag/gait precedence: permitted Sprint wins, releasing Sprint can return to still-held Run, releasing both returns to Walk. Fight/aim must not enable Sprint merely because the controller now has a binding. Preserve existing authored duration/effect policy; do not quietly redesign stamina or sprint duration.

### Stick look is a rate, mouse look is a delta

Current `OnLookTriggered` directly adds `LookVector.X * LookRateYaw` and `-LookVector.Y * LookRatePitch`. Current defaults are 1 and there is no delta-time multiplication. Adding a held stick unchanged would make camera speed depend on input evaluation rate.

Implement pad-specific yaw/pitch rates and delta-time scaling exactly once, without time-scaling mouse deltas. Preferred small solution: mapping-local dead zone/response/scalar and the installed `UInputModifierScaleByDeltaTime`, provided the current action pipeline cleanly distinguishes the mappings. If a separate pad Look-rate action and native handler are clearer, keep them narrowly scoped and use the same camera/capture/grab gates. Do not select scaling solely from CommonInput's last-active type: mouse and stick values can arrive in the same frame.

Reasonable initial editable rates are yaw 180 degrees/second and pitch 120 degrees/second at full stick, with a linear response first. Confirm the resulting sign convention through the existing negative-pitch handler; avoid a second inversion. Record the final dead zones, rates, response exponent/curve, axis modifiers, and order. Preserve current mouse sensitivity, camera limits, aim/recoil ownership, and grab camera lock. Do not add aim assist, smoothing/acceleration, camera shoulder switching, or animation speed changes as incidental work.

Expose inspectable real native/config defaults. A sensitivity settings UI is optional only if backed by actual behavior and proper Apply/Cancel persistence; do not expand the task with fake settings or overwrite the existing UI/audio preference record. Keep player animation playback at 1x.

Gate: static pipeline review shows analog magnitude survives and mouse delta is unchanged; later user acceptance confirms partial movement, neutral stability, correct axes, and comparable stick rotation per second across frame rates.

## Module 3 — preserve GAS, equipment, and action-edge semantics

The bridge in `AZ_EnhancedInputComponent.h` dispatches Started→Pressed, Completed/Canceled→Released, Triggered→Held. The PC intentionally handles some abilities on the fresh Started edge and excludes them from repeated Held activation. Do not collapse every action to Triggered or bind a second raw input callback to the same gameplay operation.

Preserve these current behaviors:

| Operation | Contract to retain |
| --- | --- |
| Jump | One fresh press makes one activation/traversal decision. A refused jump must not fire later merely because the button remains held. Existing release support remains paired. |
| Crouch | Native `UAZ_GA_Crouch` waits for the next press, not release. PC snapshots whether the ability was active so the second press ends it without immediately reactivating it. Holding or releasing B must not toggle repeatedly. |
| Run/Sprint | Existing hold/release ability ownership and cancellation; movement-domain tags feed Mover. |
| Firearm primary | The ability owns semi-auto versus automatic cadence. A blocked/failed press must not become a delayed shot; release stops an active continuous fire path. Preserve empty-magazine reload behavior and weapon source/generation validation. |
| Aim | Existing equipment-bound aim lifecycle, including release, menu cancellation, switching equipment, death/grab, and recoil cleanup. |
| Reload | Existing fresh-press activation and ammo transaction. No new repeated-held reload loop or synthetic completion. |
| HeavyStrike | Dispatch the currently tagged `Input.Action.MeleeAttack`; only equipment/abilities actually granting that operation may execute it. Do not force a melee ability onto every firearm. |
| Interact | Retain item/GAS behavior, PC immediate quest/world interaction checks, authority/duplicate-request protection, and grab escape's use of the same input tag. Do not make a repeated Triggered loop repeatedly commit a checkpoint or objective. |
| Slot/mode actions | Use `QuickSelect->ActivateSlot` and QuickBar's existing request IDs/revisions/results. Keep empty-slot refusal, assignment versus activation, and accepted/deferred outcomes. |

**Throwable behavior has changed since older notes.** Current `RouteThrowInput` is authoritative: readiness comes from selecting/readying the real item; PrimaryAttack **press** requests throw; SecondaryAttack or Aim **press** requests cancel. Their releases are consumed while the throw context owns them. Run/Sprint cancels a readied throw before the movement ability's blocked-tag evaluation, then permits that movement request. Other voluntary inputs are swallowed while throw aim owns the body, never buffered to fire after it ends. Some old comments still describe release-to-throw; follow the current executable path, not those comments or the old ThrowCompletionAudit.

Verify equipment-specific grants against the current active item/profile, not a generic weapon class assumption. A legacy `AAZ_Weapon::AddAbilities` can add a context at priority 1, but the previous audit did not establish that path as active. The current inventory equipment component grants abilities using their actual `InputTag` and owns selection generations. Do not activate a second weapon input context to paper over missing pawn mappings.

Module deliverables: mappings and any narrow edge/lifecycle repairs necessary to reach existing behavior, with a per-operation dispatch trace. Gate: one physical edge reaches one intended gameplay route, blocked actions stay blocked, and every accepted held action has a matching release/cancellation path. Existing item quantities, save semantics, ability tags, traversal rules, and animation playback stay intact.

## Module 4 — world prompts must show the button that really works

This module is required, including world surfaces outside inventory menus. Start from the installed native prompt system rather than replacing text strings:

- Owning HUD: `/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD`.
- Passive prompt templates: `/Game/AZ/Blueprints/Menu/Common/WBP_FN_ActionPrompt_Overlay` and `_Paper`. They have fixed palettes; the Context name alone does not recolor a template.
- `UAZ_Inv_CommonUI_InventoryHudWidget::RefreshInteractionPrompt` resolves the owning `AAZ_PlayerController::InputConfig` action for `Input.Action.Interact`, then uses CommonUI's current-device/effective-key lookup.
- `UAZ_ActionPrompt::ConfigureAction` is display-only. `ConfigureBinding` displays an already-owned UI binding. Neither is permission to register a second gameplay interaction command.
- `ControlMappingsRebuiltDelegate`, input-method changes, and action-router updates already refresh presentation with listener cleanup. Extend missing cases through this mechanism rather than polling a new widget every Tick.

Complete the actual prompt inventory: ordinary pickup items, weapons, magazines with live rounds/capacity, quest offers/deliveries/interactions, checkpoints/save objects, grab/mash/escape indicators where present, and any other existing world action prompt that embeds a physical key. For each, identify its owning player, actual action/tag, caption provider, visible widget, and refresh/lifetime owner. A world actor's class name or an old E-bearing string is not an input binding.

Use current key-free captions (`GetPickupCaption`, `GetInteractionCaption`, and current checkpoint/actor presentation) while preserving legacy public Blueprint APIs and compatibility strings. Do not parse localized E strings into captions, replace every E character with X, or hardcode Xbox X/PlayStation Square. Preserve item names, stack information, magazine live R/C data, actual interaction availability, and outer HUD visibility ownership. Root's new caption/carry fixes must be retained.

Existing Windows controller data is owned:

- `/Game/AZ/Blueprints/Input/Common/BP_AZ_InventoryControllerData_KBM`
- `/Game/AZ/Blueprints/Input/Common/BP_AZ_InventoryControllerData_Xbox`
- `/Game/AZ/Blueprints/Input/Common/BP_AZ_InventoryControllerData_PS4`
- `/Game/AZ/Blueprints/Input/Common/BP_AZ_InventoryControllerData_PS5`

Use the existing Auto/XboxSeries/PlayStation4/PlayStation5 preference and CommonInput family resolution. A valid mapping with no available art may truthfully show the key's display name; it must not falsely become Unbound. A genuinely unmapped action must retain honest Unbound/unavailable behavior. If a menu temporarily removes/consumes a gameplay mapping, presentation should follow the actual active context and HUD visibility rules rather than advertise an unusable world action behind the menu.

Required update cases: KBM→pad→KBM while the same target remains visible, glyph-family change, mappings/profile rebuild, UI context activation/deactivation, possession/HUD recreation, target change, and live item-caption data change. Unbind all delegates when their owner dies. Preserve `ShouldShowInputKeys` behavior.

**End-to-end gate:** with the gameplay Interact context active and its pad mapping valid, pickup/world interaction no longer says Unbound; pressing the displayed physical button invokes the same existing Interact action and performs the available operation exactly once. Repeat with an item, quest/world interaction, checkpoint, and existing grab-escape prompt. Remapping/family change must update the displayed button without changing the underlying operation. No hint-only workaround counts as completion.

## Module 5 — UI capture, pause/resume, and held input

Read the fresh completed PC/UI fixes first. `IsGameplayInputCaptured` combines inventory, Quick, menu-route ownership, and campaign busy state. `IsInventoryInputCaptured` additionally includes the existing mouse-release gates despite its narrower name. Input capture belongs to these owners; do not clear it globally from a new gamepad handler.

The installed menu-route resume helper `SuppressHeldButtonsAfterMenu` is a project-owned transient mapping barrier. It drains pending context rebuilds while capture remains held, temporarily masks held Boolean mappings, rebuilds, removes only that temporary context, then restores gameplay routing. Authored contexts/priorities/registration counts remain installed. Do not replace it with `ClearAllMappings`, synthetic key events, an engine patch, or an unverified `FlushPressedKeys` shortcut. The installed engine's flush path was already investigated and is not an equivalent substitute.

Important limits to account for during implementation:

- The current barrier targets Boolean action mappings. Analog Move/Look must retain continuous stick values when gameplay legitimately resumes. Trigger keys chosen in Module 1 must be examined against the real key-state/threshold semantics; do not assume axis-trigger gating is identical to a digital button.
- Pause/menu routes have this helper. Inventory and Quick have their own capture/release paths and mouse gates; do not assume all new controller-held cases are already covered merely because Pause is covered. After consuming root's fixes, extend the existing shared mechanism narrowly where evidence shows an uncovered handoff.
- Enhanced Input can produce a fresh Started after unpause for an action that did not trigger while paused. PC Pressed clears its suppression tag on a real fresh press. Protect the held key at mapping handoff; do not rely on a suppression tag that the new Started immediately clears.
- Deactivate/remove the owning UI context before restoring gameplay mappings, keep capture during the transition, and do not evaluate input between synchronous barrier passes. A button that accepted/closed a UI must not also attack, interact, toggle mode, or reopen a menu.
- Preserve Completed/Canceled ability release delivery and existing mouse-release behavior. Suppress an unpaired new gameplay press, not necessary cleanup for an already active action.
- When one UI opens another, the receiving owner acquires capture before the old owner releases it. Closing one owner must not clear another's capture, pause state, or campaign busy lock.
- Keep current Pause refusal/Back ordering when Inventory or Quick already owns input. `MenuRoutes` releases only pause it acquired and retains capture through checkpoint loading; do not change campaign transaction semantics.

Deliver a short teardown-path review covering Pause Resume/Back, Inventory close, Quick activate/cancel/close, menu→Inventory/Map, failed/open-aborted routes, possession/end play, and device loss. Gate: no stuck movement/aim/fire, no spontaneous action after a menu closes, no same-press open/close loop, no analog movement permanently ignored until recenter, and no leaked UI context.

## Module 6 — remaining UI controls, serialized after root's fixes

### Required follow-up: a visible map pointer moved by the controller stick

**Latest user clarification is authoritative:** mouse placement already works correctly: move the mouse to a location and place the personal marker there. The controller has no movable equivalent. Add a visible map pointer controlled by the stick, and let the existing Place-marker action commit at that pointer. Preserve working mouse RMB placement and LMB quest selection/drag panning. The user did not request an extra enter-placement/confirm/cancel mode or a second confirmation dialog. Earlier draft-mode recommendations are superseded.

The current `UAZ_QuestMapPage::HandleMapPlaceWaypoint` calls `PlaceWaypointAtCenter()`, which immediately calls `SetWaypoint` at `ViewCenter`. `ClampView` fixes that centre to 0.5 at fit zoom and restricts it to an interior interval even at finite larger zoom. D-pad panning alone therefore cannot choose arbitrary locations. Replace controller center-only targeting with an independent pointer position; do not treat zooming first as a fix or silently substitute the player's position.

Required behavior: while the map canvas owns controller input, the stick moves a clearly visible pointer smoothly over valid map locations. The current Place-marker action (currently X-position) places or replaces the personal marker at that pointer with one intended press. Cursor movement alone never changes the committed waypoint. Existing quest selection should likewise target the indicated point, while preserving the distinction between selecting an objective and placing a personal marker. The pointer may initialize to a sensible visible point on entry; afterward it must move independently of the hero and ViewCenter. A D-pad-only implementation does not meet the explicit stick requirement.

Reach all valid edges and corners at fit zoom and after zoom/pan. Preserve letterboxing/DPI, map rotation/flips, existing map bounds and calibrated world plane. Use a consistent screen/map conversion, dead zone, magnitude-sensitive speed and delta-time treatment. Keep the pointer visible during zoom, resizing and view changes, using bounded edge scrolling or equivalent deliberate behavior as needed. Keep the existing `MakeWaypointFromMap` and `SetWaypoint` as conversion/commit owners. Reject invalid points without changing the previous marker. Closing the map or merely moving the pointer leaves the previous committed marker intact; Back retains its existing navigation meaning. No draft waypoint should be published to compass/world or saved.

Integrate one coherent pointer/input owner with the installed CommonUI system. Inspect its analog/virtual-pointer mode before implementing a separate local cursor, but do not globally enable a second cursor or change inventory navigation to fix this one surface. A bare `NativeOnAnalogValueChanged` override is not sufficient evidence of stick support: the installed analog-cursor preprocessor can consume stick events before widget delivery. Ensure the chosen route actually receives controller input while Map owns it, is released on deactivation/focus handoff, and does not pan the map or move the hero simultaneously. Preserve normal journal/button navigation when those widgets own focus.

`HasSafeMapMappingContext` currently accepts exactly six non-consuming, triggerless/modifierless Boolean UI actions. An Axis2D cursor action added blindly to that IMC disables the whole Map context. If a scoped cursor action is needed, update validation and ownership narrowly while preserving the six existing commands, parent Back/category routing and gameplay capture. CommonAnalogCursor also routes virtual Accept first and may synthesize LMB when unhandled. Keep one semantic quest-selection/place route per action; do not add duplicate raw A and simulated-click handlers. No change to hardware recognition is part of this work.

When switching mouse/controller, use a deliberate handoff of the visible pointer/target so placement cannot use a stale last-mouse point or jump to an unrelated centre point. Keep glyphs tied to actual action mappings and update the Map instructions to explain stick movement and the real placement button. Do not change correct mouse behavior to compensate for missing controller movement.

Acceptance: without touching a mouse, move the map pointer using the stick at fit zoom to a point well away from the hero and place one marker there. Repeat at all edges/corners, after zoom/pan/resize, and across mouse/controller switches. Verify map/compass/world refer to the same committed point. Moving the pointer, navigating the journal, or leaving Map without pressing Place must preserve the previous marker. Confirm no duplicate placement or gameplay input leakage. This cursor support is not implemented by root's inventory fixes; marker contrast/art remains a separate deferred task.

This phase completes controller-only usability. It must consume root's finished carry/tab work and preserve it. Do not solve a focus bug by replacing the inventory system or altering quest/navigation logic.

Retain these current scoped assignments unless the final coordinated layout requires a documented change:

| Surface | Current pad behavior to preserve |
| --- | --- |
| Inventory | Face Right Back; LT/RT previous/next category; Face Bottom Select; Face Top Split; Face Left Rotate/MoveItem data overlap still requires verified consumers and a coherent carry-state policy |
| Quick Select | D-pad four-way focus; Face Bottom activate; Face Left assignment; LB/RB previous/next candidate; Face Right cancel |
| Map | LB/RB zoom; R3 recenter; Face Left place personal marker; Face Top clear; L3 track; D-pad pan; Face Bottom select at center |
| Menu routes | Face Right/Menu Back; normal CommonUI/Slate focus and virtual Accept |

The initial audit found no gamepad key for Inventory ContextMenu, and Rotate/MoveItem shared Face Left. Root may already have changed related logic by the time you execute. Re-read the completed fix instead of applying these observations as an old patch.

Recommended remaining UI assignment, only if still needed: use the existing context-menu action with Right Shoulder while Inventory owns input. Root's carry correction concerns visibility and actual target identity; it does not by itself prove that Rotate, MoveItem, or Split have working controller consumers. Implement only the remaining missing operations after inspecting the released fix. A reasonable policy is Face Bottom pickup/place, Face Left rotate only while carrying, Face Top split only when a splittable stack is selected, and Face Right cancel before Back. Do not advertise rotation if item orientation is unsupported by the canonical placement model. Right Shoulder's gameplay Sprint is suppressed by UI ownership; Map/Quick use their own scoped Right Shoulder command. Do not let an inventory context-menu binding consume a Map zoom or Quick candidate command. If the completed fix supplies another coherent layout, preserve it and update the final matrix.

Make focused items and target tiles explicit enough to complete select, context menu, action activation, move/carry/place, rotate, split, cancel, equipment transfer, and exit without a real mouse movement. CommonButtonBase and `bLinkCursorToGamepadFocus=True` already provide framework behavior. Inspect that behavior before introducing a separate cursor/navigation system. Current grid code uses hover-derived target indices and mouse coordinates; a controller must not operate on a stale last-mouse item or open a popup offscreen.

Use a single source of focus/carry target identity. Prevent double activation from both CommonUI virtual accept and a newly added action handler. Menu routes' Back-only IMC does not prove Accept is missing: CommonUI/Slate already supplies virtual gamepad accept. Do not add a second generic click route.

Specific scope conflicts requiring evidence:

- Shared L3 Quick versus Map L3 Track: Map consumes Track; Quick must not open from that same press.
- D-pad Left gameplay mode action versus Map pan and Quick focus: only the active surface acts.
- D-pad Right fire mode versus Map pan/Quick focus: UI navigation must not change a weapon's mode.
- Inventory LT/RT tabs versus gameplay aim/fire: one tab step per intended press; no shot or aim state leak.
- Face Right crouch versus UI Back: leaving UI must not toggle stance.
- Face Left Interact versus Quick assignment/Map personal marker/Inventory carry: no world interaction behind UI.
- Legacy shared Special Right Pause versus current Pause/MenuBack, and legacy D-pad Up/Down actions: classify references and avoid duplicate consumers without deleting unrelated content blindly.

Preserve all Field Notes geometry, fonts, actual data bindings, eight Quick cells, adaptive footer style, Map calibration, personal-versus-quest marker identity, and current quest tracking/progress rules. The approved marker families are Story nested open diamonds, Side circle+dot, and Personal open corners; they are a preservation gate, not an invitation to redesign markers. The personal marker's requested contrast refinement is deferred.

Gate: every exposed current UI operation is reachable with a controller; one input affects only its owning surface; focus returns to a meaningful control; carry/cancel/tab/marker fixes remain intact; keyboard/mouse operation still works.

## Module 7 — connect/disconnect hint switching, best effort per local player

This is a presentation enhancement requested by the user, not a recognition troubleshooting exercise. The current `UAZ_InputPresentationSubsystem` already listens to input-method, hardware identifier, device connection/pairing, and platform-user changes. It currently calls `SetGamepadInputType` to choose a glyph family; that does not itself select the active input method. Implement the missing behavior in this owner rather than adding another device manager.

Required policy:

1. Keep four concepts separate: connected devices, per-player ownership, last active input method, and preferred glyph family. Auto/manual family is not a lock to gamepad mode.
2. On a reliable connection or subsequent descriptor event identifying a newly connected gamepad belonging to this local player, resolve its family and perform a one-time supported `SetCurrentInputType(Gamepad)` transition. Respect installed CommonInput platform support and input-thrashing rules.
3. The next meaningful keyboard/mouse input switches hints normally. Do not force Gamepad every Tick, disable mouse/keyboard input, or persist the last active method as a preference.
4. On disconnection/reassignment, update only the affected local player. If another owned pad remains, retain a valid pad selection; if none remains and keyboard/mouse is supported, provide that fallback when the current presentation belonged to the removed pad.
5. For multiple owned controllers, use a deterministic policy favoring actual last-used valid ownership. Do not use global player 0, choose an unrelated player's device, or classify the default device ID as a gamepad without metadata.
6. First-time unknown devices may not expose a reliable family/type until their first actual input. Defer classification when necessary; do not invent a model or promise immediate switching on platforms that cannot supply the information. Later metadata should complete the one-time transition without requiring a new settings save.
7. Reconnect does not reinstate held fire/aim/interaction automatically. Inspect the installed engine's real disconnect-release behavior and add only a missing owned cancellation path if necessary. Keep action cleanup separate from icon choice.
8. Remove event subscriptions on subsystem teardown and handle controller replacement without duplicating listeners or reloading another user's preferences.

Preserve the existing UI preference record and atomic `SetUIPreferences(Family, MasterVolume, Error)` contract. It stores glyph preference and master volume together and protects unsupported/corrupt future records. Hotplug and active-input switching must not rewrite it. Do not lose the user's volume while changing glyph behavior.

Gate: source/lifecycle review establishes per-player event-driven behavior; later user-run acceptance covers real method switching, disconnect/reconnect, and no stuck action. Lack of immediate classification on an unsupported platform is an explicitly documented limitation, not a reason to repeat connectivity diagnosis.

## Module 8 — build, asset readback, and user-run acceptance

Follow the project's current [C++ build skill](C:/UnrealEngine/Games/AZ/.agents/skills/cpp-build-livecoding/SKILL.md) and [asset authoring skill](C:/UnrealEngine/Games/AZ/.agents/skills/asset-modification-via-python/SKILL.md). Prefer existing purpose-built editor tools and bounded, guarded authoring scripts. Back up only touched owned assets, preserve unrelated mappings/graph connections, and read back after native compile/save. Do not rerun old broad setup recipes over the finished UI.

For native changes, the normal command is:

```powershell
& 'C:/UnrealEngine/Engine/Build/BatchFiles/Build.bat' AZEditor Win64 Development '-Project=C:/UnrealEngine/Games/AZ/AZ.uproject' -WaitMutex -FromMsBuild
```

Use the actual build workflow appropriate to the editor state. New reflected classes/functions/properties require a proper build/restart boundary; do not call a successful Live Coding patch proof that a new reflected symbol is available. Inspect `C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt` for the real result and errors. Only tell the user the build succeeded after `Result: Succeeded`, and identify exactly which changes it includes.

**Do not add automated tests. Do not start PIE, run editor tests, or inject synthetic input without explicit user authorization.** Normally finish code/assets/readback, then give the user a concise manual pass and inspect logs after their run. Do not ask them to test a stale or failed build. No physical hardware recognition test is part of this pass.

Before requesting gameplay acceptance, provide:

- Successful build evidence where source changed; compile/save/readback evidence for touched owned assets.
- Final physical mapping table with actual Unreal keys, action types, trigger/modifier order, and context priorities.
- The supported/dormant action matrix, without claiming old unused features were implemented.
- A concise changed-file/asset manifest, unrelated-state preservation notes, and remaining limitations.
- Evidence that world prompts resolve the same real action/key that gameplay uses.

User-run acceptance matrix:

| Scenario | Observable success / failure to catch |
| --- | --- |
| Basic movement | Partial stick movement, diagonals, neutral stop, camera-relative forward; no fixed full-speed normalization or residual drift |
| Camera | Correct horizontal/vertical signs; smooth controllable small deflections; stable rotation per second at different frame rates; unchanged mouse feel |
| Run/Sprint | LB Run and RB exploration Sprint work independently; releasing Sprint while Run held returns to Run; Fight/aim restrictions remain; releasing both restores Walk |
| Jump and crouch | One press requests one jump/traversal; holding does not retry refused actions; B toggles once per press; crouch release alone does not stand |
| World prompts and action parity | While the same item/quest/checkpoint remains targeted, switch KBM/pad/family; hint shows the actual mapped key and that key invokes the operation once; no stale E or false Unbound |
| Grab escape | Existing escape/mash input and its existing prompt use the same Interact mapping; no new bypass of grabbed-state rules |
| Firearms | Aim/fire/release, semi-auto versus automatic, reload, empty/no magazine, supported fire-mode change, and equipment switching preserve existing behavior |
| Melee and throwable | Existing primary/heavy actions apply only to valid grants; ready item then RT throws/LT cancels; Run/Sprint cancellation ordering remains; no delayed action or unintended quantity spend |
| Quick Select | L3 open/close; every cell 0–7 reachable; item assignment distinct from activation; empty cells truthful; mode destination correct; actual item GUID/revision semantics retained |
| Inventory | Controller-only tabs, item actions/context menu, carry/place/rotate/split/cancel and close; no stale mouse target, double accept, or regression in root's carry/tab fixes |
| Map and markers | Scope-isolated pan/zoom/track/personal marker operations; Back/focus works; Story/Side/Personal visual distinction and independent marker removal remain |
| Menu handoff | Hold a gameplay button before opening UI and through close/resume; no shot/interact/crouch/mode toggle or immediate reopen; necessary releases still clean up; analog stick not permanently blocked |
| Settings/loading | Controller navigation/accept/back works; cancel/display rollback stays real-time while paused; input remains captured through existing load completion/failure rules |
| Context lifecycle | Possession/reopen/HUD recreation/mapping rebuild does not duplicate bindings or preserve stale input/focus; effective profile still supplies the intended pad actions |
| Connection presentation | Real disconnect/reconnect updates the correct player's hints when supported; next KBM input works; no stuck fire/aim, forced gamepad mode, or UI preference save churn |

Do not claim every row passed from a screenshot, designer thumbnail, asset readback, or build success. Record what the user actually exercised, what the logs establish, and what remains untested. Fix failures in the owning layer; avoid a glyph patch that conceals a missing binding or a binding patch that bypasses gameplay rules.

## Completion criteria and exclusions

Complete means the current supported game is playable through the agreed/recommended controller layout, its existing UI operations are reachable, and all displayed world/menu action hints correspond to actual local-player mappings. Core gameplay must not be deferred behind cosmetic polish. Broader UI coverage follows the serialized Module 6 and must be completed before claiming full controller support.

Leave existing campaign formats, checkpoint generations, item/equipment ownership, animation rates, vendor/engine content, map calibration, marker identities, and unrelated UI fixes intact. No frontend level/episode travel implementation, new game mechanics, aim assist, general remapping screen, transport integration, or controller-model detection project is included.

If a product choice genuinely prevents progress, state the exact choice and its consequence while completing independent authorized work. Otherwise make a reasonable narrow implementation choice, record it, and carry the assignment through build/readback and the user-run acceptance handoff. The final response should summarize implemented behavior, final controls, build/readback results, actual manual acceptance evidence, and precise remaining limitations.
