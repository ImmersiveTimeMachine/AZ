---
name: reference-rider-mcp-new-tools
description: "Rider MCP update (~2026-08-21) added tool families worth preferring over old habits: lint C++ without compiling, editor/viewport/asset screenshots, solution-wide refactorings for the migration phase, call-hierarchy for seam audits, native debugger attach, UE asset search/property tools."
metadata: 
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-27T22:20:32.721Z
---

Rider's MCP server gained large tool families (discovered 2026-08-21). Prefer these where they beat our
older habits:

- **`get_file_problems` / `lint_files`** — Rider code analysis on edited C++ BEFORE spending a
  Live Coding compile. Verified working on `AZ_CmcAnimInstance.cpp`. Cheap pre-compile gate.
- **`take_screenshot`** — kind = `editor_window` | `viewport` | `asset_preview` (PNG to disk). Visual
  anim debugging without asking the user to describe what they see.
- **`search_assets`** — by name/baseClass/packagePath; `source=editor` uses live AssetRegistry.
- **`analyze_calls`** — INCOMING/OUTGOING call hierarchy. Made for the concrete-cast seam audits
  ([[project_cmc_backport_spike]] doctrine rule 3) and dead-code checks.
- **Refactoring suite** — `rename_refactoring`, `safe_delete`, `change_api_signature`,
  `extract_method/base_class/interface`, all with `preview=true`, solution-wide and language-aware.
  This is the tooling for the post-verdict migration (renaming `Cmc*`, deleting v1/v2, interface seams).
- **Debugger** — `attach_to_process` / `xdebug_*` (breakpoints, stacks, frame values, mixed-mode,
  memory dumps) can attach natively to the running UnrealEditor.
- **`build_solution_start`/`_state`** — Rider-driven build; hot-reloads via LC when the editor is
  connected, UBT otherwise. Alternative to our Build.bat habit when the editor is open.
- **dotTrace suite** — profiling snapshots + `dotTraceOpenReport` (strict JSON schema; read the
  `dottrace-analyze` skill section before building a payload).
- UE-specific (older but same family): `get_asset_properties`, `find_default_value_overrides`,
  `search_tags`, `ue_export/import_blueprint_nodes`, `ue_play`, `ue_get_logs`.

## ★ Verified dead-code recipe (2026-08-27) — use this, not ripgrep

Rider is **Blueprint-aware**; ripgrep is not. A reflected symbol with zero C++ hits may still be bound in an
ABP graph, a chooser column, or a BP CDO.

1. `get_file_problems(header, errorsOnly=false)` → `WARNING: Function 'X' is never used in Blueprint or
   C++ code`. This is the authoritative dead-**function** list. Verified across all 5 CMC headers.
2. `safe_delete(filePath, symbolName, preview=true)` → `{ok, applied:false, deleted:[…], conflicts:[…]}`.
   Runs the full conflict analysis and **refuses instead of deleting** when usages exist. Pass the **bare
   member name** (`GetLocomotionChooser`); `Type.Member` raises "not found".
3. `find_default_value_overrides(className, fieldName)` → which BP CDOs override a UPROPERTY.
   Needed because Rider deliberately treats reflected FIELDS as always-live, so step 1 never flags them.

For content assets, the live AssetRegistry is authoritative — `get_referencers` via `ue_execute_python`,
re-checked immediately before any delete.

### ★★ TRAP: `safe_delete` returns `ok:true` but does NOT reach disk when the file is open in a tab
Measured 2026-08-27: 14 `safe_delete(preview=false)` calls all returned `{ok:true, conflicts:[]}`; only the
**3** targeting a file with no open editor tab actually changed the file. The other 11 were applied to
Rider's in-memory buffer and never flushed — on-disk content and mtimes unchanged, `git diff` empty. The
tell: re-running `safe_delete preview=true` afterwards answers **"Symbol … not found"** (Rider's model has
already deleted it) while the disk file still contains the code. There is no save/flush tool in the MCP
surface, and `get_file_problems` / `git_status` do not force one.
**Rule: after any `safe_delete`, verify ON DISK (ripgrep / `git diff`), never on the tool's `ok`.** To
resolve, have the user Save All in Rider — first probe the buffer with `mcp__rider__read_file` (it reads
the BUFFER, not disk) to confirm it holds the intended state before asking them to save. Closing the tabs
first makes `safe_delete` take the direct-to-disk path.
Related: [[feedback_parallel_editor_edits]] (open tabs re-save stale state).

Gone: `mcp__rider__skill_search` (removed server-side; do not look for it).
Related: skill `unrealclaude-mcp-tools` (the other MCP server; port 3000).
