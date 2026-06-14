---
name: feedback_parallel_editor_edits
description: "When the user is hands-on in the editor, scripted asset edits race their edits and saves — verify ground truth via file mtimes + dirty-package list before bulk ops; an open asset editor can re-save stale state over a scripted save; clip saves during PIE invalidate the PoseSearch index and contaminate what's being judged on screen."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Scripted asset edits vs a hands-on user: coordinate, verify on disk

During the 2026-06-06/07 jump sessions the user and I edited the same assets in parallel, which produced four distinct failure modes. All are detectable cheaply.

**Why:** the editor process holds ONE shared loaded copy of each asset. My Python edits, the user's hand edits, and either side's saves all hit the same objects — last save wins, and an asset editor TAB holds its own unsaved view that can overwrite a scripted save made seconds earlier.

**How to apply:**
1. **Before any bulk asset pass, check who's been editing:** `Get-ChildItem ... | Sort LastWriteTime` on the target folder + `EditorLoadingAndSavingUtils.get_dirty_content_packages()`. A file saved minutes ago that I didn't save = the user is in it — ASK before overwriting (concrete hit: clips re-saved one-by-one 22:47→23:42 were the user hand-editing notifies while I batch-authored the same notifies).
2. **An open asset editor can revert a scripted save:** my PSD entry-removal was raced by the user's open PSD editor saving its stale 7-entry view 13s later. If `LogAssetEditorSubsystem: Opening Asset editor for <asset>` appears near the work, hand that asset's edits to the user or wait until the tab is closed.
3. **Saves can fail silently mid-session:** `save_loaded_asset` returning False with no obvious cause (async PoseSearch index build in flight, modal, etc.) — always check the return AND re-verify after `reload_packages`.
4. **Don't edit clips while PIE-judging anims:** EVERY `Modify()` on a sequence PreCancels the PoseSearch index build ("PreCancelled because of <clip>" in LogPoseSearch); during each rebuild window ALL MotionMatch searches return null → frame-0 fallbacks → the on-screen behavior being judged is contaminated. Finish edits, let the index build, THEN test.
5. File mtimes are the arbiter of "did the manual step happen": e.g. CHT saved 23:07 (after my audit) = user's row deletion landed; PSD still at 22:59 = their entry deletion did NOT.

See [[project_jump_system_status]], [[feedback_chooser_column_reorder]].
