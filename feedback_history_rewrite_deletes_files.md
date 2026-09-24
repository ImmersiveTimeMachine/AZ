---
name: feedback-history-rewrite-deletes-files
description: "★★★ Stripping files from git history (filter-branch/filter-repo) DELETES them from the working tree — on 2026-09-22 it wiped the hero face mesh, all of UI Design/ and a Fab plugin (731 files); hero rendered as a transparent second head. Restore recipe + prevention."
metadata:
  node_type: memory
  type: feedback
  originSessionId: aeb43ab9-4e73-47d4-94b2-81b8ebc7e151
  modified: 2026-09-23T01:08:45.341Z
---

On 2026-09-22 a push hit GitHub's size limits, and a `filter-branch` rewrite stripped oversized paths from the
pending commits (backup branch `backup/pre-bigfile-strip-2026-09-22`, then `.gitignore` in 460ed2e). When
HEAD moved to the rewritten commits, git **removed every stripped file from disk**, because they were
tracked in the old HEAD and absent from the new one. Ignoring them afterwards does not bring them back.

**Lost (731 files):** `Content/AZ/Blueprints/Character/AZ_MHC_Hero/Face/SKM_MHC_Hero_FaceMesh.uasset`, the two
`CHALKTeacher/Lookdev/*_FacePreview` meshes, all of `UI Design/`, and `Plugins/Marketplace/MetaHumanBodyTracker`
(Source/Content only; its untracked Binaries survived).
**Symptom (Artur, 2026-09-23):** after an editor restart the hero showed "two heads, one transparent" — the
face mesh was gone, and the grooms (hair, brows, peach fuzz) rendered without it as a ghostly head shell.
The hero BPs still held the reference; only the file was missing. After the restore + an editor restart
Artur confirmed the face is back (2026-09-23) — no BP repair was needed.

**Restore recipe (worked, byte-identical by `git hash-object`):** compute paths present in the backup
branch's tree but not in HEAD's and missing on disk, write them to a list file, then
`GIT_LITERAL_PATHSPECS=1 git restore --source=<backup> --worktree --pathspec-from-file=<list>` — worktree
only, nothing staged, and the paths stay ignored.

**Enforced (Artur 2026-09-23: "не должно повторяться никогда"):**
- Claude Code PreToolUse hook `.claude/hooks/guard_git_destructive.py` (wired in `.claude/settings.local.json`,
  matcher `Bash|PowerShell`) DENIES `git filter-branch`, `git filter-repo`, BFG and `git clean` (dry-run `-n`
  allowed). Verified firing in both shells under bypassPermissions. Never work around it.
- `AGENTS.md` carries the same rule for every agent, including Codex, which the hook does not cover.
- Not detectable by the hook, so rule-only: a rebase/reset that drops commits which ADDED files deletes
  them from disk the same way.

**How to apply:**
- Before ANY history rewrite that drops paths: copy those paths aside first, or `git rm --cached` them in
  a normal commit so they become untracked-and-ignored BEFORE the rewrite. Verify they still exist on
  disk after the rewrite.
- Never delete the `backup/pre-bigfile-strip-2026-09-22` branch while those files have no other copy — it
  is currently the ONLY copy of the face mesh, UI Design sources and the plugin sources outside the disk.
- An editor that started without the file keeps a null reference in memory: restart it before saving any
  hero BP, or the null gets written into the asset.

Related: [[feedback_metahuman_modular_hero]], [[project_protagonist_face_design]], [[feedback_verify_never_presume]].
