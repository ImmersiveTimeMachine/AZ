---
name: reference_gasp_anim_notifies
description: Complete catalog of GASP animation notifies and curves by category — reference for configuring AZ anims
type: reference
originSessionId: f6181671-d4a5-4b82-954f-4f2f5396f92f
---
# GASP Animation Notifies & Curves Reference

Source: `/Game/Characters/UEFN_Mannequin/Animations/` (1374 AnimSequences across 14 subfolders)

All GASP locomotion anims have `EnableRootMotion = true`.

## Global Findings

### PoseSearch Notify Types Used
| Notify | Purpose | Where Used |
|--------|---------|------------|
| `PoseSearchBlockTransition` | Prevents MM from transitioning away during this window | Stops, Turns, Jumps, Lands, Transitions |
| `PoseSearchBranchIn` | Marks a point where MM can branch into this anim | Loops, Starts, Stops, Pivots, some Transitions |
| `PoseSearchExcludeFromDatabase` | Excludes a time range from the MM search database | Stops, Pivots, Jumps (sprint variants), Lands |
| `PoseSearchModifyCost` | Modifies the cost of selecting this anim | Land anims (Heavy/Light variants) |
| `PoseSearchOverrideContinuingPoseCostBias` | Overrides continuing-pose cost bias | Turn-in-place, Reface Starts |

### Curve Types Used
| Curve | Purpose | Where Used |
|-------|---------|------------|
| `contact_l` | Left foot contact (0/1) | Nearly ALL anims |
| `contact_r` | Right foot contact (0/1) | Nearly ALL anims |
| `movedata_speed` | Animation speed data for MM | All locomotion (not idle breaks) |
| `MoveData_Speed` | Same as above, capitalized variant | Idle loops/breaks, Turn-in-place |
| `enable_warping` | Enable stride/orientation warping | Loops, Starts, Pivots, Lands, Jumps |
| `Phase` | Locomotion phase (gait cycle) | Loops (cardinal F/B/L/R), Pivots, some Starts |
| `phase` | Same, lowercase variant | Sprint loops, Crouch turns |
| `enable_turninplacesteering` | Enable TIP steering | Turn 90/180, some Stops |
| `steeringtargettime` | Steering target time for TIP | Turn 90/180, Reface Starts |
| `disableleftfootik` | Disable left foot IK | Jump/Land anims |
| `disablerightfootik` | Disable right foot IK | Jump/Land anims |
| `disablepelvisadjustment` | Disable pelvis adjustment | Jump/Land anims |
| `disable_ao` | Disable aim offset | Some idle breaks |
| `maxdynamicplayrate` | Max dynamic play rate for MM | Run strafe loops, Land strafe |
| `enable_orientationwarping` | Enable orientation warping | Relaxed run loop (Troy) |
| `disable_orientationwarping` | Disable orientation warping | Some run direction changes |
| `disable_additiveleans` | Disable additive leans | Some walk/run direction changes |
| `Enable_StrafeWarping` | Enable strafe warping | Jump forward anims |
| `enable_warping_2` | Secondary warping enable | Jump forward land light |
| `sprintweight` | Sprint blend weight | Sprint direction changes |
| `pivoting` | Pivoting state | Run direction changes |
| `enable_steering` | Enable steering | Some run direction changes |

---

## Category Details

### Idle (20 anims)
- **bLoop:** Loop=true (Idle_Loop), Loop=false (Idle_Break)
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Idle_Loop: `PoseSearchBranchIn` (only on Neutral, not Relaxed)
  - Idle_Break: NONE
- **Curves:**
  - ALL: `contact_l`, `contact_r`
  - Neutral: `MoveData_Speed` (capitalized)
  - Some breaks: `disable_ao`
- **Foley:** BP_FoleyEvent_Walk_L_C, BP_FoleyEvent_Walk_R_C (some breaks only)
- **Pattern:** Idle loops get BranchIn so MM can enter them. Breaks have NO PoseSearch notifies.

### Turn-in-Place (18 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL turns (except generic idle_turn): `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
  - 90/180 degree turns: also `PoseSearchBranchIn` (Relaxed variants)
- **Curves:**
  - ALL: `contact_l`, `contact_r`
  - Neutral turns: `MoveData_Speed`
  - 90/180 degree turns: `enable_turninplacesteering`, `steeringtargettime`
  - 45/135 degree turns: NO steering curves
- **Foley:** BP_FoleyEvent_Walk_L/R_C (Neutral), FoleyEvent: Walk/Run (Relaxed)
- **Pattern:** BlockTransition prevents MM from leaving mid-turn. OverrideContinuingPoseCostBias biases toward completing the turn. Steering curves only on 90/180.

### Crouch Idle (14 anims)
- **bLoop:** Loop=true (Crouch_Idle_Loop), Loop=false (breaks/turns)
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Idle_Loop: `PoseSearchBranchIn`
  - Idle_Turn: `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
  - Idle_Break: NONE
- **Curves:**
  - ALL: `contact_l`, `contact_r`
  - Breaks: `MoveData_Speed`, some `disable_ao`
  - Turns 90/180: `enable_turninplacesteering`, `steeringtargettime`, `movedata_speed`
  - Turns 45/135: `movedata_speed` only (no steering)
- **Pattern:** Mirrors standing idle/turn pattern exactly.

### Walk Loop (24 anims)
- **bLoop:** true
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Cardinal directions (F, B, LL, LR, RL, RR): `PoseSearchBranchIn`
  - Diagonal/offset variants: NO PoseSearch notifies (just foley)
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Cardinal+Phase variants: `Phase` curve added
- **Foley:** FoleyEvent: Walk (forward), WalkBackwds (backward), RunStrafe (side LL)
- **Pattern:** Loops always have BranchIn on cardinal directions. Phase curve on F/B/L/R cardinal only.

### Walk Start (54 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Regular starts: NO PoseSearch notifies (just foley)
  - Reface starts: `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
  - Some reface starts: also `PoseSearchBranchIn`
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Most: `Phase`
  - Reface starts: `steeringtargettime` (some)
- **Pattern:** Regular starts have NO PoseSearch notifies. Reface starts (turning while starting) get BlockTransition + OverrideContinuingPoseCostBias.

### Walk Stop (36 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL stops: `PoseSearchBlockTransition` + `PoseSearchExcludeFromDatabase`
  - Cardinal directions (F, B, LL, LR, RL, RR): also `PoseSearchBranchIn`
  - Diagonal stops: NO BranchIn
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `movedata_speed`
  - Relaxed reface stops: `Phase`, `enable_turninplacesteering`, `steeringtargettime`
  - Relaxed non-reface stops: `Phase`, some `enable_warping`
- **Foley:** FoleyEvent: Walk, WalkBackwds
- **Pattern:** ALL stops have BlockTransition + ExcludeFromDatabase. Cardinal stops add BranchIn.

### Walk Pivot (28 anims, direction changes while walking)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL pivots: `PoseSearchExcludeFromDatabase`
  - Cardinal pivots (F_B, B_F, LL_RL, etc.): also `PoseSearchBranchIn`
  - Diagonal pivots: NO BranchIn
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Cardinal pivots: `Phase`
- **Pattern:** All pivots excluded from DB. Cardinal pivots get BranchIn + Phase.

### Walk Direction Changes (198 anims, Box/Diamond/Hourglass/Prism/Shuffle patterns)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Box patterns: `PoseSearchBranchIn` + `PoseSearchExcludeFromDatabase`
  - Diamond/Hourglass/Prism patterns: `PoseSearchExcludeFromDatabase` only (most)
  - Shuffle patterns: `PoseSearchBranchIn` only
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Box patterns: `Phase`
  - Others: NO Phase
- **Pattern:** Complex direction change patterns mostly excluded from DB.

### Run Loop (27 anims)
- **bLoop:** true (most), false (Relaxed BL/BR/FL/FR)
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Cardinal directions (F, B, LL, LR, RL, RR): `PoseSearchBranchIn`
  - Diagonal/offset: NO PoseSearch notifies
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Cardinal: `Phase`
  - Strafe (LL): `maxdynamicplayrate`
- **Pattern:** Same as Walk Loop -- BranchIn on cardinals, Phase on cardinals.

### Run Start (52 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Regular starts: `PoseSearchBranchIn` (some)
  - Reface starts: `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
  - Some: `PoseSearchModifyCost`
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Most: `Phase`, `maxdynamicplayrate`
  - Reface: `steeringtargettime`
- **Pattern:** Same pattern as walk starts. Reface starts get BlockTransition + OverrideContinuingPoseCostBias.

### Run Stop (36 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL stops: `PoseSearchBlockTransition` + `PoseSearchExcludeFromDatabase`
  - Cardinal directions: also `PoseSearchBranchIn`
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `movedata_speed`
  - F/B only: `enable_turninplacesteering`, `steeringtargettime`
- **Pattern:** Identical to walk stops. BlockTransition + ExcludeFromDatabase on ALL, BranchIn on cardinals.

### Run Pivot (28 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL: `PoseSearchExcludeFromDatabase`
  - Cardinal pivots: `PoseSearchBranchIn`
  - Some: `BP_NotifyState_EarlyTransition_C`
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Cardinal: `Phase`, `maxdynamicplayrate`
- **Pattern:** Same as walk pivots.

### Run Direction Changes (203 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:** Mix of BranchIn, ExcludeFromDatabase, BlockTransition, ModifyCost, OverrideContinuingPoseCostBias
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Most: `Phase`, `maxdynamicplayrate`
  - Some: `disable_additiveleans`, `disable_orientationwarping`, `enable_steering`, `pivoting`, `steeringtargettime`
- **Pattern:** Complex; varies per pattern type.

### Sprint Loop (6 anims)
- **bLoop:** true
- **Root Motion:** true
- **PoseSearch Notifies:**
  - F only: `PoseSearchBranchIn`
  - FL/FR/F_L_20/F_R_20: NO PoseSearch notifies
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - F: `phase` (lowercase)
- **Pattern:** Same as walk/run loops.

### Sprint Start (16 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - F only: `PoseSearchBlockTransition` + `PoseSearchBranchIn`
  - Reface starts: `PoseSearchBranchIn` (+ BlockTransition for Relaxed)
  - Others: NO PoseSearch
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Reface: `phase`/`Phase`, `steeringtargettime`
- **Pattern:** Similar to walk/run starts.

### Sprint Stop (8 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - F only: `PoseSearchBranchIn`
  - Relaxed: `PoseSearchBlockTransition` + `PoseSearchBranchIn` + `PoseSearchExcludeFromDatabase`
  - Diagonal: NO PoseSearch
- **Curves:**
  - ALL: `contact_l`, `contact_r`
  - F + Relaxed: `movedata_speed`, `enable_warping` (some)

### Sprint Direction Changes (36 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:** Mix of BlockTransition, BranchIn, ExcludeFromDatabase
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Some: `Phase`, `sprintweight`, `steeringtargettime`

### Crouch Locomotion (159 anims)
- **bLoop:** true (Loops), false (all others)
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Loops (cardinal F/B): `PoseSearchBranchIn`
  - Loops (side LL/LR/RL/RR): `PoseSearchBranchIn`
  - Box patterns: `PoseSearchBranchIn` + `PoseSearchExcludeFromDatabase`
  - Diamond/Hourglass/Prism: `PoseSearchExcludeFromDatabase`
  - Shuffle: `PoseSearchBranchIn`
  - Pivots (cardinal): `PoseSearchBranchIn` + `PoseSearchExcludeFromDatabase`
  - Pivots (diagonal): `PoseSearchExcludeFromDatabase`
  - Transitions (Stand<->Crouch): `PoseSearchBlockTransition` + `PoseSearchBranchIn`
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Loops/Box: `Phase`
  - Turns 90/180: `phase`, `steeringtargettime`
  - Transitions: NO enable_warping

### Crouch Transition (48 anims: Start/Stop/Reface)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Starts: NO PoseSearch (just foley)
  - Stops (cardinal): `PoseSearchBranchIn`
  - Stops (diagonal): NO PoseSearch
  - Reface: NO PoseSearch
- **Curves:**
  - ALL: `contact_l`, `contact_r`, `enable_warping`, `movedata_speed`
  - Starts/Stops: `Phase`
  - Reface: `phase`, `steeringtargettime`
  - Stops: NO enable_warping, NO Phase

### Jump Start (34 anims)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - ALL: `PoseSearchBlockTransition`
  - Most: `PoseSearchBranchIn`
  - Sprint/Relaxed variants: `PoseSearchExcludeFromDatabase`
- **Curves:**
  - Neutral: `contact_l`, `contact_r`, `disableleftfootik`, `disablepelvisadjustment`, `disablerightfootik`, `enable_warping`
  - Relaxed: `Enable_StrafeWarping` only
- **Pattern:** ALL jump starts get BlockTransition. Most get BranchIn. IK disable curves present.

### Jump Fall (2 anims)
- **bLoop:** true
- **Root Motion:** true
- **PoseSearch Notifies:** NONE
- **Curves:**
  - Neutral: `contact_l`, `contact_r`, `disableleftfootik`, `disablepelvisadjustment`, `disablerightfootik`
  - Relaxed: NONE
- **Pattern:** Fall loops have NO PoseSearch notifies. IK disabled during fall.

### Jump Forward (20 anims, full jump arc)
- **bLoop:** false
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Forward (Walk/Run): `PoseSearchBlockTransition` + `PoseSearchBranchIn` + `PoseSearchExcludeFromDatabase`
  - B/LL/RL: NO PoseSearch (just foley)
- **Curves:**
  - Forward: `Enable_StrafeWarping`, `contact_l`, `contact_r`, `disableleftfootik`, `disablepelvisadjustment`, `disablerightfootik`, `enable_warping`
  - Others: `contact_l`, `contact_r` only

### Land (86 anims)
- **bLoop:** false
- **Root Motion:** true (most), false (Roll variants)
- **PoseSearch Notifies:**
  - ALL moving lands: `PoseSearchBlockTransition` + `PoseSearchBranchIn`
  - Heavy/Light with speed: `PoseSearchModifyCost`
  - Light run lands: `PoseSearchExcludeFromDatabase`
  - Roll lands: `PoseSearchOverrideContinuingPoseCostBias` (instead of BranchIn)
  - Stumble: NO PoseSearch
- **Curves:**
  - Moving lands: `Phase`, `contact_l`, `contact_r`, `disableleftfootik`, `disablepelvisadjustment`, `disablerightfootik`, `enable_warping`, `movedata_speed`
  - Standing lands: `contact_l`, `contact_r`, `disableleftfootik`, `disablepelvisadjustment`, `disablerightfootik`
  - Strafe lands: additionally `maxdynamicplayrate`
  - Light run lands: additionally `enable_warping_2`
- **Pattern:** ALL lands get BlockTransition. ModifyCost used to differentiate Heavy vs Light.

### Slide (40 anims)
- **bLoop:** true (Loop), false (others)
- **Root Motion:** true
- **PoseSearch Notifies:**
  - Loops: NONE or `PoseSearchBranchIn`
  - Direction changes: `PoseSearchExcludeFromDatabase`, some `PoseSearchBranchIn`
  - Into/Exit: NO PoseSearch or minimal
- **Curves:**
  - ALL: `contact_l`, `contact_r`
  - Some: `enable_warping`, `movedata_speed`, `Phase`

---

## Summary Rules for Applying to AZ RTG Anims

### Rule 1: Universal Curves
ALL locomotion anims need: `contact_l`, `contact_r`
ALL moving anims need: `movedata_speed`
ALL loops/starts/pivots need: `enable_warping`

### Rule 2: Loop Anims
- `bLoop = true`
- `PoseSearchBranchIn` on cardinal directions (F, B, L, R)
- `Phase` curve on cardinal directions
- Foley events per footstep
- NO BlockTransition, NO ExcludeFromDatabase

### Rule 3: Start Anims
- `bLoop = false`
- Regular starts: NO PoseSearch notifies (or just BranchIn)
- Reface/turning starts: `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
- `Phase` curve, `enable_warping`

### Rule 4: Stop Anims
- `bLoop = false`
- ALL: `PoseSearchBlockTransition` + `PoseSearchExcludeFromDatabase`
- Cardinal directions: also `PoseSearchBranchIn`
- `movedata_speed` (NO enable_warping, NO Phase usually)
- F/B stops: `enable_turninplacesteering`, `steeringtargettime`

### Rule 5: Pivot / Direction Change Anims
- `bLoop = false`
- ALL: `PoseSearchExcludeFromDatabase`
- Cardinal pivots: `PoseSearchBranchIn`
- `Phase`, `enable_warping`, `movedata_speed`

### Rule 6: Turn-in-Place Anims
- `bLoop = false`
- ALL: `PoseSearchBlockTransition` + `PoseSearchOverrideContinuingPoseCostBias`
- 90/180: `enable_turninplacesteering`, `steeringtargettime`
- 45/135: NO steering curves
- `MoveData_Speed` (capitalized), `contact_l`, `contact_r`

### Rule 7: Jump Anims
- `bLoop = false` (except Fall Loop)
- Jump Start: `PoseSearchBlockTransition` + `PoseSearchBranchIn`
- Fall Loop: NO PoseSearch notifies, `bLoop = true`
- ALL jump/fall: `disableleftfootik`, `disablerightfootik`, `disablepelvisadjustment`

### Rule 8: Land Anims
- `bLoop = false`
- ALL: `PoseSearchBlockTransition` + `PoseSearchBranchIn`
- Heavy/Light: `PoseSearchModifyCost`
- `disableleftfootik`, `disablerightfootik`, `disablepelvisadjustment`
- Moving lands: `Phase`, `enable_warping`, `movedata_speed`

### Rule 9: Idle Anims
- Idle Loop: `bLoop = true`, `PoseSearchBranchIn`
- Idle Break: `bLoop = false`, NO PoseSearch notifies
- `MoveData_Speed` (capitalized), `contact_l`, `contact_r`
- Some breaks: `disable_ao`

### Rule 10: Crouch/Stand Transitions
- `PoseSearchBlockTransition` + `PoseSearchBranchIn`
- `contact_l`, `contact_r`, `movedata_speed`
