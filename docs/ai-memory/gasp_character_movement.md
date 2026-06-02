---
name: gasp_character_movement
description: GASP character pawn (Mover-based APawn), movement modes, input flow, interfaces, traversal, camera — full BP architecture
type: reference
---

# GASP Character & Movement System

## SandboxCharacter_Mover — Main Pawn

**Parent:** APawn (NOT ACharacter — uses Mover plugin instead of CMC)

### Components
- CharacterMover (UCharacterMoverComponent) — core movement
- SkeletalMesh (USkeletalMeshComponent)
- Camera (UCameraComponent) — legacy
- GameplayCamera (UGameplayCameraComponent) — new UE5.7 camera system
- AC_TraversalLogic — traversal/parkour
- AC_FoleyEvents — audio
- AC_VisualOverrideManager — mesh swapping

### Key Variables

**Mover (Pre/Post Sim pattern):**
- MoverDefaultInputs_PreSim / _PostSim (CharacterDefaultInputs)
- MoverCustomInputs_PreSim / _PostSim (S_MoverCustomInputs)
- MovementModeMap (Map<FName, byte>) — maps Mover mode names to E_MovementMode
- FloorNormal, FloorLocation

**Input:**
- PlayerInputState (S_PlayerInputState): WantsToSprint, WantsToWalk, WantsToStrafe, WantsToAim, WantsToCrouch

### Pre/Post Simulation Input Pattern (Critical)
- PreSim: set locally before Mover simulates, NOT replicated
- PostSim: retrieved FROM Mover after sim, ARE replicated by Mover
- Animation/camera ALWAYS use PostSim for network correctness

### Input Flow
```
EnhancedInput (IA_Move, IA_Look, IA_Jump, IA_Sprint, IA_Crouch, IA_Walk, IA_Strafe, IA_Aim)
  --> Update PlayerInputState on pawn
  --> Interface functions derive: Get_Gait(), Get_RotationMode(), Get_OrientationIntent()
  --> Pack into CharacterDefaultInputs + S_MoverCustomInputs
  --> Send to Mover
  --> Movement modes read from InputCollection during sim
```

### Key Functions

**Get_Gait() -> E_Gait:**
- WantsToSprint=false: check WantsToWalk → Walk, else check analog stick deflection > 0.8 → Run vs Walk
- WantsToSprint=true: OrientToMovement/Aim → Sprint; Strafe → only if input aligns with facing

**Get_RotationMode() -> E_RotationMode:**
- TargetedActor → Aim if WantsToAim, else Strafe
- TwinStickMode → check right stick
- Otherwise: WantsToAim → Aim, WantsToStrafe → Strafe, else OrientToMovement

**Get_OrientationIntent() -> Vector:**
- Walking+input: OrientToMovement → MoveInput direction; Strafe/Aim → AimingRotation
- Walking-input: OrientToMovement/Strafe → keep last; Aim → update if delta > 60° (turn in place)
- Falling: OrientToMovement → keep last; Strafe/Aim → AimingRotation
- Sliding: OrientToMovement → velocity dir; Strafe/Aim → AimingRotation

**CacheInputsFromMover():** Gets GetLastInputCmd from Mover, stores as _PostSim variables. Critical for anim/camera sync.

## Movement Modes

### BP_MovementMode_Walking (parent: USmoothWalkingMode)
Speed/accel per gait:
- Walk: WalkSpeed, WalkAcceleration
- Run: RunSpeed, RunAcceleration  
- Sprint: SprintSpeed, SprintAcceleration
- Crouch: CrouchSpeed

**GenerateWalkMove override:**
1. Call parent
2. Set MaxSpeedOverride by Gait (with crouch override)
3. Set Acceleration by Gait
4. Set Deceleration: StoppingDeceleration (no input) or GaitChangeDeceleration
5. Set TurningStrength: interpolated walk→sprint based on velocity
6. Set FacingSmoothingTime: considers facing delta, velocity, idle/moving times
7. Apply RotationOffset from MoverCustomInputs to DesiredFacing

### BP_MovementMode_Falling (parent: UFallingMode)
- Overrides GenerateMove (33 nodes)

### BP_MovementMode_Slide (parent: USmoothWalkingMode)
- InitialBoost: time-limited speed boost on slide start
- Slope-dependent: SteepSlopeSpeed, ShallowSlopeSpeed, FlatGroundSpeed
- Deceleration varies by slope angle

### Transitions
- Walking → Sliding: speed > 380 AND tag Mover.Stance.IsCrouching
- Sliding → Walking: speed <= 200 OR not crouching
- Walking ↔ Falling: engine Mover code (ground detection)

## Interfaces

### BPI_SandboxCharacter_Pawn (4 functions)
1. Get_PropertiesForAnimation() → S_CharacterPropertiesForAnimation
2. Get_PropertiesForCamera() → S_CharacterPropertiesForCamera
3. Get_PropertiesForTraversal() → S_CharacterPropertiesForTraversal
4. Set_CharacterInputState(S_PlayerInputState)

### BPI_SandboxCharacter_ABP (6 functions)
1. Get_PoseHistory() → PoseHistoryReference
2. Get_InteractionTransform() → Transform
3. Set_InteractionTransform(Transform)
4. Set_NotifyTransition_ReTransition(bool)
5. Set_NotifyTransition_ToLoop(bool)
6. Get_Gait() → E_Gait

## Traversal (AC_TraversalLogic)
- TryTraversalAction(): capsule trace → select montage → play with motion warping
- Jump input tries traversal first, only jumps if check fails
- Toggles client-authoritative replication during traversals
- DoingTraversalAction bool prevents overlapping

## Camera System (Dual)
- New: GameplayCameraComponent (DDCVar.NewGameplayCameraSystem.Enable)
- Legacy: CameraComponent fallback
- Camera style cycling via D-pad (Close/Medium/Far/Debug)
- CHT_CameraRig Chooser table selects camera rig

## GameMode (GM_Sandbox)
- PawnClasses array — switchable via console var
- CyclePawn/CycleVisualOverride functions
- GetDefaultPawnClassForController reads from DDCVar

## PlayerController (PC_Sandbox)
- IA_NextPawn → cycles pawn class
- IA_NextVisualOverride → cycles visual
- IA_TeleportToTarget → teleport

## E_MovementMode Values
Walking(4), Falling(5), Sliding(6), Traversing(7)
