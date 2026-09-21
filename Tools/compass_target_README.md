# Explicit navigation target example

Authoring script: `C:/UnrealEngine/Games/AZ/Tools/compass_target_setup.py`.
Destination: `/Game/AZ/Blueprints/Menu/HUD/Navigation/Examples/BP_AZ_NavigationTarget`.

This is an invisible Actor anchor with an editor-validated DefaultSceneRoot. It adds no quest logic, global player lookup, automatic BeginPlay registration, tick, or placed level instance. The icon and text are supplied through the existing ProHUD marker structs.

## Installed state — September 19, 2026

The target, bridge, module and owned leaf widgets are authored, compiled and saved in Unreal. The module is attached to the existing GameHUD. These instructions describe an installed feature; do not rerun authoring stages just to use it.

The target's saved defaults enable its compass marker and disable its optional world marker. Both marker structs use the owned CHALK diamond icon and distance display. Enabling the world channel also enables its source screen-edge arrow. Runtime marker behavior still awaits Artur's Play validation.

For a real destination, place the target at that location and call `RegisterForPlayer` from the event that makes that destination known, passing that event's actual local PlayerController. The object appears on the selected displays; no placed target appears merely because it exists in the level. Call `UnregisterForPlayer` when it stops being relevant. Moving the target updates its location automatically while registered.

For an existing Actor or SceneComponent, use the same controller's `BPC_AZ_CompassBridge` directly: `AddOrUpdateCompassMarker` and optionally `AddOrUpdateWorldMarker`, with that exact object as `Marker_Object` and the corresponding ProHUD info struct. Remove with the same object key. Different SceneComponents on one Actor are distinct targets.

## Authoring order

1. Compile the existing `BPC_AZ_CompassBridge` first. Its retention/detached-removal patch and the leaf removal-identity fixes must be installed.
2. Import the script in an Idle editor, call `prepare()`, then use the dedicated native Blueprint compile tool.
3. Call `wire()`, then native compile again.
4. Call `defaults(show_on_compass=True, show_in_world=True)` or explicitly select the desired channels. Parent authors `CompassInfo` and `WorldInfo` using the actual vendor struct fields and approved icons; this script does not guess those fields.
5. Save the target explicitly. The script does not place it or start PIE. Partial stages fail closed for inspection; completed stages do not duplicate graphs.

## Gameplay integration

Call `Target.RegisterForPlayer(ExplicitPlayerController)` from the relevant gameplay event or Level Blueprint. Only that local controller is accepted. The function finds its existing bridge or creates one using AddComponentByClass with a wired identity transform and immediate registration. This works before HUD construction: the bridge queues the marker until its module becomes ready.

The target itself is the stable key for both channels. Re-registering updates the selected channels and removes any channel now switched off. The target remembers each used bridge once. Repeated registration does not create a duplicate bridge or a second marker for the same target.

Call `Target.UnregisterForPlayer(ExplicitPlayerController)` when this target should stop being visible to that player. It removes both channels from remembered bridges owned by that controller and forgets those bridges. Other local players' registrations remain intact.

Actor `ReceiveEndPlay` snapshots and clears remembered bridges, then asks each still-valid bridge to remove both channels using the exact same Self identity. Cleanup deliberately does not require the target or its controller to pass a target-side owner-validity test. The bridge retains its own ownership guards. Leaf removal must also permit deleting a stored key whose Actor is pending kill.

The anchor tracks its actual Actor transform. Place/move it explicitly later when there is a real gameplay destination; this example creates no demo/story objective. For a SceneComponent key or another Actor key, existing callers can use the bridge API directly rather than manufacture a second anchor.

## User-run checks after integration

### Ready-to-play demonstration in L_001

On September 19 Artur authorized three temporary demonstration targets in the current test level. `BP_AZ_NavigationPreviewTarget` is a child of the production target. Its single BeginPlay chain calls the parent, enumerates existing PlayerControllers once, and uses inherited `RegisterForPlayer` (which requires a valid local controller). The existing bridge queues these requests until HUD readiness. Inherited EndPlay removes the registrations. The production target still requires explicit registration.

The level contains `CompassPreview_A`, `CompassPreview_B`, and `CompassPreview_C`, grouped under **Navigation/CompassPreview** in the World Outliner. Both display channels are enabled and the world labels are **TEST A**, **TEST B**, **TEST C**. Relative to the default PlayerStart facing +X: A is about18m ahead, B about20m to the right, C about16m behind on a raised platform. Marker anchors are120cm above the traced support surface. They have no physical collision or gameplay objective logic.

Start Play yourself, rotate through a full turn and approach the targets. Look for compass diamonds/distances and world markers/edge indicators. The positions are fixed in the level; starting from the editor camera changes their initial directions. To remove this demonstration later, delete exactly those three actors from its Outliner folder and save the level. No HUD/production-target rollback is needed. The demo is intended for the existing local player at normal PIE startup; late-joining players are outside its scope.

Receipts: [placed targets](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/preview-targets-placed.json) and [saved targets](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/preview-targets-saved.json). Native Blueprint compilation and map save passed; Codex has not launched Play or verified the markers visually.

Check registration before/after HUD readiness, duplicate registration, updated text/icons, toggling either channel and re-registering, per-player unregistration, HUD removal/reattachment, remove/re-add during fade, and destroying the target. Confirm there are no ghost markers and that another local player's markers survive one player's unregistration.

Authoring, native Blueprint compilation and saving have now been completed. See [the integration ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-integration-progress.md) for receipts and the remaining manual acceptance gates. No automated tests were added and no PIE session was started by Codex.
