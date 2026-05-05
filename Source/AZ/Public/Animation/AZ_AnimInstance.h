#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "Weapon/AZ_WeaponTypes.h"
#include "Animation/TrajectoryTypes.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "Animation/AnimNodeReference.h"
#include "AZ_AnimInstance.generated.h"

class UCharacterMovementComponent;
class UAbilitySystemComponent;
class UPoseSearchDatabase;
class UMoverComponent;
class UMoverTrajectoryPredictor;
class UChooserTable;
class UMirrorDataTable;
class AAZ_HeroPawn;

UCLASS()
class AZ_API UAZ_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ========================================
	// GASP UPDATE PIPELINE — mirrors SandboxCharacter_Mover_ABP functions
	// ========================================

	/** One-time init: cache Mover / Predictor / owner-presence flags from the owning pawn. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update")
	void InitializeMoverPredictor();

	/** Read CVar-driven values (LocomotionSetup, MMDatabaseLOD, …). Empty stub — populated later. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_CVarDrivenVariables();

	/** Copy pawn-side state (falling / crouching / aiming / CharacterProperties) onto the instance.
	 *  GAME-THREAD ONLY — touches CMC and Controller. Wire to Event Blueprint Update Animation, NOT thread-safe variant. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update")
	void Update_PropertiesFromCharacter();

	/** Orchestrator: Trajectory → EssentialValues → States → AimOffset → AdditiveLean. Worker-thread safe. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_Logic(float DeltaSeconds);

	/** Generate Mover/CharacterTrajectory samples, derive Trj_* (future/past velocities, facing delta, angular vel). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_Trajectory(float DeltaSeconds);

	/** Update velocity / acceleration / character transform / relative accel essentials.
	 *  Also caches RootTransform from the AnimGraph's OffsetRootBone node (GASP parity). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_EssentialValues(float DeltaSeconds);

private:
	/** Reflect into the AnimGraph property table to find the FAnimNode_OffsetRootBone instance.
	 *  Used by Update_EssentialValues to read the post-offset root transform without needing
	 *  a BP-wired FAnimNodeReference (which our pure-C++ AnimInstance can't easily provide). */
	struct FAnimNode_OffsetRootBone* FindOffsetRootBoneNode();

	/** Debug-only: enumerate every FAnimNode_Steering instance in this AnimBP. */
	TArray<struct FAnimNode_Steering*> FindAllSteeringNodes();

public:

	/** Update MovementMode / Stance / MovementState / Gait / MovementDirection and run GASP 5-var tracking. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_States(float DeltaSeconds);

	/** Critical-spring-damp AimingRotation → SmoothedAimTarget; compute AO Vector2D and EnableAO gates. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_AimOffset(float DeltaSeconds);

	/** Lateral-acceleration lean: normalize VelocityAcceleration.Y into LeanAmount Vector2D per MovementDirection. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update", meta = (BlueprintThreadSafe))
	void Update_AdditiveLean(float DeltaSeconds);

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

	/** Trajectory generated each frame via MoverTrajectoryPredictor (HeroPawn) or
	 *  CharacterTrajectoryComponent (legacy Character). Feeds Pose History and MM node. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FTransformTrajectory Trajectory;

	/** Trajectory world collision results from PoseSearchHandleTrajectoryWorldCollisions. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FPoseSearchTrajectory_WorldCollisionResults TrajectoryCollision;

	/** Cached predictor (currently points at HeroPawn's MoverTrajectoryPredictor).
	 *  Set in InitializeMoverPredictor. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	TObjectPtr<UMoverTrajectoryPredictor> Predictor;

	/** Yaw value persisted between trajectory generation calls — required by
	 *  PoseSearchGenerateTrajectoryUsingPredictor to compute smooth deltas. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Trajectory")
	float PreviousDesiredControllerYaw = 0.f;

	/** Current PoseSearch database selected by Chooser. Updated in Update_MotionMatching, read by MM node. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> CurrentSelectedDatabase;

	/** Currently selected animation from the last MM search. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UObject> CurrentSelectedAnim;

	/** Array of PoseSearch databases fed to the MM node this frame. Populated from Chooser. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching")
	TArray<TObjectPtr<UPoseSearchDatabase>> ValidDatabases;

	/** LOD index for MM database selection (Dense/Sparse/ExtremeSparse tiers). CVar-driven. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching")
	int32 MMDatabaseLOD = 0;

	/** Search cost from the last MM node evaluation (not the SM path SearchCost). */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|MotionMatching", meta = (DisplayName = "MM Search Cost"))
	float MMSearchCost = 0.f;

	/** True when the locomotion database changed this frame — signals MM node to force interrupt. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|MotionMatching")
	bool bLocomotionDatabaseChanged = false;

	/** Chooser table that selects the SM-path animation per StateMachineState + state combo.
	 *  GASP equivalent: CHT_MoverCharacterAnimations. Assigned by the ABP designer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UChooserTable> CharacterAnimationChooser;

	/** Chooser table that selects PoseSearch databases for the MM node based on state.
	 *  GASP equivalent: CHT_PoseSearchDatabases_Relaxed. Assigned by the ABP designer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UChooserTable> LocomotionDatabaseChooser;

	/** Mirror data table (e.g. MDT_CHR_M16). Used for left/right mirroring of MM results. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|MotionMatching")
	TObjectPtr<UMirrorDataTable> CharacterMirrorDataTable;

	// ========================================
	// STATE MACHINE (SM + BlendStack, GASP pattern)
	// ========================================

	// ----------------------------------------
	// STATE TRACKING — GASP 5-variable pattern for all 6 state enums.
	// Per state: X (current), X_LastFrame, X_Recent (delayed previous),
	// X_Time (duration in current state), X_LastStateTime (duration of previous state).
	// Updated in NativeUpdateAnimation via UpdateStateTracking helper.
	// ----------------------------------------

	/** Current SM state. Set by OnStateEntry functions. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|StateMachine")
	EAZ_StateMachineState StateMachineState = EAZ_StateMachineState::IdleLoop;

	// --- MovementState ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementState MovementState = EAZ_MovementState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementState MovementState_LastFrame = EAZ_MovementState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementState MovementState_Recent = EAZ_MovementState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementState_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementState_LastStateTime = 0.f;

	// --- MovementMode ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode_LastFrame = EAZ_MovementMode::OnGround;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementMode MovementMode_Recent = EAZ_MovementMode::OnGround;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementMode_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementMode_LastStateTime = 0.f;

	// --- Gait ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Gait Gait = EAZ_Gait::Run;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Gait Gait_LastFrame = EAZ_Gait::Run;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Gait Gait_Recent = EAZ_Gait::Run;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float Gait_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float Gait_LastStateTime = 0.f;

	// --- Stance ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Stance Stance = EAZ_Stance::Standing;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Stance Stance_LastFrame = EAZ_Stance::Standing;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_Stance Stance_Recent = EAZ_Stance::Standing;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float Stance_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float Stance_LastStateTime = 0.f;

	// --- MovementDirection ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementDirection MovementDirection_LastFrame = EAZ_MovementDirection::F;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_MovementDirection MovementDirection_Recent = EAZ_MovementDirection::F;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementDirection_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float MovementDirection_LastStateTime = 0.f;

	// --- RotationMode ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_RotationMode RotationMode_LastFrame = EAZ_RotationMode::OrientToMovement;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	EAZ_RotationMode RotationMode_Recent = EAZ_RotationMode::OrientToMovement;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float RotationMode_Time = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "AZ|States")
	float RotationMode_LastStateTime = 0.f;

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

	// --- FootContact (driven by contact_l/contact_r curves baked on locomotion loops) ---
	// Read each tick from the currently-playing BlendStack anim. Chooser binds to these
	// so _LU vs _RU stop/start variants can be selected based on which foot is currently down.
	UPROPERTY(BlueprintReadOnly, Category = "AZ|FootContact")
	bool bLeftFootDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|FootContact")
	bool bRightFootDown = false;

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

	/** World-space transform used during interaction/traversal montage placement. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Essential")
	FTransform InteractionTransform;

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

	/** Past angular velocity from trajectory (full vector; GASP uses .Z for yaw rate). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_PastAngularVelocity = FVector::ZeroVector;

	/** Current angular velocity from trajectory (full vector; GASP uses .Z for yaw rate). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Trajectory")
	FVector Trj_CurrentAngularVelocity = FVector::ZeroVector;

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
	// DEFAULT — Cached refs + feature flags
	// ----------------------------------------

	/** Cached owner mover component (from HeroPawn). Set in InitializeMoverPredictor. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Default")
	TObjectPtr<UMoverComponent> Mover;

	/** True when OwningHeroPawn or OwningCharacter is valid this frame. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Default")
	bool bHasOwningActor = false;

	/** True when a MoverComponent was cached in Mover. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Default")
	bool bHasMover = false;

	/** When true, update dispatch runs via BlueprintThreadSafeUpdateAnimation (worker thread). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Default")
	bool bUseThreadSafeUpdateAnimation = true;

	// ----------------------------------------
	// OFFSET ROOT BONE — Tuning Parameters
	// ----------------------------------------

	/** OffsetRootBone enabled flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone")
	bool bOffsetRootBoneEnabled = true;

	// [ANIMSPIN] DEBUG PROBE — temp throttle accumulator for per-frame log; remove after diagnosis.
	float AnimSpinProbeAccum = 0.f;
	float AnimSpinParamDumpAccum = 0.f;
	float LastSimYaw = 0.f;

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

	/** Rotation smoothing halflife. Lower = mesh re-aligns to actor faster.
	 *  Engine default is 0.2s which produces a visible steady-state offset
	 *  (~17° at 60°/s actor turn). 0.05s shrinks that to ~4°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float OffsetRootRotationHalfLife = 0.05f;

	/** Hard cap on visible rotation offset (degrees). -1 disables.
	 *  Use to bound worst-case mesh-vs-capsule drift during fast turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|OffsetRootBone", meta = (ClampMin = "-1.0", ClampMax = "180.0"))
	float OffsetRootMaxRotationError = 10.f;

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

	/** 0 = ControlRig, 1 = Basic FootPlacement+LegIK. Selected per movement mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Procedural")
	int32 FootPlacementMode = 0;

	/** Distance-moved threshold above which JustTeleported triggers a foot placement reset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Procedural")
	double TeleportThreshold = 500.0;

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

	/** GASP parity: MovingTraversal curve > 1 AND DefaultSlot NOT driving pose. */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	bool JustTraversed() const;

	/** Z-component of LandVelocity (landing impact speed). GASP parity: returns a double. */
	UFUNCTION(BlueprintPure, Category = "AZ|Transition", meta = (BlueprintThreadSafe))
	double Get_LandVelocity() const { return CharacterProperties.LandVelocity.Z; }

	/** GASP-exact cumulative facing delta across provided sample times. Sums consecutive
	 *  signed yaw deltas from trajectory samples; initializes PrevYaw from the first sample
	 *  to avoid the SK_SurvivalMan mesh-component −90° offset bias. */
	UFUNCTION(BlueprintPure, Category = "AZ|Trajectory", meta = (BlueprintThreadSafe))
	float Get_TotalFacingDelta(const TArray<float>& Times) const;

	/** Trivial getter for the cached Trj_TurnAngle (cumulative facing delta across GASP sample set). */
	UFUNCTION(BlueprintPure, Category = "AZ|Trajectory", meta = (BlueprintThreadSafe))
	float Get_TrajectoryTurnAngle() const { return Trj_TurnAngle; }

	// ========================================
	// WARPING / PROCEDURAL — node-bound helpers (Phase 9b)
	// All take FAnimNodeReference; engine marshals from the AnimGraph node.
	// ========================================

	/** Per-frame play-rate scalar driven by capsule speed vs MoveData_Speed curve, clamped by
	 *  Min/MaxDynamicPlayRate curves, blended via Enable_Warping curve, multiplied by a
	 *  circling bonus (|Trj_CurrentAngularVelocity.Z| × Trj_CirclingTime). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	double Get_DynamicPlayRate(FAnimNodeReference BlendStackInput) const;

	/** Steering target = trajectory facing at time MapRange(SteeringTargetTime curve, [0..1] → [0.1..1.5]). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	FQuat Get_DesiredFacing(FAnimNodeReference Node) const;

	/** Steering enable: (BlendStackInputs.bLoop OR anim active) AND (IsMoving OR InAir OR Sliding). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	bool EnableSteering(FAnimNodeReference Node) const;

	/** Orientation Warping space: RootBoneTransform if OffsetRootBone enabled, else ComponentTransform. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	uint8 Get_OrientationWarpingWarpingSpace() const;

	/** Strafe-warp direction: lerp(LastNonZeroVelocity, Trj_NearFutureVelocity, MapRange(|AngularZ|, [20..100], [0..1])). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	FVector Get_StrafeWarpDirection() const;

	/** Stride-warp alpha: clamp(Enable_Warping curve + Enable_StideWarping curve, 0..1). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	double Get_StrideWarpAlpha(FAnimNodeReference Node) const;

	/** Strafe-warp alpha: clamp(Enable_Warping curve + Enable_StrafeWarping curve, 0..1). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	double Get_StrafeWarpAlpha(FAnimNodeReference Node) const;

	/** Procedural target time: MapRange(SteeringTargetTime curve, [0..1] → [0.1..0.3]); default 0.4 if no anim. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Warping", meta = (BlueprintThreadSafe))
	double Get_ProceduralTargetTime(FAnimNodeReference Node) const;

	/** Foot-placement OnBecomeRelevant callback: forces a foot placement reset next frame. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Procedural")
	void Biped_FootPlacement_OnBecomeRelevant(FAnimUpdateContext Context, FAnimNodeReference Node);

	// ========================================
	// MOTION MATCHING NODE CALLBACKS (Phase 9c)
	// Bound to the MotionMatching AnimGraph node's OnUpdate / PostSelection.
	// Get_PoseHistoryReference stays in ABP (K2Node_AnimNodeReference resolves a
	// node by name in the AnimGraph at compile time — not portable to C++).
	// ========================================

	/** OnUpdate: evaluate LocomotionDatabaseChooser → ValidDatabases, push to MM node with InterruptMode. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update")
	void Update_MotionMatching(FAnimUpdateContext Context, FAnimNodeReference Node);

	/** PostSelection: cache CurrentSelectedAnim/Database/MMSearchCost/CurrentDatabaseTags from the search result;
	 *  override the next blend with HermiteCubic 0.2s, no inertial blend. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Update")
	void Update_MotionMatching_PostSelection(FAnimUpdateContext Context, FAnimNodeReference Node);

	// ========================================
	// SM STATE CONTROLLER (Phase 9d)
	// ABP supplies the BlendStack node ref + chooser eval (since K2Node_AnimNodeReference
	// resolves "State Machine Blend Stack" by name at BP compile time, not portable to C++).
	// 9 OnStateEntry_* are ABP thunks that call SetBlendStackAnimFromChooser with the right StateID.
	// ========================================

	/** Core SM-path function. ABP evaluates CHT_MoverCharacterAnimations chooser and supplies
	 *  the result. C++ does the heavy lifting: cache previous, reset notify bools, optional
	 *  single-frame MotionMatch, populate BlendStackInputs, force blend if requested. */
	UFUNCTION(BlueprintCallable, Category = "AZ|StateMachine", meta = (BlueprintThreadSafe))
	void SetBlendStackAnimFromChooser(
		EAZ_StateMachineState State,
		bool bForceBlend,
		FAnimNodeReference BlendStackNode,
		UAnimationAsset* ChosenAnim,
		FAZ_ChooserOutputs ChooserOut);

	/** Seconds-remaining threshold for IsAnimationAlmostComplete. Default 0.25s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|StateMachine", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float AnimationAlmostCompleteThreshold = 0.25f;

	/** SM transition helper: returns true when the BlendStack's current asset is non-looping
	 *  and has ≤ AnimationAlmostCompleteThreshold seconds remaining. ABP supplies the BlendStack node ref. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|StateMachine", meta = (BlueprintThreadSafe))
	bool IsAnimationAlmostComplete(FAnimNodeReference BlendStackNode) const;

	// ========================================
	// ANIMGRAPH BINDINGS — Thread-Safe getters for node pins
	// ========================================

	/** Get MM interrupt mode based on state changes. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	uint8 Get_MMInterruptMode() const;

	/** Get MM blend time based on movement mode. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_MMBlendTime() const;

	/** GASP MM notify recency timeout — 0.16s for Sprint, 0.2s otherwise (matches GASP select). */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_MMNotifyRecencyTimeOut() const { return Gait == EAZ_Gait::Sprint ? 0.16f : 0.2f; }

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

	/** Get OffsetRootBone rotation half-life. Bind to the Rotation Halflife pin. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_OffsetRootRotationHalfLife() const { return OffsetRootRotationHalfLife; }

	/** Get OffsetRootBone max rotation error (degrees). Bind to the Max Rotation Error pin. */
	UFUNCTION(BlueprintPure, Category = "AZ|AnimGraph", meta = (BlueprintThreadSafe))
	float Get_OffsetRootMaxRotationError() const { return OffsetRootMaxRotationError; }

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

	/** GASP IsMoving: future velocity != 0 (tol 10) AND acceleration != 0.
	 *  Fires Idle immediately on input release while Speed2D may still be high —
	 *  lets Walk Stops chooser rows (SM=TransIdle, speed 10..200) win. */
	UFUNCTION(BlueprintPure, Category = "AZ|Chooser", meta = (BlueprintThreadSafe))
	bool IsMoving() const
	{
		const FVector FutureVel2D(Trj_FutureVelocity.X, Trj_FutureVelocity.Y, 0.f);
		const FVector Accel2D(Acceleration.X, Acceleration.Y, 0.f);
		return FutureVel2D.SizeSquared() > 100.f && Accel2D.SizeSquared() > 1.f;
	}

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

	/** Previous frame's selected database — for detecting database changes and forcing MM interrupt. */
	UPROPERTY()
	TObjectPtr<UPoseSearchDatabase> PreviousSelectedDatabase;

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

	/** RecentTimer countdowns for the 5-variable state pattern.
	  * On change: timer = RecentTimeLimit, X_Recent = X_LastFrame.
	  * After timer expires: X_Recent = X. */
	float MovementState_RecentTimer = 0.f;
	float MovementMode_RecentTimer = 0.f;
	float Gait_RecentTimer = 0.f;
	float Stance_RecentTimer = 0.f;
	float MovementDirection_RecentTimer = 0.f;
	float RotationMode_RecentTimer = 0.f;
};
