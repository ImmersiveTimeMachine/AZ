---
name: project_mm_implementation_plan
description: MM implementation plan — SM+BlendStack path (GASP Path 1), phased approach, decided 2026-04-04
type: project
---

# Motion Matching Implementation Plan (2026-04-04)

## Decision: SM + BlendStack (GASP Path 1)
- More control over blend times/profiles per state
- Lower CPU (search only on state change, not every frame)
- Per-state Chooser outputs (BlendTime, BlendProfile, UseMM, MMCostLimit)
- GASP defaults to this path (LocomotionSetupMover=1)

## Phase 1 — Wire Existing Assets (next session)

### Step 1: Wire Chooser → MM node
- CHT_NoWeapon_Locomotion exists but isn't connected to MM Database pin
- OnMMUpdate callback exists but only does SetInterruptMode, not SetDatabasesToSearch
- Add SetDatabasesToSearch call using Chooser evaluation result (CurrentLocomotionDatabase)

### Step 2: Add OffsetRootBone node
- After MM output, before Output Pose
- Settings: Translation=Interpolate, Rotation=Accumulate, HalfLife=0.2, MaxError=30

### Step 3: Add Inertialization node
- After MM, before OffsetRootBone
- Set bUseInertialBlend=true on MM node

### Step 4: Create PSN normalization set
- 1 set covering all 19 NoWeapon databases
- Ensures consistent cost comparison across databases

## Phase 2 — SM + BlendStack Architecture

### Step 5: Create SM "State Controller" (logical, no pose output)
States matching GASP:
- IdleLoop (0)
- LocomotionLoop (1)
- InAirLoop (2)
- TransitionToIdle (3) — stops/pivots to idle
- TransitionToLocomotion (4) — starts/pivots to movement
- TransitionToInAir (5) — jump/fall
- (Skip IdleBreak and SlideLoop for now)

### Step 6: Create SetBlendStackAnimFromChooser function
- Evaluate CHT_NoWeapon_Locomotion with current state
- Get S_ChooserOutputs (UseMM, BlendTime, BlendProfile, etc.)
- If UseMM: single-frame MotionMatch on filtered results
- Feed result to BlendStack node

### Step 7: Add BlendStack node
- Replace static MM pose output with BlendStack
- SM OnStateEntry calls SetBlendStackAnimFromChooser

### Step 8: Create data model enums
- E_Gait (Walk, Run, Sprint) — already have GetGait() returning int
- E_MovementState (Idle, Moving)
- E_MovementDirection (Forward, Backward, Left, Right, LeftFwd, RightFwd)
- These become Chooser input columns

## Phase 3 — Trajectory & Movement

### Step 9: Initialize MoverTrajectoryPredictor
- Call InitializeMoverPredictor() in ABP Initialize
- Replace CharacterTrajectoryComponent reflection hack
- Use PoseSearchGenerateTransformTrajectoryWithPredictor()

### Step 10: Custom BP movement modes
- BP_MovementMode_Walking with per-gait speed/accel/decel
- BP_MovementMode_Falling

## Phase 4 — Post-Processing Pipeline

### Step 11: Add post-processing chain
- Procedural_PreLayering (future)
- AdditiveLeans (walk/run lean blend spaces)
- AimOffset (already have for rifle, extend to general)
- Slot 'DefaultSlot'
- OffsetRootBone (done in Phase 1)
- Procedural foot IK (future)
- PoseHistory → Output Pose

## Assets Already Ready
- 19 PoseSearch databases (80 anims)
- 1 Schema (PSS_NoWeapons)
- 1 Chooser (CHT_NoWeapon_Locomotion)
- 5+ Blend Profiles on SKEL_SurvivalMan (all GASP profiles copied)
- AZ_SkeletonUtils C++ utility (blend profile operations)
- AZ_PoseSearchUtils C++ utility (database operations)
- DDCVars configured
- Network Prediction at 60Hz
- All GASP reference content in project

## Current Progress (2026-04-05)

### Done:
- C++ enums & structs: `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h`
- C++ AnimInstance: ~50 variables (states, essential values, trajectory), transition conditions, AnimGraph bindings
- C++ utilities: AZ_SkeletonUtils, AZ_AnimBlueprintUtils, AZ_ChooserUtils, AZ_PoseSearchUtils, AZ_BlueprintNodeUtils
- AZ_BlueprintNodeUtils: Full BP function builder (Cast, BreakStruct, SetFields, local vars, EnsureFunctionPins)
- ABP SM: "State Controller" with 6 states (transitions need manual wiring)
- ABP Functions: SetBlendStackAnimFromChooser (100% match), Get_DynamicPlayRate (100% match), OnStateEntry_*, etc.
- Chooser table: CHT_AZ_CharacterAnimations — nested, 17 sub-tables, 118 anims, GASP context pattern
- Blend profiles: All 9 GASP profiles on SKEL_SurvivalMan
- DDCVars, NetworkPrediction 60Hz, all settings aligned
- CLAUDE.md: Always use full absolute paths

### Next Session — TODO:
1. Wire SM transitions manually in editor (13 transitions + IdleLoop as entry state)
2. Implement SetBlendStackAnimFromChooser function body in ABP (evaluate CHT_AZ_CharacterAnimations, optional MM, feed BlendStack)
3. Add BlendStack + Inertialization + OffsetRootBone nodes to AnimGraph
4. Wire the new signal chain: SM → BlendStack → Inertialization → OffsetRootBone → PoseHistory → Output
5. Bind OnStateEntry functions to SM states
6. Test the full pipeline in PIE

### Chooser Table — DONE:
- CHT_AZ_CharacterAnimations: Nested structure with 17 root rows → 17 sub-tables → 118 anims
- Input columns: EAZ_StateMachineState, EAZ_Gait, EAZ_Stance
- Output column: FAZ_ChooserOutputs (UseMM, BlendTime, BlendProfile)
- Context: UAZ_AnimInstance
- Fully automated via AZ_ChooserUtils (can repopulate anytime)

## What's NOT Needed Yet
- Weapon-stance databases (Phase 5+)
- Directional database decomposition (Phase 5+)
- Multiple schemas per motion type (Phase 5+)
- Traversal system (Phase 5+)
- EarlyTransition notifies (only needed for SM path, Phase 2+)
