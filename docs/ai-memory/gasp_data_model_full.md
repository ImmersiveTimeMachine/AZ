---
name: GASP Data Model Full
description: Authoritative dump of every GASP struct (S_*), enum (E_*), curve (Curve_*, CHT_RotationOffsetCurve), and BFL_HelpfulFunctions. Field-by-field with AZ-equivalent parity check.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP Data Model — Authoritative Reference

Source: `/Game/Blueprints/Data/` in GASP project (`C:/UnrealEngine/Games/GameAnimationSample/Content/Blueprints/Data/`).
Audited 2026-05-03 by parsing `.uasset` binaries and live editor reflection on the AZ-imported copy.

AZ equivalents: `Source/AZ/Public/Animation/AZ_LocomotionTypes.h`.

---

## 1. ENUMS

UserDefinedEnums use opaque internal names (`NewEnumeratorN`) plus a `DisplayNameMap` that supplies the human-readable label. The **value→display** mapping below is the ground truth.

### 1.1 E_Gait

| Value | Display | AZ EAZ_Gait | Match? |
|---|---|---|---|
| 0 | Walk | Walk | OK |
| 1 | Run | Run | OK |
| 2 | Sprint | Sprint | OK |

### 1.2 E_Stance

| Value | Display | AZ EAZ_Stance | Match? |
|---|---|---|---|
| 0 | Stand | Standing | LABEL DIFF (`Stand` vs `Standing`) |
| 1 | Crouch | Crouching | LABEL DIFF (`Crouch` vs `Crouching`) |

### 1.3 E_MovementMode

| Value | Display | AZ EAZ_MovementMode | Match? |
|---|---|---|---|
| 0 | OnGround | OnGround | OK |
| 1 | InAir | InAir | OK |
| 2 | Sliding | Slide | LABEL DIFF (`Sliding` vs `Slide`) |
| 3 | Traversing | Traversing | OK |

NOTE: GASP's UserDefinedEnum stores the values internally as `NewEnumerator4..7` due to deletion history, but the actual runtime int values are 0..3 (UDE renumbers contiguously). AZ matches numerically. Choosers compare by display name string under the hood, so a label mismatch can break Chooser parity.

### 1.4 E_MovementState

| Value | Display | AZ EAZ_MovementState | Match? |
|---|---|---|---|
| 0 | Moving | Idle | **VALUE FLIPPED** (AZ has Idle=0, Moving=1) |
| 1 | Idle | Moving | **VALUE FLIPPED** |

CRITICAL DIVERGENCE: AZ has `Idle=0, Moving=1`. GASP has `Moving=0, Idle=1`. Any int<->enum cast or persisted state crossing the boundary will silently swap meaning.

### 1.5 E_MovementDirection

| Value | Display | AZ EAZ_MovementDirection | Match? |
|---|---|---|---|
| 0 | F | F | OK |
| 1 | B | B | OK |
| 2 | LR | LL | **VALUE SWAPPED** (AZ LL is 2, GASP LR is 2) |
| 3 | LL | LR | **VALUE SWAPPED** |
| 4 | RL | RL | OK |
| 5 | RR | RR | OK |

CRITICAL DIVERGENCE: AZ swapped `LL`<->`LR` for indices 2 and 3. AZ's enum comments document `LL=Left side, Left foot leading` at value 2, but GASP labels value 2 as `LR`. This affects strafe foot-phase selection in any Chooser/MM lookup that crosses GASP and AZ data.

### 1.6 E_MovementDirectionBias

| Value | Display | AZ EAZ_MovementDirectionBias | Match? |
|---|---|---|---|
| 0 | LeftFootForward | LeftBias | **LABEL DIFF** |
| 1 | RightFootForward | RightBias | **LABEL DIFF** |

### 1.7 E_RotationMode

| Value | Display | AZ EAZ_RotationMode | Match? |
|---|---|---|---|
| 0 | OrientToMovement | OrientToMovement | OK |
| 1 | Strafe | Strafe | OK |
| 2 | Aim | Aiming | LABEL DIFF (`Aim` vs `Aiming`) |

### 1.8 E_ExperimentalStateMachineState

| Value | Display | AZ EAZ_StateMachineState | Match? |
|---|---|---|---|
| 0 | Idle Loop | IdleLoop | OK |
| 1 | Locomotion Loop | TransitionToIdle | **VALUE MISMATCH** |
| 2 | In Air Loop | LocomotionLoop | **VALUE MISMATCH** |
| 3 | Transition to Idle Loop | TransitionToLocomotion | **VALUE MISMATCH** |
| 4 | Transition to Locomotion Loop | InAirLoop | **VALUE MISMATCH** |
| 5 | Transition to In Air Loop | TransitionToInAir | **VALUE MISMATCH** |
| 6 | Idle Break | IdleBreak | OK |
| 7 | Transition to Slide | TransitionToSlide | OK |
| 8 | Slide Loop | SlideLoop | OK |

CRITICAL DIVERGENCE: AZ comment claims "Values MUST match GASP E_ExperimentalStateMachineState for chooser parity" but the values 1..5 are reordered. GASP order is Loop-then-Transition pairs (IdleLoop, LocomotionLoop, InAirLoop, then the three Transitions). AZ alternates them (IdleLoop, TransitionToIdle, LocomotionLoop, TransitionToLocomotion, InAirLoop, TransitionToInAir). Any Chooser or anim asset that compares these enums by integer will produce wrong results.

### 1.9 E_TraversalActionType

| Value | Display | AZ EAZ_TraversalActionType | Match? |
|---|---|---|---|
| 0 | None | None | OK |
| 1 | Vault | Hurdle | **VALUE MISMATCH** |
| 2 | Hurdle | Vault | **VALUE MISMATCH** |
| 3 | Mantle | Mantle | OK |

CRITICAL DIVERGENCE: AZ swaps Hurdle (1) and Vault (2). GASP Vault=1, Hurdle=2.

### 1.10 E_AnalogStickBehavior

| Value | Display | AZ EAZ_AnalogStickBehavior | Match? |
|---|---|---|---|
| 0 | Fixed Speed - Single Gait | FixedSpeed_SingleGait | OK |
| 1 | Fixed Speed - Walk / Run | FixedSpeed_WalkRun | OK |
| 2 | Variable Speed - Single Gait | VariableSpeed_SingleGait | OK |
| 3 | Variable Speed - Walk / Run | VariableSpeed_WalkRun | OK |

Tooltips (per value, from binary):
- 0: Character will move at a fixed speed regardless of stick deflection.
- 1: Character will move at a fixed walking speed with slight stick deflection, and a fixed running speed at full stick deflection.
- 2: Full analog movement control with stick, character will remain walking or running based on gait input.
- 3: Full analog movement control with stick, character will switch from walk to run gait based on stick deflection.

---

## 2. STRUCTS

Field native names extracted from `VariablesDescriptions[].VarName` in the binary (`<Name>_<idx>_<GUID>` triplet pattern). Property TYPE inferred from accompanying type-keyword tokens (BoolProperty, DoubleProperty, EnumProperty, etc.) and cross-referenced asset paths.

### 2.1 S_BlendStackInputs (Tooltip: "Struct used in Experimental State Machine to drive Blend Stack inputs")

| Idx | Name | Type | AZ FAZ_BlendStackInputs | Match? |
|---|---|---|---|---|
| 3 | Anim | TObjectPtr<UAnimationAsset> | Anim | OK |
| 10 | Loop | bool | bLoop | **NAME DIFF** (`Loop` vs `bLoop`) |
| 13 | StartTime | double | StartTime | OK |
| 15 | BlendTime | double | BlendTime | OK |
| 19 | Tags | TArray<FName> | Tags | OK |
| 30 | BlendProfile | TObjectPtr<UBlendProfile> | BlendProfile | OK |

NAMING DIVERGENCE: GASP uses bare `Loop`. AZ added `b` prefix. UE BP boolean fields conventionally drop the `b` prefix in display. This may be cosmetic but breaks string-based serialization parity.

### 2.2 S_ChooserOutputs

| Idx | Name | Type | AZ FAZ_ChooserOutputs | Match? |
|---|---|---|---|---|
| 1 | UseMM | bool | bUseMM | **NAME DIFF** |
| 6 | StartTime | double | StartTime | OK |
| 10 | Tags | TArray<FName> | Tags | OK |
| 14 | MMCostLimit | double | MMCostLimit | OK |
| 17 | BlendTime | double | BlendTime | OK |
| 20 | BlendProfile | FName | BlendProfile | OK |

### 2.3 S_PlayerInputState

| Idx | Name | Type | AZ FAZ_PlayerInputState | Match? |
|---|---|---|---|---|
| 1 | WantsToSprint | bool | bWantsToSprint | **NAME DIFF** |
| 3 | WantsToWalk | bool | bWantsToWalk | **NAME DIFF** |
| 5 | WantsToStrafe | bool | bWantsToStrafe | **NAME DIFF** |
| 7 | WantsToAim | bool | bWantsToAim | **NAME DIFF** |
| 9 | WantsToCrouch | bool | bWantsToCrouch | **NAME DIFF** |

### 2.4 S_MoverCustomInputs

| Idx | Name | Type | AZ FAZ_MoverCustomInputs | Match? |
|---|---|---|---|---|
| 2 | MovementDirection | E_MovementDirection | MovementDirection | OK |
| 5 | Gait | E_Gait | Gait | OK |
| 11 | RotationMode | E_RotationMode | RotationMode | OK |
| 14 | RotationOffset | double | RotationOffset | OK |
| 28 | WantsToCrouch | bool | bWantsToCrouch | **NAME DIFF** |
| 29 | ControlRotationRate | double | ControlRotationRate | OK |

### 2.5 S_CharacterPropertiesForAnimation

| Idx | Name | Type | AZ FAZ_CharacterPropertiesForAnimation | Match? |
|---|---|---|---|---|
| 2 | MovementMode | E_MovementMode | MovementMode | **MISSING in AZ** |
| 5 | RotationMode | E_RotationMode | — | **MISSING in AZ** |
| 9 | Stance | E_Stance | — | **MISSING in AZ** |
| 12 | Gait | E_Gait | — | **MISSING in AZ** |
| 16 | ActorTransform | FTransform | — | **MISSING in AZ** |
| 19 | Velocity | FVector | — | **MISSING in AZ** |
| 21 | InputAcceleration | FVector | — | **MISSING in AZ** |
| 24 | CurrentMaxAcceleration | double | — | **MISSING in AZ** |
| 26 | CurrentMaxDeceleration | double | — | **MISSING in AZ** |
| 29 | AimingRotation | FRotator | AimingRotation | OK |
| 32 | InputState | S_PlayerInputState | — | **MISSING in AZ** |
| 35 | JustLanded | bool | bJustLanded | NAME DIFF |
| 40 | LandVelocity | FVector | LandVelocity | OK |
| 49 | OrientationIntent | FRotator | OrientationIntent | OK |
| 52 | SteeringTime | double | SteeringTime | OK |
| 55 | MovementDirection | E_MovementDirection | MovementDirection | OK |
| 58 | GroundNormal | FVector | GroundNormal | OK |
| 62 | GroundLocation | FVector | GroundLocation | OK |

CRITICAL DIVERGENCE: AZ is missing 9 fields. Since AZ already exposes most of these state values directly on AAZ_HeroPawn / AZ_AnimInstance, the struct is currently a thin subset. If you're feeding GASP-derived helper functions or chooser bindings the struct directly, the missing fields will fail.

### 2.6 S_CharacterPropertiesForCamera

| Idx | Name | Type | AZ FAZ_CharacterPropertiesForCamera | Match? |
|---|---|---|---|---|
| 6 | CameraMode | uint8 | CameraMode | OK |
| 9 | CameraStyle | (likely FName/Enum) | — | **MISSING in AZ** |
| 10 | Stance | E_Stance | Stance | OK |
| 23 | Gait | E_Gait | Gait | OK |

### 2.7 S_CharacterPropertiesForTraversal

| Idx | Name | Type | AZ FAZ_CharacterPropertiesForTraversal | Match? |
|---|---|---|---|---|
| 12 | Gait | E_Gait | Gait | OK |
| 15 | Mesh | TObjectPtr<USkeletalMeshComponent> | Mesh | OK |
| 18 | MotionWarping | TObjectPtr<UMotionWarpingComponent> | — | **MISSING in AZ** |
| 21 | Capsule | TObjectPtr<UCapsuleComponent> | Capsule | OK |
| 22 | MovementMode | E_MovementMode | MovementMode | OK |
| 26 | Speed | double | Speed | OK |

### 2.8 S_DebugGraphLineProperties

| Idx | Name | Type | AZ | Match? |
|---|---|---|---|---|
| 8 | Values | TArray<double> | — | **MISSING in AZ** |
| 9 | Color | FLinearColor | — | **MISSING in AZ** |
| 10 | Name | FString | — | **MISSING in AZ** |

NOTE: AZ has no equivalent (debug-only struct).

### 2.9 S_MovementDirectionThresholds

| Idx | Name | Type | AZ FAZ_MovementDirectionThresholds | Match? |
|---|---|---|---|---|
| 3 | FL | double | FL | OK |
| 5 | FR | double | FR | OK |
| 7 | BL | double | BL | OK |
| 9 | BR | double | BR | OK |

NOTE: Defaults in GASP are not visible in our parse (binary defaults serialised separately). AZ uses `FL=FR=55.0`, `BL=BR=125.0` — confirm against GASP at the Chooser/BFL call sites.

### 2.10 S_RotationOffsetCurveChooser_Inputs

| Idx | Name | Type | AZ FAZ_RotationOffsetChooserInputs | Match? |
|---|---|---|---|---|
| 2 | MovementDirection | E_MovementDirection | MovementDirection | OK |
| 5 | MovementMode | E_MovementMode | MovementMode | OK |

### 2.11 S_TraversalCheckInputs

| Idx | Name | Type | AZ FAZ_TraversalCheckInputs | Match? |
|---|---|---|---|---|
| 2 | TraceForwardDistance | double | TraceForwardDistance | OK |
| 5 | TraceForwardDirection | FVector | TraceForwardDirection | OK |
| 7 | TraceOriginOffset | FVector | TraceOriginOffset | OK |
| 10 | TraceRadius | double | TraceRadius | OK |
| 12 | TraceHalfHeight | double | TraceHalfHeight | OK |
| 15 | TraceEndOffset | FVector | TraceEndOffset | OK |

### 2.12 S_TraversalCheckResult

| Idx | Name | Type | AZ FAZ_TraversalCheckResult | Match? |
|---|---|---|---|---|
| 2 | ActionType | uint8 (E_TraversalActionType byte) | ActionType | OK |
| 17 | ObstacleDepth | double | ObstacleDepth | OK |
| 25 | HasBackFloor | bool | bHasBackFloor | NAME DIFF |
| 34 | HasFrontLedge | bool | bHasFrontLedge | NAME DIFF |
| 41 | HasBackLedge | bool | bHasBackLedge | NAME DIFF |
| 50 | FrontLedgeLocation | FVector | FrontLedgeLocation | OK |
| 51 | FrontLedgeNormal | FVector | FrontLedgeNormal | OK |
| 55 | BackLedgeLocation | FVector | BackLedgeLocation | OK |
| 56 | BackLedgeNormal | FVector | BackLedgeNormal | OK |
| 58 | BackFloorLocation | FVector | BackFloorLocation | OK |
| 63 | ObstacleHeight | double | ObstacleHeight | OK |
| 64 | BackLedgeHeight | double | BackLedgeHeight | OK |
| 68 | HitComponent | TObjectPtr<UPrimitiveComponent> | HitComponent | OK |
| 71 | ChosenMontage | TObjectPtr<UAnimMontage> | ChosenMontage | OK |
| 74 | PlayRate | double | PlayRate | OK |
| 76 | StartTime | double | StartTime | OK |

### 2.13 S_TraversalChooserInputs

| Idx | Name | Type | AZ FAZ_TraversalChooserInputs | Match? |
|---|---|---|---|---|
| 3 | ActionType | uint8 | ActionType | OK |
| 6 | ObstacleHeight | double | ObstacleHeight | OK |
| 12 | Speed | double | Speed | OK |
| 14 | ObstacleDepth | double | ObstacleDepth | OK |
| 15 | Gait | E_Gait | Gait | OK |
| 19 | HasFrontLedge | bool | bHasFrontLedge | NAME DIFF |
| 20 | HasBackLedge | bool | bHasBackLedge | NAME DIFF |
| 21 | HasBackFloor | bool | bHasBackFloor | NAME DIFF |
| 28 | BackLedgeHeight | double | BackLedgeHeight | OK |
| 29 | MovementMode | E_MovementMode | MovementMode | OK |
| 32 | PoseHistory | FPoseHistoryReference (PoseSearch) | — | **MISSING in AZ** |
| 35 | DistanceToLedge | double | DistanceToLedge | OK |

CRITICAL: AZ is missing `PoseHistory` (FPoseHistoryReference). Without it, traversal Chooser cannot evaluate motion-matching cost — it must be wired through any C++ Make/Break that constructs this struct for a Chooser call.

### 2.14 S_TraversalChooserOutputs

| Idx | Name | Type | AZ FAZ_TraversalChooserOutputs | Match? |
|---|---|---|---|---|
| 3 | ActionType | uint8 | ActionType | OK |
| 18 | MontageStartTime | double | MontageStartTime | OK |

---

## 3. CURVES (CurveFloat)

All are `UCurveFloat`. Sampled at 21 evenly-spaced points across the time range. Tangents/interp modes are protected and not exposed via Python; the binaries indicate `RichCurveKey` / `ERichCurveExtrapolation` standard storage. AZ has no equivalent assets — these are expected to be re-imported at the same paths in AZ if you want to feed Chooser_RotationOffset.

### 3.1 Curve_RotationOffset_F (Forward direction; identity)
- Time range: -135 to +135  →  Value range: -135 to +135
- Profile: y = x (linear identity). Used to pass through actor yaw delta unmodified for forward locomotion.

### 3.2 Curve_RotationOffset_B (Backward; sign-flipping S-curve)
- Time range: -180 to +180  →  Value range: -60 to +60
- Profile: At -180→0; rises to +60 around -90 to -54; back to 0 at 0; -60 at 90; back to 0 at 180. Used to invert offset for backward motion so character faces away from movement.

### 3.3 Curve_RotationOffset_LL (Left-side, Left foot leading)
- Time range: -180 to +180  →  Value range: 0 to +60
- Profile: Zero outside [-90,+90]; rises from 0 at ±90 to peak +60 at 0. Drives the orient-vs-strafe blend for left-leg-leading strafe loop.

### 3.4 Curve_RotationOffset_LR (Left-side, Right foot leading)
- Time range: -180 to +180  →  Value range: -60 to 0
- Mirror of LL: 0 in center [-90,+90], dips to -60 at ±180.

### 3.5 Curve_RotationOffset_RL (Right-side, Left foot leading)
- Time range: -180 to +180  →  Value range: 0 to +60
- Mirror of LR (positive values at ±180, zero in center).

### 3.6 Curve_RotationOffset_RR (Right-side, Right foot leading)
- Time range: -180 to +180  →  Value range: -60 to 0
- Inverted profile of LL (negative peak at 0).

### 3.7 Curve_RotationOffset_Slide_Knees
- Time range: -180 to +180  →  Value range: ~-77 to +77
- Continuous sinusoidal-like profile. Drives knee additive offset during slide pose blend.

### 3.8 Curve_StrafeSpeedMap
- Time range: 0 to +180  →  Value range: 0 to +2
- Step-like remap: 0 below 45°, ramps to 1 by 81°, plateau through 99°, ramps to 2 by 135°, plateau to 180°. Used to convert |yaw delta| into a strafe-speed multiplier (1× walk, 2× run for higher delta).

---

## 4. CHT_RotationOffsetCurve (Chooser Table)

- **Class**: `ChooserTable`
- **ResultType**: `OBJECT_RESULT`
- **OutputObjectType**: `CurveFloat`
- **Context input struct**: `S_RotationOffsetCurveChooser_Inputs` (MovementDirection + MovementMode)
- **Output assets** (referenced from binary):
  - `/Game/Blueprints/Data/Curve_RotationOffset_F`
  - `/Game/Blueprints/Data/Curve_RotationOffset_B`
  - `/Game/Blueprints/Data/Curve_RotationOffset_LL`
  - `/Game/Blueprints/Data/Curve_RotationOffset_LR`
  - `/Game/Blueprints/Data/Curve_RotationOffset_RL`
  - `/Game/Blueprints/Data/Curve_RotationOffset_RR`
  - `/Game/Blueprints/Data/Curve_RotationOffset_Slide_Knees`
- **Columns**: at least one `EnumColumn` bound to `MovementDirection` (matches `EEnumColumnCellValueComparison::MatchEqual`); MovementMode appears as a secondary filter for Slide rows (specifically `E_MovementMode::NewEnumerator6` = `Sliding`).
- **Consumer**: AnimBP samples this Chooser per tick to pick a rotation-offset curve based on current movement direction and (when sliding) MovementMode. The selected curve is then sampled at `RootRotationDelta_Yaw` to produce a smooth rotation offset for the additive offset-from-root pose.

AZ status: AZ has the matching `FAZ_RotationOffsetChooserInputs` struct. It is **missing the curves and the Chooser asset** — these are reference-only for now; if AZ wants the same offset behaviour they must be re-imported under the same names or the C++ logic must hard-code the lookup.

---

## 5. BFL_HelpfulFunctions

Blueprint Function Library, parent `UBlueprintFunctionLibrary`. Path: `/Game/Blueprints/Data/BFL_HelpfulFunctions`. No instance variables. Contains 10 static utility functions, all debug-draw / CVAR helpers. None contain locomotion logic. Summary:

| Function | Inputs | Outputs | Purpose |
|---|---|---|---|
| `DrawDebugArrowWithCircle` | DrawCircle, DrawAxis, DrawArrow, DrawString, String, StringOffset, Center, Direction, Offset, Radius, Length, Size, Segments, Color, Duration, Thickness, WCO | — | Composite debug primitive: ring + axes + arrow + label at world location. Used for orientation visualisation. |
| `DrawDebugAngleThresholds` | Center, Offset, InRotation, YawAngles[], StartRadius, EndRadius, Color, Duration, Thickness, WCO | — | Draws radial spokes at given yaw angles between two radii. Used to visualise FL/FR/BL/BR thresholds in `S_MovementDirectionThresholds`. |
| `DebugDraw_MultiLineGraph` | Location, Rotation, Offset, X Length, Y Length, GraphSize, MinValue, MaxValue, Xaxis Label, Yaxis Label, Lines (TArray<S_DebugGraphLineProperties>), WCO | — | Renders a line graph in world space using arrays of values+colors per line. Used for plotting locomotion metrics. |
| `DebugDraw_BoolStates` | Location, Rotation, Offset, BoolNames[], BoolValues[], WCO | — | Renders aligned label+state grid (true/false) in world space. Used to show flag state on the character. |
| `DebugDraw_StringArray` | Location, Rotation, Offset, Label, Prefix, Strings[], Highlighted String, Highlight, WCO | — | Renders a vertical list of strings with one optionally highlighted. Used for showing chooser candidate names. |
| `DebugDraw_ObjectNameArray` | Location, Rotation, Offset, ArrayLabel, Objects[], WCO | — | Wraps `GetObjectNames` + `DebugDraw_StringArray` for object reference debugging. |
| `AddToStringHistoryArray` | InOutValues (ref), NewValue, MaxHistoryCount, WCO | — | Appends `NewValue` to `InOutValues` and trims the array to `MaxHistoryCount` entries (rolling history buffer). |
| `GetObjectNames` | Objects[], WCO | TArray<FString> | Helper: returns display names of an object array. Implemented as ForEachLoop calling `Get Display Name`. |
| `GetPawnClassWithCVAR` | PawnClasses[], DefaultPawnClass, WCO | Pawn Class | Picks a pawn class from an array using `GetConsoleVariableIntValue`. Returns DefaultPawnClass if the index is invalid. CVar name expected: project-specific. |
| `GetVisualOverrideWithCVAR` | VisualOverrides[], WCO | Actor* | Same pattern: indexes a visual override array via console variable. |

AZ status: None of these are reused (AZ has its own debug pipeline). No port required.

---

## 6. Other Data/ assets

- **BPI_InteractionTransform** (Interface): three functions `SetInteractionTransform_Old(FTransform)`, `GetInteractionTransform_Old() -> FTransform`, `Get_PoseHistory_Old() -> FPoseHistoryReference`. Referenced by traversal montage targets to expose interaction frames. AZ has no equivalent — would be needed if traversal code is ported.

---

## 7. SUMMARY OF AZ DIVERGENCES (the gap list)

### Enum value/order divergences (will silently break Chooser/Mover net replication parity):

1. **EAZ_MovementState** — values flipped vs GASP. AZ `Idle=0, Moving=1`; GASP `Moving=0, Idle=1`.
2. **EAZ_MovementDirection** — `LL` and `LR` swapped at indices 2 and 3. AZ `LL=2, LR=3`; GASP `LR=2, LL=3`.
3. **EAZ_StateMachineState** — values 1..5 reordered. GASP groups Loops first (0/1/2 = Idle/Locomotion/InAir), Transitions next (3/4/5). AZ alternates Loop/Transition pairs.
4. **EAZ_TraversalActionType** — Vault and Hurdle swapped. AZ `Hurdle=1, Vault=2`; GASP `Vault=1, Hurdle=2`.

### Enum display-label divergences (cosmetic, but breaks BP refactor + Chooser inspector readability):

5. **EAZ_Stance** — `Standing/Crouching` vs GASP `Stand/Crouch`.
6. **EAZ_MovementMode** — `Slide` vs GASP `Sliding`.
7. **EAZ_RotationMode** — `Aiming` vs GASP `Aim`.
8. **EAZ_MovementDirectionBias** — `LeftBias/RightBias` vs GASP `LeftFootForward/RightFootForward`.

### Struct boolean-prefix divergences (`b` prefix added in AZ, not in GASP — affects FName-keyed lookups):

9. **FAZ_BlendStackInputs.bLoop** — GASP: `Loop`.
10. **FAZ_ChooserOutputs.bUseMM** — GASP: `UseMM`.
11. **FAZ_PlayerInputState.bWantsTo*** (all 5) — GASP: `WantsTo*` (no `b`).
12. **FAZ_MoverCustomInputs.bWantsToCrouch** — GASP: `WantsToCrouch`.
13. **FAZ_TraversalCheckResult.bHasFrontLedge / bHasBackLedge / bHasBackFloor** — GASP: `HasFrontLedge / HasBackLedge / HasBackFloor`.
14. **FAZ_TraversalChooserInputs.bHasFrontLedge / bHasBackLedge / bHasBackFloor** — GASP: same as 13.
15. **FAZ_CharacterPropertiesForAnimation.bJustLanded** — GASP: `JustLanded`.

### Missing struct fields:

16. **FAZ_CharacterPropertiesForAnimation** — missing 9 of 18 GASP fields:
    - `MovementMode` (E_MovementMode)
    - `RotationMode` (E_RotationMode)
    - `Stance` (E_Stance)
    - `Gait` (E_Gait)
    - `ActorTransform` (FTransform)
    - `Velocity` (FVector)
    - `InputAcceleration` (FVector)
    - `CurrentMaxAcceleration` (double)
    - `CurrentMaxDeceleration` (double)
    - `InputState` (S_PlayerInputState nested)
17. **FAZ_CharacterPropertiesForCamera** — missing `CameraStyle`.
18. **FAZ_CharacterPropertiesForTraversal** — missing `MotionWarping` (TObjectPtr<UMotionWarpingComponent>).
19. **FAZ_TraversalChooserInputs** — missing `PoseHistory` (FPoseHistoryReference). This is the Motion-Matching pose-history feed; without it Chooser cannot do MM cost evaluation for traversal.

### Missing assets (no AZ equivalents):

20. **CHT_RotationOffsetCurve** Chooser asset — no AZ replacement.
21. **Curve_RotationOffset_F/B/LL/LR/RL/RR/Slide_Knees** — 7 CurveFloat assets used by RotationOffset chooser. AZ has none.
22. **Curve_StrafeSpeedMap** — used by speed remapping. AZ has none.
23. **BPI_InteractionTransform** — interface for traversal interaction transforms. AZ has no equivalent.

### Not divergent / debug-only:

- `S_DebugGraphLineProperties` (debug-only, no AZ equivalent needed).
- `BFL_HelpfulFunctions` (10 debug helpers; AZ uses its own debug pipeline).

---

## 8. RECOMMENDED REMEDIATION ORDER

If matching GASP exactly is the goal:

1. **Fix enum values** (highest priority — breaks Chooser numerics and net serialisation): EAZ_MovementState, EAZ_MovementDirection, EAZ_StateMachineState, EAZ_TraversalActionType.
2. **Rename enum values to match labels** (Chooser inspector + display): EAZ_Stance, EAZ_MovementMode (Slide→Sliding), EAZ_RotationMode (Aiming→Aim), EAZ_MovementDirectionBias.
3. **Rename boolean fields** (drop `b` prefix to match GASP names; this affects FString-keyed property lookups and Chooser bindings): all 7 structs listed above.
4. **Add missing fields to FAZ_CharacterPropertiesForAnimation, FAZ_CharacterPropertiesForCamera, FAZ_CharacterPropertiesForTraversal, FAZ_TraversalChooserInputs**.
5. **Re-import the 8 curve assets and CHT_RotationOffsetCurve** to identical paths if the rotation-offset behaviour is desired.
