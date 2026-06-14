---
name: gasp_posesearch_choosers
description: GASP PoseSearch schemas (29), databases (168), normalization sets, Chooser tables (14) — full MM data architecture
type: reference
---

# GASP PoseSearch & Chooser Architecture

## Overview
- 29 PoseSearch Schemas
- 168 PoseSearch Databases (4 density tiers)
- 4 Normalization Sets
- 14 Chooser Tables
- Skeleton: SK_UEFN_Mannequin

## Database Density Tiers

| Tier | Databases | Used By | Normalization |
|------|-----------|---------|---------------|
| Dense | 35 | CMC ABP | PSN_Dense_All |
| Sparse | 16 | CMC ABP (LOD) | PSN_Sparse_All |
| Extreme Sparse | 17 | CMC ABP (LOD) | PSN_Extreme_Sparse_All |
| Relaxed | 89 | Mover ABP | PSN_Relaxed_All |
| Experimental SM | 8 | SM+BlendStack path | per-schema |

## Schemas — Motion Matching

### Core (Dense/Sparse)
| Schema | Used For |
|--------|----------|
| PSS_Default | Loops, Starts, Pivots, SpinTransitions, Crouch Walks, TurnInPlace |
| PSS_Default_Mover | Mover-specific equivalents |
| PSS_Idle | Stand/Crouch Idles |
| PSS_Jump | Jumps, Jumps_Far, Jumps_FromTraversal |
| PSS_Stop | Walk/Run/Sprint Stops |
| PSS_Traversal | PSD_Traversal (custom channels: PSC_Traversal_Head, PSC_Traversal_Pos) |

### Relaxed (Mover ABP — 15+ schemas)
| Schema | Used For |
|--------|----------|
| PSS_Relaxed_Idle | Relaxed idles |
| PSS_Relaxed_Loops | Walk/Run/Sprint loops (F, B, LL, RL, LR, RR directions) |
| PSS_Relaxed_Starts | Relaxed starts |
| PSS_Relaxed_Stops | Relaxed stops |
| PSS_Relaxed_Pivots | Relaxed pivots |
| PSS_Relaxed_RunSpins | Run spins |
| PSS_Relaxed_WalkSpins | Walk spins |
| PSS_Relaxed_StandTurn | Stand turn in place |
| PSS_Relaxed_SprintPivots | Sprint pivots |
| PSS_Relaxed_Jump | Relaxed jumps |
| PSS_Relaxed_Slide | Slide (FeetOut, KneesOut) |
| PSS_Relaxed_SlideExit | Slide exit databases |

### SM Schemas
PSS_SM_CMC_Idles, PSS_SM_CMC_LocoLoops, PSS_SM_CMC_LocoTransitions, PSS_SM_Mover_Loops, PSS_SM_Mover_Stops, PSS_SM_Mover_Transitions, PSS_SM_Mover_Spins

## Database Contents (Dense Tier)

**Stand Walk:** Loops (18), Starts (28), Stops (26), Pivots (134+), SpinTransition (8), Lands_Light, Lands_Heavy, FromTraversal
**Stand Run:** Loops, Starts, Stops, Pivots, SpinTransition, Lands
**Stand Sprint:** Loops (7), Starts, Stops, Pivots, Lands
**Stand:** Idles (2), TurnInPlace (8)
**Crouch Walk:** Loops (10), Starts, Stops, Pivots
**Crouch:** Idles, TurnInPlace
**Jumps:** 21 anims, Jumps_Far, Jumps_FromTraversal

## Relaxed Database Naming Pattern
`PSD_Relaxed_{Stance}_{Gait}_{Direction}_{MotionType}`

**Directional decomposition:** Walk/Run databases split by direction (F, B, LL, RL, LR, RR) for Loops, Starts, Pivots, Spins

**Slide system:** Slide_FeetOut, Slide_KneesOut + 6 exit databases (ExitToCrouchIdle, ExitToCrouchWalk, ExitToRun, ExitToSprint, ExitToStandIdle, ExitToWalk)

**Gait transitions:** Run_F_GaitTransitions, Run_F_Turns, Walk_F_Turns

## Chooser Tables — MM Database Selection

### CHT_PoseSearchDatabases_Relaxed (Mover ABP — primary)
**Input columns:** E_Gait, E_Stance, E_MovementMode, E_MovementState, E_MovementDirection
**Output:** Array of PoseSearchDatabase*
**Contains:** All 63+ Relaxed databases, selected by state combination

### CHT_PoseSearchDatabases (CMC meta-chooser)
Delegates to Dense, Sparse, or ExtremeSparse sub-tables based on MMDatabaseLOD setting

### CHT_PoseSearchDatabases_Dense
**Input columns:** E_Gait, E_Stance, E_MovementMode, E_MovementState
**Output:** All 35 Dense databases

### SM Character Animation Choosers
| Chooser | Deps | Input Columns |
|---------|------|---------------|
| CHT_MoverCharacterAnimations | 372 | E_Gait, E_Stance, E_MovementMode, E_ExperimentalStateMachineState, E_MovementDirection, S_ChooserOutputs |
| CHT_CMCCharacterAnimations | 393 | E_Gait, E_Stance, E_ExperimentalStateMachineState, E_MovementDirection, S_ChooserOutputs |

### Traversal Choosers
| Chooser | Content |
|---------|---------|
| CHT_TraversalMontages_Mover | 33 montages (Vault, Climb, Hurdle, Mantle, Catch) keyed by E_MovementMode, E_TraversalActionType |
| CHT_TraversalMontages_CMC | CMC traversal montages |

### Utility Choosers
| Chooser | Purpose |
|---------|---------|
| CHT_RotationOffsetCurve | 7 CurveFloats (F, B, LL, LR, RL, RR, Slide_Knees) by E_MovementDirection |
| CHT_CameraRig | 10 CameraRigAssets by E_CameraMode, E_CameraStyle |

## Custom PoseSearch Channels
- PSC_Traversal_Head — head position for traversal matching
- PSC_Traversal_Pos — body position for traversal matching
- PSC_DistanceToTraversalObject — distance-based channel

## Key Architecture Patterns

1. **Schema-per-motion-type**: Different schemas for Loops vs Stops vs Jumps vs Idle — channels tuned per motion type
2. **Normalization per tier**: One PSN set per density tier, shared by all databases in tier
3. **Chooser hierarchy**: Meta-chooser → density sub-table (CMC) or direct Relaxed chooser (Mover)
4. **Chooser input columns**: Gait + Stance + MovementState + MovementMode (+ Direction for Relaxed)
5. **Direction-aware databases** (Relaxed only): F, B, LL, RL, LR, RR decomposition for loops/starts/pivots
6. **LOD system**: Dense (full quality) → Sparse → Extreme Sparse, fewer databases searched = less CPU
7. **Two paths**: CMC uses Dense/Sparse/ExtremeSparse, Mover uses Relaxed exclusively
