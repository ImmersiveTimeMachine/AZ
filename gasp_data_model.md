---
name: gasp_data_model
description: GASP enums (Gait, Stance, MovementMode, MovementState, MovementDirection, RotationMode, CameraMode), structs (all S_ types), curves, data flow
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP Data Model — Enums, Structs, Curves

## Enums

### E_Gait (3 values)
| Index | Value |
|-------|-------|
| 0 | Walk |
| 1 | Run |
| 2 | Sprint |

### E_Stance (2 values)
| Index | Value |
|-------|-------|
| 0 | Standing |
| 1 | Crouching |

### E_MovementMode (4 values, offset from 4)
| Index | Value |
|-------|-------|
| 4 | OnGround (Walking) |
| 5 | InAir (Falling) |
| 6 | Slide |
| 7 | Traversing |

### E_MovementState (2 values)
| Index | Value |
|-------|-------|
| 0 | Idle |
| 4 | Moving |

### E_MovementDirection (6 values)
| Index | Value |
|-------|-------|
| 0 | Forward |
| 1 | Backward |
| 2 | Left (LL) |
| 3 | Right (RR) |
| 4 | Left-Forward (LR) |
| 5 | Right-Forward (RL) |

### E_MovementDirectionBias (2 values)
| Index | Value |
|-------|-------|
| 0 | LeftBias |
| 1 | RightBias |

Used to resolve ambiguous diagonal directions.

### E_RotationMode (3 values)
| Index | Value |
|-------|-------|
| 0 | OrientToMovement |
| 1 | Strafe |
| 2 | Aiming |

### E_AnalogStickBehavior (4 values)
| Index | Value |
|-------|-------|
| 0 | FixedSpeed_SingleGait |
| 1 | FixedSpeed_WalkRun (analog deflection) |
| 2 | VariableSpeed_SingleGait |
| 3 | VariableSpeed_WalkRun |

### E_CameraMode (4 values)
| Index | Value |
|-------|-------|
| 0 | Close |
| 1 | Medium |
| 2 | Far |
| 3 | Debug |

### E_CameraStyle (3 values)
| Index | Value |
|-------|-------|
| 0 | Freecam |
| 1 | Strafe |
| 2 | Aim |

### E_ExperimentalStateMachineState
SM states: IdleLoop(0), LocomotionLoop(1), InAirLoop(2), TransitionToIdle(3), TransitionToLocomotion(4), TransitionToInAir(5), IdleBreak(6), SlideLoop(9)

### E_TraversalActionType
Vault, Hurdle, Mantle, Climb, Catch variations

### E_EarlyTransition_Condition
Used by BP_NotifyState_EarlyTransition: 0=GaitChange, 1=Unconditional

### E_EarlyTransition_Destination
0=Re-Transition, 1=ToLoop

### E_TraversalBlendOutCondition
0=HasAcceleration, 1=IsInAir, 2=Unconditional

## Structs

### S_PlayerInputState
| Field | Type |
|-------|------|
| WantsToSprint | bool |
| WantsToWalk | bool |
| WantsToStrafe | bool |
| WantsToAim | bool |
| WantsToCrouch | bool |

### S_CharacterPropertiesForAnimation
| Field | Type |
|-------|------|
| MovementDirection | E_MovementDirection |
| AimingRotation | Rotator |
| JustLanded | bool |
| LandVelocity | Vector |
| OrientationIntent | Rotator |
| SteeringTime | double |
| GroundNormal | Vector |
| GroundLocation | Vector |

### S_CharacterPropertiesForCamera
| Field | Type |
|-------|------|
| CameraMode | E_CameraMode |
| Stance | E_Stance |
| Gait | E_Gait |

### S_CharacterPropertiesForTraversal
| Field | Type |
|-------|------|
| Mesh | SkeletalMeshComponent* |
| Capsule | CapsuleComponent* |
| MovementMode | E_MovementMode |
| Gait | E_Gait |
| Speed | double |

### S_MoverCustomInputs
| Field | Type |
|-------|------|
| MovementDirection | E_MovementDirection |
| Gait | E_Gait |
| RotationMode | E_RotationMode |
| RotationOffset | double |
| WantsToCrouch | bool |
| ControlRotationRate | double |

### S_BlendStackInputs
| Field | Type |
|-------|------|
| Anim | AnimationAsset* |
| Loop | bool |
| StartTime | double |
| BlendTime | double |
| BlendProfile | BlendProfile* |
| Tags | TArray\<FName\> |

> **Verified 2026-04-25 via live GASP MCP** — `unreal_blueprint_query operation=get_node_pins` on `Set members in S Blend Stack Inputs` (GUID `DF125D0E45C4A41D03F54090BE7126D6`) shows Tags pin `type=TArray<FName>`.
> **CAUTION:** The summary `operation=get_nodes` view truncates `TArray<FName>` to just `name` — only `get_node_pins` (per-node deep query) gives the full type. Always use `get_node_pins` for type fidelity verification.

### S_ChooserOutputs
| Field | Type |
|-------|------|
| UseMM | bool |
| StartTime | double |
| BlendTime | double |
| BlendProfile | FName |
| Tags | TArray\<FName\> |
| MMCostLimit | double |

> Verified 2026-04-25 via `get_node_pins` on `Break S Chooser Outputs` (GUID `899921C148DD9543C8D5B6B6E0C938A5`): Tags pin = `TArray<FName>`, BlendProfile pin = `FName`.

### S_MovementDirectionThresholds
| Field | Type |
|-------|------|
| FL (ForwardLeft) | double |
| FR (ForwardRight) | double |
| BL (BackwardLeft) | double |
| BR (BackwardRight) | double |

### S_RotationOffsetCurveChooser_Inputs
| Field | Type |
|-------|------|
| MovementMode | E_MovementMode |
| MovementDirection | E_MovementDirection |

### S_TraversalCheckInputs
| Field | Type |
|-------|------|
| TraceForwardDirection | Vector |
| TraceForwardDistance | double |
| TraceOriginOffset | Vector |
| TraceEndOffset | Vector |
| TraceRadius | double |
| TraceHalfHeight | double |

### S_TraversalCheckResult
| Field | Type |
|-------|------|
| ActionType | E_TraversalActionType |
| ObstacleDepth | double |
| ObstacleHeight | double |
| HasFrontLedge | bool |
| FrontLedgeLocation | Vector |
| FrontLedgeNormal | Vector |
| HasBackLedge | bool |
| BackLedgeLocation | Vector |
| BackLedgeNormal | Vector |
| HasBackFloor | bool |
| BackFloorLocation | Vector |
| BackLedgeHeight | double |
| HitComponent | PrimitiveComponent* |
| ChosenMontage | AnimMontage* |
| StartTime | double |
| PlayRate | double |

### S_TraversalChooserInputs
| Field | Type |
|-------|------|
| ActionType | E_TraversalActionType |
| HasFrontLedge | bool |
| HasBackLedge | bool |
| HasBackFloor | bool |
| ObstacleHeight | double |
| ObstacleDepth | double |
| BackLedgeHeight | double |
| DistanceToLedge | double |
| MovementMode | E_MovementMode |
| Gait | E_Gait |
| Speed | double |
| PoseHistory | PoseHistoryReference |

### S_TraversalChooserOutputs
| Field | Type |
|-------|------|
| ActionType | E_TraversalActionType |
| MontageStartTime | double |

## Curves
| Curve | Purpose |
|-------|---------|
| Curve_StrafeSpeedMap | Movement direction angle → speed multiplier |
| Curve_RotationOffset_F | Rotation offset for Forward |
| Curve_RotationOffset_B | Rotation offset for Backward |
| Curve_RotationOffset_LL | Rotation offset for Left |
| Curve_RotationOffset_LR | Rotation offset for Left-Forward |
| Curve_RotationOffset_RL | Rotation offset for Right-Forward |
| Curve_RotationOffset_RR | Rotation offset for Right |
| Curve_RotationOffset_Slide_Knees | Rotation offset for Slide |

Selected via CHT_RotationOffsetCurve Chooser by MovementMode + MovementDirection.

## Data Flow
```
Character BP
  ├── S_PlayerInputState → Input processing (Gait, RotationMode)
  ├── S_MoverCustomInputs → Mover system (PreSim/PostSim)
  ├── S_CharacterPropertiesForAnimation → ABP via BPI_SandboxCharacter_Pawn
  ├── S_CharacterPropertiesForCamera → Camera Director
  └── S_CharacterPropertiesForTraversal → AC_TraversalLogic

ABP
  ├── State enums (MovementMode, MovementState, Gait, Stance, RotationMode)
  ├── E_MovementDirection + E_MovementDirectionBias → direction resolution
  ├── S_BlendStackInputs → Experimental SM BlendStack
  ├── S_ChooserOutputs ← Chooser evaluation
  └── S_RotationOffsetCurveChooser_Inputs → CHT_RotationOffsetCurve

AC_TraversalLogic
  ├── S_TraversalCheckInputs → trace geometry
  ├── S_TraversalCheckResult ← geometry analysis
  ├── S_TraversalChooserInputs → CHT_Traversal
  └── S_TraversalChooserOutputs ← montage selection
```

## BFL_HelpfulFunctions (Blueprint Function Library)
Key functions: DrawDebugArrowWithCircle, DrawDebugAngleThresholds, DebugDraw_MultiLineGraph, DebugDraw_BoolStates, GetPawnClassWithCVAR, GetVisualOverrideWithCVAR
