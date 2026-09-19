# CHALK compass and target markers — complete integration plan

**Owner and implementer: Codex. Requested September 18, 2026; implementation subsequently authorized.** Artur wants the existing ProHUD V2 Horror compass, its target markers and demonstrated behavior integrated into CHALK, with small verifiable steps. Reuse the supplied implementation. This is not an assignment to build a second navigation system or hand the work to Claude.

**September 19 checkpoint:** the owned compass, world-marker widgets, local-player bridge, HUD host, target example and CHALK artwork are authored, compiled and saved. Initialization was inspected during Artur's own Play session. Manual marker, lifecycle and visual acceptance remain pending; this is not a completed gameplay test pass. The [module ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-integration-progress.md) records current evidence; the module descriptions below remain the acceptance contract.

Progress and gate receipts belong in [the module checklist](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-integration-progress.md). Every module below has an output, a verification gate and a rollback boundary. Do not call the integration finished merely because a compass widget appears.

## Outcome and scope

The existing CHALK HUD gains its approved slim top-center compass. Turning the camera scrolls the heading correctly. Registered Actor or SceneComponent targets appear at their correct bearing, move as their targets move, and support the pack's icon, highlight, distance, ping and visibility options. Optional linked world markers retain the pack's projection and screen-edge behavior. Updating/removing a target operates predictably without duplicates or orphan widgets.

The compass uses CHALK's actual owning player/camera and coexists with health, Fight/Explore, weapon/ammo, reticle, pickup feedback, inventory and quick select. The integration supplies a usable registration API and an owned demonstration/authoring example, so a real gameplay caller can create a target rather than relying on baked sample labels.

**Navigation here means the source pack's bearing/target presentation.** It does not imply automatic walking, NavMesh pathfinding, a route around obstacles, a quest manager or enemy detection. Minimap, mission notifications and the other HUD-pack systems are separate siblings; they are not pulled into this task merely because the demonstration exposes them. ScreenPersistance is a display option, not save-game persistence. This integration must not claim to implement any of those additional systems.

## Evidence already collected

Evidence folder: `C:/UnrealEngine/Games/AZ/Saved/CompassPlanning/`.

- `az-integration-audit.md`: current PlayerController, PlayerUI, CommonUI HUD creation/visibility and ownership.
- `vendor-api-demo-audit.md`, `vendor-facade-demo-receipt.json`: supplied facade, settings container and actual demonstration call chains.
- `world-marker-source.json`, `world-collapsed-graphs.json`, `world-defaults.json`: world projection, distance, object-location resolution, construction and cleanup.
- `reuse-dependencies.json`: compass/container/base/helper/material dependency inventory.
- `current-hud-tree.json`: the current GameHUD contains34 concrete widgets and no installed compass. HitMarker is firearm feedback, not a navigation target layer.
- `vendor-compass-audit.md`, `vendor-compass-source.json`, `cardinal-direction-source.json`, `compass-detail-source.json`: parent-verified live compass heading, north source, marker mapping, deferred initialization and removal. The completed parent section supersedes the interrupted delegate's historical limitations in that audit.

The original package is `/Game/ProHUDV2_Horror`. The demo map was **not opened or played** during planning. Inspecting its referenced Blueprints is not a runtime parity test. No Source, game assets, map, input bindings or GameMode were changed in this pass.

### Source assets to reuse

| Role | Existing asset |
|---|---|
| Compass ribbon and compass marker registry | `/Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_Compass_H` |
| One compass target marker | `/Game/ProHUDV2_Horror/Widgets/Content/Compass/WB_CompassMarker_H` |
| Screen/world-marker container | `/Game/ProHUDV2_Horror/Widgets/Content/WorldMarker/WB_WorldContainer_H` |
| One world marker | `/Game/ProHUDV2_Horror/Widgets/Content/WorldMarker/WB_WorldMarker_H` |
| Construction base | `/Game/ProHUDV2_Horror/Widgets/Base/WB_Construct_H` |
| Style/config and forwarding example | `/Game/ProHUDV2_Horror/Widgets/Content/WB_HUD_Container_H` |
| Public source interface | `/Game/ProHUDV2_Horror/Blueprints/Libraries/BPi_HUDManagerV2_H` |
| Global-context helpers to adapt | `/Game/ProHUDV2_Horror/Blueprints/Libraries/BP_PHV2_Functions_H` |
| Reusable widget helpers/macros | `/Game/ProHUDV2_Horror/Blueprints/Libraries/BP_PHV2_Widget_Functions_H`, `BP_PHV2_Widget_Macros_H` |
| Marker data | `S_CompassMarkerInfo_H`, `S_WorldMarkerInfo_H`, `S_WorldMarkerValues_H` under `/Game/ProHUDV2_Horror/Blueprints/Structs/` |
| Compass drawing | `/Game/ProHUDV2_Horror/Materials/M_Compass_H`, `M_Compass_Inst_H`, referenced masks/strips/pointers |
| North reference | `/Game/ProHUDV2_Horror/Blueprints/Minimap/BP_CardinalDirection_H` — shared naming does not make minimap a requirement |
| Actual demo compass caller | `BP_DemoMarker_H`, `BP_CompassArea_H`, `BP_DemoTrigger_Marker` under `/Game/ProHUDV2_Horror/DemoContent/` |
| Actual demo world caller | `BP_DemoWorldMarker_H`, `BP_DemoTrigger_WorldMarker`, task example in `ThirdPersonCharacter_H` |

Keep source pack assets unchanged. Duplicate/adapt only the behavior-bearing assets that need changes, preserving source enum/struct field types. Unchanged textures, fonts, structs, materials and pure helpers may remain referenced. Record each retained reference deliberately; do not duplicate the entire pack.

### Exact source contracts and pitfalls

1. `AddOrUpdateCompassMarker_H(Marker_Object, MarkerInfo)` and `AddOrUpdateWorldMarker_H(MarkerObject, Marker_Info)` use **object identity**. Remove receives that same object. The demo uses Self for one marker and a MaterialBillboard SceneComponent for another. There is no returned GUID or direct FVector target in these API signatures.
2. The source location resolver handles Actor and SceneComponent. An arbitrary UObject falls through to world zero. The AZ entry must reject unsupported/invalid targets rather than drawing a false marker at the origin.
3. The supplied manager routes through an AHUD, root vendor widget and HUD container to the actual compass/world registries. CHALK already owns its HUD, so adapt that forwarding/config boundary; do not replace the game's HUD class with the demo manager or spawn a second complete HUD.
4. Source helpers use PlayerController index0 and the first widget returned by GetAllWidgetsOfClass. The world marker also projects with PlayerController0 and `bPlayerViewportRelative=false`. Replace those context lookups in AZ-owned paths with the explicit owning controller, its camera and the receiving widget's local geometry.
5. World marker distance already reads GetOwningPlayerPawn. Child CreateWidget calls must therefore receive OwningPlayer as well; a correct root owner is not sufficient when newly created markers omit it.
6. The HUD container initializes the world marker container with `S_WorldMarkerValues_H`; it also supplies ping icons and style settings. Bare class defaults include zero/transparent/unset values. Preserve the initialization flow rather than assuming a copied leaf will style itself.
7. The forwarded add/update call is dropped if its child is invalid; no readiness queue was found there. Initialization and registration need an explicit ready handshake.
8. The demo uses tagged-actor lookup followed by the first result and casts to its own demo character. Those are demonstration conveniences, not the target identity model or a reason to require ACharacter in CHALK.
9. Demo “compass/minimap/world” booleans form a priority branch, not independent simultaneous registrations. A target shown on both compass and screen requires two explicit calls using the same target object.
10. World-marker radius is a maximum visibility distance: inspected logic hides beyond it and optionally fades toward that boundary. It is not objective completion on arrival. Camera-facing/edge rules are distinct from line-of-sight tracing; do not invent wall occlusion or gameplay discovery based on a variable's name.
11. No marker-priority or clustering contract was established in the source API. Preserve its multi-marker behavior; use its existing highlight option when needed. Do not quietly add nearest-target selection, overlap suppression or a new objective-selection UI as part of the reuse task.
12. Compass marker removal is latent: fade, wait 0.5 seconds, remove the object key, detach the widget. Rapid remove/re-add needs protection against the old callback removing the new registration. This is a source-derived lifecycle risk, not a runtime failure reproduced in this audit.

### Verified compass behavior to preserve

| Source behavior | Verified contract / integration consequence |
|---|---|
| Heading | Connected camera branch reads camera-manager POV yaw, maps [-180,180] to [-1,1], then material Offset [-5,5]. Preserve parameter semantics and validate rendered wrap; this range does not establish visible FOV. |
| North | GetCardinalDirection selects the first BP_CardinalDirection_H; GetDirection rounds its DirectionMesh world yaw. Missing reference returns -90 after a diagnostic. Compass maps [180,-180] to Orientation [0,1], then material [-0.5,0.5]. Use explicit configured north with this convention; do not guess a world axis. |
| Marker position | Source uses a clamped XY dot mapping. MarkerPrecision 0..1 maps its limit 0.75..0.33; side dot maps [-limit,+limit] to [0,1], then to plus/minus half the widget's local geometry width. Preserve it before evaluating any behavior change. |
| View basis | Strip uses camera POV rotation; marker helpers flatten camera-manager actor right/forward vectors. Verify their agreement with CHALK's camera, pitch and effects; a difference in source accessors alone is not proof of a bug. |
| Behind-camera behavior | Facing gate is forward XY dot >=0.6. Without ScreenPersistance it fades the marker base. The not-facing position branch chooses an edge and includes a one-shot random-side fallback. Document the actual behavior; do not substitute an unverified signed-angle solver. |
| Construction | WB_Construct_H Construct defers Init through Delay(0). Signal Ready after actual configuration/child setup; adding another arbitrary delay is not readiness handling. |
| Marker text | Compass struct has distance text settings, no target-name field. Optional world markers provide names. Approved compass layout needs icon/distance, so no extra name system is required. |

Core graph behavior is source-verified. Actual widget/texture dimensions, shader/cardinal visual alignment and runtime behavior are captured or checked at the relevant implementation gate; no runtime acceptance is implied by this audit.

## Integration architecture

Reuse the existing local HUD creation: `AAZ_PlayerController::CreateHUDWidget` creates the assigned HUD with the owning controller and AddToPlayerScreen. Preserve the native `UAZ_Inv_CommonUI_InventoryHudWidget` parent and its initial snapshots/delegate cleanup.

Proposed AZ-owned location: `/Game/AZ/Blueprints/Menu/HUD/Navigation/`.

| Planned piece | Responsibility |
|---|---|
| `WBP_AZ_CompassModule` | Small passive root/config host, extracted from the source navigation-related container behavior; compass strip plus world marker layer, no demo notifications/minimap. |
| `WBP_AZ_Compass`, `WBP_AZ_CompassMarker` | Owned adaptations of the source compass widgets, retaining its algorithms and marker map. |
| `WBP_AZ_WorldMarkerContainer`, `WBP_AZ_WorldMarker` | Owned adaptations of source world-marker behavior, used only when that channel is requested. |
| `BPC_AZ_CompassBridge` | Thin local PlayerController-owned bridge: one module reference, owner/context/config injection, registration forwarding and readiness handling. No tick-based compass math, objective ownership or new world subsystem. |
| Owner-aware navigation helper functions | Required replacements for global manager/style/player discovery. Pure vendor formatting/interpolation helpers remain reusable where they do not contain those globals. |
| Owned example target/registration bridge | Demonstrates Actor and SceneComponent targets and explicit add/update/remove from CHALK context, with no dependency on ThirdPersonCharacter_H. |

These names are proposed outputs, not claims that those assets already exist. If a smaller extension to current PlayerUI/HUD can satisfy the same contract, use it and record the substitution in the module receipt. Do not introduce a native subsystem or port the vendor math to C++ without a demonstrated need. A minimal native accessor/event bridge is acceptable only where the existing C++/Blueprint boundary requires it; such a change follows the full build/reflection rules.

```text
CHALK gameplay caller / owned example
  → explicit owning PlayerController
  → local compass bridge
  → one navigation module hosted inside WBP_AZ_GameHUD
      ├─ adapted vendor compass → adapted compass-marker map
      └─ optional adapted world container → adapted world-marker map

Owner camera/pawn + north/style context feed the existing vendor calculations.
```

The two vendor widget maps remain the presentation registries. Avoid a competing global target registry. The bridge can retain the module across HUD reattachment and keep bounded pending requests until it is ready; those are lifecycle support, not a quest database. Destroy/unpossess/world-end paths must release registrations and subscriptions according to their actual lifetime.

## Execution rules and checkpoints

Every module ends with a receipt listing changed assets/source, configuration and connection readback, compile/save results, and any user check still pending. Never mark a runtime gate passed from static inspection alone.

Project rules: no new automated tests; ask before starting PIE/editor tests. Default workflow is **Codex prepares and verifies assets/source; Artur performs the stated Play check; Codex reads the logs/capture**. No forced map changes or PIE starts during this planning pass. Static authoring readback is part of asset verification, not permission to run automated gameplay scenarios.

Read/inspect existing state without permission prompts. Ask only when a real new design choice, runtime-test authorization or conflicting unsaved edit blocks progress. Existing vendor behavior and already-approved CHALK styling do not need to be re-approved repeatedly.

### M00 — Baseline and dependency freeze

**Output:** source feature/API map, current HUD snapshot, asset dependency manifest and confirmed scope.

Actions:
- Preserve the evidence listed above and record current package state before implementation, because other work continues in this shared workspace.
- Capture source compass heading/north/marker formulas, settings defaults, initialization order and demo calls. Separate compass-marker behavior from world-marker behavior.
- Inventory the current HUD's34 widgets and properties, local owner creation, collapse behavior, reticle and quick-select interfaces.
- Choose the exact smallest copy/adaptation set and mark references that remain vendor-owned and read-only.

**Gate:** every required behavior has an identified source asset/function and owner; no claim depends only on a screenshot or asset name. Runtime demonstration, if desired, requires user permission and is recorded separately. Planning audit is already available; do not restart discovery from scratch.

**Rollback:** none needed; this stage is inspection and documentation.

### M01 — Create isolated AZ-owned navigation assets

**Depends on:** M00. **Output:** the minimal copied widgets/config/helper paths, not yet replacing the active HUD.

Actions:
- Duplicate the two compass widgets and their necessary construction/helper dependencies where edits will occur. Prepare the world-marker copies in the same namespace.
- Create a slim module host containing only the navigation configuration/forwarding portions required from WB_HUD_Container_H. Reuse its initialization fields and structs; do not rebuild marker math.
- Remap nested widget classes, creation nodes, parent/context types and helper calls to the intended AZ-owned paths. A renamed widget still referencing the wrong original marker class is incomplete.
- Reuse unchanged vendor textures/structs/enums/materials. Create owned material instances only for CHALK style overrides.
- Keep backups/hashes of any existing AZ assets that later modules will modify. Do not touch the original ProHUD package.

**Static gate:** each owned Blueprint compiles through the safe native workflow; saved references match the manifest; no missing classes/pins, source pack differences or demo-character dependencies were introduced. Active HUD remains on its current version at this stage.

**Rollback:** detach the new module/reference; retain new assets for inspection. Never bulk-delete a folder or restore unrelated changes.

### M02 — Explicit local owner, configuration and readiness

**Depends on:** M01. **Output:** one initialized navigation module for the owning local controller.

Actions:
- Add the thin bridge to the existing owning controller path or use the agreed equivalent small PlayerUI/HUD extension. Guard local ownership and duplicate initialization.
- Pass OwningPlayer to every CreateWidget, including compass/world marker children. Read the current owning pawn through that controller; never cast it to the demo ACharacter.
- Inject explicit module/config/camera references into adapted helpers. Eliminate first-player/first-widget discovery from the active navigation call chain; pure helpers that are independent of ownership can remain unchanged.
- Resolve/cache the north reference at initialization. Prefer an explicit configured reference/angle using the source orientation convention. If discovery is used once as a setup convenience, diagnose zero/multiple matches instead of silently taking an arbitrary first actor. Do not search the world for it every frame.
- Preserve source style and world-values initialization in its correct order, including the base widget's deferred Init. The module signals Ready only after child widgets/configuration and marker classes are valid; an arbitrary fixed delay does not satisfy this contract.
- Guard Designer/thumbnail construction, where no gameplay player or camera exists. Use the source design-time branch for preview rather than running runtime manager lookups or delayed callbacks against a missing owner.
- Queue a bounded last desired state per target/channel while not ready; repeated Add updates, Remove cancels a pending Add, RemoveAll clears the relevant pending channel. Replay once on readiness with validity checks.

**Static gate:** the entire active branch has an explicit owner and valid settings. Invalid/missing owner yields no crash or world-origin marker. No second full vendor HUD, minimap or notification stack is created.

**User check when authorized:** fresh spawn and delayed HUD readiness produce one module, with no Accessed None warnings and no markers appearing for another player/context.

**Rollback:** remove the bridge/host binding; original HUD creation and gameplay ownership continue.

### M03 — Host the compass in the existing CHALK HUD

**Depends on:** M02. **Output:** visible compass strip integrated with the current HUD.

Actions:
- Add a passive navigation host to `/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD`. Preserve all34 existing widgets and their properties/slots.
- Keep the compass top-center inside its safe-area layout. Keep any world-marker canvas in the correct owning-player projection space; do not apply a safe-area/DPI offset twice.
- Preserve existing root HUD collapse when inventory opens. Set the navigation layer nonfocusable and hit-test-invisible; do not change input mode or add a gameplay key just to display it.
- Match startup show/hide through the source API. With no registered targets, show only the heading strip, not a sample destination or fabricated distance.
- Preserve reticle centering, weapon/ammo, Fight/Explore, health and current throwable presentation.

**Static gate:** saved widget-tree diff contains only the new navigation host/children and necessary bindings; native HUD parent/controller assignment unchanged. Correct parent/anchors/Z-order and owning context are read back.

**User checkpoint A:** start Play, look around, open/close inventory and quick select, verify ordinary controls and existing HUD remain correct. Record screenshot and relevant log excerpt before moving from “built” to “verified.”

**Rollback:** remove/collapse the navigation host and disconnect its bridge; no existing HUD reconstruction is required.

### M04 — Heading, north, camera and geometry parity

**Depends on:** M03. **Output:** correct bearing behavior with the pack's scrolling/normalization/material logic.

Actions:
- Preserve the source material Offset/Orientation/mask behavior and its actual cardinal convention. Feed the owning camera's yaw for the approved view-relative compass, not the skeletal mesh's delayed facing or an unrelated player camera.
- Rebind after possession/view-target changes. Guard a temporarily missing camera/pawn; do not continue using a destroyed pawn reference.
- Compare four known world directions and a full camera rotation, including the angular wrap boundary. Use a nonzero north orientation to prove the offset is applied exactly once.
- Verify camera-only turning while the character stays still, and both free exploration and aiming camera/FOV changes.
- Keep source layout dimensions for the first parity check. When resizing later, update ribbon window, marker mapping and mask geometry consistently; an outer SizeBox alone is not proof of aligned positions.

**Static gate:** active functions/material parameters use the intended owner/north settings and retained source math. No independent compass solver has been introduced.

**User check:** north/east/south/west headings and smooth wrap agree with the configured north reference; no jump caused by actor facing, FOV, viewport resize or a stale camera.

**Rollback:** revert only the owning-context/north/layout adapter changes recorded for this module; do not alter world coordinate conventions globally.

### M05 — Compass target API and one-to-many target presentation

**Depends on:** M04. **Output:** complete supplied compass-marker behavior for registered targets.

Actions:
- Expose owner-aware equivalents of AddOrUpdateCompassMarker, RemoveCompassMarker, RemoveAllCompassMarkers, ShowCompass and HideCompass. Preserve source target identity and info structs.
- Register one Actor, then one SceneComponent target. Moving the actor/component changes the marker bearing without recreating the widget each frame.
- Repeated Add on the same object updates one existing marker; changing icon/color/distance/ping/visibility updates that marker. Remove affects only its exact key. Two different component targets on one actor remain distinct if explicitly registered.
- Reject unsupported/null/destroyed targets. No implicit FVector→zero fallback. A fixed location can be represented by an explicit owned actor/component target when needed, rather than inventing a new source API contract.
- Exercise the pack's optional distance units/steps, highlight, ping and visibility behavior. Use metric units for the initial CHALK presentation; select the actual named enum entry, not a guessed ordinal.
- Keep compass hide/show separate from removing registered targets. Do not copy the demo area's “hide then remove all” as an unconditional gameplay rule.

**Static gate:** actual target object flows through creation, map update and removal; no demo tag→first-actor lookup; child OwningPlayer set; same-key updates remain idempotent.

**User checkpoint B:** target ahead, left, right, behind and at the wrap seam; walk toward/away; move a component independently; update/remove/re-add and show/hide. Check real distances and no duplicates.

**Rollback:** unregister only the owned validation targets; compass heading remains operational.

### M06 — Optional world markers and synchronized channels

**Depends on:** M05. **Output:** source world-marker projection, distance/name, edge arrows, ping and visibility integrated with the same owner.

Actions:
- Initialize the world container through its required values/config flow. Preserve the source setting meanings; zero class-default structs are not the finished runtime configuration.
- Adapt PlayerController0 projection to the owning local controller and receiving widget geometry. Check viewport-relative versus absolute pixel coordinates and DPI conversion as one coherent pipeline.
- Preserve the source front/behind transition, clamping, arrow direction, opacity/position interpolation, text rules, ping and screen-persistence settings.
- Allow compass-only, world-only and both-channel registrations. Dispatch both explicitly when requested; the source demo's priority switches do not accomplish this automatically.
- Updating/removing one combined target must affect its selected channels without removing unrelated targets. Keep same Actor/SceneComponent identity across both displays.
- No extra line-of-sight or navigation routing is inferred. Preserve source navigation visibility first; hiding beyond a radius is not completing a mission.

**Static gate:** both widget maps, owner references and delegate removal paths are valid; projection outputs are in the actual local canvas coordinates, and both-channel calls are deliberate.

**User check:** target outside each screen edge and behind camera, high/low relative elevation, camera pitching/rotating, close/far radius thresholds and settings combinations. Arrow/marker transitions must not jump to the wrong side or strand a label.

**Rollback:** disable/detach the optional world-marker layer; the compass and its target list continue independently.

### M07 — CHALK-facing registration example and authoring workflow

**Depends on:** M05; M06 for both-channel examples. **Output:** real integration entry points usable by level/gameplay code, not only the vendor demonstration.

Actions:
- Adapt the useful demo call pattern into a small AZ-owned example/target bridge. Accept an explicit owning controller or gameplay instigator resolving to one; do not require ThirdPersonCharacter_H, Q input or the vendor GameMode.
- Expose target object, display channel(s), marker info and register/update/remove operations. Keep editor-only debug presentation optional and collision-free.
- Demonstrate a stationary destination, a SceneComponent attachment point and a moving target. Production markers are opt-in; do not automatically reveal every enemy, pickup or unknown point of interest.
- Use explicit gameplay events to register/remove objectives. The compass does not invent progress or auto-complete a goal solely because its distance is small.
- Document how a future mission/interaction system calls the bridge. No full mission framework or save-game subsystem is required to make the existing API usable.
- Keep validation targets in an agreed existing test area or owned validation level; do not modify the vendor demo or silently add story objectives to the user's level.

**Static gate:** callers reference the AZ bridge/module and original info schemas as intended, with valid player context and target identity. No unrelated input mappings, GameMode or inventory state changes.

**User checkpoint C:** a CHALK gameplay/example event adds a target, walking/camera movement updates it, and the corresponding event removes it on both requested displays. Repeat without accumulating widgets.

**Rollback:** disable the owned example/registration call sites; the integration API remains available.

### M08 — Lifecycle, teardown and HUD reconstruction

**Depends on:** M07. **Output:** no stale markers, duplicate dispatchers or lost ready requests through normal lifecycle transitions.

Actions:
- On target EndPlay/destruction/component invalidation, unregister its presentation. Validate targets on updates as a fallback; never render world zero from a dead reference.
- Preserve the visual removal behavior while making logical deregistration safe: world markers unbind TickUpdate; compass markers currently delay key removal by 0.5 seconds. Define a removing-state/cancel policy or detach the old registry entry before fading, and guard late cleanup by widget identity/generation. Remove/update/re-add within the fade window must leave the latest intended marker alive exactly once. Repeated remove and world teardown during the fade must be safe. RemoveAll clears pending operations and the correct channel without mutating a live iteration unsafely.
- On HUD hide/reattach, distinguish invisibility from destruction. Prefer one controller-owned module instance that reattaches with its existing valid registrations. If reconstruction is necessary, snapshot/replay the current requested presentation state once; do not create a second competing registry.
- On pawn replacement, update viewer/camera context; do not clear valid world targets merely because the hero pawn changed. On controller/world end, unbind and clear the associated context completely.
- For level unload/stream-out, invalidate targets owned by that level. Re-registration of newly loaded actors comes from the actual gameplay/provider owner; raw Actor/widget pointers are not save identifiers.
- If two local contexts are available later, all creation/updates stay within their own controller/module. No player0 or global first-widget shortcut may remain in the active branch.

**Static gate:** owner lifetime, delegate handles/Blueprint bindings and cleanup paths have explicit matching teardown. Current registries and pending requests are bounded and inspectable.

**User check:** remove/destroy target; respawn/repossess; inventory open/close; module detach/reattach; repeat Play sessions; level unload/travel where supported. No ghost markers, duplicate compass, stale pawn distance or Accessed None warnings.

**Rollback:** revert the specific lifecycle adapter change; retain receipts and reproduce the smallest failing case before additional edits.

### M09 — Apply the approved CHALK visual treatment

**Depends on:** functional M04–M08. **Output:** reused working source behavior in the already-approved visual language.

References:
- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/CHALK_HUD_v03_NATIVE.xcf`
- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/01_HUD_Compass.png`
- `C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-design-next-session.md`

Actions:
- Reuse the slim top-center layout, warm neutral/peach palette and existing Oswald/Roboto hierarchy. Do not restart a three-concept selection or replace the rest of the HUD.
- Use stock compass strips/pointers/masks and marker settings where they match. Art changes, if needed, are Codex-owned native GIMP/vector work with editable sources and owned Unreal instances.
- Preserve the source marker mapping when narrowing the strip. Check the pointer, cardinal spacing, marker range and clipping together at the new dimensions.
- Use real target names/distances. No hardcoded126m, sample objective name or arbitrary sample marker in production.
- Keep Show/Hide and style controls exposed. A new settings menu/save framework is not part of this integration; wire an existing settings surface only if one exists. Runtime enable/disable remains available through the bridge.
- During quick select, apply the approved quieter compass presentation through the existing owning UI state or a minimal visibility notification; do not duplicate selector state or poll the world. Inventory collapse remains inherited from the HUD.
- Check contrast in dark streets and bright prototype ground, DPI/aspect ratios and overlaps with other UI. Do not use bloom or loss of depth/visibility semantics to hide layout defects.

**Static gate:** only owned style/layout assets changed; saved dimensions/colors/fonts/references agree with the approved source and layout calculations.

**User check:** full HUD and quick-select screenshots at normal and wide aspect ratios, bright/dark backgrounds and multiple markers. Readability and layout require visual approval; a compile receipt is not that approval.

**Rollback:** switch to the prior owned style/layout settings; source pack and functionality remain intact.

### M10 — Bounded work, final regression and delivery

**Depends on:** M01–M09. **Output:** complete integrated feature with evidence and maintainable controls.

Actions:
- Check zero/one/multiple-marker work. Preserve source keyed widget reuse: updates must not allocate widgets/materials every frame. Remove global widget/world discovery from the tick path and avoid updates for invalid contexts.
- Profile only the navigation branch if there is a measured issue. Do not add a new pooling framework or rewrite the math preemptively. Record useful marker counts/update behavior rather than promising an unmeasured performance budget.
- Compile/save only affected packages, verify post-save/reload references and full graph/property diffs. If native bridge changes were required, confirm the correct build is loaded; new reflected fields need a normal build/restart.
- Compare effective runtime instance settings with authored class defaults during the user's check. Earlier project work showed that a correct CDO readback can coexist with stale Blueprint instance initialization; a CDO snapshot alone is not proof of actual displayed values.
- Confirm the original vendor assets and all existing HUD/core gameplay inputs/data flows remain intact.
- Give Artur the final concise manual checklist and inspect the resulting logs. Do not run PIE or automated tests without explicit authorization.
- Deliver exact asset paths, public API usage, exposed settings, source-to-owned mapping, screenshots and the module ledger. Clearly separate implemented, built/saved, and user-verified states.

**Final checkpoint D:** heading, target tracking/upsert/removal, optional world projection/edge behavior, context/lifecycle, UI coexistence and visual acceptance all pass. The feature is not “done” with only a scrolling strip or a static demo marker.

**Rollback:** navigation can be disabled/detached as one owned module; no restoration of unrelated inventory, animation, weapon or traversal work should be necessary.

## Acceptance matrix

| Scenario | Required result |
|---|---|
| No targets | Heading only; no fabricated marker/name/distance. |
| Camera turns, pawn stays still | Compass follows the intended camera bearing. |
| Full rotation / wrap boundary | Continuous heading and correct side for target markers. |
| Nonzero north orientation | Orientation is applied once, consistently to ribbon and targets. |
| Actor / SceneComponent target | Correct location, including an independently moving component. |
| Add same target twice | One marker updated, no duplicate. |
| Remove / remove-all / invalid target | Correct registry and tick-binding cleanup; no world-origin fallback. |
| Remove then re-add during fade | Latest registration survives once; an old delayed callback cannot delete it. |
| Hidden compass then shown | Visibility policy does not accidentally destroy targets. |
| Compass-only / world-only / both | Exactly the requested displays, shared target identity. |
| Behind/outside viewport | Source-configured compass/world behavior; correct world edge arrow where enabled. |
| Distance/radius/settings change | Real units and source-defined fade/visibility; no fake mission completion. |
| HUD startup / delayed ready | One initialized module; pending updates are applied once. |
| Inventory / quick select | Existing input/focus preserved; navigation obeys agreed visibility. |
| Possession / HUD rebuild / level end | Correct new viewer, no dangling refs or orphan callbacks. |
| Resolution / DPI / aspect | Strip, target mapping and screen projection stay aligned. |
| Ordinary HUD/gameplay | Health, modes, weapons, reticle, pickups, throwables and traversal unchanged. |

## Boundaries that remain explicit

No separate compass solver, passive GAS ability, new quest engine, automatic enemy/loot revelation, minimap integration, route/pathfinding or save-game persistence is implied. Source-screen persistence must not be described as saving a target across sessions. A future save system stores stable gameplay target descriptions and reconstructs presentation; it does not serialize these widgets or transient pointers.

The current package's useful behavior is the reference. Any departure during implementation must have a concrete integration reason, a recorded before/after comparison and a matching gate—not simply a preference for a different architecture.
