#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "Weapon/AZ_WeaponTypes.h"
#include "Animation/TrajectoryTypes.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "AZ_AnimInstance.generated.h"

class UCharacterMovementComponent;
class UAbilitySystemComponent;
class UPoseSearchDatabase;
class AAZ_HeroPawn;

UCLASS()
class AZ_API UAZ_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ========================================
	// BLENDSPACE INPUTS
	// ========================================

	/** Normalized forward/backward speed (-1 to 1) for the 2D locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float NormalizedWalkForwardSpeed;

	/** Normalized right/left speed (-1 to 1) for the 2D locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float NormalizedWalkRightSpeed;

	/** Raw ground speed (XY plane magnitude) in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float GroundSpeed;

	/** The maximum ground speed used to normalize blendspace inputs.
	 *  This should cover the full speed range (walk + sprint). Default 500. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Movement")
	float MaxGroundSpeed = 500.f;

	// ========================================
	// WEAPON STATE (driven by GAS tags)
	// ========================================

	/** Current weapon gameplay tag from the ASC. Used by ABP to select the correct locomotion state machine. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	FGameplayTag CurrentWeaponTag;

	/** Index for Blend Poses by int: 0=Unarmed, 1=Rifle, 2=Pistol, 3=Shotgun, 4=Melee, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	int32 WeaponAnimIndex = 0;

	/** World-space transform of the weapon's left hand grip socket. Used by Hand IK in the AnimBP. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	FTransform LeftHandIKTransform;

	/** Whether left hand IK is enabled (set in editor). Runtime: true only if enabled AND weapon has grip socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	bool bEnableLeftHandIK = true;

	/** Runtime: true when IK is enabled AND weapon grip socket was found this frame. Read by ABP. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	bool bUseLeftHandIK = false;

	/** Interpolation speed for smoothing IK transform when animation state changes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	float LeftHandIKInterpSpeed = 15.f;

	/** World-space aim target point (where the camera crosshair hits). Used for weapon aim correction. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	FVector AimTarget;

	/** Aim yaw offset relative to character forward (degrees). For Aim Offset blend space. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	float AimYaw;

	/** Aim pitch offset relative to character forward (degrees). For Aim Offset blend space. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	float AimPitch;

	/** Max trace distance for the crosshair aim target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	float AimTraceDistance = 10000.f;

	/** Interpolation speed for AimPitch/AimYaw smoothing. Higher = snappier, lower = smoother. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	float AimInterpSpeed = 15.f;

	/** Multiplier applied to blendspace speed inputs when a weapon is equipped.
	 *  Increase to make weapon locomotion animations play faster (e.g. 1.3). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	float WeaponAnimSpeedMultiplier = 1.0f;

	/** Speed at which the weapon interpolates between relaxed and aim positions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon|Positioning")
	float WeaponPoseInterpSpeed = 15.f;


	/** Socket on character mesh where weapon is always attached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon|Positioning")
	FName WeaponAttachSocket{TEXT("hand_r")};

	/** Get the primary weapon attached to the character. Callable from ABP. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Weapon")
	class AAZ_Weapon* GetPrimaryWeapon() const;

	// ========================================
	// JUMP / FALL STATE
	// ========================================

	/** True while the character movement component reports falling (covers both jump and walk-off-edge). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsFalling;

	/** True if the character actively jumped (as opposed to walking off a ledge). Set by GA_Jump, cleared on land. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Movement")
	bool bIsJumping;

	/** True while the character is crouching. Set by GA_Crouch, cleared on uncrouch. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsCrouching;

	/** True while the character is aiming (ADS). Set/cleared by GA_Aim toggle. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsAiming;

	/** True briefly while firing. Moves weapon to aim socket during fire animation. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Weapon")
	bool bIsShooting;

	/** True when aiming OR shooting — use this for SM transitions to aim state. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	bool bWantsAimPose;

	/** Current weapon pose state resolved from movement + action bools. Use for SM transitions and IK. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	EAZ_WeaponPoseState CurrentWeaponPoseState = EAZ_WeaponPoseState::Relaxed;

	// ========================================
	// MOTION MATCHING
	// ========================================

	/** Toggle between old blend space and new Motion Matching locomotion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|MotionMatching")
	bool bUseMotionMatching = false;

	/** Trajectory from CharacterTrajectoryComponent, updated each frame. Feed to Pose History node. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|MotionMatching")
	FTransformTrajectory CharacterTrajectory;

	/** Current PoseSearch database selected by Chooser. Updated in EventGraph, read by MM node in AnimGraph. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> CurrentLocomotionDatabase;

	/** True when the locomotion database changed this frame — signals MM node to force interrupt. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|MotionMatching")
	bool bLocomotionDatabaseChanged = false;

	// ========================================
	// STATE MACHINE (SM + BlendStack, GASP pattern)
	// ========================================

	// ----------------------------------------
	// STATE TRACKING — Current + LastFrame for all 6 states
	// Updated in NativeUpdateAnimation via Update_States()
	// ----------------------------------------

	/** Current SM state. Set by OnStateEntry functions. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	EAZ_StateMachineState StateMachineState = EAZ_StateMachineState::IdleLoop;

	// --- MovementState ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementState MovementState = EAZ_MovementState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementState MovementState_LastFrame = EAZ_MovementState::Idle;

	// --- MovementMode ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode_LastFrame = EAZ_MovementMode::OnGround;
	/** Delayed mode tracking — holds previous value briefly after change. Used by Pivot rule. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode_Recent = EAZ_MovementMode::OnGround;
	float MovementModeRecentTimer = 0.f;

	// --- Gait ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Gait Gait = EAZ_Gait::Run;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Gait Gait_LastFrame = EAZ_Gait::Run;

	// --- Stance ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Stance Stance = EAZ_Stance::Standing;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Stance Stance_LastFrame = EAZ_Stance::Standing;

	// --- MovementDirection ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementDirection MovementDirection_LastFrame = EAZ_MovementDirection::F;

	// --- RotationMode ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_RotationMode RotationMode_LastFrame = EAZ_RotationMode::OrientToMovement;

	/** Whether the character is currently turning-in-place. Cached from
	 *  ShouldTurnInPlace() each frame so Chooser BoolColumns can bind to it. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	bool bIsTurning = false;

	// ----------------------------------------
	// ESSENTIAL VALUES — Velocity, Acceleration, Transforms
	// Updated in NativeUpdateAnimation via Update_EssentialValues()
	// ----------------------------------------

	/** Current velocity vector (full 3D). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector Velocity = FVector::ZeroVector;

	/** Previous frame's velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector Velocity_LastFrame = FVector::ZeroVector;

	/** XY speed (same as GroundSpeed but explicitly 2D). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	float Speed2D = 0.f;

	/** True if Velocity XY > dead zone. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	bool bHasVelocity = false;

	/** Last non-zero velocity direction (for facing when stopped). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector LastNonZeroVelocity = FVector::ForwardVector;

	/** Current acceleration vector. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector Acceleration = FVector::ZeroVector;

	/** Previous frame's acceleration. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector Acceleration_LastFrame = FVector::ZeroVector;

	/** Acceleration magnitude. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	float AccelerationAmount = 0.f;

	/** True if Acceleration > dead zone. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	bool bHasAcceleration = false;

	/** Rate of velocity change (velocity delta / dt). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector VelocityAcceleration = FVector::ZeroVector;

	/** Acceleration relative to character facing. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector RelativeAcceleration = FVector::ZeroVector;

	// ========================================
	// ADDITIVE LEAN
	// ========================================

	/** Lateral acceleration in velocity space, normalized to -1..1. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Lean")
	float LateralAccelerationAmount = 0.f;

	/** Final lean amount packed for the 2D blendspace (X = fwd/back lean, Y = left/right lean). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Lean")
	FVector2D LeanAmount = FVector2D::ZeroVector;

	// ========================================
	// AIM OFFSET (GASP Update_AimOffset parity)
	// ========================================

	/** Current frame aim target rotation from CharacterProperties.AimingRotation. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	FRotator AO_AimTarget = FRotator::ZeroRotator;

	/** Smoothed aim rotation (worldspace), updated via critical spring damp. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	FRotator SmoothedAimTarget = FRotator::ZeroRotator;

	/** Spring damp angular velocity state for SmoothedAimTarget. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	FVector InOutAngularVelocity = FVector::ZeroVector;

	/** Aim offset this frame (X=Yaw, Y=Pitch) in degrees, delta from root rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	FVector2D AO = FVector2D::ZeroVector;

	/** AO from last frame (used to detect discontinuities). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	FVector2D Previous_AO = FVector2D::ZeroVector;

	/** True when aim offset is allowed to drive the AO layer (Strafe/Aim rotation modes, no big snap). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|AimOffset")
	bool EnableAO = false;

	/** Spring damp smoothing time for aim target (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|AimOffset")
	float AimTargetSmoothingTime = 0.2f;

	/** Character world transform this frame. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FTransform CharacterTransform;

	/** Character world transform last frame. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FTransform CharacterTransform_LastFrame;

	/** Root bone transform from OffsetRootBone node (if active). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FTransform RootTransform;

	/** Smoothed ground normal for foot placement / slope adaptation. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Essential")
	FVector SmoothedGroundNormal = FVector::UpVector;

	// ----------------------------------------
	// TRAJECTORY — Derived from MoverTrajectoryPredictor
	// Updated in NativeUpdateAnimation via Update_Trajectory()
	// ----------------------------------------

	/** Past velocity sampled from trajectory (around -0.2 to -0.3s). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_PastVelocity = FVector::ZeroVector;

	/** Near future velocity (around +0.1 to +0.2s). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_NearFutureVelocity = FVector::ZeroVector;

	/** Future velocity (around +0.4 to +0.5s). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_FutureVelocity = FVector::ZeroVector;

	/** Previous frame's future velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_PreviousFutureVelocity = FVector::ZeroVector;

	/** Future facing direction (around +1.5s). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FRotator Trj_FutureFacing = FRotator::ZeroRotator;

	/** Total cumulative facing delta across trajectory samples (can exceed 180). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float FutureFacingDelta = 0.f;

	/** Previous frame's facing delta. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float FutureFacingDelta_LastFrame = 0.f;

	/** Turn angle derived from trajectory. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float Trj_TurnAngle = 0.f;

	/** Past angular velocity from trajectory. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float Trj_PastAngularVelocity = 0.f;

	/** Current angular velocity from trajectory. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float Trj_CurrentAngularVelocity = 0.f;

	/** True when character is circling (sustained turning while moving). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	bool Trj_IsCircling = false;

	/** Time spent circling (resets when not circling). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	float Trj_CirclingTime = 0.f;

	/** Facing captured at the start of TransitionToLocomotion state. Used for pivot break detection. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Trajectory")
	FRotator FutureFacingOnTransitionStart = FRotator::ZeroRotator;

	/** Target rotation for steering/orientation (matches GASP TargetRotation). */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Trajectory")
	FRotator TargetRotation = FRotator::ZeroRotator;

	/** Target rotation captured at transition start (matches GASP TargetRotationOnTransitionStart). */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Trajectory")
	FRotator TargetRotationOnTransitionStart = FRotator::ZeroRotator;

	// ----------------------------------------
	// TRANSITION CONDITIONS — helpers for SM transitions
	// ----------------------------------------

	/** Tags from the currently selected PoseSearch database. Used by IsStarting/ShouldSpinTransition. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Transition")
	TArray<FName> CurrentDatabaseTags;

	/** Threshold for heavy vs light landing velocity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Transition")
	float HeavyLandSpeedThreshold = -800.f;

	/** Locomotion setup: 0=PureMM, 1=SM+BlendStack. Driven by DDCVar. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Transition")
	int32 LocomotionSetup = 1;

	// ----------------------------------------
	// OFFSET ROOT BONE — Tuning Parameters
	// ----------------------------------------

	/** OffsetRootBone enabled flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone")
	bool bOffsetRootBoneEnabled = true;

	/** Max distance root can offset from capsule before clamping. Higher = wider turns, lower = tighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "0", ClampMax = "100"))
	float OffsetRootTranslationRadius = 30.f;

	/** Translation smoothing when idle. Lower = faster settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float OffsetRootHalfLife_Idle = 0.2f;

	/** Translation smoothing when walking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float OffsetRootHalfLife_Walk = 0.4f;

	/** Translation smoothing when sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float OffsetRootHalfLife_Sprint = 0.5f;

	// ----------------------------------------
	// CHARACTER PROPERTIES — Fed from character to anim for procedural systems
	// ----------------------------------------

	/** Character properties for procedural animation (ground normal, etc.). Updated in NativeUpdateAnimation. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Procedural")
	FAZ_CharacterPropertiesForAnimation CharacterProperties;

	/** Enable foot placement / Control Rig IK. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Procedural")
	bool bFootPlacementEnabled = true;

	/** Force foot placement reset (after teleport, montage, etc.). */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Procedural")
	bool bForceFootPlacementReset = false;

	/** Debug draw for foot placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Procedural")
	bool bFootPlacementDebug = false;

	// ----------------------------------------
	// BLEND STACK
	// ----------------------------------------

	/** BlendStack inputs — written by SetBlendStackAnimFromChooser, read by BlendStack node. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	FAZ_BlendStackInputs BlendStackInputs;

	/** Previous frame's BlendStack inputs — for detecting anim changes. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	FAZ_BlendStackInputs Previous_BlendStackInputs;

	/** Chooser output struct — written by Chooser evaluation, read by SetBlendStackAnimFromChooser. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	FAZ_ChooserOutputs ChooserOutputs;

	/** Valid anims returned by last Chooser evaluation. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	TArray<TObjectPtr<UAnimationAsset>> ValidAnims;

	/** Search cost from the last MotionMatch call in SetBlendStackAnimFromChooser. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	float SearchCost = 0.f;

	/** Set by EarlyTransition notify — triggers re-transition to another transition anim. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	bool bNotifyTransition_ReTransition = false;

	/** Set by EarlyTransition notify — triggers transition from transition state to loop state. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	bool bNotifyTransition_ToLoop = false;

	/** True when Chooser returned no valid animation — forces SM to advance to next state. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	bool bNoValidAnim = false;

	// ========================================
	// STATE CHANGE DETECTION — Thread-Safe
	// ========================================

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool MovementStateChanged() const { return MovementState != MovementState_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool MovementModeChanged() const { return MovementMode != MovementMode_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool GaitChanged() const { return Gait != Gait_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool StanceChanged() const { return Stance != Stance_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool DirectionChanged() const { return MovementDirection != MovementDirection_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool RotationModeChanged() const { return RotationMode != RotationMode_LastFrame; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool HasVelocity() const { return bHasVelocity; }

	UFUNCTION(BlueprintPure, Category = "AZ|States", meta = (BlueprintThreadSafe))
	bool IsOnGround() const { return MovementMode == EAZ_MovementMode::OnGround; }

	// ========================================
	// TRANSITION CONDITIONS — Thread-Safe
	// ========================================

	/** Is character starting to move (uses trajectory future velocity + acceleration). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool IsStarting() const;

	/** Is character pivoting (changing direction while moving). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool IsPivoting() const;

	/** Should character play turn-in-place animation (facing delta while idle). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool ShouldTurnInPlace() const;

	/** Full TurnInPlace check matching GASP: ShouldTurnInPlace AND spin direction check.
	 *  Checks if Tags contain "Spin_L" (FacingDelta>45) or "Spin_R" (FacingDelta<-45)
	 *  or if Enable_TurnInPlaceSteering curve < 0.1. */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool ShouldReEnterTurnInPlace() const;

	/** Should character play spin transition (large facing change while moving). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool ShouldSpinTransition() const;

	/** Did character just land from air (light landing). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool JustLanded_Light() const;

	/** Did character just land from air (heavy landing). */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool JustLanded_Heavy() const;

	// ========================================
	// ANIMGRAPH BINDINGS — Thread-Safe getters for node pins
	// ========================================

	/** Get MM interrupt mode based on state changes. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	uint8 Get_MMInterruptMode() const;

	/** Get MM blend time based on movement mode. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_MMBlendTime() const;

	/** Get OffsetRootBone translation mode. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	uint8 Get_OffsetRootTranslationMode() const;

	/** Get OffsetRootBone rotation mode. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	uint8 Get_OffsetRootRotationMode() const;

	/** Get OffsetRootBone translation half-life. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationHalfLife() const;

	/** Get OffsetRootBone translation radius. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationRadius() const;

	// ========================================
	// FOOT PLACEMENT — Thread-Safe Getters
	// ========================================

	/** Allow foot pinning (feet lock to ground position during steps). */
	UFUNCTION(BlueprintPure, Category = "AZ|Procedural", meta = (BlueprintThreadSafe))
	bool AllowFootPinning() const;

	/** Allow slope warping (adjust stride to terrain slope). */
	UFUNCTION(BlueprintPure, Category = "AZ|Procedural", meta = (BlueprintThreadSafe))
	bool AllowSlopeWarping() const;

	/** Check if character just teleported (need to reset foot placement). */
	UFUNCTION(BlueprintPure, Category = "AZ|Procedural", meta = (BlueprintThreadSafe))
	bool JustTeleported() const;
	
	/** Slope-aligned rotation for slide pose (pitch/roll from terrain normal, yaw=0). */
	UFUNCTION(BlueprintPure, Category = "AZ|Procedural", meta = (BlueprintThreadSafe))
	FRotator Get_SlideSlopeRotation() const;

	/** Translation offset to push the root onto the slope plane during slides. */
	UFUNCTION(BlueprintPure, Category = "AZ|Procedural", meta = (BlueprintThreadSafe))
	FVector Get_SlideSlopeOffset() const;

	// ========================================
	// CHOOSER — Thread-Safe Getters
	// ========================================

	/** Is the character currently moving (speed above dead zone). Used by Chooser tables. */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	bool IsMoving() const { return GroundSpeed > 10.f; }

	/** Was the character moving last frame. Used by Chooser to detect start/stop transitions. */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	bool WasMoving() const { return bWasMovingLastFrame; }

	/** Get clamped ground speed for Chooser float range columns. */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	float GetLocomotionSpeed() const { return FMath::Clamp(GroundSpeed, 0.f, 1000.f); }

	/** Get current gait: 0=Idle, 1=Walk, 2=Run, 3=Sprint. Used by Chooser enum column. */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	int32 GetGait() const;

	/** Did movement direction reverse (for pivot detection). */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	bool IsDirectionReversed() const { return bDirectionReversed; }

protected:

	UPROPERTY()
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY()
	TObjectPtr<AAZ_HeroPawn> OwningHeroPawn;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedPrimaryWeapon;

	/** Current interpolated relative transform for weapon positioning. */
	FTransform CurrentWeaponRelativeTransform;

	/** Tracks the last pose state used for IK adjustment, to detect state changes. */
	EAZ_WeaponPoseState LastIKPoseState = EAZ_WeaponPoseState::Relaxed;

	/** Previous frame's locomotion database — for detecting database changes and forcing MM interrupt. */
	UPROPERTY()
	TObjectPtr<UPoseSearchDatabase> PreviousLocomotionDatabase;

	/** Was the character moving last frame (for start/stop detection). */
	bool bWasMovingLastFrame = false;

	/** Did the movement direction reverse this frame (for pivot detection). */
	bool bDirectionReversed = false;

	/** Last frame's normalized forward speed (for direction reversal detection). */
	float LastNormalizedForwardSpeed = 0.f;

	/** True while blending between IK states, false once blend is complete. */
	bool bIsIKBlending = false;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

private:

	/** Tracks whether we were falling last frame so we can detect landing. */
	bool bWasFalling = false;
};
