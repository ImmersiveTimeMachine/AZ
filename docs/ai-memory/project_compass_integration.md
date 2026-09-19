---
name: CHALK ProHUD compass integration
description: September19 installed/saved integration and final registry correction complete; user PIE readiness confirmed, marker/visual acceptance pending.
type: project
---

User requested reuse of /Game/ProHUDV2_Horror compass/markers/demo, then authorized Codex implementation. The user resumed after earlier shutdown pauses. **Current: installed, compiled and saved, including final marker-registry correction; manual marker/visual acceptance pending.** Old paused/five-or-seven-asset/untouched-HUD wording is historical.

Read [ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-integration-progress.md) and [checkpoint](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/current-checkpoint.md) first. Primary [plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/compass-prohud-integration-plan.md). Usage: [target README](C:/UnrealEngine/Games/AZ/Tools/compass_target_README.md).

## Installed and saved

Ten BPs compiled successfully. [final-save-readback.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/final-save-readback.json) lists thirteen explicitly saved assets with dirty[]: nine navigation BPs, existing GameHUD, three textures. Later [registry-fix-saved.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/registry-fix-saved.json) records both corrected containers saved with hashes and zero dirty content after successful native compilation.

Navigation folder contains four copied leaves, CompassModule, NavigationSettings/NavigationWidgets helper libraries, CompassBridge, Examples/BP_AZ_NavigationTarget and CHALK compass strip/pointer/target textures. Typed source-class remap applied to all four leaves using editor-only support. No new runtime compass solver.

Module preserves 53 exact vendor config fields plus NorthYawDegrees(saved=-90); world setup uses 17 exact struct links. Owning player/NavigationContext is explicit for module/children. Compass Init uses context identity and a completion notification, not a fixed wait. Owned helpers preserve source formulas while removing global player/first-widget lookups.

GameHUD preserves 34 original widgets and adds NavigationHost. Existing local CommonUI/PlayerUI host remains. No vendor AHUD/GameMode, quest framework, global registry or passive GAS ability. Local controller retains bridge and module/tree/maps across HUD reconstruction. Pending UObject-key maps cap 128/channel; latest update wins. bAttached blocks late readiness; detached removals reach retained initialized module. Target/bridge EndPlay cleanup removes exact keys and releases references.

Target example exposes vendor CompassInfo/WorldInfo and channel flags, explicit RegisterForPlayer/UnregisterForPlayer(Controller), Self identity and unique remembered bridges. Defaults/icons authored; no actor placed or story objective invented.

CHALK native art completed/imported/saved: #EEEAE0/#FFBA8C/#101515, 600×64 window, 1440 strip draw width / 150° window. Native sources: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/Compass. QuickSelect navigation opacity 0.45; component cached, writes only on change, no input changes. Visual acceptance pending.

## Runtime evidence is readiness only

[user-pie-navigation-state.json](C:/UnrealEngine/Games/AZ/Saved/CompassIntegration/user-pie-navigation-state.json) is USER-started PIE evidence: same local PC owns HUD/module/compass; module initialized/ready/compass-ready/world-ready=true; correct references; compass context identity matches, IsActive=true; bridge ready/attached/visible=true, pendingvisibility=false.

It does NOT verify markers, bearings, fades, readability/DPI, detached/rebound behavior, multiple local players or performance. No Codex-started PIE or automated tests. Never call the whole feature accepted because ready flags are true.

## Final correction resolved; next gate is user acceptance

The overlay-scan review issue is resolved. Both containers now use Map_Find→Found→valid widget→!bRetiring. Removal uses the exact existing key without requiring target validity; compass RemoveAll snapshots keys and uses that path. A world-key pin mismatch during authoring was repaired before successful native compilation/save. Final graph/hash/zero-dirty evidence is registry-fix-saved.json; the earlier marker-removal-safe.json alone did not cover this correction. **Static review/compile/save passed; runtime fade, destroy and re-add checks remain pending.**

Then user checks register/update/remove/re-add/fade/destruction, both channels, pre-ready queue, HUD-detached removal/reattachment, cardinal/wrap, QuickSelect/inventory, DPI/visuals and regressions. Identity is Actor/SceneComponent, not a demo tag or GUID. Visibility radius is maximum-distance fade, not arrival completion; ScreenPersistance is screen policy, not SaveGame.

## Constraints and separate work

Use stage receipts; do not recreate completed assets or overwrite CHALK defaults with early copies. Compile regular BPs through dedicated native tools after Python returns, then explicitly save. Check fresh PIE state before mutations. Earlier shutdown-save/native-build checkpoints are historical.

[Claude grenade-turn/strafe handoff](C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-grenade-turn-and-strafe-fix-plan.md) was delivered separately. Claude's later animation changes make it a dated baseline. Preserve unrelated animation, locomotion and throw source/assets while finishing compass integration.
