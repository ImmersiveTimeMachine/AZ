# CHALK — Field Notes implementation plan

September 20, 2026. **Direction selected and full implementation authorized by Artur.** Required order: inspect the existing systems, prepare this detailed plan, then execute in verifiable modules. Artur also requested examining MenuSystemPro for reusable menu functionality and adaptive keyboard/Xbox/PlayStation prompts. Do not start PIE or add automated tests without explicit authorization.

Visual authority: [Field Notes style contract](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-style-contract.md) and the twelve screens in [the approved GIMP master](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_UnifiedUI_v01/02_FIELD_NOTES/CHALK_AllMenus_NATIVE.xcf>). Implementation status belongs in [the execution ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-implementation-progress.md).

## Result to deliver

One consistent Field Notes interface: paper menus, dark gameplay overlays, Roboto Regular/Bold, distinguishable Story/Side/personal targets and clear focus/state feedback. Apply it to the actual inventory, item actions, Map/journal, quest tracker, compass, HUD and Quick Select. Integrate the title/pause/settings/confirmation/loading presentation through verified AZ-compatible routes. The original source packs remain untouched.

Adaptive button prompts are an explicit workstream. The foundation uses CommonUI/CommonInput and actual actions; it must support keyboard/mouse and gamepad changes without baking key names into art. Xbox and PlayStation glyph families are separate from whether an input device is categorized as a gamepad. Full controller operation must be verified independently of merely displaying the correct icon.

## Decisions already established

- Field Notes is selected; no further three-way style selection is needed.
- Keep the existing three-column inventory and eight-cell Quick Select V5. Slot0 shows the **destination action**; HUD mode shows the **current committed state**.
- Preserve real inventory capacity, item footprints, amounts, equipment, quick bindings, progression and save transactions. The mockup's10×5 grid and displayed numbers are illustrative; native defaults are11×7, and actual configured dimensions must be read live before migration.
- Separate paper and dark-overlay palettes. Never apply dark paper text blindly to gameplay overlays.
- One semantic symbol vocabulary: Story double diamond, Side circle, personal open corners. Optional/status/selection are modifiers, not competing quest categories. Selection must not erase category meaning.
- Reuse existing CommonUI text/button styles and small, owned MenuSystemPro presentation dependencies. An authoring token manifest can populate these assets and native defaults; a new global theme service or widget framework is not justified for this fixed theme.
- Keep AZ GameInstance, PlayerController, GAS/Mover, CommonUI input capture and campaign persistence. No vendor GameMode/GI/save replacement.
- Preserve source data bindings. Placeholder panels retained by prior user request remain visible, but no fabricated mockup values become gameplay truth.

## Verified MenuSystemPro reuse boundary

Read-only evidence: `C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/Audit/`. The registry contains1263packassets. Asset count is not a performance assessment.

| Capability | Verified evidence | Integration rule |
|---|---|---|
| CommonUI menus | `WBP_Menu` directly inherits CommonActivatableWidget; container uses a CommonActivatable stack | Reuse menu/focus patterns and required owned copies, with AZ local ownership and input capture |
| Focus | Last selected → default → first enabled; owning-player focus restoration | Preserve native inventory Map→Inventory→popup/drag cancel→close order |
| Styles | Menu/button/slider/dropdown/scrollbar/save-slot/dialog style types and Silence style instances exist | Duplicate/adapt required instances into AZ; connect them to the approved token manifest |
| Dialogs | Accept/decline/cancel, default-No focus, focus return and deactivation cleanup | Add explicit lifetime/cancellation guards for pending auto-revert work before reuse |
| Settings | Data-driven engine/custom/audio settings, dirty-state handling;0.2s save debounce | Connect only supported settings to their correct AZ/engine owner; settings are not campaign progress |
| Controller art/data | PS4 and PS5 each have32key brushes; actual names are `PS4`/`PS5` | Create AZ-owned controller data and preserve required art dependencies; verify real names, mapping and coverage |
| Hardware mappings | PS4: `FWinDualShock/DualShock4`; PS5: `FWinDualShock/DualSense` | These mappings depend on the active backend; do not promise automatic recognition through an Xbox-compatible wrapper |
| Controller preference | `DA_ControllerType` stores `ControllerType`; maps indices to `XSX`, `PS5`, `Switch`; defaultXSX; no direct apply method | Adapt this existing preference into AZ input/UI preferences, adding automatic mode deliberately rather than treating it as already present |
| Input widgets | `WBP_InputIcon` is a UserWidget with explicit type/family variables; mapping-text widget is a CommonActivatableWidget | Its own graph includes keyboard-specific lookup. Prefer engine CommonActionWidget for actual action-bound adaptive prompts; reuse pack artwork/layout selectively |
| Campaign screens | Vendor SaveManager uses its own GI interface, save actor and custom save object | Reuse presentation only; all actual capture/restore stays through AZ_CampaignSaveCoordinator |

Important source behaviors to review rather than inherit blindly:

1. Vendor menu desired input mode is `All`, with move/look ignore flags false. Being a CommonActivatableWidget does not by itself guarantee AZ gameplay suppression.
2. A source activation path forces Gamepad after a0.2s delay, described as a UE5.6workaround. Do not retain this without proving it necessary on the current engine; it could reverse a deliberate mouse switch.
3. Container icon refresh enumerates all matching widgets globally. Scope updates to the owning player and active/instantiated prompt widgets.
4. PreLoadSoftReferences performs16blocking class loads, including example menus. Build a bounded dependency manifest and preload only what the actual routes need.
5. Vendor GI runs a0.01s playtime timer. Do not import it incidentally as a dependency of a visual change.
6. AZ LoadCampaign rejects paused worlds, nonlocal/multiplayer context, ungrounded movement and unsafe active transactions. A pause-menu load must release only its own pause/capture state before requesting the existing load transaction; acceptance is not completion.

## Work ownership and change control

Root owns the live editor, the asset manifest, compile/save ordering and integration. Helpers may own explicitly separated source/art/script files. No competing live editor writers. Preserve unrelated current user/Claude edits, especially character/camera/throwable/equipment work.

Every module records: exact touched files/packages, before values, backup, changes, compile result, saved readback, visual/runtime result and rollback. Original source-pack packages are dependencies, never mutation targets. Do not rerun obsolete HUD/Quick Select creation scripts over the working widgets.

## Module 0 — baseline and route inventory

**Purpose:** prove the current owners and protect working behavior before writes.

1. Record editor/PIE state, loaded DLL/reflected types, dirty packages, relevant source diff and current GameInstance/GameMode/viewport/input settings.
2. Capture active widget trees and defaults for inventory/vitals/skills/grids/item leaves/popups, Quick Select, GameHUD/compass/quest module and Map page.
3. Read actual inventory grid dimensions, slot definitions, footprints and source input actions. Do not substitute mockup numbers.
4. Inventory title/pause/settings/load/dialog routes: distinguish active AZ screens, pack templates and absent route bindings. Trace callback/owner dependencies through live graphs, including inherited behavior.
5. Snapshot the source-pack dependency closure and hashes before copies; back up only exact owned edit targets. If an asset has unrelated unsaved edits, preserve that state rather than saving/resetting it broadly.

**Gate:** auditable ownership/route/asset matrix; no ambiguous active class, hidden vendor save owner or unknown input reset. Current source review is substantially complete; live asset baseline remains required before authoring.

**Rollback:** none required for read-only audit; keep receipts.

## Module 1 — shared tokens and style resources

1. Save the approved paper and dark-overlay tokens in one versioned authoring manifest, including alpha, sRGB→linear conversion, typography, role/state semantics and authored dimensions.
2. Create AZ-owned CommonButtonStyle/CommonTextStyle instances for paper and overlay contexts. Reuse existing native style APIs; avoid one monolithic global style override that affects every screen indiscriminately.
3. Create the minimal owned MenuSystemPro style instances for the controls selected by the route audit. Preserve parent schemas and only the necessary hierarchy/dependency closure.
4. Define a consistent style-application recipe for UMG templates and native-created rows. Resolve styles/fonts at construction/refresh; do not load assets per Paint/Tick.
5. Define the semantic action-prompt interface now: real action/binding, localized label, owning local player, optional hold/progress presentation. Key art is not embedded in labels or backgrounds.

**Gate:** one token edit reaches all corresponding owned style targets; paper/overlay styles are demonstrably distinct; fonts/linear tints read back correctly.

**Rollback:** remove/reassign the new owned style instances; restore captured existing references.

## Module 2 — typography, glyphs and calibrated map art

1. Reuse the existing Roboto Regular file/asset with its actual typeface name. Import Roboto Bold from the existing source TTF or create an explicitly named Regular/Bold composite. Verify fallback and Cyrillic/Latin glyph coverage.
2. Export native GIMP masters for Story double diamond, Side circle, personal corners and required state masks. Reuse current weapon/magazine/Explore/Fight/heart/grenade art where it matches; do not recreate established item art unnecessarily.
3. Re-author the compass strip's baked letter artwork in Roboto while preserving its texture dimensions, phase and360-degree UV contract.
4. Derive a paper-toned version from the actual calibrated L_001 image. Keep bounds, orientation, resolution and world-to-image transform unchanged; never replace it with the illustrative streets drawn in the mockup.
5. Validate small-size silhouettes, disabled/focused states and background contrast. The dark HUD critical-health red needs an actual readability check; the light reference sheet alone is not proof.

**Gate:** native sources and exported/imported artifacts agree; dimensions, alpha, brush use, font names and map calibration are read back. Original art remains available.

**Rollback:** restore prior owned asset references; new art stays isolated under the Field Notes folder.

## Module 3 — narrow native presentation changes and one build boundary

1. `AZ_QuestJournalEntry`: expose authored row fills, state colors and button styles so `ApplyView` cannot restore the old dark palette after refresh. Preserve focus/click/selection APIs.
2. `AZ_QuestMapPage`: expose section-heading font/color instead of constructing hardcoded FCoreStyle/green headings. Preserve selection and load callbacks.
3. `AZ_QuestMapComponent`: cache the already-existing quest category in the marker view when the definition is resolved. Do not create new persistent quest truth or perform category queries in Paint.
4. `AZ_MapCanvasWidget`: render Story/Side/personal shapes and category colors from cached metadata; add a separate selection/tracking treatment. Preserve projection, hit selection, pan/zoom, search regions and waypoint APIs.
5. Use the existing Quick Select `OnEntryViewChanged` presentation event for the equipped notch; do not add reflected source fields unless the actual view contract proves a field is missing.
6. Group necessary reflected changes for a normal UHT build/editor restart. Read the real compiler log and confirm loaded symbols before asset assignment. Body-only fixes can follow the normal Live Coding policy; new reflected members cannot be accepted from a patch alone.

**Gate:** normal build succeeds, editor loads the new fields, old gameplay/public operation contracts remain valid. No automated tests added; no request for user Play before a green build.

**Rollback:** restore narrow source changes and rebuild, then restore captured defaults. Do not revert unrelated source edits.

## Module 4 — pilot: Map, journal and one modal

1. Apply paper surfaces, Roboto and shared button/text styles to the existing Map page and native-generated rows.
2. Restyle the existing confirmation/load controls without replacing their accepted→completed transaction semantics.
3. Apply category markers to Map and compare them against the source sheet before propagating the same styles to other surfaces.
4. Verify Back, focus restoration, load cancellation and page reconstruction; selection remains separate from tracking/progress.

**Gate:** one complete menu and modal match Field Notes, retain dynamic data and remain correct after reopening. This small pilot validates token/Slate-size conversion and input ownership before broad migration.

**Rollback:** restore only Map/row/modal style defaults; shared campaign/native model is unaffected.

## Module 5 — inventory, item actions and character panels

1. Apply the paper theme to the existing three-column inventory tree, vitals/skills panels, currency, category controls and description leaves.
2. Preserve actual grid capacity and item footprints; use layout sizing/scrolling to fit the approved panel proportions. Verify empty and populated grids, stacks, magazines and nested items.
3. Restyle hover/selection/drag affordances and the real item popup. Equip, magazine loading, consume, split and drop availability still comes from the current item/context; the M16 mockup is not a new action policy.
4. Retain current data bindings and all placeholder panels explicitly kept by Artur. Unavailable real data uses the existing fallback, never the mockup sample value.
5. Convert existing action hints to the common prompt interface where bindings already exist; preserve the native cancellation/back hierarchy.

**Gate:** every category and item action remains usable; counts/equipment/capacity/save placements do not change; text and focus remain readable at the current DPI and long labels.

**Rollback:** per-widget/property backup, not wholesale inventory recreation.

## Module 6 — gameplay HUD, Quick Select, compass and quest tracker

1. Apply the **dark overlay** palette to current HUD/quick/compass/mission widgets and their actual item-composite leaves. Keep normal health/ammo neutral, reserve danger feedback for its state.
2. Preserve the eight-cell V5 layout,0–7 identities, gaps, arrows and existing controls. Add the steady equipped notch through the existing view-update event. Keep assignment animation limited to its current border-only pulse.
3. Preserve current-mode versus destination-action semantics for Explore/Fight. Read committed equipment data; do not infer state from an in-progress animation or selected UI row.
4. Fill separate quest/personal style structs and resolve Story versus Side from the existing tracked definition. Prefer existing delegate signatures and exact stable marker keys.
5. Keep snapshot-driven quest rows, native counters/order and source lifetime hardening. Do not restore the vendor's widget-owned progress/removal logic.
6. Apply font/marker changes to compass/world indicators; preserve compass math, shared navigation updates and independent marker removal.

**Gate:** HUD/Quick remain legible on bright/dark scenes; marker meaning matches Map/journal; focus/equipped/assignment are distinguishable; switching quests never deletes the personal marker or unrelated targets.

**Rollback:** owned presentation assets/defaults only; maintain the accepted runtime bindings.

## Module 7 — adaptive device prompts and controller operation

1. Read current AZ controller data and action mappings live. Current config enables CommonInput Enhanced Input support, keyboard/mouse and gamepad, but registers only KBM and Xbox data.
2. Adapt the verified PS4/PS5 brush sets into owned AZ controller data, with actual family names and appropriate light/dark glyph assets. Ensure essential buttons, axes, triggers and fallback representations exist.
3. Reuse `UCommonActionWidget` with `SetEnhancedInputAction` or an active `SetInputActionBinding`. It already subscribes to input-method and mappings-rebuilt events and cleans up those listeners. No polling detector or global widget scan.
4. Convert pickup/quest/checkpoint prompts, inventory buttons, Map footer, Quick footer/card shortcut indicators and ancillary menus. Keep the action description separate from the key glyph; hide/describe an unbound operation instead of displaying a false key.
5. Adapt the existing controller-family preference: automatic mode plus explicit family override when hardware is reported generically. Persist this in input/UI settings, not a campaign checkpoint. A PlayStation controller presented through XInput cannot be promised automatic PlayStation identification.
6. Audit and complete missing gamepad operations using the existing callbacks. Current Map has D-pad pan/face-button selection but incomplete zoom/recenter/waypoint routes; Quick assignment/cycling are still mouse-specific. Do not call controller support complete just because the glyph changes.
7. Keep this module separable: the prompt component and style bindings are established during this implementation; device-specific verification remains explicitly pending until the user can run the relevant physical-controller check. Do not silently substitute simulated checks for hardware evidence.
8. September21 user addition: place visible Previous/Next controls beside the inventory category row. Existing Q/E actions and keycap clicks must share the logical Equippables → Consumables → Craftables → Map cycle, including reverse and wrap. Use the actual mapped gamepad buttons (currently left/right triggers), with adaptive family glyphs. Preserve category memory, drag/popup guards and Back ordering. During animated transitions use the requested destination consistently; activate/focus only the visible page, and do not reactivate hidden pages after closing the menu. Show section-navigation prompts on Map without duplicate inventory footer bindings.

**Gate:** keyboard↔gamepad switching, real remapping, family preference, reconnection, focus/Back, hold/cancel and unbound actions show the correct hint and perform the same operation. Existing keyboard controls remain intact.

**Rollback:** restore the prior controller-data registration and per-widget prompt bindings; preserve stored preferences and input mappings.

## Module 8 — title, pause, settings, save/load and loading routes

1. Use the baseline route matrix to create only missing AZ-owned screen/host connections. Reuse selected MenuSystemPro controls/layouts/style types and CommonUI focus behavior with explicit owning player.
2. Keep one active modal/menu owner. Extend AZ's existing capture coordination for pause/frontend where needed; source `InputMode=All` is not a replacement for gameplay suppression.
3. Title/Continue/load actions query the real AZ campaign service. Do not expose the mockup's illustrative save list as a second save format. New-game/level-transition behavior must use the verified campaign startup route and explicit user-facing confirmation for replacing progress; do not invent save-slot policy from a picture.
4. Connect graphics options through validated GameUserSettings/cvars, audio through actual sound-class/mix routes, and controls through existing Enhanced Input settings. Omit/disable unsupported options; vendor DLSS/FSR/etc asset names do not prove their backends are available.
5. Preserve Apply/Cancel/Reset behavior. Display-mode changes require a real confirmation/revert path; cancel outstanding revert work when the modal is externally closed/reused.
6. Pause→load hands off only the pause state owned by the menu, then requests the AZ load transaction and waits for completion. It must not resume gameplay input while restoration is busy.
7. Loading presentation listens to actual asynchronous load/restore state, never a mock progress timer. Do not import the vendor GI's playtime ticker, source save actors or global player-zero accessors.

**Gate:** all delivered routes are functional, not merely styled templates. Menu navigation/focus, resume, settings persistence, confirmation and campaign load use the correct owners. Explicitly list any route awaiting a real product decision rather than pretending the mockup supplies it.

**Rollback:** detach the newly added host/routes and restore prior references; preserve AZ gameplay and saved data.

## Module 9 — performance, persistence and final acceptance

1. Measure relevant startup loading, widget creation and Slate work before/after a representative user-run session. Do not advertise optimization from replacing assets alone.
2. Cache style resources and role metadata. No synchronous asset loads, world scans, quest resolution or disk writes inside Paint/Tick. Reuse Quick entries; refresh journal on events. Preserve the existing settings debounce.
3. Hide/gate full-map drawing when closed while leaving the shared compass/navigation component functioning. Avoid indiscriminate RetainerBoxes, expensive blur layers and duplicate active menu stacks.
4. Compile each small Blueprint group via the native tool outside Python; save exact owned packages and re-read persisted values. No Python Blueprint compile or generic promotable-operator authoring on the known problematic path.
5. Compare actual screens to all twelve references at the current DPI, including long text, empty/full inventory, mouse/controller focus, bright/dark HUD, repeated opening and resolution changes.
6. User-run integrated route: accept/track/complete main and side objectives, personal waypoint, inventory/equipment changes, save/load, modal cancel, HUD recreation and keyboard/gamepad switching. Verify no duplicated callbacks, markers or restored items.
7. Finish the edited-asset manifest, source changes, build/readback evidence and any device-specific limitation. Keep layered art and rollback references.

**Gate:** chosen appearance and existing functionality both accepted. A successful compile, a matching mockup or correct button art alone is not end-to-end completion.

## Execution order

Module0 → Module1 → Module2 + narrow Module3 source work → one normal build/restart → Module4 pilot → Modules5/6 → Module7 prompt/controller completion → Module8 ancillary routes → Module9 acceptance. The prompt contract and ownership decisions are designed in Module1, before new screens bake in fixed keyboard labels.

Independent art/source preparation can run in parallel, but editor writes, compilation, save and adaptive verification are sequential. No further broad design choice is required to start; only a genuinely unresolved gameplay/route decision or build/editor boundary should interrupt execution.
