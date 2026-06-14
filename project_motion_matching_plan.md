---
name: project_motion_matching_plan
description: Complete migration plan — Hybrid Pose Search with short clips, Distance Matching, Inertialization, Choosers, root motion strategy
type: project
---

## Animation System Migration Plan (2026-03-30)

Planning to migrate from SM+BlendSpace to Hybrid Pose Search architecture using short clips (Starts, Loops, Stops, Pivots) with Distance Matching, Inertialization, and Choosers.

**Why:** Achieve "Last of Us" quality locomotion with existing marketplace animation clips. Eliminate state machine complexity, add weight/momentum to movement, enable automatic foot-matching on transitions.

**How to apply:** Phased migration over months. Current CMC+MM system continues working during migration. Gameplay development proceeds in parallel.

### Architecture Target (Final AnimGraph)
```
Trajectory (CharacterTrajectoryComponent or Mover)
    → Chooser Table (selects database by speed/direction/stance)
    → Motion Matching (searches selected database)
    → Distance Matching (scales starts/stops to predicted stop distance)
    → Stride Warping (matches foot speed to actual velocity)
    → Orientation Warping (rotates body to match trajectory)
    → Inertialization (momentum-preserving transitions)
    → Control Rig (foot IK, hand IK, procedural adjustments)
    → Output Pose
```

### Phase 0: Foundation (DONE)
- MM working for locomotion loops (9 anims, 8-directional)
- SM hybrid with MM inside states
- CharacterTrajectoryComponent feeding trajectory
- PoseSearch databases, schema, Python tooling
- Jump with MM (bShouldSearch=false) + foot variant selection

### Phase 1: Animation Preparation
- Retarget /Game/MovementAnimsetPro/Animations/RootMotion/ to SKEL_SurvivalMan
- Need IK Retargeter: UE4_Mannequin_Skeleton → SKEL_SurvivalMan
- Organize into categories: Idle, WalkLoops(8dir), WalkStarts(7dir), WalkStops(2), RunLoops(8), RunStarts(7), RunStops(2), Pivots(2), TurnInPlace(4)
- Add Anim Notifies: foot plants (L/R), PoseSearch Block Transition, Override Continuing Pose Cost Bias
- ~50-70 clips total with proper root motion data

### Phase 2: Distance Matching + Stride Warping
- Bake Animation Modifier curves: FootSpeed_L/R, DistanceCurve, OrientationWarpingAlpha
- Study GASP's 27 Animation Modifiers and replicate key ones
- Add Distance Matching node: scales stop/start playback by predicted stop distance
- Add Stride Warping node: stretches legs to match actual vs animation speed
- Add Orientation Warping node: rotates hips/feet to match trajectory
- These eliminate foot sliding and make stops feel precise/heavy

### Phase 3: Inertialization
- Add Inertialization node after MM output, before Output Pose
- Set bUseInertialBlend=true on MM nodes
- Preserves bone momentum during clip transitions (no floaty crossfades)
- This is what makes transitions feel like one continuous motion

### Phase 4: Chooser Tables Replace SM Logic
- Create CHT_NoWeapon_Locomotion: columns for GroundSpeed, MovementDirection, IsMoving, Stance
- Rows map conditions → PSD databases (Idle, WalkLoops, RunLoops, Starts, Stops)
- Wire Chooser → MM node database input
- Remove SM transition rules for locomotion (keep SM for jump/fall/land macro states)
- PoseSearch Column in Chooser (UE 5.7 experimental) for tighter integration

### Phase 5: Root Motion Decision
**Option A: CMC Hybrid (safer, no engine dependency)**
- Keep CMC for physics/networking
- SetAnimRootMotionTranslationScale dynamically: 0 for locomotion, 1 for starts/stops
- Hybrid: CMC drives general movement, root motion drives precise placement

**Option B: Mover Plugin (full commitment, wait for 5.8)**
- Wait for UE 5.8 UAF stabilization
- Replace CMC with Mover plugin
- All movement root-motion-driven
- Trajectory reads input intent, not velocity
- Study GASP Mover character as template

### Phase 6: Motion Symmetry + Coverage
- Mirror Data Table on PoseSearch schema: auto-generate right foot from left foot clips
- Doubles directional coverage without new clips
- GASP uses this for all databases

### Key Technologies
| Technology | Purpose | UE5 Plugin |
|---|---|---|
| Motion Matching | Frame-level animation selection | PoseSearch |
| Chooser Tables | Data-driven database selection | Chooser |
| Distance Matching | Scale anims to predicted stop distance | AnimationLocomotionLibrary |
| Stride Warping | Match foot speed to actual velocity | AnimationWarping |
| Orientation Warping | Rotate body to match trajectory | AnimationWarping |
| Inertialization | Momentum-preserving transitions | Built-in AnimGraph |
| Control Rig | Procedural IK, foot placement | ControlRig |
| Motion Symmetry | Auto-mirror L/R foot variants | PoseSearch Mirror Data Table |

### Foot Plant Notifies (Critical for Distance Matching)
All locomotion clips need L/R foot plant notifies. Without them:
- Distance Matching can't know where feet land
- Stride Warping can't adjust step length
- Mirror symmetry can't identify foot phase
Animation Modifiers can auto-detect and bake these from foot bone speed curves.

### Timeline
- Phase 1 (animation prep): 1-2 weeks
- Phase 2 (distance/stride warping): 1 week
- Phase 3 (inertialization): 2 days
- Phase 4 (Chooser tables): 1 week
- Phase 5 (root motion): depends on UE 5.8 timing
- Phase 6 (mirror/coverage): ongoing

### What to do NOW
1. Set up IK Retargeter for RootMotion folder animations
2. Study GASP Animation Modifiers (bake foot speed, distance curves)
3. Add foot plant notifies to LM_RM_* animations
4. Keep building gameplay on current CMC setup — independent of movement architecture
5. Wait for UE 5.8 UAF before committing to Mover plugin

### Expert Pitfalls & Solutions
| Phase | Risk | Solution |
|---|---|---|
| Phase 1.1 Retargeting | Retargeting drift — character floats over time due to Z displacement | Use "Root Motion Modifier" in IK Retargeter to zero out vertical displacement on flat ground |
| Phase 2.2 Stride Warping | Legs look like spaghetti at high speeds (overstretch) | Set hard Clamped Alpha (1.2x max), let CMC handle speed beyond that |
| Phase 2 Distance Matching | Stutter-stepping if foot plant notifies are off | Foot plant notifies must be frame-perfect — off by 2 frames causes IK reconciliation artifacts |
| Phase 6 Mirror | Jitter on asymmetric meshes (sword on one side) | Mirror Data Table must handle socket flipping for weapon attachments |
| Performance | 150+ clips in one DB kills framerate | Chooser pre-filters to relevant PSD, MM only searches subset (GASP pattern) |

### Key Insights (Animation Tech Lead perspective)
- Distance Matching is the "secret sauce" — drives playhead by predicted stop distance, not time
- Chooser = Pre-Filter for MM search, drastically reduces CPU cost
- Inertialization preserves bone momentum (no floaty crossfade) — critical for weight feel
- Orientation Warping: don't make new anims for slight direction changes, warp the existing ones
- For single-player: go Mover (animation = source of truth). For multiplayer: stay CMC
- Use PoseSearch Debugger in 5.7 to see Chooser Score — shows WHY a clip was picked
- Order of operations MUST be: Chooser → MM → Distance Matching → Warping → Inertialization

### References
- GASP project: E:\UE_Projects\GameAnimationSample (downloaded)
- GASP Animation Modifiers: /Content/Blueprints/AnimModifiers/ (27 modifiers)
- Simon Clavet GDC 2016: "Motion Matching and The Road to Next-Gen Animation" (foundational talk)
- Epic Unreal Fest 2024: "Motion Matching and the Game Animation Sample in UE 5.4"
- Biunivoca tutorials: Motion Matching breakdown parts 1-7
