// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"                  // FTransformTrajectory
#include "Animation/AZ_CmcAnimTypes.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"     // FPoseSearchTrajectoryData, FPoseSearchTrajectory_WorldCollisionResults
#include "PoseSearch/PoseSearchLibrary.h"               // EPoseSearchInterruptMode
#include "AnimationWarpingTypes.h"                      // EOffsetRootBoneMode
#include "BoneControllers/AnimNode_OrientationWarping.h" // EOrientationWarpingSpace
#include "AZ_CmcAnimInstance.generated.h"

class AAZ_CmcCharacterBase;
class UPoseSearchDatabase;
struct FAnimNodeReference;
struct FAnimNode_OffsetRootBone;
struct FMotionMatchingAnimNodeReference;

/**
 * UAZ_CmcAnimInstance — native base for the CMC (v3) hero AnimBP. [SPIKE: spike/cmc-backport]
 *
 * A NODE-FOR-NODE port of GASP 5.8 SandboxCharacter_CMC_ABP's update chain, read out of the asset with
 * UAZ_BlueprintNodeUtils::ListFunctionNodes on 2026-08-17 and screenshot-verified. Function names,
 * variable names and every literal match the Blueprint deliberately, so this file can be diffed against
 * Epic's asset and against [[project-gasp-cmc-abp-spec]] without a translation step. Where a bool is
 * `bX` here it is UE convention only — Blueprint still displays it as GASP's `X`.
 *
 * WHAT RUNS. GASP's Update_Logic ends in Branch(UseExperimentalStateMachine), and that flag is FALSE on
 * the shipped CDO, so Update_MovementDirection and Update_TargetRotation never execute on the Motion
 * Matching path. Update_TargetRotation is therefore NOT ported — dead code that compiles is how the v1
 * pawn confused every audit for months. Update_MovementDirection IS ported, but as OUR requirement: the
 * MM node selects by trajectory and needs no direction bucket; our CHT chooser rows do.
 *
 * THREADING. NativeUpdateAnimation (game thread) does one thing: pull CharacterProperties from the pawn.
 * Everything else — trajectory, collisions, derived state — runs in NativeThreadSafeUpdateAnimation on
 * the worker thread, which is where GASP puts it (BlueprintThreadSafeUpdateAnimation is the only update
 * event its ABP implements) and what the PoseSearch trajectory API is explicitly marked safe for.
 *
 * ROOT TRANSFORM. RootTransform is the OffsetRootBone node's simulated root. GASP reads it inline in
 * Update_EssentialValues via AnimationWarpingLibrary::GetOffsetRootTransform(NodeReference: OffsetRoot),
 * and we read it in the same place — see FindOffsetRootBoneNode. An earlier revision of this comment
 * claimed C++ could not: that is true of FAnimNodeReference (it constructs only from
 * (UAnimInstance*, FAnimNode_Base&) or a compile-time node index) but NOT of the problem. GetClass() at
 * runtime IS the generated child class, so IAnimClassInterface::GetAnimNodeProperties finds the node
 * struct directly, no reference needed. UAZ_AnimInstance has done it that way since v2.
 *
 * Latency is one frame, and identical to GASP's: FAnimNode_OffsetRootBone advances its simulated
 * transform in Evaluate_AnyThread, which runs AFTER every update, so whoever reads it during an update
 * — Blueprint or native — reads what the previous frame's evaluation left behind.
 *
 * Abstract: an AnimInstance with no AnimGraph evaluates the ref pose silently, and we have already
 * assigned a raw native anim class to a mesh once on this branch.
 */
UCLASS(Abstract, Blueprintable)
class AZ_API UAZ_CmcAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UAZ_CmcAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// ==================================================================================
	// THE UPDATE CHAIN — GASP's Update_Logic, in order.
	// ==================================================================================

	/** Update_Trajectory -> Update_EssentialValues -> Update_States -> Update_MovementDirection. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_Logic(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_Trajectory(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_EssentialValues(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_States();

	/** AZ-only (see class comment): the 6-way bucket our CHT rows are addressed by. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	void Update_MovementDirection();

	// ==================================================================================
	// MOTION MATCHING — these need the MM node, which C++ cannot obtain on its own, so the
	// AnimGraph passes it in. Same arrangement as SetOffsetRootTransform.
	// ==================================================================================

	/** Bind to the Motion Matching node's **On Update**. Picks the database pool and hands it to the
	 *  node with our interrupt mode. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	void Update_MotionMatching(const FAnimNodeReference& Node);

	/** Bind to the Motion Matching node's **On Update, after selection**. Reads back what the search
	 *  actually chose — this is the ONLY way CurrentDatabaseTags, bCurrentAssetLooping and SearchCost
	 *  ever get filled, and three predicates run degraded until it does. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	void Update_MotionMatching_PostSelection(const FAnimNodeReference& Node);

	/** Steering is on while we are going somewhere and an animation is actually playing. Takes the
	 *  BlendStack node because the relevant blend stack lives INSIDE the MM node's own graph. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	bool EnableSteering(const FAnimNodeReference& Node) const;

	/**
	 * Which database pool the search may draw from this frame: the union of every DatabaseGates row
	 * matching (MovementMode, Stance, MovementState, Gait) — GASP's CHT_PoseSearchDatabases_Dense
	 * evaluation, typed (see FAZ_DatabaseGate for why not a chooser asset).
	 *
	 * The gate that carries correctness weight is STANCE: gait and speed are self-gating through the
	 * trajectory feature vector, but crouched and standing trajectories at equal speed are identical —
	 * only the pose channel separates them, so without stance rows a crouch clip can win while standing.
	 * MovementMode joins it the day airborne pools are ingested. As a side effect of matching,
	 * refreshes MatchedGateLabels for the debug line.
	 */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|MotionMatching", meta = (BlueprintThreadSafe))
	TArray<UPoseSearchDatabase*> Get_DatabasesToSearch() const;

	/**
	 * DEAD as of the native pull (see FindOffsetRootBoneNode) and called by nothing — verified against
	 * the ABP's package name table. Kept only because removing a UFUNCTION forces an editor-closed
	 * rebuild; delete at the next one. Do NOT wire this up: the native pull owns RootTransform, and a
	 * second writer would make "which one won this frame" unanswerable.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe, DeprecatedFunction,
		DeprecationMessage = "RootTransform is read natively now. This setter is dead and will be removed."))
	void SetOffsetRootTransform(const FTransform& InOffsetRootTransform);

	// ==================================================================================
	// PREDICATES + GETTERS — GASP names, GASP semantics.
	// ==================================================================================

	/** Velocity is non-zero AND acceleration is non-zero.
	 *  NOTE the Trj_FutureVelocity term GASP's graph still contains is ORPHANED — the node exists, its
	 *  output is wired to nothing, and the AND pin it fed is a bare `true`. Ported as it actually runs,
	 *  not as it reads. (Our older memory note "IsMoving = FutureVelocity + Accel" describes GASP 5.5.) */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsMoving() const;

	/** |trajectory turn angle| past a per-rotation-mode threshold, and moving. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsPivoting() const;

	/** Aim-vs-body yaw past threshold, AND (aiming OR we stopped this very frame). */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool ShouldTurnInPlace() const;

	/** Yaw between the acceleration direction and the velocity direction — the pivot signal. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_TrajectoryTurnAngle() const;

	/** VelocityAcceleration normalised by max accel (or max braking when decelerating), actor space. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector CalculateRelativeAccelerationAmount() const;

	/** X = lateral lean scaled by speed; Y is always 0 in GASP. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector2D Get_LeanAmount() const;

	/** X = yaw, Y = pitch, faded out by the Disable_AO curve. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FVector2D Get_AOValue() const;

	/** lerp(1, clamp(Speed2D / MoveData_Speed, Min, Max), Enable_PlayRateWarping).
	 *  Returns 1 when the clip has no MoveData_Speed — the honest no-op. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_DynamicPlayRate(float MinPlayRate = 0.8f, float MaxPlayRate = 1.2f) const;

	// ==================================================================================
	// DERIVED PREDICATES — GASP 5.8 semantics. Diffed against the 5.5-era UAZ_AnimInstance
	// port on 2026-08-17; where the two disagreed 5.8 won, and each divergence is noted at
	// its implementation site rather than silently reconciled.
	// ==================================================================================

	/** Accelerating out of rest: moving, future speed clearly exceeds current, not already pivoting. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsStarting() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool IsOnGround() const { return MovementMode == EAZ_MovementMode::OnGround; }

	/** The body has fallen far enough behind the capsule that it needs to spin to catch up. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	bool ShouldSpinTransition() const;

	/** Where steering should aim: the trajectory's facing a fixed time ahead. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FQuat Get_DesiredFacing() const;

	/** Aim-offset yaw. Non-zero ONLY while strafing — see the implementation for why. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	float Get_AO_Yaw() const;

	/** DYNAMIC, not a constant: the quadrant boundaries widen or tighten with what we are doing. */
	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	FAZ_MovementDirectionThresholds Get_MovementDirectionThresholds() const;

	// ==================================================================================
	// ANIMGRAPH NODE SETTINGS — bind these to node pins. Returning the real enums rather than
	// uint8 is deliberate: EOffsetRootBoneMode runs Accumulate 0 / Interpolate 1 / Lock 2-4 /
	// Release 5, and hand-written integers in that range have bitten this project before.
	// ==================================================================================

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_MMBlendTime() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_MMNotifyRecencyTimeOut() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EPoseSearchInterruptMode Get_MMInterruptMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOffsetRootBoneMode Get_OffsetRootRotationMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOffsetRootBoneMode Get_OffsetRootTranslationMode() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationHalfLife() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	float Get_OffsetRootTranslationRadius() const { return OffsetRootTranslationRadius; }

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	EOrientationWarpingSpace Get_OrientationWarpingWarpingSpace() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim|NodeSettings", meta = (BlueprintThreadSafe))
	bool AllowFootPinning() const;

	UFUNCTION(BlueprintPure, Category = "AZ|Cmc|Anim", meta = (BlueprintThreadSafe))
	AAZ_CmcCharacterBase* GetCmcCharacter() const { return Cached_Character; }

	// ==================================================================================
	// PUBLISHED STATE — the AnimGraph binds to these by name, as it would to BP variables.
	// ==================================================================================

	/** The whole pawn seam, refreshed once per frame on the game thread. GASP's `Character Properties`. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim")
	FAZ_CmcAnimContract CharacterProperties;

	// ---- Trajectory ----

	/** Bind the PoseSearchHistoryCollector node's TransformTrajectory pin to this. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FTransformTrajectory Trajectory;

	/** Carries TimeToLand + LandSpeed — a landing predictor the collision pass produces for free. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectory_WorldCollisionResults TrajectoryCollision;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_PastVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	FVector Trj_FutureVelocity = FVector::ZeroVector;

	/** Persisted across frames by the generator so predicted facing stays smooth. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Trajectory")
	float PreviousDesiredControllerYaw = 0.f;

	// ---- Essential values ----

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform CharacterTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform CharacterTransform_LastFrame = FTransform::Identity;

	/** The OFFSET ROOT (yaw + 90 for the mesh convention), not the capsule — every actor-relative value
	 *  is measured against this. That is how the anim layer stays coherent while the capsule turns
	 *  instantly. Falls back to CharacterTransform when the AnimGraph has not pushed one. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FTransform RootTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Acceleration_LastFrame = FVector::ZeroVector;

	/** |Acceleration| / CurrentMaxAcceleration — normalised 0..1, not a raw magnitude. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	float AccelerationAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	bool bHasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector Velocity_LastFrame = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential", meta = (ForceUnits = "cm/s"))
	float Speed2D = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	bool bHasVelocity = false;

	/** (Velocity - Velocity_LastFrame) / max(DeltaTime, 0.001) — world space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector VelocityAcceleration = FVector::ZeroVector;

	/** VelocityAcceleration unrotated into RootTransform space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector RelativeAcceleration = FVector::ZeroVector;

	/** Held while stopped, so facing logic has a direction to work with at zero speed. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Essential")
	FVector LastNonZeroVelocity = FVector::ZeroVector;

	// ---- States (each paired with a one-frame history, which is what makes transitions detectable) ----

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementMode MovementMode_LastFrame = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_RotationMode RotationMode_LastFrame = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementState MovementState = EAZ_MovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_MovementState MovementState_LastFrame = EAZ_MovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Gait Gait_LastFrame = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Stance Stance = EAZ_Stance::Standing;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|State")
	EAZ_Stance Stance_LastFrame = EAZ_Stance::Standing;

	// ---- AZ-only derived state (CHT chooser inputs) ----

	/** Tags of the database MM actually selected. Filled by the post-selection step once the AnimGraph
	 *  hands us the MM node; empty until then, which degrades the predicates that read it to
	 *  "not a pivot" rather than breaking them. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TArray<FName> CurrentDatabaseTags;

	/** Whether the selected asset loops. GASP reads this off its BlendStack inputs, which only exist on
	 *  the state-machine path; on the MM path it comes straight off the search result's bLoop. Defaults
	 *  true so the direction thresholds start in their steady-state (wider) configuration. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	bool bCurrentAssetLooping = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TObjectPtr<UObject> CurrentSelectedAnim;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	TObjectPtr<const UPoseSearchDatabase> CurrentSelectedDatabase;

	/** Cost of the winning pose. Pure instrumentation, but the single most useful number when the
	 *  character picks something odd: a high cost means nothing in the pool matched, which is a content
	 *  problem, not a logic one. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|MotionMatching")
	float SearchCost = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction")
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction", meta = (ForceUnits = "deg"))
	float MovementDirectionAngle = 0.f;

	/**
	 * Left foot planted, from FootSpeed_L — NOT contact_l.
	 * Our library authors a real per-foot speed on 824 clips where GASP authors a 0/1 flag; that is also
	 * why we skip GASP's RemapCurves node, whose only job is faking a speed out of a flag. Measured
	 * 2026-08-17: just 2 of the 104 clips in our databases carry contact_l at all, which is why the v2
	 * stack's foot phase was permanently false and every foot-aware chooser row picked its default.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Cmc|Anim|Direction")
	bool bLeftFootDown = false;

protected:
	// ==================================================================================
	// CONFIG — every GASP literal, exposed rather than baked in.
	// ==================================================================================

	/** Prediction shaping while standing still. Speed/braking/friction are NOT here: the generator's
	 *  internal FDerived reads them off the movement component every frame, so predictor and simulation
	 *  cannot drift apart (PoseSearchTrajectoryLibrary.h:31-48). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectoryData TrajectoryGenerationData_Idle;

	/** Prediction shaping while moving. GASP switches on Speed2D > 0.0 — any motion at all. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FPoseSearchTrajectoryData TrajectoryGenerationData_Moving;

	/** GASP: -1.0. NEGATIVE means "collect a history sample every update" rather than on an interval. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "s"))
	float TrajectoryHistorySamplingInterval = -1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ClampMin = "2"))
	int32 TrajectoryHistoryCount = 30;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "s"))
	float TrajectoryPredictionSamplingInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ClampMin = "2"))
	int32 TrajectoryPredictionCount = 15;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	bool bHandleTrajectoryCollisions = true;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	bool bTrajectoryApplyGravity = true;

	/** GASP: 0.01, not the 5-ish you would guess — the samples ride the surface, they do not hover. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "cm"))
	float FloorCollisionsOffset = 0.01f;

	/** GASP: 150. The engine default is 10000, which lets a distant ceiling distort the prediction. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory", meta = (ForceUnits = "cm"))
	float MaxObstacleHeight = 150.f;

	/** Sample windows for the three velocities (seconds; negative = history). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D PastVelocityWindow = FVector2D(-0.3f, -0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D CurrentVelocityWindow = FVector2D(0.0f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Trajectory")
	FVector2D FutureVelocityWindow = FVector2D(0.4f, 0.5f);

	// ---- Thresholds ----

public:
	// ==================================================================================
	// ANIM -> MOVEMENT SNAPSHOT. The one place the movement layer is allowed to read the anim layer.
	//
	// Direction of dependency is deliberate and narrow: the character asks "what does the selected stop
	// clip depict right now" and matches the capsule to it, so stopping distance is a property of the
	// CONTENT rather than a tuned constant. Everything else still flows movement -> anim through
	// FAZ_CmcAnimContract; this is the single reverse edge and it carries two floats.
	//
	// Written once per frame in NativeUpdateAnimation (game thread). The character ticks BEFORE the mesh,
	// so it reads last frame's values — see the note at the write site.
	// ==================================================================================

	/** True while a Stops-tagged clip is the current selection. */
	bool IsStopClipSelected() const { return bStopClipSelected_GT; }

	/** Ground speed the selected stop clip depicts at its current playback time, from MoveData_Speed.
	 *  Zero means either no stop is selected, the clip carries no curve, or the clip has reached its
	 *  plant — callers must treat "not positive" as "fall back to the latched stop contract". */
	float GetStopClipDepictedSpeed() const { return StopClipSpeed_GT; }

	/** Playback time the stop clip's curve was sampled at. Exposed for diagnostics: it is the only way to
	 *  see whether Motion Matching entered a stop near its beginning or deep in its tail. */
	float GetStopClipSampleTime() const { return StopClipSampleTime_GT; }

	// ---- Turn-in-place reverse edge — same _GT snapshot discipline as the stop trio above. The hero's
	// TIP lock reads these to decide RELEASE: the capsule does not rotate during the lock, so remaining
	// yaw can only be measured against the MESH root, which is an anim-side fact. ----

	/** True while a TurnInPlace-tagged clip is the current selection. */
	bool IsTurnInPlaceClipSelected() const { return bTipClipSelected_GT; }

	/** Fraction of the selected TIP clip already played (0..1). 0 when no TIP clip is selected. */
	float GetTurnInPlaceClipFraction() const { return TipClipFraction_GT; }

	/** World yaw of the offset mesh root (actor convention — RootTransform already carries the +90).
	 *  What the TIP lock measures its remaining rotation against, and what the capsule snaps to at
	 *  release. */
	float GetTipRootYaw() const { return TipRootYaw_GT; }

protected:
	/** Mirror of the contract's stop flag plus its previous value, so the graph can see the STOP EDGE.
	 *  Get_MMInterruptMode invalidates the continuing pose on that edge — without it a spent stop clip
	 *  from a previous stop is inherited wholesale, and the new stop begins at whatever time that clip
	 *  had reached (measured 2026-08-23: clipTime 0.5-0.99s while the body was at full walk speed). */
	bool bStopActive = false;
	bool bStopActive_LastFrame = false;

	bool bStopClipSelected_GT = false;
	float StopClipSpeed_GT = 0.f;
	float StopClipSampleTime_GT = 0.f;

	/** TIP contract mirror + edge twin — same pattern and same reason as the bStopActive pair. */
	bool bTurnInPlaceActive = false;
	bool bTurnInPlaceActive_LastFrame = false;

	bool bTipClipSelected_GT = false;
	float TipClipFraction_GT = 0.f;
	float TipRootYaw_GT = 0.f;

	/** How far ahead (s) the predicted facings fully converge on the TIP target. Shorter = the query
	 *  depicts a harder turn = MM prefers the bigger/faster turn clips. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "s"))
	float TurnInPlaceFacingConvergeTime = 0.5f;

	/** The selected asset's own playback time, straight from the search result. */
	float CurrentSelectedTime = 0.f;

	/** bHasVelocity = Speed2D > this. GASP: 5. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float HasVelocityThreshold = 5.f;

	/** IsMoving's velocity comparison tolerance. GASP: 0.1. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds")
	float IsMovingVelocityTolerance = 0.1f;

	/** IsMoving's acceleration comparison tolerance (engine default on the compare node). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds")
	float IsMovingAccelerationTolerance = 0.0001f;

	/** Pivot turn-angle threshold per rotation mode. GASP: OrientToMovement 45, Strafe 30, Aiming 0. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_OrientToMovement = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_Strafe = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float PivotAngleThreshold_Aiming = 0.f;

	/** ShouldTurnInPlace yaw threshold. GASP: 50. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float TurnInPlaceAngleThreshold = 50.f;

	/** Lean speed ramp: Speed2D mapped from [In] to [Out] and multiplied into the lateral lean.
	 *  GASP: 165..375 -> 0.5..1.0. Those are exactly our WalkSpeed and RunSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean")
	FVector2D LeanSpeedRangeIn = FVector2D(165.f, 375.f);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Lean")
	FVector2D LeanSpeedRangeOut = FVector2D(0.5f, 1.f);

	// ---- Direction (AZ-only) ----

	/** false: left foot PLANTED means the right foot leads the next step -> LR / RR. There is no legacy
	 *  behaviour to preserve (v2's foot phase never fired), so this can only be settled by watching it —
	 *  and a CDO bool means settling it costs a click, not a rebuild. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	bool bInvertFootPhase = false;

	/** Below this speed the velocity direction is numerical noise, so the angle HOLDS its last value. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction", meta = (ForceUnits = "cm/s"))
	float DirectionHoldSpeed = 10.f;

	// ---- Curve names (an authoring convention, not a content path) ----

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName FootSpeedCurveL = TEXT("FootSpeed_L");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName FootSpeedCurveR = TEXT("FootSpeed_R");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName MoveDataSpeedCurve = TEXT("MoveData_Speed");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName PlayRateWarpingCurve = TEXT("Enable_PlayRateWarping");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves")
	FName DisableAOCurve = TEXT("Disable_AO");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Curves", meta = (ClampMin = "0"))
	float FootPlantedSpeedThreshold = 1.f;

	// ---- Database selection (editor-assigned; no /Game/ paths in code) ----

	/** The database-selection table: every row matching the current (MovementMode, Stance,
	 *  MovementState, Gait) contributes its databases to the search union. Empty axis = Any.
	 *  Authored on the ABP class defaults; evaluated by Get_DatabasesToSearch. All member databases
	 *  must share one normalization set (PSN_AZ_CMC) or cross-database costs are not comparable. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Databases")
	TArray<FAZ_DatabaseGate> DatabaseGates;

	/** Labels of the rows Get_DatabasesToSearch matched last evaluation — bDebugAnim diagnostics only.
	 *  Mutable because the getter is const and thread-safe; only this instance's anim worker writes it. */
	mutable TArray<FName> MatchedGateLabels;

	/** Latches the empty-union warning so it logs once per dead spot, not once per frame. */
	bool bWarnedEmptyGateUnion = false;

	// ---- Predicate tuning (GASP 5.8 values) ----

	/** IsStarting: how far future speed must exceed current speed. GASP: 100. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float StartingFutureSpeedMargin = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "deg"))
	float SpinTransitionAngleThreshold = 130.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "cm/s"))
	float SpinTransitionMinSpeed = 150.f;

	/** How far ahead Get_DesiredFacing samples the trajectory. GASP 5.8: 0.5. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Thresholds", meta = (ForceUnits = "s"))
	float DesiredFacingSampleTime = 0.5f;

	/** Database tag marking a pivot pool. IsStarting and ShouldSpinTransition both suppress themselves
	 *  while one is playing, so a pivot is never mistaken for a start or a spin. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|MotionMatching")
	FName PivotDatabaseTag = TEXT("Pivots");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	FName MontageSlotName = TEXT("DefaultSlot");

	// ---- Direction thresholds: THREE sets, chosen per frame. GASP stores these signed (-60/60/-120/120);
	// we keep our positive-magnitude convention and negate in Update_MovementDirection. ----

	/** Forward/backward travel, and sideways travel during a pivot. GASP: 60 / 120. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_Cardinal = { 60.0, 60.0, 120.0, 120.0 };

	/** Sideways, settled into a loop, not aiming — widest back quadrant. GASP: 60 / 140. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_SideLoop = { 60.0, 60.0, 140.0, 140.0 };

	/** Sideways, mid-transition or aiming — tightest forward quadrant. GASP: 40 / 140. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Direction")
	FAZ_MovementDirectionThresholds DirectionThresholds_SideTight = { 40.0, 40.0, 140.0, 140.0 };

	// ---- Node settings (GASP 5.8 values) ----

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Ground = 0.5f;

	/** The touchdown frame blends FAST, so a land reads as an impact rather than a cross-fade. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_JustLanded = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Rising = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMBlendTime_Falling = 0.5f;

	/** Above this upward speed we are still rising from a jump rather than falling. GASP: 100. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "cm/s"))
	float MMRisingVelocityZ = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Walk = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Run = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float MMNotifyRecency_Sprint = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float OffsetRootHalfLife_Idle = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings", meta = (ForceUnits = "s"))
	float OffsetRootHalfLife_Moving = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	float OffsetRootTranslationRadius = 0.1f;

	/** Mirrors whether the AnimGraph actually contains an OffsetRootBone node. Drives which space
	 *  orientation warping works in — wrong here and warping fights the offset instead of riding it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|NodeSettings")
	bool bOffsetRootBoneEnabled = true;

	// ---- Debug ----

	/** Once-a-second dump. Deliberately a CDO bool, not a CVar: our CVar rule (register in FAZModule,
	 *  unregister by NAME, never a static TAutoConsoleVariable) makes a CVar the expensive option here,
	 *  and this way it is per-character rather than global. */
	/** Drives BOTH the once-a-second log line and the per-frame viewport overlay. Deliberately one
	 *  toggle and not two: a second UPROPERTY cannot be added by Live Coding, and splitting them would
	 *  have cost an editor restart for no behavioural gain. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Cmc|Anim|Debug")
	bool bDebugAnim = false;

	// ==================================================================================
	// Internals
	// ==================================================================================

	/** Re-resolved lazily — on the first frames the pawn is unpossessed and has no ASC. */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_CmcCharacterBase> Cached_Character;

	/**
	 * The AnimGraph's OffsetRootBone node, found by walking the GENERATED class's anim node properties.
	 * Ported from UAZ_AnimInstance::FindOffsetRootBoneNode. Deliberately uncached: the scan is a handful
	 * of properties, and caching a pointer into the instance would dangle across the class reinstancing
	 * that follows every ABP recompile. Returns the FIRST such node — we author exactly one; a second
	 * would make this ambiguous, and the property scan cannot see the node's tag to disambiguate.
	 */
	FAnimNode_OffsetRootBone* FindOffsetRootBoneNode();

	/**
	 * Adds the CURRENTLY PLAYING one-shot's database back into Pool while its clip is still running.
	 *
	 * The gate table is addressed by MovementState, so the frame speed reaches 0 the matching row flips
	 * (e.g. WalkMove -> StandIdle) and the Stops database leaves the union — InterruptOnDatabaseChange then
	 * invalidates the continuing pose because its database is no longer LISTED. The clip is evicted by set
	 * membership, not by cost, which is why a -1.0 continuing-pose bias on the stop clips changed nothing
	 * (measured 2026-08-22: 82ms median survival against 0.93-1.53s of content, unchanged by the bias).
	 *
	 * Deliberately additive: everything the new state wants stays in the pool, so a re-press mid-stop still
	 * competes normally. Looping assets are skipped — they never end, so holding one would pin the search.
	 */
	void KeepPlayingOneShotSearchable(
		const FMotionMatchingAnimNodeReference& MotionMatchingNode, TArray<UPoseSearchDatabase*>& Pool) const;


	/** Written only by the dead SetOffsetRootTransform. Retire together with it. */
	FTransform OffsetRootTransform = FTransform::Identity;
	bool bHasOffsetRootTransform = false;

	/**
	 * Paints the current selection onto the viewport. GAME THREAD ONLY — called from
	 * NativeUpdateAnimation, never from the thread-safe update: GEngine->AddOnScreenDebugMessage
	 * touches shared engine state and is not safe from the anim worker thread.
	 *
	 * Reads values produced by the PREVIOUS frame's thread-safe update, which is what we want anyway:
	 * they are the values that produced the pose currently on screen.
	 */
	void DrawDebugAnimOverlay() const;

	/** Once-a-second dump of the CMC values ApplyMovementFeelParams actually wrote. GAME THREAD ONLY. */
	void LogMovementFeelOncePerSecond(float DeltaSeconds) const;

	float DebugAccumulator = 0.f;
	bool bLoggedInit = false;
};
