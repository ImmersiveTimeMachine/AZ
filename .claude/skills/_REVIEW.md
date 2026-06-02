# AZ Skills — Strict Review (2026-05-03)

Read-only audit of the 9 skills against `_DESIGN.md`, the live memory inventory at
`C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\`, the trimmed
`MEMORY.md`, the live filesystem, and `.mcp.json`. Brutal pass — every defect
that would affect cold-start cost, trigger correctness, or accuracy is called
out. No skill files were modified.

---

## Verdict per skill

### 1. `agent-and-research-discipline` — **PASS** (one nit)

- 63 lines. Compact, well-structured "5 rules in order".
- All 5 referenced memory files exist and the summaries faithfully match the
  source bodies (verified `feedback_check_memory_first.md`,
  `feedback_use_agent_teams.md`, `feedback_validate_agent_findings.md`,
  `feedback_no_read_approval.md`, `feedback_file_paths.md`).
- Trigger surface (description) reads well as a sticky baseline rule but is
  unlikely to *autoload* on most prompts — its description doesn't include
  any user-typed phrases (e.g. "research", "explore", "spawn agents"). That's
  borderline OK because every other skill cross-references it. **Nit:** add a
  trigger phrase like *"plan research", "spawn explore agents", "before
  starting research"* into the `description` so the matcher fires when it
  should.
- Section "When to save NEW memory" is useful but technically scope-creep vs
  the design outline — kept because it's tightly relevant.

### 2. `cpp-build-livecoding` — **PASS** (two nits)

- 74 lines. Tight, ordered, concrete commands.
- All paths absolute. The CLI invocation matches the engine's `Build.bat`
  layout (`C:/UnrealEngine/Engine/Build/BatchFiles/Build.bat`). Log paths match
  the engine's actual `C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt`
  (file confirmed to exist).
- Step 2 says "load via `ToolSearch query=\"select:mcp__unrealclaude__unreal_execute_script\"`" — correct.
- **Nit 1 — script_type quoting drift.** Body uses both
  `script_type: console` (with colon, no quote) and elsewhere
  `script_type=console` (equals). Pick one — actual MCP call format is
  the JSON parameter shape (`"script_type": "console"`).
- **Nit 2 — overlap with `unrealclaude-mcp-tools`.** Step 2 redirects users to
  load `execute_script` deferred tool; `unrealclaude-mcp-tools` covers the same
  loading. Acceptable per design § Open question 5 (kept separate); if both
  trigger together you'll get duplicate loading instructions. See
  *Cross-cutting issues* below.

### 3. `bp-to-cpp-port` — **PASS**

- 113 lines. Faithfully condenses the 98-line `feedback_bp_to_cpp_port_review_checklist.md`
  into the 7-gate format with the past-misses summary. Cross-checked Gates
  1-7 verbatim against the source — they match.
- All 6 cross-referenced memory files exist
  (`feedback_bp_to_cpp_port_review_checklist.md`, `gasp_pawn_cpp_port_plan.md`,
  `project_gasp_pawn_port_audit_2026-05-02.md`, `project_gasp_abp_port_ledger.md`,
  `project_gasp_animbp_cpp_port_plan.md`).
- The "ASSET REGISTRY lag" pitfall is novel content (not in the source
  feedback) — fine, it's a real gotcha, but it's also stated in
  `unrealclaude-mcp-tools`. **Duplication.**
- Description has clear trigger phrases — good.

### 4. `unrealclaude-mcp-tools` — **NEEDS FIXES** (2 issues)

- 81 lines. Decision-tree table is sharp. Standing rule, gotcha list,
  patch list all check out.
- **Issue 1 — `gassample` MCP is described as "deprecated" but is still
  active in `C:\UnrealEngine\Games\AZ\.mcp.json`** (lines 12-20, port 3001). The
  skill, the design doc, and the trimmed `az-workspace` all say "deprecated /
  ignore". If the user actually wants it removed, the JSON should be edited;
  if they want it left registered, the skills should soften "deprecated" to
  "do not use for GASP queries — query AZ's `/Game/Blueprints/` instead".
  As written, a future Claude reading this skill will be confused why the
  deprecated server is still wired.
- **Issue 2 — line 35 says "Full direct-tool list ... lives in `az-workspace`
  § 'unrealclaude MCP tools'"** but the trimmed `az-workspace/SKILL.md` no
  longer has a per-tool catalog (it only points back to this skill). Broken
  cross-reference. Either restore a brief catalog in `az-workspace` or
  inline the catalog here.
- Trigger description is good; matches "MCP", "blueprint_query",
  "execute_script" naturally.

### 5. `az-cpp-utility-tools` — **PASS** (one nit + one suggestion)

- 109 lines. Per-utility table, pin-name cheat sheet, key rules, transition
  rule patterns, BlendStack chain, Python recipe template, proven outputs.
- Cross-checked function signatures against the actual headers at
  `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_*.h` (all 6 exist:
  `AZ_BlueprintNodeUtils.h`, `AZ_AnimGraphNodeUtils.h`, `AZ_AnimBlueprintUtils.h`,
  `AZ_ChooserUtils.h`, `AZ_PoseSearchUtils.h`, `AZ_SkeletonUtils.h`). Names
  match.
- "Proven outputs" section claims `SetBlendStackAnimFromChooser` (53 nodes),
  `Get_DynamicPlayRate` (42), `IsAnimationAlmostComplete` (9). These match
  `reference_bp_node_tools.md:79-82` exactly. Good.
- **Nit — Live Coding rule appears in 3 skills** (here at rule 10, in
  `cpp-build-livecoding`, in `bp-to-cpp-port`). Keep this one as it's
  the right place for "adding new UFUNCTION" specifically; trim from
  `bp-to-cpp-port`.
- **Suggestion** — the description names utilities but no pin-name terms.
  A user typing "Two Bone IK pin" or "FunctionEntry pin direction" might not
  trigger this skill. Add `K2Node`, `pin direction`, `EnsureFunctionPins`
  as trigger phrases.

### 6. `anim-debug-pitfalls` — **PASS** (one minor accuracy concern)

- 198 lines — **the largest skill** and the one most worth its size when
  triggered. Five well-organized "symptom → check → fix → memory file"
  sections matching the design outline.
- Cross-checked content against source memory files: post-event vs
  thread-safe section is verbatim-faithful to
  `feedback_animbp_post_event_vs_thread_safe.md`; FAnimNodeReference
  section faithful to `feedback_animgraph_node_reference_wiring.md`;
  the `IsMoving` formula (`Trj_FutureVelocity != 0 (tol 10) AND
  Acceleration != 0`) matches `gasp_update_logic_flow.md:13-19`.
- **Concern — OffsetRootBone enum mapping** says `Release = 5` and
  states "Other values exist: BlendOut, LockOffsetAndConsumeAnimation —
  verify the engine header before assuming". Source
  `project_idle_tip_implementation.md` says enum values were `Accumulate=0,
  Interpolate=1, …, Release=5` and the AZ fix uses `Release=5` for
  Idle/InAir. The skill captures this but **does not state the engine
  header path** to verify it
  (`C:\UnrealEngine\Engine\Source\Runtime\AnimGraphRuntime\Public\BoneControllers\AnimNode_OffsetRootBone.h`
  or wherever `EOffsetRootBoneMode` actually lives). Without that pointer
  the "verify the engine header" advice is hollow.
- Description is rich with trigger phrases ("mesh spinning", "A-pose",
  "stuck in state", "OffsetRootBone", "TIP not triggering", "foot slide").
  Strong.

### 7. `gasp-parity-reference` — **NEEDS FIXES** (2 issues)

- 112 lines.
- **Issue 1 — `gasp_animbp_full_audit.md` claimed at 1156 lines.** Live
  file is **342 lines** (verified just now via Get-Content). The DESIGN.md
  also states 1156 (Section 2 line 71). One of the dumps shrank or never
  was that large. Caller will see "1156 lines, large" and budget large
  context that isn't needed. Fix the line count or drop the parenthetical.
- **Issue 2 — gassample deprecation claim** (line 19) duplicates the same
  issue as `unrealclaude-mcp-tools`. As long as `.mcp.json` still
  registers `gassample`, calling it "deprecated" is misleading.
- Otherwise this is exactly what the design wanted: pure pointer file with
  no inline content. The "Quick lookup" table at the bottom is great UX.
- Trigger surface is comprehensive (`GASP`, `GameAnimationSample`,
  `parity audit`, `S_BlendStackInputs`, etc.). Strong.

### 8. `asset-modification-via-python` — **PASS** (one accuracy fix needed)

- 109 lines. Recipes for PoseSearch creation, anim notifies, BP/AnimBP
  edits, chooser, IMC, IK retargeter, CDO defaults, save-safety table.
- The "Saving safely" table is excellent — concrete and actionable.
- Cross-checked notify helper names against `UAZ_PoseSearchUtils` per
  `reference_ue5_python_anim_notifies.md` and the design — all 5 helpers
  named correctly (`AddBlockTransitionNotify`, `AddBranchInNotify`,
  `AddExcludeFromDatabaseNotify`, `AddModifyCostNotify`,
  `AddOverrideContinuingPoseCostBiasNotify`).
- **Accuracy fix — bulk add** says "80% coverage, 10% margin at start/end
  across all anims in a DB". Source `reference_ue5_python_anim_notifies.md`
  agrees, but the bulk fn signature only takes the database param
  (no user-tunable numbers). If user later changes the C++ implementation
  the skill drifts silently. Cite the source memory and tell the future
  reader "values hardcoded in C++; check header to confirm".
- **Cross-cutting overlap** — the GC-crash rule appears in this skill, in
  `cpp-build-livecoding`, and in `az-cpp-utility-tools`. **Three
  copies.** Recommend single owner: this skill (`asset-modification-via-python`)
  since it's the trigger surface for Python work; the others should
  point here.

### 9. `az-workspace` (trimmed) — **PASS** (one issue + one nit)

- 113 lines. Folder map + memory map + GASP block + engine map + MCP
  servers table + git facts.
- The trimming-via-cross-reference at the top is well done — the
  "Workflow skills" callout block is the right pattern.
- **Issue — broken back-reference from `unrealclaude-mcp-tools`** (see
  Issue 2 in §4 above). When the MCP-tools skill says "Full direct-tool
  list with one-line purposes lives in `az-workspace` § 'unrealclaude MCP
  tools'", `az-workspace` no longer has that section. Either:
  - (a) restore a 1-line-per-tool catalog in `az-workspace` (~15 rows), OR
  - (b) move the catalog into `unrealclaude-mcp-tools` and delete the
    cross-reference. Choice (a) keeps `az-workspace` as the catalog of
    record (matches "workspace map" framing); choice (b) keeps
    `unrealclaude-mcp-tools` self-sufficient.
- **Nit** — the "Naming Conventions", "Animation System", "Weapon System"
  sections from the original `az-workspace` (per design Section 1) appear to
  have been moved to `MEMORY.md` rather than into a skill. That's fine for
  static state but means `MEMORY.md` is still bearing those tokens
  always-loaded — see Token Budget below.
- Trigger phrasing is spot-on: "start of any non-trivial AZ task".

---

## Cross-cutting issues

### A. `gassample` MCP — actually still registered

- `C:\UnrealEngine\Games\AZ\.mcp.json` lines 12-20 still register the `gassample`
  server on port 3001 with the same Node bridge.
- Three skills (`unrealclaude-mcp-tools`, `gasp-parity-reference`, plus
  the trimmed `az-workspace`) call it "deprecated". That's
  *aspirational*, not factual. Either:
  1. Edit `.mcp.json` to remove the `gassample` entry, OR
  2. Soften the language to "Do not use `gassample` for GASP content
     in AZ — GASP is imported into `/Game/Blueprints/`. Server is
     still registered for legacy reasons; unaffected by removal."

### B. Duplicated content across skills

| Item | Appears in | Should live in |
|---|---|---|
| GASP-imported-into-AZ standing rule | `unrealclaude-mcp-tools`, `gasp-parity-reference`, `az-workspace` | All 3 reasonable (different angles); accept |
| Python GC-crash rule | `cpp-build-livecoding`, `az-cpp-utility-tools`, `asset-modification-via-python` | `asset-modification-via-python` (trigger-side); short pointer in others |
| `inspect`-truncation pitfall | `bp-to-cpp-port`, `unrealclaude-mcp-tools` | Acceptable per design § Open question 4 |
| `get_nodes` truncation pitfall | `bp-to-cpp-port`, `unrealclaude-mcp-tools`, `anim-debug-pitfalls` | Same — OK |
| Live Coding "can't add new UFUNCTION" | `cpp-build-livecoding`, `az-cpp-utility-tools` | `cpp-build-livecoding` (build-time rule); pointer in az-cpp-utility-tools |
| Asset Registry lag pitfall | `bp-to-cpp-port`, `unrealclaude-mcp-tools` | `unrealclaude-mcp-tools` (it's a tool gotcha, not a port-process rule) |
| Plugin-patch list (re-apply after sync) | `cpp-build-livecoding` (full), `unrealclaude-mcp-tools` (full) | `unrealclaude-mcp-tools`; `cpp-build-livecoding` keeps a 1-line pointer |

If both `cpp-build-livecoding` and `unrealclaude-mcp-tools` trigger
together (highly likely on any "compile + MCP" task), the user gets
the GC-crash rule twice and the plugin-patch list twice. Net waste
~30 tokens per double-trigger. Small but accumulates over a long port
session.

### C. Trigger conflict candidates

| User input | Auto-loads |
|---|---|
| "Compile via Live Coding and run a Python notify script" | `cpp-build-livecoding` + `asset-modification-via-python` + likely `unrealclaude-mcp-tools` (if "MCP" mentioned) |
| "Port the next phase of the AnimBP" | `bp-to-cpp-port` + `gasp-parity-reference` + likely `cpp-build-livecoding` |
| "Mesh is spinning" | `anim-debug-pitfalls` only — clean |
| "Help me query GASP for X" | `gasp-parity-reference` + `unrealclaude-mcp-tools` |
| "I need to add a transition rule" | `az-cpp-utility-tools` + `bp-to-cpp-port` |

The 3-way pile-ups (build + python + MCP, or port + GASP + build) are
the worst case. Each skill is ~80-110 lines = 2.4-3.3 k tokens.
3-way ≈ **9 k tokens** added on top of `MEMORY.md` (still cheaper than
the old single-skill `az-workspace` from the design, which had it all
inline).

### D. Reference integrity

All filesystem paths verified:
- All 9 `SKILL.md` files exist at the expected
  `C:\UnrealEngine\Games\AZ\.claude\skills\<name>\SKILL.md` paths.
- All cross-referenced memory files exist (verified via Glob against
  the memory dir).
- All AZ source headers cited in `az-cpp-utility-tools` exist
  (`AZ_BlueprintNodeUtils.h`, `AZ_AnimGraphNodeUtils.h`,
  `AZ_AnimBlueprintUtils.h`, `AZ_ChooserUtils.h`, `AZ_PoseSearchUtils.h`,
  `AZ_SkeletonUtils.h`, `AZ_AnimInstance.h`).
- All GASP `/Game/Blueprints/` BPs cited exist in
  `C:\UnrealEngine\Games\AZ\Content\Blueprints\` (verified
  `SandboxCharacter_Mover.uasset`, `SandboxCharacter_Mover_ABP.uasset`,
  `SandboxCharacter_CMC*.uasset`).
- UnrealClaude plugin patches reference real files
  (`MCPTool_BlueprintQuery.cpp`, `ScriptPermissionDialog.cpp`,
  `Resources/mcp-bridge/tool-router.js` — all exist).
- UnrealBuildTool log path `C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt`
  exists.

**One broken back-reference:** `unrealclaude-mcp-tools` line 35 →
`az-workspace` "unrealclaude MCP tools" section that no longer exists.

### E. Frontmatter validity

All 9 files pass: `name` matches folder, `description` is single-line
YAML, no parser errors. Descriptions are detailed (50-100+ words) — good
for trigger matching, but some run long. None violates max-length.

### F. Accuracy drift vs sources

Spot-checked summaries against source memory:
- `agent-and-research-discipline` rules 1-5: match sources verbatim.
- `cpp-build-livecoding` GC-crash bullet: matches `feedback_python_gc_crash.md`.
- `bp-to-cpp-port` 7 gates: match `feedback_bp_to_cpp_port_review_checklist.md`
  near-verbatim.
- `anim-debug-pitfalls` post-event rule + FAnimNodeReference rule: match.
- `anim-debug-pitfalls` IsMoving formula: matches `gasp_update_logic_flow.md`.
- `gasp-parity-reference` line counts: `gasp_animbp_full_audit.md`
  **claims 1156, actual 342** — only mismatch found.
- `asset-modification-via-python` notify helpers: match.

Not bad — only one significant drift.

---

## Recommended fixes (priority ordered)

1. **Decide `gassample` policy and align all 3 skills + `.mcp.json`.**
   - File: `C:\UnrealEngine\Games\AZ\.mcp.json` (lines 12-20) and skills
     `unrealclaude-mcp-tools` line 16, `gasp-parity-reference` line 19,
     `az-workspace` lines 102-104.
   - Replace "deprecated" with either: (a) actually remove from
     `.mcp.json`, or (b) "Registered but do not query for GASP — AZ has
     GASP imported at `/Game/Blueprints/`."

2. **Fix `unrealclaude-mcp-tools` → `az-workspace` broken back-reference.**
   - File: `C:\UnrealEngine\Games\AZ\.claude\skills\unrealclaude-mcp-tools\SKILL.md`
     line 35.
   - Replace "Full direct-tool list with one-line purposes lives in
     `az-workspace` § 'unrealclaude MCP tools'" with an inline 12-row
     table here, OR add a 12-row catalog section to `az-workspace`.
   - Recommended: inline here so this skill is self-sufficient (a user
     loading the MCP-tools skill probably doesn't want a second skill load).

3. **Fix `gasp_animbp_full_audit.md` line-count claim in
   `gasp-parity-reference`.**
   - File: `C:\UnrealEngine\Games\AZ\.claude\skills\gasp-parity-reference\SKILL.md`
     line 44.
   - Replace "(1156 lines, large)" with "(342 lines)" or just remove the
     parenthetical entirely.
   - Also fix the same number in `_DESIGN.md` Section 2 line 71 for future
     accuracy.

4. **Add engine header path for OffsetRootBone enum verification.**
   - File: `C:\UnrealEngine\Games\AZ\.claude\skills\anim-debug-pitfalls\SKILL.md`
     section 3 ("OffsetRootBone misbehaving"), inside the **Check** bullet.
   - Add: "Engine header:
     `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\...`
     (verify exact path; search for `EOffsetRootBoneMode`)". Without a
     pointer the "verify the engine header" advice is hollow.

5. **Trim duplicated GC-crash rule to a single owner.**
   - Owner: `asset-modification-via-python` (full content).
   - Files to trim:
     - `cpp-build-livecoding` § "The Python-from-MCP GC trap" (lines 62-68):
       cut to a 2-line summary + pointer.
     - `az-cpp-utility-tools` § "Hard rule" inside Python recipe template
       (lines 105-106): leave the ~3-line warning, point to the python
       skill for the full body.
   - Saves ~12 lines × 30 tokens ≈ 360 tokens per double-trigger.

6. **Trim duplicated plugin-patch list to a single owner.**
   - Owner: `unrealclaude-mcp-tools` § "Local plugin patches" (already
     present, good).
   - File to trim: `cpp-build-livecoding` § "Plugin DLL flakiness" + §
     "Local plugin patches — re-apply after upstream sync" (lines 50-74).
     Leave the "close-editor + CLI build" advice (build-specific), but
     replace the patch enumeration with a 1-liner: "If MCP behavior
     regresses after a plugin sync, see skill
     `unrealclaude-mcp-tools` § 'Local plugin patches'."
   - Saves ~10 lines.

7. **Fix `script_type` quoting drift in `cpp-build-livecoding`.**
   - File: `C:\UnrealEngine\Games\AZ\.claude\skills\cpp-build-livecoding\SKILL.md`.
   - Pick one: `script_type=console` (CLI-style) OR `"script_type":
     "console"` (JSON tool-call literal). Recommend the JSON literal
     since that's what the actual MCP tool body looks like.

8. **Standardise the line-count parentheticals in `gasp-parity-reference`.**
   - Either include line counts for ALL files or NONE. Currently only
     two have explicit counts ("1156 lines, large", "460 lines"); rest
     don't. If keeping, audit them all (most match DESIGN.md verified
     above except the one already called out).

9. **Add trigger phrases to `agent-and-research-discipline` and
   `az-cpp-utility-tools` descriptions.**
   - `agent-and-research-discipline`: append "Use before launching any
     Task/Explore agent; covers the 'check memory first' rule."
   - `az-cpp-utility-tools`: append ", and the EnsureFunctionPins / pin
     direction inversion (FunctionEntry params are output, FunctionResult
     return is input) cheat sheet."

10. **Fix `MEMORY.md` "Workflow / Behavior Rules" block redundancy.**
    - File: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\MEMORY.md`
      lines 36-41.
    - These 6 bullets enumerate *every* feedback file with a "covered by
      skill X" tag — that's fine for an INDEX of which files exist, but
      it adds ~6 lines × 30 tokens ≈ 180 tokens to every cold start
      that won't be needed unless the skill triggers. Compress to 2
      lines: "All `feedback_*.md` workflow rules are surfaced via skills
      (see Skills section above); files retained for full context. Index
      below."

---

## Token budget reality check

### Actual line counts vs DESIGN.md estimates

| Skill | Design est. | Actual | Variance |
|---|---|---|---|
| `cpp-build-livecoding` | 70 | 74 | +4 |
| `bp-to-cpp-port` | 140 | 113 | -27 (better) |
| `unrealclaude-mcp-tools` | 110 | 81 | -29 (better) |
| `az-cpp-utility-tools` | 150 | 109 | -41 (better) |
| `anim-debug-pitfalls` | 120 | **198** | **+78** (largest skill — worth it) |
| `gasp-parity-reference` | 70 | 112 | +42 |
| `agent-and-research-discipline` | 60 | 63 | +3 |
| `asset-modification-via-python` | 90 | 109 | +19 |
| `az-workspace` (was 200+, trimmed) | n/a | 113 | n/a |
| **Total skill content** | **810** | **972** | **+162 (+20%)** |

At ~30 tokens/line → **~29.2 k tokens** of skill content vs the design's
24.3 k estimate. Difference: ~5 k tokens; concentrated in
`anim-debug-pitfalls` (+78 lines = +2.3 k). That skill earned its size —
it's the highest-value diagnostic surface.

### MEMORY.md reality

- **Before (per design Section 5):** 132 lines, ~4 k tokens always loaded.
- **After (verified):** **76 lines**, **8884 bytes** (~2.2 k tokens).
- **Saving:** ~1.8 k tokens per cold start. **Matches design's "~1700
  token saving" promise within rounding.** Promise delivered.

### Net effect

- Always-loaded budget shrunk ~40% as promised. ✅
- Skill loading per task adds 2.4 k - 9 k tokens *only when the matching
  trigger fires*. Most short tasks (e.g. "rename a function", "add a
  log") trigger NO skill and pay zero extra cost.
- The 3-way pile-up scenario (build + python + MCP, or port + GASP +
  build) costs ~9 k tokens — a worst-case, not average.

---

## Skills that should be MERGED or DELETED

**None.** All 9 skills earn their place. Specific assessments:

- `agent-and-research-discipline` (63 lines): smallest skill, but covers 5
  workflow rules that came from 5 different feedback files. Consolidating
  was the whole point. Keep.
- `cpp-build-livecoding` (74 lines): the design's § Open question 5
  asked whether to merge into `unrealclaude-mcp-tools`. Recommendation
  stands: keep separate. Build is workflow; MCP is catalog.
- `gasp-parity-reference` (112 lines): pure index, low value per token —
  but indispensable when GASP is mentioned. Keep.
- `az-workspace` (113 lines): trimmed correctly; still earns its place
  as the workspace map.

**One delete candidate** (not a skill, a memory file): per design §
Section 4 last entry, `project_gas_gameplay.md` is a 51-day-old
empty stub. Nothing references it; safe to delete. Out of scope for
this skills review but worth flagging.

---

## Final verdict

**The skills set is READY TO SHIP** *after* the top-3 priority fixes (the
`gassample` policy decision, the broken `az-workspace` back-reference, and
the `gasp_animbp_full_audit.md` 1156-line claim).

The remaining 7 fixes are quality-of-life improvements that can ship
incrementally. The MEMORY.md token saving target (~1700 tokens) was hit;
the trigger-conflict pile-ups are acceptable; reference integrity is high
(only one broken back-reference); accuracy vs source is high (only one
line-count drift).

This is a strong first cut. Brutal final note: the design doc's
"~1700 token saving" was conservative — actual measurement matches it
within rounding, no inflation. That's rare.
