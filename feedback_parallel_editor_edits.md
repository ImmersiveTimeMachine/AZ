---
name: feedback_parallel_editor_edits
description: "★★★ USER RULE: this workspace is shared with OTHER AGENTS working in parallel — re-check every file for modification immediately before reading, proposing, or editing it; a file changed since your last read is stale and any proposal built on it is wrong. Plus: scripted asset edits race the user's hands-on editor edits — verify ground truth via mtimes + dirty-package list before bulk ops; an open asset editor re-saves stale state over a scripted save; clip saves during PIE invalidate the PoseSearch index."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
  modified: 2026-09-07T20:55:10.407Z
---

# Parallel writers: other agents AND the hands-on user

## ★★★ RULE 0 — Other agents are editing this workspace right now (user, 2026-09-07)

> *"one more rule we work in parallel with diff agents so before propose anything recheck the code for
> modification if you see the file was modified"*

**Before reading, proposing, or editing anything: re-check the file for modification.** A file that changed
since your last read makes every claim built on it stale — line numbers, symbol names, "this function does
X", the lot. Re-read before you speak, not after you are contradicted.

**Why:** more than one agent works this repo simultaneously. On 2026-09-07 the rifle/aim workstream held
~200 uncommitted lines inside `AZ_MoverAnimInstance.cpp` while I was restructuring the same function. Two
concrete hits in one session:
- I quoted a `[WeaponAnim]` log line from a diff taken minutes earlier; by the time I read the file again
  it had lost a line. The quote was already wrong.
- `git diff --stat` on the same file moved 196/21 → 171/20 between two of my own tool calls.

**How to apply:**
1. **Cheap check, every time:** `stat -c '%y %n' <file>` and `git diff --stat -- <file>`. Compare against
   when you last read it. `find Source Content -newermt '-10 minutes'` shows who else is live.
2. **Anchor edits on CONTENT, never on line numbers** carried over from an earlier read. Assert the anchor
   is unique and that the surrounding text still matches before writing.
3. **Assert-before-write:** capture `os.path.getmtime` before building an edit and re-assert it right
   before writing; abort if it moved.
4. **Do not restructure a file another agent has open work in.** Whole-function moves collide with every
   hunk they hold. Small, region-local edits in lines nobody else has touched are fine — verify the region
   is clear first (their hunk boundaries, not just the file).
5. **Committing from a shared file:** stage *only* your change. Git will pair your deletions with their
   nearby additions into one hunk, so hunk-splitting is not always enough — the reliable technique is to
   build the index content as `HEAD + your transformation`, assert the region you touched is byte-identical
   to HEAD's, then `git hash-object -w` + `git update-index --cacheinfo`. Working tree keeps both. Used
   successfully for `f2e7d55` and `17c59af`.
6. **Say so in the commit message** when a file carries unrelated in-flight work, and state that only your
   hunks are staged.

## Scripted asset edits vs a hands-on user

During the 2026-06-06/07 jump sessions the user and I edited the same assets in parallel, which produced four distinct failure modes. All are detectable cheaply.

**Why:** the editor process holds ONE shared loaded copy of each asset. My Python edits, the user's hand edits, and either side's saves all hit the same objects — last save wins, and an asset editor TAB holds its own unsaved view that can overwrite a scripted save made seconds earlier.

**How to apply:**
1. **Before any bulk asset pass, check who's been editing:** `Get-ChildItem ... | Sort LastWriteTime` on the target folder + `EditorLoadingAndSavingUtils.get_dirty_content_packages()`. A file saved minutes ago that I didn't save = the user is in it — ASK before overwriting (concrete hit: clips re-saved one-by-one 22:47→23:42 were the user hand-editing notifies while I batch-authored the same notifies).
2. **An open asset editor can revert a scripted save:** my PSD entry-removal was raced by the user's open PSD editor saving its stale 7-entry view 13s later. If `LogAssetEditorSubsystem: Opening Asset editor for <asset>` appears near the work, hand that asset's edits to the user or wait until the tab is closed.
3. **Saves can fail silently mid-session:** `save_loaded_asset` returning False with no obvious cause (async PoseSearch index build in flight, modal, etc.) — always check the return AND re-verify after `reload_packages`.
4. **Don't edit clips while PIE-judging anims:** EVERY `Modify()` on a sequence PreCancels the PoseSearch index build ("PreCancelled because of <clip>" in LogPoseSearch); during each rebuild window ALL MotionMatch searches return null → frame-0 fallbacks → the on-screen behavior being judged is contaminated. Finish edits, let the index build, THEN test.
5. File mtimes are the arbiter of "did the manual step happen": e.g. CHT saved 23:07 (after my audit) = user's row deletion landed; PSD still at 22:59 = their entry deletion did NOT.

See [[project_jump_system_status]], [[feedback_chooser_column_reorder]], [[feedback_verify_never_presume]].
