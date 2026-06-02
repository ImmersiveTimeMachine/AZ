---
name: gasp_cpp_architecture
description: GASP C++ architecture — zero custom C++, 100% Blueprint + engine plugins (Mover, PoseSearch, Chooser), full data flow from input to animation
type: reference
---

# GASP C++ Architecture — 100% Blueprint Project

GASP has NO custom C++ classes. The entire project assembles engine plugin C++ building blocks via Blueprints.

## Engine Plugins Used

Mover, PoseSearch, Chooser, BlendStack, AnimationWarping, MotionWarping, SmartObjects, Locomotor, NetworkPrediction

## Mover Plugin — Movement System

### UCharacterMoverComponent (subclass of UMoverComponent)
- Replaces CharacterMovementComponent
- Manages a state machine of movement modes
- Handles Jump/Crouch/Stance
- Queries: IsCrouching(), IsFalling(), IsOnGround(), IsAirborne()

### Movement Mode State Machine
- `UMovementModeStateMachine` — owns TMap<FName, UBaseMovementMode>
- `UBaseMovementMode` — abstract base with GenerateMove() + SimulationTick()
- `UBaseMovementModeTransition` — evaluates conditions, triggers mode switches
- Default mode names: Walking, Falling, Flying, Swimming

### Default Modes
- **UWalkingMode** — ground traversal, uses UCommonLegacyMovementSettings
- **UFallingMode** — air/jump, AirControlPercentage, terminal speeds, OnLanded delegate

### Movement Settings (UCommonLegacyMovementSettings)
- MaxSpeed=800, Acceleration=4000, Deceleration=4000
- GroundFriction=8.0, TurningRate=500, TurningBoost=8.0
- JumpUpwardsSpeed=500, MaxStepHeight=40
- MaxWalkSlopeCosine=0.71 (~45 deg)

### Stance Settings (UStanceSettings)
- CrouchingMaxSpeed=200, CrouchHalfHeight=55

## Input Flow

```
Enhanced Input (IA_Move, IA_Look)
  --> Blueprint Input Producer (IMoverInputProducerInterface)
  --> FCharacterDefaultInputs:
      MoveInput (FVector), OrientationIntent, ControlRotation
      bIsJumpJustPressed, bIsJumpPressed, SuggestedMovementMode
  --> UMoverComponent::ProduceInput()
  --> Active Mode GenerateMove() --> SimulationTick()
  --> FMoverDefaultSyncState: Location, Orientation, Velocity, MoveDirectionIntent
```

## Layered Moves (Jump, Traversal)
- `FLayeredMoveBase` — time-limited movement effects (additive/override)
- `FLayeredMove_AnimRootMotion` — extracts root motion from montage for traversal
- Queued via UMoverComponent::QueueLayeredMove()

## PoseSearch / Motion Matching Bridge

### UMoverTrajectoryPredictor
- Implements IPoseSearchTrajectoryPredictorInterface
- Takes UMoverComponent* via Setup()
- Predict() fills FTransformTrajectory with history + prediction
- MoverSamplingFrameRate = 60fps
- This is the bridge between Mover and PoseSearch MM node

### UPoseSearchTrajectoryLibrary (Blueprint Function Library)
- PoseSearchGenerateTransformTrajectoryWithPredictor() — for Mover characters
- PoseSearchGenerateTransformTrajectory() — for CMC characters
- HandleTransformTrajectoryWorldCollisions() — gravity + floor collision

## Mover Component Key API
**Delegates:**
- OnPreSimulationTick, OnPostMovement, OnPostSimulationTick
- OnMovementModeChanged, OnPostFinalize
- ProcessGeneratedMovement

**Queries:**
- GetVelocity(), GetMovementIntent(), GetTargetOrientation()
- GetMovementModeName(), GetMovementMode()
- GetPredictedTrajectory(), GetUpDirection(), GetGravityAcceleration()

## Complete Data Flow
```
Enhanced Input
  --> BP Input Producer --> FCharacterDefaultInputs
  --> UCharacterMoverComponent --> MovementModeStateMachine
  --> Active Mode (Walking/Falling) + Layered Moves
  --> FMoverDefaultSyncState (applied to actor)
  --> UMoverTrajectoryPredictor (queries Mover for pos/vel/facing)
  --> AnimBP Motion Matching Node (PoseSearch)
  --> Chooser selects database based on state
  --> BlendStack / SM / Animation Warping
  --> Final Pose Output
```

## Key Takeaway for AZ
The critical integration point is `UMoverTrajectoryPredictor` — bridges UCharacterMoverComponent to PoseSearch MM node. Everything else (Chooser tables, database selection, state transitions) is Blueprint/data-driven.
