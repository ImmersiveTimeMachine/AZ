---
name: project_motion_matching_progress
description: Motion Matching implementation progress — databases built, next step is ABP wiring
type: project
---

## Motion Matching Implementation Progress (2026-03-28)

### Completed
1. **Plugins enabled** in AZ.uproject: PoseSearch, Chooser, BlendStack, MotionTrajectory, PythonScriptPlugin
2. **PoseSearch module** added to AZ.Build.cs PublicDependencyModuleNames
3. **C++ bridge** created: `UAZ_PoseSearchUtils` (Source/AZ/Public/Animation/AZ_PoseSearchUtils.h) — exposes AddSequenceToDatabase/AddSequencesToDatabase to Python since engine doesn't
4. **MCP tools unlocked**: `run_console_command` and `execute_script` unhidden in tool-router.js (Engine/Plugins/Marketplace/UnrealClaude/Resources/mcp-bridge/tool-router.js)
5. **Schema created**: `PSS_NoWeapon_Locomotion` with:
   - Skeleton: SKEL_SurvivalMan
   - Pose Channel: root, pelvis, foot_l, foot_r (Position + Velocity)
   - Trajectory Channel: 5 samples (-0.3, -0.15, +0.15, +0.3, +0.5s)
   - NOTE: Channels had to be configured manually in editor — Python can create but not properly persist channel subobjects
6. **10 databases created and built** (all BuildIndex Succeeded):
   - PSD_NoWeapon_Idle (8 anims)
   - PSD_NoWeapon_TurnInPlace (4 anims)
   - PSD_NoWeapon_WalkLoops (20 anims)
   - PSD_NoWeapon_WalkStarts (10 anims)
   - PSD_NoWeapon_WalkStops (4 anims)
   - PSD_NoWeapon_RunLoops (18 anims)
   - PSD_NoWeapon_RunStarts (8 anims)
   - PSD_NoWeapon_RunStops (2 anims)
   - PSD_NoWeapon_Pivots (4 anims)
   - PSD_NoWeapon_Sprint (2 anims)
   - Total: 80 animations (vs 17 in the old AZ_BS_NoWeapon blend space)

### Current State (2026-03-29)
- ABP wired: Pose History → Blend by Bool → Output Pose, Motion Matching → True Pose
- bUseMotionMatching toggle works
- PSD_NoWeapon_All database: 77 unique clean anims
- Schema: Pose(1.0) + Trajectory(7.0), per-sample weights graduated
- Generate Trajectory enabled on Pose History node
- Root motion disabled, force root lock enabled on all sv_* anims

### SOLVED: Trajectory
- `Generate Trajectory` on Pose History node doesn't work well with capsule-driven movement
- **Solution**: `CharacterTrajectoryComponent` on the character feeds trajectory via `CharacterTrajectory` variable in AnimInstance
- Trajectory read via UE reflection (`FProperty::CopyCompleteValue`) because `UCharacterTrajectoryComponent::Trajectory` is protected
- Pose History node: uncheck Generate Trajectory, bind `CharacterTrajectory` to Trajectory pin

### SOLVED: Jump with Motion Matching
- MM keeps re-searching within one-shot clips, creating "sliding window" effect
- **Solution for playback**: `bShouldSearch = false` + `SearchThrottleTime = 10.0` + Block Transition + Override Continuing Pose Cost Bias notifies on anim
- **Solution for exit**: `GetRelevantAnimTimeRemainingFraction < 0.1` in SM transition rule (Jump → Locomotion)
- MM picks correct foot variant (lu/ru) based on current pose at jump time
- Database: `PSD_NoWeapon_Jump` with `ContinuingPoseCostBias = -0.1`

### Key MM Settings Reference
| Setting | Locomotion | Jump/One-shot |
|---|---|---|
| SearchThrottleTime | 0.1 | 10.0 |
| bShouldSearch | true | false |
| PoseJumpThresholdTime | (0, 0.5) | (0, 0.5) |
| PoseReselectHistory | 0.5 | 0.5 |
| BlendTime | 0.3 | 0.3 |
| PlayRate | (0.85, 1.15) | (1, 1) |
| bResetOnBecomingRelevant | true | true |

### Key Anim Notifies for PoseSearch
- **PoseSearch: Block Transition** — prevents MM from entering mid-animation (cover all except first frames)
- **PoseSearch: Override Continuing Pose Cost Bias** — negative value = strongly prefer continuing current anim
- **PoseSearch: Exclude From Database** — removes segment entirely from search
- **PoseSearch: Event** — tags event with GameplayTag for TimeToEvent channel (predictable jumps)

### Architecture: SM + MM Hybrid (GASP pattern)
```
SM (macro states):
  ├── Locomotion → MM (PSD_NoWeapon_All, continuous search)
  ├── Jump → MM (PSD_NoWeapon_Jump, bShouldSearch=false) or Sequence Player
  ├── Fall → Sequence Player / Blend Space
  ├── Landing → Sequence Player / Blend Space
  └── Crouch → Blend Space

Main AnimGraph:
  SM → Pose History (CharacterTrajectory) → Output Pose
```

### Pose History Placement
- **MUST be in main AnimGraph** (after SM, before Output Pose) — always runs
- All MM nodes inside SM states find this single Pose History automatically
- Do NOT put Pose History inside SM states — they stop running when state becomes inactive

### Schema Settings (PSS_NoWeapons)
- Pose Channel weight: 1.0 (bones: root, pelvis, foot_l, foot_r with Position + Velocity)
- Trajectory Channel weight: 7.0 (samples at -0.3, -0.15, +0.15, +0.3, +0.5s with Position + FacingDirection)
- Per-sample weights: graduated (past lower, future higher)
- Data preprocessor: Normalize

### Session 2026-04-04/05 Progress
- **SM+BlendStack architecture** decided (GASP Path 1)
- **C++ enums/structs**: `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h`
- **C++ utilities created**:
  - `AZ_SkeletonUtils` — blend profile copy/create/query
  - `AZ_AnimBlueprintUtils` — SM transition creation (uses SpawnNodeFromTemplate + MarkBlueprintAsStructurallyModified)
  - `AZ_ChooserUtils` — Chooser table config, columns, flat rows, nested sub-tables, database population
- **Chooser table**: CHT_AZ_CharacterAnimations — 17 nested sub-tables, 118 anims from 19 databases
- **Blend profiles**: All 9 GASP profiles on SKEL_SurvivalMan (FastFeet_InstantRoot, FastFeet, UpperBody, Start, Pivot, etc.)
- **ABP**: State Controller SM with 6 states + 9 functions created (transitions need manual editor wiring)
- **Project settings**: DDCVars, NetworkPrediction 60Hz, all GASP settings
- **GASP reference**: Full content copied to AZ project, 6 memory files with complete architecture docs
- **Dual MCP**: UnrealClaude port config patch (AZ:3000, GASP:3001) — no longer needed, GASP content in AZ

### Key Learnings (Chooser API)
- Chooser uses InstancedStruct heavily: ColumnsStructs, ResultsStructs, ContextData
- Columns: FEnumColumn (filter), FOutputStructColumn (output), created via InitializeAs<T>()
- Rows: FAssetChooser (asset ref) or FNestedChooser (sub-table ref)
- Context: FContextObjectTypeClass wraps the AnimInstance class
- Property binding: FChooserEnumPropertyBinding.PropertyBindingChain = {"PropertyName"}
- Nested choosers stored in NestedObjects (NOT NestedChoosers array!)
- Must call Compile(true) after modifications
- Live Coding can't add new UFUNCTIONs — need full rebuild for new Python bindings

### Next Steps
1. **Wire SM transitions** in editor (13 transitions + IdleLoop entry)
2. **Implement SetBlendStackAnimFromChooser** BP function body
3. **Add BlendStack + Inertialization + OffsetRootBone** to AnimGraph
4. **Test pipeline** in PIE
5. **Rifle locomotion** — create weapon-stance databases
6. **Explore Predictable Jumps** — PoseSearch Events + TimeToEvent channel

### Python Scripts (Content/Python/)
- `create_posesearch_databases.py` — creates schema + database assets
- `populate_databases.py` — adds animations via AZ_PoseSearchUtils bridge
- `configure_schema.py` — sets skeleton + channels (channels need manual editor config)
- `check_schema.py` — diagnostic: prints schema state
- `resave_all.py` — resaves all assets to trigger rebuild

### Key Learnings
- PoseSearch database `AddAnimationAsset()` is NOT a UFUNCTION — needs C++ bridge for Python access
- Schema channels created via Python store as NoneType — must configure in editor manually
- Databases auto-build when opened in editor after schema becomes valid
- `AnimAsset` is the property name on FPoseSearchDatabaseAnimationAsset (not Sequence)
- `DatabaseAnimationAssets` is private — can't read/write from Python
- `UCharacterTrajectoryComponent::Trajectory` is protected — use UE reflection to read
- MM BlendStack internally re-searches even with one clip — not suitable for one-shot anims without bShouldSearch=false
- Loop flag on animations matters for MM — loops cycle, non-loops play through
- Force Root Lock prevents root bone movement data needed by trajectory — use CharacterTrajectoryComponent instead
- Debug commands: `a.MotionMatch.DrawQuery.Enable 1`, `a.MotionMatch.DrawMatch.Enable 1`
- EPoseSearchInterruptMode: DoNotInterrupt, InterruptOnDatabaseChange, ForceInterrupt — use ForceInterrupt when switching databases
