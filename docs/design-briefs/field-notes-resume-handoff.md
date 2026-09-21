# Field Notes — paused checkpoint, 21 September 2026

**Superseded by [the current integration status](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-current-status.md).** The source batch has been built and the asset recipes have now been executed, compiled, saved and verified. Do not replay the preparation/application or authoring commands below. Remaining work is user-run acceptance and the next gamepad binding task. Startup-menu work is design only.

## Current resume state

**BUILD SUCCEEDED.** User closed Unreal; root confirmed no editor process and built AZEditor. First attempt exposed pointer deduction, ticker-handle and inherited Slot-name errors; second exposed two direct module dependencies. Root corrected only menu component/widget code and AZ.Build.cs (ApplicationCore, DeveloperSettings). Third attempt succeeded: 242 actions, 74.95 seconds; DLL linked and all12checked class/function exports present. [Build receipt](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/build-receipt.json) contains31finalsourcehashes and DLL hash; [compiler fixes](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/CompileFixes/compiler-fixes.patch) preserve the delta from initial applied code. No editor launch, PIE or tests. Next: Artur opens Unreal; verify reflected APIs/config and Play Idle, then execute the asset integration sequence below. Do not rerun source prepare/apply or rebuild without a new reason. Earlier build-pending statements are historical.

**LATEST: the 30-file native batch is now APPLIED to live Source and every resulting file hash matches the staged version.** Receipt: [source-applied-receipt.json](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/source-applied-receipt.json). Do not rerun prepare/apply. The user had successfully rebuilt/reopened the previous source (UBT reported up to date); root verified 20 original files and 10 absent new files, explained the mismatch and applied the already-authorized batch without editing live assets or compiling. Next: user closes Unreal, root performs normal AZEditor build and fixes any errors, then user reopens. The build of this new batch has NOT run. Older unapplied statements below are historical.

Artur has returned and requested continuation from this checkpoint. **The user pause is lifted.** Root reread this checkpoint, verified all proposal/live-source hashes, regenerated the unchanged 30-file batch and passed patch applicability/whitespace checks. It remains NOT applied and NOT compiled. The freshly connected editor process is 33380; Play is Idle and dirty content/maps are both empty. The immediate next step requires Artur to close Unreal for the normal build. He opens it himself after success. No scheduled reminder or automatic continuation was created.

All three Field Notes helpers completed. No build is running. No editor asset writes or live native source application occurred during the resume checks. The stopping-point details below are retained; the old process 52888 and earlier pause are historical.

## Exact stopping point

The Field Notes visual family is selected and its full implementation is authorized. Previously completed inventory, map/journal, HUD, Quick Select and compass styling is saved. The next **combined native batch is prepared but NOT applied or compiled**:

- 30 source files: 10 new, 20 modified.
- `source_applied: false`; `build_result: Not run`.
- Baseline/source hashes checked; overlapping journal/map and controller changes merged in staging.
- Combined patch passed `git apply --check --whitespace=error-all`.
- Independent source reviews completed; runtime acceptance remains pending.

Canonical batch: [manifest](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/manifest.json), [patch](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/combined-native.patch), [review checkpoint](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/CombinedNative/review-checkpoint.json).

The last completed live native build was the inventory style metadata fix: three style properties in two headers changed to EditAnywhere; 17 build actions succeeded. Do not confuse that earlier successful build with this unbuilt 30-file batch.

## Ready in staging

1. Journal category/status presentation, preserving quest progress, tracking and saves.
2. Quick Select and Map commands, scoped input ownership, balanced press/release routing, parent-key conflict handling and adaptive prompt refresh.
3. Shared action prompts and per-local-player keyboard/Xbox/PS4/PS5 appearance, with separate UI/audio preferences.
4. Pause, settings, title, checkpoint loading and confirmations; actual graphics settings and Master audio, display rollback and input capture ownership.
5. User's new inventory navigation request: Q/E and clickable controls beside the category row; logical Equippables → Consumables → Craftables → Map order, reverse and wrap. Actual controller mappings are left/right triggers. Navigation uses the pending destination during fades; activation/focus follows visible arrival, including a closed-parent guard.
6. Adaptive world interaction hints: real Input.Action.Interact lookup, key-free caption fields, preserved magazine names/live rounds/capacity and explicit legacy-caption migration. That gameplay action currently has E and no gamepad mapping. Unbound device input uses readable full-width text; no invented binding.

All source proposals are frozen. Their manifests are loaded by [the batch preparation script](C:/UnrealEngine/Games/AZ/Tools/field_notes_native_batch.py). Keep original source/user changes: if a baseline hash has changed, review and rebase affected hunks; never overwrite unrelated edits with staged whole files.

## Saved asset progress

- All 23 inventory first-pass assets styled; three linear vitals, corrected column/tab/skills layout and existing 11×7 grid with TileSize50 preserved.
- Map/journal styling and expandable selected-task details compiled/saved. Existing calibration and quest bindings preserved.
- HUD/Quick/compass styles, transparent Quick frames, equipped notch and category marker branch compiled/saved.
- 19 shared style/font resources and 10 production textures saved, including owned PS5 Create glyph.
- Owned PS4/PS5 controller data and configuration saved; active configuration cache still needs verification after restart.
- Fifteen UI actions, two Quick/Map contexts and generic CommonUI metadata saved. Owned Click/Back defaults assigned. Map Enhanced Input actions are non-consuming so parent mappings remain discoverable.
- Shared prompt templates/hosts, new inventory keycaps, journal native opt-in, menu-route assets and world prompt/caption migration are **not yet executed**; they depend on the new native build.

Receipts are under [Saved/FieldNotesImplementation](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation). Do not rerun already completed first-pass styling/recovery scripts against the structurally changed widget trees.

## First steps after Artur resumes

1. Read this checkpoint and [the progress ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-implementation-progress.md). Check actual editor/process state. Artur closes and reopens Unreal himself; do not launch it automatically.
2. Confirm the existing combined manifest still says not applied. Revalidate staged/proposal/live source hashes. If unchanged, prepare/check the combined patch and apply it only at the closed-editor build boundary.
3. Run the normal AZEditor build, repair compiler errors within the authorized patch, and read the real UnrealBuildTool log. Record the actual successful result and source hashes. No automated tests or PIE.

```powershell
python 'C:\UnrealEngine\Games\AZ\Tools\field_notes_native_batch.py' prepare
git apply --check --whitespace=error-all 'C:\UnrealEngine\Games\AZ\Saved\FieldNotesImplementation\CombinedNative\combined-native.patch'
python 'C:\UnrealEngine\Games\AZ\Tools\field_notes_native_batch.py' apply
& 'C:\UnrealEngine\Engine\Build\BatchFiles\Build.bat' AZEditor Win64 Development '-Project=C:\UnrealEngine\Games\AZ\AZ.uproject' -WaitMutex -FromMsBuild
```

Real compiler log: [UnrealBuildTool Log.txt](C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt). These commands are recorded instructions, not commands executed at pause.

4. After a successful build, tell Artur; he launches Unreal. Reconnect, verify new reflected classes/functions and active input settings, and confirm Play is stopped before asset writes.
5. Finish asset integration in bounded steps, native-compile externally and explicitly save each group. Root remains the sole editor writer. Final runtime testing is only after all integration is finished, as Artur requested.

## Asset integration order and recipes

1. [Prompt hosts v3](C:/UnrealEngine/Games/AZ/Tools/field_notes_prompt_hosts.py), with [execution recipe](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/Module7/PromptHosts/recipe.md): `preflight`; `prepare_prompt('Paper'/'Overlay')` returns an external native Blueprint creation request; run it outside Python, then `author_prompt`. Compile/save each template before hosting it. Canonical names end in `_Paper` and `_Overlay`; no bare ActionPrompt asset.
2. [Inventory navigation](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/InventoryNavigationNativeProposal/author_inventory_navigation.py): capture with native build confirmed, author six new widgets; compile/save Switcher then Menu. Existing category buttons/grid order/bindings are retained.
3. Prompt host tree/binding/configuration stages for Quick, both cards, Map and Inventory. Inventory footer gets Back only; Q/E belongs beside the tabs. Map shows section navigation using the actual parent actions. Append the shared gamepad Quick toggle only through the guarded recipe after checking real key conflicts.
4. [Journal presentation](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/JournalNativeProposal/author_journal_assets.py): use a fresh post-build capture, author category/state glyphs, compile/save Entry then Page. Preserve already saved expandable details.
5. [Menu routes](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/MenuRoutesNativeProposal/author_menu_assets.py): preflight, prepare inputs, native-create page outside Python, configure, compile/save page and controller. Title-on-startup remains off to preserve direct editor entry. Continue uses the existing current-map campaign load service.
6. [World prompt recipe](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/WorldInteractionPromptNativeProposal/author_world_prompts.py), [README](C:/UnrealEngine/Games/AZ/Saved/FieldNotesImplementation/WorldInteractionPromptNativeProposal/README.md): capture/author the one real Overlay HUD prompt, compile/save/verify. Audit captions in bounded owned-asset batches; migrate exact known quest fixtures and individually reviewed custom item/actor captions. Preserve old strings and unrelated values. Loaded instance overrides and unloaded maps must not be silently claimed covered.
7. Inspect editor previews, correct concrete layout defects, save/read back and then request the final user-run acceptance pass. Check logs afterward. Do not start Play independently.

## Important implementation findings

- Plain engine FlushPressedKeys clears its own held-key ignore flags through simulated releases. The staged menu uses a transient public-API mapping barrier; no engine edit or input injection. Do not simplify back to the ineffective flush.
- Quick/Map input fixes and navigation fade/focus fixes are already included. Preserve them when resolving any new source drift.
- CreateWidgetBlueprint internally compiles. Creation and compilation must be external native tool calls, never nested inside Python. The corrected prompt recipe validates the current default root before creation.
- CommonTextBlock derives font/color/etc. from its style. Keep the owned Caption_Story style instead of fighting it with transient tint overrides.
- Designer previews ignore runtime Collapsed in some scaffolding cases and contain no live inventory/map data. Do not misdiagnose those previews as gameplay failures.
- Explicit TextureFactory was used for production imports. Avoid replaying old failed generic-import/recovery steps.
- No engine files, source-pack assets, gameplay authority, inventory transactions or campaign save format are part of this style/input patch.

## Reference documents

- [Implementation plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-implementation-plan.md)
- [Style contract](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-style-contract.md)
- [Execution ledger](C:/UnrealEngine/Games/AZ/docs/design-briefs/field-notes-implementation-progress.md)
- [Project UI memory](C:/UnrealEngine/Games/AZ/docs/ai-memory/project_unified_ui_design.md)

Full Field Notes implementation is still in progress. This document records the saved stopping point and subsequent resumption, not completed runtime integration.
