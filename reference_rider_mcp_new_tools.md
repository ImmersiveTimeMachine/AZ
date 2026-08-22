---
name: reference-rider-mcp-new-tools
description: "Rider MCP update (~2026-08-21) added tool families worth preferring over old habits: lint C++ without compiling, editor/viewport/asset screenshots, solution-wide refactorings for the migration phase, call-hierarchy for seam audits, native debugger attach, UE asset search/property tools."
metadata: 
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-21T21:07:54.995Z
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

Gone: `mcp__rider__skill_search` (removed server-side; do not look for it).
Related: skill `unrealclaude-mcp-tools` (the other MCP server; port 3000).
