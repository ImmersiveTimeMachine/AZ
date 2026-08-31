---
name: feedback_log_reading_traps
description: "★ Four log/asset-reading traps that produced WRONG claims in one evening: `Cmd: Slomo` (7-second jumps, 1/9 play rate — grep it first), T3D export omits WITH_EDITORONLY_DATA props (absent ≠ default — read the property directly), FLT_MAX cost = no index (not a bad match), and GIT_INDEX_FILE left set makes `git status` report every file deleted."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T01:00:27.520Z
---

# Log- and asset-reading traps (2026-08-31)

Each of these produced a confident wrong statement to the user before being caught.

1. **`Cmd: Slomo 0.1` in the log.** Every timing looked 10× slow: "7-second jumps", "land clip plays
   at 1/9 speed", a play-rate hypothesis half-built. The user had slow motion on to watch the artifact.
   **Before reading ANY timing off a log, `grep -i "slomo\|TimeDilation"` on that PIE's window.**
   Frame counters (`[NNN]` in the timestamp) are the honest clock: frames per event, not seconds.
2. **T3D export omits `WITH_EDITORONLY_DATA` properties regardless of value.** I read
   "`ExcludeFromDatabaseParameters` absent from the export" as "at the engine default (0,-0.3)" and
   announced a smoking gun on the Mover databases. Direct `get_editor_property` read: they were
   (0,0) all along. (The same direct read then showed the CMC databases genuinely ARE at (0,-0.3) —
   38 of them — so the mechanism was real, just on the other branch.) **Absent from an export ≠
   default. Read the property.**
3. **`cost=+340282346638528859811704183484516925440.00` is FLT_MAX = "no candidate", not "bad match".**
   It appears while a database's derived data is rebuilding (`LogPoseSearch … PreCancelled because of
   …`) or when every pose is non-selectable. A run showing it proves nothing about selection quality.
4. **`export GIT_INDEX_FILE=…` in a shell, then `rm` the temp index, then `git status` in the SAME
   shell** → every tracked file reported `D` (deleted). Nothing was wrong; the variable was still set.
   Verify in a fresh shell before reacting. (The plumbing pattern itself — `read-tree` → `hash-object`
   → `update-index --cacheinfo` → `write-tree` → `commit-tree` → `update-ref` — is the right way to
   commit to ANOTHER branch while the editor holds the working tree; `.gitattributes` has
   `*.uasset !filter`, so uassets are plain blobs and hashing them directly is safe.)

Also earned tonight, same family: **never `git checkout` while the editor is open** — check
`Get-Process UnrealEditor` first, every time, not once. Two checkouts under an open editor left the
working tree a mix of two branches and needed a `checkout -f` to recover.

Related: [[feedback_verify_never_presume]], [[feedback_stop_the_patch_loop]], [[feedback_mover_spine_search_continuity]].
