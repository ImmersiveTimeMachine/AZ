---
name: gasp_project_settings
description: GASP project settings — DDCVars, blend profiles, physics, network prediction, input actions, plugins, skeleton/IK rigs, gameplay tags
type: reference
---

# GASP Project Settings & Configuration

## Critical DDCVars (Data-Driven Console Variables)

| CVar | Type | Default | Purpose |
|---|---|---|---|
| DDCvar.LocomotionSetupMover | Int | **1** | Mover is DEFAULT locomotion (0=MM, 1=SM+BlendStack) |
| DDCvar.LocomotionSetupCMC | Int | 0 | CMC locomotion config |
| DDCVar.ThreadSafeAnimationUpdate.Enable | Bool | **true** | Thread-safe anim updates enabled |
| DDCVar.NewGameplayCameraSystem.Enable | Bool | **true** | New UE5.7 camera system |
| DDCvar.FootPlacementMode | Int | **1** | 0=off, 1=foot placement node, 2=biped CR |
| DDCVar.AttributeBasedRootMotion.Enable | Bool | false | Root motion toggle (OFF by default) |
| DDCvar.OffsetRootBone.TranslationRadius | Float | 0.0 | OffsetRootBone radius |
| DDCVar.ExperimentalStateMachine.Enable | Bool | false | SM-based animation system |
| DDCvar.MMDatabaseLOD | Int | 0 | Motion Matching database LOD tier |
| DDCvar.StrafeStyle | Int | 1 | Strafe movement style |
| DDCvar.AimStyle | Int | 0 | Aim style |
| DDCvar.CameraStyle | Int | 1 | Camera style (0=Close,1=Medium,2=Far,3=Debug) |
| DDCvar.AnalogInputStyle | Int | 0 | Analog stick behavior |
| DDCvar.PawnClass | Int | -1 | Override pawn class |
| DDCvar.ControlStyle | Int | 0 | Control style |

## Console Variables
```
p.EnableCharacterAccelerationReplication=1
```

## Physics
- bTickPhysicsAsync=**False** (no async physics)

## Network Prediction (Mover)
- PreferredTickingPolicy=Independent
- FixedTickFrameRate=**60**
- SimulatedProxyNetworkLOD=Interpolated
- FixedTickInterpolationBufferedMS=100
- IndependentTickInterpolationBufferedMS=100
- IndependentTickInterpolationMaxBufferedMS=250

## GPU Skinning (animation-relevant)
- r.GPUSkin.Support16BitBoneIndex=True
- r.GPUSkin.UnlimitedBoneInfluences=True
- r.SkinCache.CompileShaders=True
- SkeletalMesh.UseExperimentalChunking=1

## Custom Collision Channel
- ECC_GameTraceChannel1 = "Traversable" (trace type, default ignore)

## Enabled Plugins
PoseSearch, Chooser, Mover, AnimationWarping, MotionWarping, AnimationLocomotionLibrary, NetworkPrediction, Locomotor, CurveExpression, SmartObjects, GameplayInteractions, DrawDebugLibrary, LiveLink, LiveLinkControlRig, RigLogic, HairStrands

## Input Actions (16)
IA_Move, IA_Move_WorldSpace, IA_Look, IA_Look_Gamepad, IA_Jump, IA_Sprint, IA_Crouch, IA_Walk, IA_Aim, IA_Strafe, IA_Traverse, IA_Interact, IA_NextPawn, IA_NextVisualOverride, IA_TeleportToTarget, IA_TwinStick_AimDirection

IMC: IMC_Sandbox at /Game/Input/

## Blend Profiles
Embedded in SK_UEFN_Mannequin skeleton (not standalone assets). Need to check skeleton editor manually. Referenced by S_ChooserOutputs.BlendProfile field and S_BlendStackInputs.BlendProfile field.

## Skeleton Assets
- Primary: **SK_UEFN_Mannequin** at /Game/Characters/UEFN_Mannequin/Meshes/
- IK Rig: **IK_UEFN_Mannequin**
- Retargeters: RTG_UEFN_to_Echo, RTG_UEFN_to_TwinBlast, RTG_UEFN_to_UE4_Mannequin, RTG_UEFN_to_UE5_Mannequin, RTG_UEFN_to_Metahuman

## Blend Spaces
| BlendSpace | Purpose |
|---|---|
| BS_Neutral_AO_Stand | Aim offset with smoothing |
| BS_Neutral_AO_Stand_NoSmoothing | Aim offset no smoothing (used by Mover ABP) |
| BS_Relaxed_Lean_Head | Head lean additive |
| BS_Relaxed_Run_Leans | Run lean additive |
| BS_Relaxed_Walk_Leans | Walk lean additive |
| BS_Relaxed_Run_Lean_FB | Run lean forward/back |
| BS_Relaxed_Run_Lean_LR | Run lean left/right |

AO grid: 16 stand poses, 21 crouch poses at X/Y: {-135 to +135} x {-90 to +90}

## Gameplay Tags (MM related)
MotionMatching, MotionMatching.Default, MotionMatching.Idle, MotionMatching.Loops, MotionMatching.Pivots, MotionMatching.Starts, MotionMatching.Stops

## Misc
- SandboxAnimCurveCompressionSettings at /Game/Misc/ — custom anim curve compression
- AO_Blend_Curve at AimOffset/ — AO blend curve
