---
name: az-workspace
description: Workspace map for the AZ project — folders for code, content, configs, GASP reference, engine plugins, MCP servers, and persistent memory. Use this skill at the start of any non-trivial AZ task to confirm where things live before searching or guessing. Detailed workflows live in dedicated skills (build/Live Coding → cpp-build-livecoding; MCP tool usage → unrealclaude-mcp-tools; BP→C++ porting → bp-to-cpp-port; anim debugging → anim-debug-pitfalls; GASP parity index → gasp-parity-reference; C++ utility classes → az-cpp-utility-tools; Python asset modification → asset-modification-via-python; research/agent discipline → agent-and-research-discipline).
---

# AZ Workspace Map

UE 5.7 third-person survival/action project. Active work: porting GASP `SandboxCharacter_Mover_ABP` to C++ (`UAZ_AnimInstance`), wiring SM+BlendStack hybrid, idle TIP, locomotion.

> **Workflow skills** (load on demand — this skill is just the map):
> - **Build / Live Coding** → skill `cpp-build-livecoding`
> - **UnrealClaude MCP tools, decision tree, gotchas** → skill `unrealclaude-mcp-tools`
> - **BP → C++ porting + 7-gate dual-reviewer checklist** → skill `bp-to-cpp-port`
> - **Anim graph / AnimBP diagnostic pitfalls** → skill `anim-debug-pitfalls`
> - **GASP parity reference index** → skill `gasp-parity-reference`
> - **AZ C++ utility classes (BlueprintNodeUtils, AnimGraphNodeUtils, ChooserUtils, PoseSearchUtils, SkeletonUtils, AnimBlueprintUtils)** → skill `az-cpp-utility-tools`
> - **Modifying assets via Python through MCP execute_script** → skill `asset-modification-via-python`
> - **Research / agent / file-path discipline** → skill `agent-and-research-discipline`

## Project root

`C:\UnrealEngine\Games\AZ\`

| Folder | Purpose |
|---|---|
| `Source\AZ\Public\` and `Source\AZ\Private\` | C++ source. Mirrors: `Animation\`, `Character\`, `Player\`, `Equipment\`, `Items\`, `Inventory\`, `InventoryOld\`, `InventoryUI\`, `Weapon\` |
| `Source\AZ\Public\Animation\` | `AZ_AnimInstance.h/.cpp` (ported AnimBP), `AZ_LocomotionTypes.h` (FAZ_BlendStackInputs / FAZ_ChooserOutputs), `AZ_AnimBlueprintUtils`, `AZ_BlueprintNodeUtils`, `AZ_AnimGraphNodeUtils`, `AZ_ChooserUtils`, `AZ_PoseSearchUtils`, `AZ_SkeletonUtils` |
| `Source\AZ\Public\Character\` | `AZ_HeroPawn.h/.cpp` (Mover-based pawn, GASP pre/post-sim packer, idle TIP accumulator) |
| `Source\AZ\Public\Player\` | `AZ_PlayerController.h/.cpp` (IMC, GAS InputConfig) |
| `Source\AZ\AZ.Build.cs` | Module deps. Editor target: `Source\AZ\AZEditor.Target.cs` |
| `Content\AZ\Blueprints\` | Tracked game BPs only (per `.gitignore` policy — anims/textures excluded to bound LFS) |
| `Content\AZ\Blueprints\Animation\AZ_ABP_Mover.uasset` | Active ABP (parent = `UAZ_AnimInstance`) — has 9-state SM, 9 OnStateEntry_* graphs |
| `Content\AZ\Blueprints\Animation\MotionMatching\CHT_AZ_CharacterAnimations.uasset` | Active chooser table |
| `Content\AZ\Blueprints\Animation\MotionMatching\CHT_NoWeapon_Locomotion.uasset` | Locomotion DB chooser |
| `Content\AZ\Blueprints\Animation\AZ_Hero_MDT.uasset` | Mirror data table |
| `Content\AZ\Blueprints\Character\Hero\AZ_BP_HeroPawn.uasset` | BP child of `AAZ_HeroPawn` (set as default Pawn in GameMode) |
| `Content\AZ\Blueprints\Player\BP_AZ_PlayerController.uasset` | BP child of `AAZ_PlayerController` (IMC + InputConfig assigned here) |
| `Content\AZ\Blueprints\Game\BP_AZ_GameMode.uasset` | Default GameMode |
| `Content\AZ\Assets\RTG\NoWeapons\RootMotions\` | 191 source anims (NOT tracked — local only, see `reference_noweapon_anim_catalog.md`) |
| `Config\DefaultEngine.ini` | CoreRedirects (`+ClassRedirects`, `+PropertyRedirects`), physics/network settings, plugins enable list |
| `Config\DefaultGame.ini` | Game-side settings |
| `Saved\Autosaves\Game\AZ\Blueprints\` | UE editor autosaves — used to recover .uasset after the LFS rewrite (e.g. `_Auto1.uasset` files) |
| `.claude\settings.local.json` | Claude Code permissions allowlist (per-project) |
| `.claude\skills\` | Project-scoped skills (this file lives here) |
| `.mcp.json` | Project-scope MCP server registrations |

## Memory (persistent across sessions)

`C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\`

| File | Topic |
|---|---|
| `MEMORY.md` | Index — always loaded into context. Other files are read on demand |
| `gasp_*.md` | GASP reference: data model, choosers, character/movement, ABP architecture, SM topology, OrientationIntent + TIP, project settings, update logic flow |
| `project_*.md` | AZ-specific: pawn port, ABP port (Phases 0–9), session logs, idle TIP design, MM plan/progress, weapon swap, GAS gameplay |
| `feedback_*.md` | Workflow rules: BP→C++ port reviewer checklist, retarget root motion, check memory first |
| `reference_*.md` | Catalogs: NoWeapon anim pool (191), GASP anim notifies, BP node tools, AnimGraph node tools, retargeting tutorials, CHT structure |

Always read `MEMORY.md` first; it points to the rest with one-line summaries.

## GASP reference — IMPORTED INTO AZ PROJECT

**The entire GASP project content has been copied into AZ at `/Game/Blueprints/`** (filesystem: `C:\UnrealEngine\Games\AZ\Content\Blueprints\`). Inspect via the regular `unrealclaude` MCP on port 3000 — **NO need for `gassample` MCP, NO need to launch a separate GASP editor**.

| `/Game/Blueprints/` path | Purpose |
|---|---|
| `SandboxCharacter_Mover.uasset` | GASP pawn BP — reference for `AAZ_HeroPawn` parity |
| `SandboxCharacter_Mover_ABP.uasset` | GASP AnimBP — reference for `UAZ_AnimInstance` + `AZ_ABP_Mover` parity (OffsetRootBone settings, all driver functions, every binding) |
| `SandboxCharacter_CMC.uasset` / `SandboxCharacter_CMC_ABP.uasset` | Older CMC variant (skip unless explicitly needed) |
| `Data/S_*.uasset` | GASP structs: `S_BlendStackInputs`, `S_ChooserOutputs`, `S_PlayerInputState`, `S_MoverCustomInputs`, etc. |
| `Data/E_*.uasset` | GASP enums: `E_Gait`, `E_Stance`, `E_MovementMode`, `E_RotationMode`, `E_MovementDirection`, etc. |
| `Data/Curve_*.uasset` and `CHT_RotationOffsetCurve.uasset` | Rotation offset curves + selector chooser |
| `Data/BFL_HelpfulFunctions.uasset` | Debug-draw + visual override helpers |
| `AnimNotifies/` | Foley + traversal notifies, including `BP_NotifyState_EarlyTransition` |
| `AnimModifiers/`, `Cameras/`, `ControlRigs/`, `MovementModes/`, `RetargetedCharacters/`, `SmartObject/` | Other GASP subsystems |
| `BPI_SandboxCharacter_*` | Pawn↔ABP interfaces |
| `AC_PreCMCTick.uasset`, `AC_TraversalLogic.uasset`, `AC_VisualOverrideManager.uasset` | GASP actor components |
| `GM_Sandbox.uasset`, `PC_Sandbox.uasset` | GASP GameMode + PlayerController |

**Standing rule:** when comparing AZ to GASP (parity audits, debugging divergence, copying a missing function/property), use `mcp__unrealclaude__unreal_blueprint_query` against `/Game/Blueprints/SandboxCharacter_Mover_ABP` etc. directly. Do **not** ask the user to launch GASP, do **not** look in `C:\UnrealEngine\Games\GameAnimationSample\` (that external copy may exist but is not the source of truth — the imported AZ copy is).

The external GASP path `C:\UnrealEngine\Games\GameAnimationSample\` and the `gassample` MCP at port 3001 are deprecated for this project; ignore them.

For per-topic GASP memory file index → skill `gasp-parity-reference`.

## Engine path

`C:\UnrealEngine\Engine\`

| Path | Purpose |
|---|---|
| `Plugins\Experimental\Mover\` | Mover plugin (GASP movement system) |
| `Plugins\Animation\PoseSearch\` | PoseSearch / Motion Matching plugin |
| `Plugins\Chooser\` | Chooser plugin (CHT tables) |
| `Plugins\Marketplace\UnrealClaude\` | UnrealClaude MCP plugin (git remote: `Natfii/UnrealClaude`) |
| `Build\BatchFiles\Build.bat` | UnrealBuildTool — see skill `cpp-build-livecoding` for the exact invocation |

## MCP servers (`C:\UnrealEngine\Games\AZ\.mcp.json`)

| Name | Port | Lives in | Tool prefix | Use when |
|---|---|---|---|---|
| `unrealclaude` | 3000 | UE editor running with AZ project open | `mcp__unrealclaude__*` | All inspection / modification of AZ assets — including GASP content imported at `/Game/Blueprints/` |
| ~~`gassample`~~ | ~~3001~~ | DEPRECATED | — | GASP content is imported into AZ; use the `unrealclaude` MCP against `/Game/Blueprints/...` paths instead |

For the full per-tool catalog, decision tree, deferred-tool loading, and per-tool gotchas → skill `unrealclaude-mcp-tools`.

## Quick git facts

- Current branch: `feature/rootmotion`
- Main branch: `main`
- Remote: `git@github.com:ImmersiveTimeMachine/AZ.git`
- Repo was rewritten via `filter-repo` on `2026-05-01` to stay under LFS quota; only `Source/`, `Config/`, `Data/`, `Content/AZ/Blueprints/`, `AZ.uproject`, `.gitattributes`, `.gitignore` retained
- LFS migration export converted all old `*.uasset` LFS pointers to plain blobs in history
