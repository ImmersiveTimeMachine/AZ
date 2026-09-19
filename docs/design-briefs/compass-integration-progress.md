# CHALK compass integration — implementation ledger

Primary [plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-prohud-integration-plan.md). Usage: [target README](C:/UnrealEngine/Games/AZ/Tools/compass_target_README.md).

**September 19, 2026: implementation and final registry correction are compiled and saved; manual acceptance remains pending.** The user resumed after the earlier shutdown. Ten Blueprints compiled successfully. Thirteen assets were explicitly saved: nine navigation Blueprints, existing GameHUD and three CHALK textures. User-started PIE reached actual navigation readiness. Marker behavior, heading accuracy and visual gameplay acceptance remain unverified.

The thirteen-asset save snapshot is supplemented by [registry-fix-saved.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/registry-fix-saved.json), which records both corrected containers saved with hashes and zero dirty content. Both were compiled successfully after the final correction. No automated tests or agent-started PIE were performed.

| Module | Deliverable | Current state | Evidence / remaining gate |
|---|---|---|---|
| M00 | Vendor/API/current-HUD baseline | Complete | Source audit, manifest, original HUD tree and backups in Saved/CompassPlanning and Saved/CompassIntegration. |
| M01 | Owned copies and dependency remap | Compiled and saved | Four leaves, module and two helpers; 53 exact config fields transferred. Typed reference remap recorded 55/7/45/15 changes across the leaves. |
| M02 | Owner/config/readiness bridge | Compiled, saved; readiness observed in user PIE | Local controller, explicit context, object-key pending maps and real Init completion. |
| M03 | Existing HUD integration | Compiled, saved; runtime instances observed | All 34 original GameHUD widgets preserved; NavigationHost added. Visual/coexistence checks pending. |
| M04 | Camera/north/geometry parity | Authored and saved | Explicit owner/camera and NorthYawDegrees=-90; source mapping retained. Cardinal/wrap/camera-only/DPI/local viewport checks pending. |
| M05 | Compass marker API | Final correction statically verified, compiled and saved | Map-authoritative lookup and exact-key removal replace the old overlay/target-validity dependency. Compass RemoveAll snapshots keys and uses the same removal path. Runtime marker acceptance pending. |
| M06 | World marker API | Compiled and saved, including registry correction | All 17 exact source world-values mappings preserved; map-authoritative lookup and exact-key removal verified. Projection/edge/name/distance/ping/channel checks pending. |
| M07 | Registration example | Compiled and saved | BP_AZ_NavigationTarget, editable vendor Info structs and explicit per-player API; icon defaults authored. No level placement/fake objective. |
| M08 | Retention/teardown/rebind | Compiled and saved | Retained module/tree/maps, bAttached, detached removal forwarding, target/bridge EndPlay cleanup. Lifecycle gameplay checks pending. |
| M09 | CHALK styling | Native art completed, imported and saved | Three textures; warm neutral/peach palette; 600×64 window, 1440 strip draw width / 150° window. In-game visual acceptance pending. |
| M10 | Bounds/regression/delivery | Pending user acceptance | Pending maps 128/channel; QuickSelect dim writes on change. Performance and gameplay regression checks remain. |

## Installed content and behavior

Under `/Game/AZ/Blueprints/Menu/HUD/Navigation`:

- WBP_AZ_CompassModule, WBP_AZ_Compass, WBP_AZ_CompassMarker, WBP_AZ_WorldMarkerContainer, WBP_AZ_WorldMarker.
- BFL_AZ_NavigationSettings, BFL_AZ_NavigationWidgets, BPC_AZ_CompassBridge.
- Examples/BP_AZ_NavigationTarget.
- Art/T_CHALK_CompassStrip, T_CHALK_CompassPointer, T_CHALK_CompassTarget.

The existing WBP_AZ_GameHUD hosts the module through NavigationHost. It preserves the local CommonUI/PlayerUI host and reuses the controller's module rather than creating duplicate trees. No vendor AHUD/GameMode, quest framework, global player-zero lookup or passive GAS ability was installed.

The module owns 53 transferred settings and both children. Initialization supplies owning player/NavigationContext, configures world values, and waits for the compass's actual Init completion. The compass context-identity guard prevents deferred source Init from restarting presentation. Child marker factories set owner/context before construction. NorthYawDegrees is saved at -90.

The bridge queues latest state per object/channel before readiness, caps pending entries at 128/channel, cancels pending adds on removal and preserves visibility intent. Initial visibility is on; Hide before readiness takes precedence. HUD detach retains module/tree/maps but clears readiness/attachment. Reattachment restores readiness; bAttached rejects late readiness while detached. Detached Remove/RemoveAll reaches the retained initialized module. Target and bridge EndPlay cleanup release exact keys/references.

QuickSelect dims navigation to 0.45 opacity. Its component is cached; opacity writes occur only on state changes. Input handling and the existing inventory/HUD lifecycle are preserved.

## Evidence and limits

[final-save-readback.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/final-save-readback.json) records thirteen saved paths, owned strip/pointer assignments, north -90 and dirty[] at that snapshot. Ten Blueprint compilations succeeded through dedicated native tools. The editor-only typed-reference remap helper supported authoring; it is not a runtime compass solver.

[user-pie-navigation-state.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/user-pie-navigation-state.json), read during **user-started PIE**, records:

- GameHUD, module and CompassView share the same local PlayerController.
- Module initialized/ready/compass-ready/world-ready flags are all true, with correct child/bridge references.
- Compass NavigationContext matches InitializedNavigationContext; IsActive=true.
- Bridge ready/attached/visible=true, matching ModuleRef, pendingvisibility=false.

This proves construction, ownership and readiness for that session. It does not prove marker updates/removal, bearing accuracy, rendering/readability, HUD reconstruction, multiple local players or performance.

Supporting receipts: [HUD preservation](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/hud-host-added.json), [class remap](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/reference-remap-applied.json), [lifecycle hooks](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/lifecycle-hooks.json), [retention](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/retention-wired.json), [QuickSelect dim](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/quickselect-dimming.json), [style layout](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/style-layout-authored.json), [target defaults](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/target-info-defaults.json).

## Final registry correction — resolved in static review

Earlier marker-removal-safe.json records immediate map detach and guarded visual fade. Subsequent review found MarkerObjectInUse still scanned overlay children. A fading child could remain after its map entry was removed, making lookup disagree with the registry during remove/re-add.

Both containers now resolve MarkerObjectInUse through Map_Find, its Found result, widget validity and !bRetiring. Removal uses the exact supplied key without requiring the target object to remain valid. Compass RemoveAll snapshots map keys and invokes the same exact-key removal. The world-key pin mismatch encountered during one authoring attempt was explicitly repaired before successful native compilation; it is not an outstanding partial graph change.

[registry-fix-saved.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/registry-fix-saved.json) contains the saved graph readback and hashes for both containers, with dirty_content empty. **The review issue is resolved and statically verified/compiled/saved.** User-run fade, destroy, same-key re-add and detached-removal behavior remain pending; readiness observed before this correction does not establish those results.

## Resume and manual acceptance

Read [current checkpoint](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/current-checkpoint.md). Preserve stage guards and completed content; do not recopy early defaults over CHALK settings. Use the [target README](C:/UnrealEngine/Games/AZ/Tools/compass_target_README.md) for explicit local registration.

User-run gates remain: cardinal/wrap/bearing; Actor/SceneComponent keys; same-key update/remove/re-add; fade/pending-kill cleanup; independent channels; pre-ready queue; detached removal/reattachment; QuickSelect/inventory; display-size/readability and normal gameplay/performance checks. The user controls PIE/tests; no automated tests should be added.

Earlier five-asset/seven-asset shutdown checkpoints are historical only. shutdown-save-checkpoint.json records a real user-requested pause, but “HUD untouched / module empty / bridge unexecuted” is now obsolete.

Parallel animation work remains separate. The dated [Claude grenade-turn/strafe handoff](C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-grenade-turn-and-strafe-fix-plan.md) was delivered; Claude owns implementation. Compass integration did not alter animation/locomotion/throw behavior. Unrelated Claude source/assets remain separate and must be preserved.
