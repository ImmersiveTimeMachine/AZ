# Quest and full Map — implementation ledger

**Latest user feedback / next session:** Artur reports the first playable check appears to work, but markers share the same appearance. He asked to inspect ProHUD and record this as the next task, then continue later. Read [marker and quest-presentation next-task plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/quest-marker-presentation-next-task.md). Review confirmed identical quest/personal marker presets and no Story/Side distinction in the native Map painter. No gameplay source/assets changed in this review. Comprehensive save/load/lifecycle acceptance remains unconfirmed; unified whole-game design stays separate.

## Current editor integration checkpoint (supersedes the older C++ pause below)

**LATEST: functional asset integration is compiled and saved; ready for user Play acceptance.** QuestModule runtime bindings, all six snapshot renderer functions, exact-key compass/world marker adaptation, source lifetime hardening, the retained GameHUD host, Map page and confirmed checkpoint-load controls are connected. Final readback reports loaded generated classes, zero graph errors for GameHUD/QuestModule/MapPage, and no dirty content/map packages (`Saved/QuestMapImplementation/ready-for-user-play.json`). No gameplay success is claimed yet.

Eight clearly labeled TEST fixtures are placed and saved in L_001. All21pickup IDs survived an editor restart with no missing IDs or duplicates. Native GIMP map/calibration, two quest definitions, seven example BPs/signs and the inventory MapPageClass are saved. The earlier pending statements below are historical intermediate checkpoints.

Two editor hangs occurred in the generic comparison-node spawner during RefreshTrackedMissionVisuals authoring. The saved phase2 was restored; the existing AZ utility created equivalent concrete K2Node_CallFunction comparisons. Phases3/4 then completed and compiled. Final renderer compilation exposed a read-only-array/ref mismatch; a local ProgressRowsSnapshot now supplies the row renderer. All six functions subsequently compiled and passed the read-only graph audit. Exact internal engine hang cause remains unproven; before/after operation traces and recipes document the successful bypass.

Immediate user route: open Inventory→MAP; near the TEST MAIN sign press E, verify tracker/compass/Map destination; enter TEST REACH then use TEST PANEL. Side route uses TEST SIDE, the blue sample pickup and TEST DELIVERY. Use SAVE POINT, then Map→LOAD CHECKPOINT→CONFIRM LOAD to check restore and item consistency. Root has not started Play or added automated tests. Further changes depend on actual user observations/logs; final acceptance remains open.

**Latest art direction:** Artur explicitly notes the inventory and Map currently differ visually. He wants a unified visual language across the whole game/menu system, to be reviewed later. Treat the selected Field Journal implementation as a functional working layout; do not expand this integration into an unsolicited redesign or claim global visual consistency is finished.

Artur explicitly asked to remember the order: finish functional quests, Map and saves first; then prepare alternative mockups and agree one common design for the entire game UI. No redesign work is part of the current continuation.

User reopened the editor and explicitly resumed full integration. Native classes are loaded; the current hero uses MoverNetworkPredictionLiaisonComponent. No PIE or gameplay tests have been started by Codex. User Play sessions are detected before writes and authoring pauses while they run.

- Owned ProHUD context adaptation is now compiled and saved across all six packages. Thirty-eight leaf helper links and one cross-library link receive explicit QuestContext. Recursive child/factory propagation and three module configuration functions are wired. The post-compile audit reports no missing contexts or graph errors. Tracked-mission presentation and native delegate/compass attachment are still in preparation.
- The approved Field Journal UMG page, journal row and map canvas are created, styled, compiled and saved. The existing inventory menu now points MapPageClass at WBP_AZ_QuestMapPage; its existing widget tree is unchanged and backed up. Runtime display/input has not yet been tested.
- Real L_001 artwork is captured without gameplay actors, toned in native GIMP and imported at 2048 square. It includes Landscape, unlike the earlier static-only bounds. DA_AZ_Map_L001 is saved with origin (-1435,3620,0), spans12400cm, rotation0, no UV flips. Texture and page assignment are saved. Source and layered master live under UI Design/CHALK_QuestMap_v01/sources/map.
- Two neutral TEST quest Data Assets and seven configured provider/checkpoint BPs are compiled and saved. Native definition/catalog validation passed. No example actors are placed yet. Visible TEST signage is being authored separately.
- Existing placed pickup-ID authoring has not completed. Python lacks ComponentCreationMethod exposure and rejects EditInstanceOnly on these component wrappers; the recipe now resolves authored components through SubobjectData and uses native property editing on verified map-instance paths. No IDs were written before the current Play pause. The failed first attempt only marked a map object modified; preserve/check dirty state before shutdown.

Next: finish runtime/presentation recipes, stop Play before writes, assign and save pickup identities, create reviewed example placements, attach quest HUD/compass and save/load feedback, then invite user-run acceptance. Do not call either system complete based on compile success alone.

September19: Artur authorized full implementation of both systems and requested the same method used for the successful compass: reuse owned ProHUD copies, explicit context, staged compile/save/readback. He approved autosave at important moments and save/checkpoint locations, with campfires as one possible save-point presentation. One tracked quest/one personal waypoint/one outdoor layer are initial implementation defaults, not a limit on future authored content.

Baseline: editor connected, PIE stopped and no dirty packages at implementation entry. Existing unrelated working changes in Config/DefaultEngine.ini and character/throwable source were observed and will be preserved. The compass has user acceptance; no unrelated rework or test pass is planned.

**Layout decision:** Artur chose **01 Field Journal** (journal left / large map right) after all three native GIMP mockups were shown. Approved layout sources: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuestMap_v01/CHALK_QuestMap_01_NATIVE.xcf` and corresponding PNG. Map/quest content in the study is illustrative; actual playable-map artwork/calibration and neutral sample definition assets remain to be authored.

Source work underway: quest definition/progress/providers + PlayerState wiring; Navigation map/target registry types; inventory/campaign persistence; root UI/QuestMapComponent for local map/waypoint/same-key compass bridging. Native map canvas delegated after Navigation foundation. No build yet; reflected classes require full editor restart/build before authoring related assets. Source dependencies and API ownership are in runtimecontract.

**Current build checkpoint: FINAL FULL BUILD SUCCEEDED.** User requested a pause for rebuild, then explicitly authorized finishing C++ after compiler errors. All missing implementations and compiler errors were resolved. Final AZEditor Win64 Development build completed53actions with **Result: Succeeded,28.24seconds** and linked the project DLL. Evidence: Saved/QuestMapImplementation/final-cpp-UBT.log and final-cpp-build.json. C++ source is frozen; editor may be reopened. Runtime asset/content integration is still pending and was not resumed while the editor was closed. No Play or tests were run.

Owned ProHUD prep is saved: three mission/task widgets + two libraries + empty QuestModule with11typedsettings. Class/graph/CDO references remapped; WidgetTree preview children replaced through native ReplaceWidgetWithTemplate (no unmatched referenced members). All6BPs compiled/saved; last successful dirty-package readback was empty. Context/helper adaptation and runtimeattachment remain incomplete. First authoring receipts: Saved/QuestPlanning/ProHUDAuthoring.

| Module | State |
|---|---|
| 0 — baseline/layout/sample contract | Three native GIMP mockups saved/shown; user selected01 Field Journal; actual map/sample assets still pending |
| 1 — definitions/ownership | C++ definitions, stable IDs and PlayerState progress compiled; filled Data Assets pending |
| 2 — progression/providers | C++ reach/interact/offer/delivery providers + controller interaction compiled; authored scenarios/Play acceptance pending |
| 3 — ProHUD presentation/compass | Six isolated BPs compiled/saved; context adaptation, presentation hardening and runtime attachment pending |
| 4 — full map page | C++ calibration/canvas/page/input shell compiled; UMG page asset and real map texture/calibration pending |
| 5 — journal/waypoint | C++ local component/journal selection/waypoint compiled; BP compass-event bridge and UI asset wiring pending |
| 6 — inventory/delivery | Canonical count + receipt delivery + staged inventory/equipment/quickbar restore compiled; runtime validation pending |
| 7 — saves/lifecycle | Versioned two-slot checkpoint/autosave, pickup reconciliation, controlled Mover load/rollback, teardown and missing-ID guards compiled in finalgreenbuild; asset setup/runtimevalidation pending |
| 8 — art/final acceptance | Pending |

No PIE/editor gameplay tests or automated tests authorized. Root coordinates editor work and full build/restart boundaries; helper agents work only on assigned source files. See [implementation plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/quest-map-implementation-plan.md) and [coordination contract](C:/UnrealEngine/Games/AZ/docs/design-briefs/quest-map-runtime-contract.md).

## Build evidence and scope

First root full build found only three `auto*` deductions from UE5.8 `GetPawn()`'s TObjectPtr return in the coordinator after the other user-build errors were fixed. Root changed these to explicit APawn pointers. The next full AZEditor build returned Result:Succeeded,11.93seconds. After bounded teardown/missing-pickup-ID cleanup, the **final full build succeeded in28.24seconds**. Teardown/possession cancels staging and releases locks without old-world reconciliation; missing-ID authored pickup removals latch a save refusal, and runtime origins are marked before BeginPlay to avoid false missing-ID tombstones. These are compiled source results, not gameplay validation.

Other fixed compiler issues: UWidget-inherited Navigation member shadowing in the two new widgets (renamed QuestNavigation), TSubclassOf conditional ambiguity, inherited-name shadowing locals. A separate existing throwable compiler error was fixed narrowly: preprocessor branches were moved outside UE_LOG's argument list; the nearest-pawn diagnostic behavior remains unchanged. Pre-fix file backup: Saved/QuestMapImplementation/UnrelatedCompileFix/AZ_ThrowableProjectile.cpp.before-macro-fix.txt. Other grenade/character/config working changes remain owned by their prior work.

Save support is explicitly for the current single-protagonist/same-loaded-map checkpoint, with inventory, equipped/quickbar selections, supported attributes, quest progress/tracking, waypoint, authored/dynamic pickup records and world facts. It does not claim full arbitrary AI simulation or cross-map/death-respawn loading. Cancellation uses a token-gated synchronous Mover teleport effect; an asynchronous Mover backend fails safely and requires its own acknowledged application path. Actual hero backend still needs live verification after restart; never label checkpoint gameplay verified based on compilation.

Before any save test, assign stable CampaignPickupId values to placed pickup components in editor and validate duplicates. Existing authored pickups have not yet received the new IDs. No filled quest Data Assets, active MapPageClass, actual map texture or sample quest/checkpoint actors have been installed yet. The original compass preview actors are a separate already-saved demonstration.
