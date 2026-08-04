// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "Character/AZ_JumpRequester.h"
#include "Character/AZ_PawnCameraMovementComponent.h"   // FAZ_CameraStanceConfig (per-mode camera framing)
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "MoverSimulationTypes.h"
#include "AZ_PawnMoverHeroCharacter.generated.h"

class UAbilitySystemComponent;
class UAZ_GameplayAbility;
class UAZ_PawnMoverComponent;
class UAZ_MovementDirectionCapabilityComponent;
class UNetworkPredictionComponent;
class UMoverTrajectoryPredictor;
class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * AAZ_PawnMoverHeroCharacter — v2 hero pawn (foundation skeleton).
 *
 * APawn-based, Mover-driven. Lean foundation per the v2 architecture:
 * components only at this stage; per-frame state derivation, GASP Get_* functions,
 * and chooser context are owned by the AnimInstance (UAZ_PawnMoverAnimInstance).
 * Rotation policy and RM bridging belong to the Mover mode (added in a later step).
 *
 * Interfaces:
 *  - IAbilitySystemInterface       — GAS bridge (returns PlayerState ASC; nullptr until Step 4).
 *  - IMoverInputProducerInterface  — Mover input producer (zero-input stub until Step 2).
 *  - IGameplayTagAssetInterface    — tag queries route through the ASC; lets any system
 *                                    (Niagara, AI perception, UMG, gameplay code) check
 *                                    the pawn's tag state without knowing about GAS.
 *  - IGenericTeamAgentInterface    — AI perception / faction. Player default team id;
 *                                    AI subclass overrides at spawn.
 *
 * INavAgentInterface is inherited from APawn — no override needed.
 *
 * Implementation order (architecture doc):
 *  Step 1 — this class: components + lifecycle stubs + interface stubs.
 *  Step 2 — input wiring (Enhanced Input → ProduceInput → Mover).
 *  Step 3 — UAZ_PawnMoverSmoothWalkingMode with ResolveRotationTarget() virtual.
 *  Step 4 — GAS link (ASC accessor → PlayerState), tag taxonomy.
 *  Step 5+ — per-state work (Idle → Start → Locomotion → ...).
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_PawnMoverHeroCharacter
	: public APawn
	, public IAbilitySystemInterface
	, public IMoverInputProducerInterface
	, public IGameplayTagAssetInterface
	, public IGenericTeamAgentInterface
	, public IAZ_JumpRequester
{
	GENERATED_BODY()

public:
	AAZ_PawnMoverHeroCharacter(const FObjectInitializer& ObjectInitializer);

	// ========================================
	// APawn
	// ========================================
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** PC pulls this on OnPossess to push the pawn's IMC into the local player's
	 *  EnhancedInput subsystem — keeps the PC pawn-agnostic across hero/vehicle classes. */
	UFUNCTION(BlueprintPure, Category = "AZ|Input")
	UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

	// ========================================
	// IAbilitySystemInterface (Step 1 stub — returns nullptr; PlayerState in Step 4)
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========================================
	// IMoverInputProducerInterface (Step 1 stub — emits zero input; Step 2 wires real input)
	// ========================================
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ========================================
	// IGameplayTagAssetInterface — routes queries through the ASC.
	// Anything that needs to ask "is this pawn aiming / sprinting / slowed?" goes here.
	// ========================================
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	// ========================================
	// IGenericTeamAgentInterface — AI perception / faction.
	// Default team is configurable per pawn class; AI subclass sets enemy team at spawn.
	// ========================================
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override;

	// ========================================
	// Component accessors
	// ========================================
	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UAZ_PawnMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UMoverTrajectoryPredictor* GetTrajectoryPredictor() const { return TrajectoryPredictor; }

	/** The "where can I move" query — clamps move intent to free directions in ProduceInput. */
	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UAZ_MovementDirectionCapabilityComponent* GetMovementCapability() const { return MovementCapability; }

	/** RAW (pre-clearance-clamp) world-space move intent produced this sim tick. The obstacle sensor reads THIS
	 *  (not the clamped Mover input cmd) so a straight-in wall hit still registers after the clamp zeroes the cmd. */
	FVector GetWorldMoveIntentRaw() const { return CachedWorldMoveIntentRaw; }

	/** GRAB facing: while State.Grabbed, ProduceInput points OrientationIntent at this actor so the
	 *  body turns to meet the grabber (struggle reads face-to-face, not back-bitten). Set by
	 *  GA_PlayerGrabbed on catch, cleared (nullptr) on every grab exit.
	 *  Also drives the cinematic camera: a non-null target STAMPS the pre-grab control rotation and
	 *  starts the yaw sweep; clearing it starts the restore back to that stamped rotation. */
	void SetGrabFacingTarget(const AActor* Target);

	/** The current grabber (null outside a grab). AnimInstance reads this for the grab hand-IK targets. */
	const AActor* GetGrabFacingTarget() const { return GrabFacingTarget.Get(); }

	/** GRAB HEIGHT MATCH: lift the MESH inside the capsule until our socket meets the grabber's hand.
	 *  The capsule cannot carry this — Walking mode floor-snaps it every tick — so the "held off the
	 *  ground" read is a mesh offset, leaving the capsule grounded and collidable. Called by
	 *  GA_PlayerGrabbed on catch; ClearGrabMeshAnchor eases the mesh back on every exit. */
	void SetGrabMeshAnchor(const USkeletalMeshComponent* InAnchorMesh, FName InAnchorSocket,
		FName InOwnSocket, const FVector& InAnchorSpaceOffset);
	void ClearGrabMeshAnchor();

	/** Grab-victim ability, EDITOR-ASSIGNED (no hardcoded /Game/ paths in C++, user rule 2026-07-24):
	 *  point at BP_GA_PlayerGrabbed in the hero pawn BP; unset falls back to the native class. Granted
	 *  natively at possess with the Interact dynamic tag (E-mash routing). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<class UGameplayAbility> GrabbedAbilityClass;

	// ========================================
	// Components
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCameraComponent> Camera;

	// ========================================
	// Camera — per rotation-mode framing (Explore / Strafe / Aiming)
	// ========================================
	// The boom (TargetArmLength + SocketOffset) and FOV are interpolated each Tick toward the config for the
	// CURRENT rotation mode (resolved from GAS tags), so switching mode glides the framing instead of snapping.
	// "Mode" mirrors ProduceInput's RotationMode pick: Aiming > Strafe > Explore. Set per-mode SocketOffset (etc.)
	// in the details panel; leave fields equal across modes to make that field not change between them.

	/** OrientToMovement (explore) — the default framing. Defaults match the boom set up in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraExplore;

	/** Strafe (combat-ready) — e.g. shifted over-the-shoulder for an aim-able framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraStrafe;

	/** Aiming (ADS) — close + narrow. Used once the Aiming rotation mode is wired (ProduceInput sets it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraAiming;

	/** Grabbed (State.Grabbed) — pulled in tight on the struggle. HIGHEST precedence: while a Chalkie
	 *  holds you nothing else frames the shot. Look input is frozen separately (OnLookTriggered gate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraGrabbed;

	// Grabbed-camera ROTATION framing (the boom/FOV part lives in CameraGrabbed above): the camera
	// auto-turns to the grabber while look input is frozen; these shape that look-at.

	/** Yaw offset off the direct look-at line (deg): + swings the camera right for a side/over-shoulder
	 *  composition, 0 = dead-on at the grabber. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed")
	float GrabbedCameraYawOffsetDeg = 0.f;

	/** Pitch offset added to the look-at (deg): negative tilts down onto the struggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed")
	float GrabbedCameraPitchOffsetDeg = -5.f;

	/** Rotation catch-up speed (per second) — separate from CameraGrabbed.InterpSpeed (boom/FOV glide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed", meta = (ClampMin = "0.5"))
	float GrabbedCameraRotationSpeed = 5.f;

	// --- CINEMATIC SWEEP: the camera arcs around the clinch for the length of the hold, then returns ---
	// GrabbedCameraYawOffsetDeg above is the sweep's START; this is where it ends. The arc is deliberately
	// NOT linear — a constant-rate orbit reads as a machine. Easing gives it weight: heavy to start (you
	// are caught), committed through the middle, settling rather than stopping at the end.
	// Set End == Offset to disable the sweep and keep the old fixed framing.

	/** Yaw the sweep arrives at, in degrees, relative to the look-at basis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed")
	float GrabbedCameraYawEndDeg = 55.f;

	/** Seconds the arc takes. Pair with the escape window so it lands as the struggle resolves; a sweep
	 *  shorter than the hold finishes early and sits still, longer and it is cut off mid-arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed", meta = (ClampMin = "0.1"))
	float GrabbedCameraSweepSeconds = 6.f;

	/** Ease exponent for the built-in curve. 1 = linear, 2 = gentle, 3 = the dramatic default, higher =
	 *  the move hangs then lunges. Ignored when GrabbedCameraSweepCurve is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed", meta = (ClampMin = "1.0"))
	float GrabbedCameraSweepExponent = 3.f;

	/** Optional authored easing: X = sweep progress 0..1, Y = eased 0..1. Overrides the exponent when set,
	 *  so a curve asset can give the arc a hitch or an overshoot that no single exponent can express. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed")
	TSoftObjectPtr<class UCurveFloat> GrabbedCameraSweepCurve;

	/** Seconds to glide the camera back to where it was before the catch, once the grab releases. 0 =
	 *  hand control back instantly. Any look input from the player cancels the restore immediately —
	 *  the camera returning is a courtesy, not something to wrestle away from them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Grabbed", meta = (ClampMin = "0"))
	float GrabbedCameraRestoreSeconds = 0.8f;

	// AUTHORING NOTE — left/right lurching comes from the CURVE, not from extra code. Treat the two yaw
	// values as the arc's extremes (e.g. Offset -40, End +40) and let GrabbedCameraSweepCurve oscillate
	// 0 -> 1 -> 0 -> 1 between them; packing its keys tighter toward the end makes the swings accelerate.
	// Curve values outside 0..1 extrapolate past the extremes, which is how you get an overshoot.

	// ========================================
	// Grab — MESH height match (see SetGrabMeshAnchor)
	// ========================================

	/** How fast the mesh rises to / settles back from the hold (per second). Low = a heave, high = a snatch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Grab|MeshLift", meta = (ClampMin = "0.5"))
	float GrabMeshLiftSpeed = 8.f;

	/** Sanity clamp on the lift, cm, relative to the mesh's authored offset. Min guards a socket authored
	 *  too high (mesh sinking into the floor); Max stops a bad socket from hoisting the hero into orbit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Grab|MeshLift", meta = (ForceUnits = "cm"))
	float GrabMeshLiftMin = -30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Grab|MeshLift", meta = (ForceUnits = "cm"))
	float GrabMeshLiftMax = 150.f;

protected:
	// ========================================
	// Input (Enhanced Input)
	// ========================================

	/** IMC pushed onto the local player's EnhancedInput subsystem when the PC possesses
	 *  this pawn. Pawn-owned so each pawn class declares its own input surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRateYaw = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRatePitch = 1.f;

	// Cached input state — written on the game thread (Enhanced Input handlers for
	// movement, IAZ_JumpRequester for jump), read by ProduceInput_Implementation to
	// fill the deterministic Mover InputCmd.
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	bool bIsJumpPressed = false;
	bool bIsJumpJustPressed = false;

	/** See SetGrabFacingTarget. */
	TWeakObjectPtr<const AActor> GrabFacingTarget;

	// --- Cinematic grab camera (see SetGrabFacingTarget / UpdateCameraForMode) ---

	/** Where the camera was looking the instant before the catch. Stamped on grab start, restored on
	 *  release — so the player is handed back the view they chose, not wherever the struggle ended. */
	FRotator PreGrabControlRotation = FRotator::ZeroRotator;

	/** World time the sweep began; sweep progress is (now - this) / GrabbedCameraSweepSeconds. */
	double GrabCameraSweepStartTime = 0.0;

	/** World time the restore began, and the rotation it started from. Easing must run from a FIXED
	 *  start — lerping from "wherever the camera is now" every frame turns any curve into an
	 *  exponential approach and throws the authored shape away. */
	double GrabCameraRestoreStartTime = 0.0;
	FRotator GrabCameraRestoreFrom = FRotator::ZeroRotator;

	/** Restore in flight. Cleared by a new grab, by completion, or by any look input (see OnLookTriggered). */
	bool bRestoringGrabCamera = false;

	/** Eased 0..1 sweep progress — the authored curve when set, otherwise an ease-in-out of
	 *  GrabbedCameraSweepExponent. Split out so the easing choice is testable in one place. */
	float ComputeGrabCameraSweepAlpha() const;

	// --- Grab MESH height match (see SetGrabMeshAnchor). Weak: a grabber destroyed mid-hold must not
	// keep the mesh hoisted — the lift eases back the moment the anchor goes stale. ---
	TWeakObjectPtr<const USkeletalMeshComponent> GrabAnchorMesh;
	FName GrabAnchorSocket = NAME_None;
	FName GrabOwnSocket = NAME_None;
	FVector GrabAnchorSpaceOffset = FVector::ZeroVector;

	/** The mesh's authored relative Z, captured on BeginPlay so BP overrides of the mesh transform are
	 *  respected — the lift is always measured from (and restored to) THIS, never a hardcoded -92. */
	float DefaultMeshRelativeZ = 0.f;

	/** Eases the mesh toward the height that puts GrabOwnSocket on the grabber's hand, and back to the
	 *  authored offset once the anchor clears. Called every Tick; early-outs when nothing is pending. */
	void UpdateGrabMeshAnchor(float DeltaTime);

	/**
	 * MOVEMENT CANCEL. Asking to move during an attack's RECOVERY phase abandons the attack.
	 *
	 * The attack publishes its own recovery window as State.Combat.CancelWindow; this reads that tag and
	 * the movement intent, and cancels by ABILITY TAG. Deliberately inverted — movement does not ask the
	 * attack for permission and the attack does not poll the Mover input pipeline; each side owns what it
	 * already knows. Startup and the strike are unaffected: the window simply is not open yet.
	 *
	 * Cancelling the ABILITY (rather than just releasing the root-motion drive) is what makes this look
	 * right: EndAbility releases the drive AND the montage task stops the montage with its blend, so
	 * locomotion takes the body back. Releasing the drive alone would hand back control while the
	 * full-body montage still owned the pose — control without the animation to match it.
	 *
	 * Runs on the game thread from Tick, NOT from ProduceInput: cancelling an ability inside the Mover
	 * simulation callback would re-enter GAS from the movement step.
	 */
	void TryMovementCancelAttack();

	/** Stick/key deflection that counts as "I want to move" for the cancel above. High enough that a
	 *  resting stick's drift never silently eats an attack the player meant to finish. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float AttackCancelInputDeadzone = 0.25f;

	// The RAW world-space move intent produced this sim tick, captured in ProduceInput BEFORE the movement-
	// capability clamp. Exposed via GetWorldMoveIntentRaw() so the obstacle sensor keeps sensing the wall even
	// after the clamp zeroes the shipped Mover input cmd (otherwise a straight-in hit would self-blind).
	FVector CachedWorldMoveIntentRaw = FVector::ZeroVector;

	// Strafe: at idle the body HOLDS its facing (no camera follow); a move aligns it to the camera. This latch
	// keeps the align going until the body reaches the camera even if the move was a brief tap, so one tap aligns
	// fully instead of freezing part-way. While set (or move held), face the align target; else (idle) hold current facing.
	bool bStrafeAligning = false;
	// The yaw the post-release align finishes to: the camera yaw CAPTURED at the moment input was released (kept
	// current while a move is held). The latch drives to THIS frozen heading, not the live camera, so turning the
	// camera after you stop does not drag the idle body around (that was the "idle still adjusts to camera" bug).
	float StrafeAlignTargetYaw = 0.f;

	/** Interp the camera boom (arm length + socket offset) and FOV toward the current rotation-mode config.
	 *  Local-viewer only; called every Tick. */
	void UpdateCameraForMode(float DeltaTime);

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);

public:
	// ========================================
	// IAZ_JumpRequester — flag-flipper called by UAZ_GA_PawnJump (GAS gates,
	// Mover executes). Mover reads bIsJumpPressed from the cached flag during its
	// next sim tick; the deterministic InputCmd carries it through NetworkPrediction.
	// ========================================
	virtual void SetJumpPressed(bool bPressed) override;

protected:

	// ========================================
	// AI-audible movement noise (TLOU-style stealth: speed = loudness)
	// ========================================

	/** How far a SPRINTING footstep carries to AI Hearing. 0 disables movement noise entirely. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Noise", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SprintNoiseRange = 1200.f;

	/** How far a RUNNING footstep carries. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Noise", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RunNoiseRange = 800.f;

	/** How far a WALKING footstep carries. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Noise", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WalkNoiseRange = 300.f;

	/** Crouched movement noise = range * this (crouch-walk is nearly silent — the stealth verb). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchNoiseScale = 0.25f;

	/** Seconds between noise reports while moving (throttle — one "footstep" per interval). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Noise", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float NoiseIntervalSeconds = 0.4f;

	/** Report a movement noise event to AI Hearing, throttled + scaled by current speed/stance. */
	void ReportMovementNoise();

	double LastMovementNoiseTimeSeconds = 0.0;

	// ========================================
	// GAS
	// ========================================

	/** Abilities granted to the player ASC on first possession. Granting happens on
	 *  the server in PossessedBy; the player ASC lives on PlayerState so abilities
	 *  persist across pawn-switch (vehicle entry / respawn). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|GAS")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupAbilities;

	/** Init the ASC's ActorInfo with (PlayerState owner, this pawn avatar) on possess.
	 *  On server also grants StartupAbilities (idempotent — re-possession is a no-op). */
	void InitAbilitySystem();

	// ========================================
	// Mover
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UAZ_PawnMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UNetworkPredictionComponent> NetworkPredictionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|MotionMatching")
	TObjectPtr<UMoverTrajectoryPredictor> TrajectoryPredictor;

	/** "Where can I move" clearance query — clamps the move intent to free directions in ProduceInput (reuses
	 *  Mover's slide math). Created in the ctor; self-contained (finds the Mover + capsule in BeginPlay). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UAZ_MovementDirectionCapabilityComponent> MovementCapability;

	/** Motion warping — deforms the root-motion delta of an attack montage so a lunge lands on its target
	 *  instead of covering the clip's authored distance. Self-wiring: UMoverComponent::BeginPlay finds it
	 *  by class and builds a UMotionWarpingMoverAdapter. Idle unless a montage carries a MotionWarping
	 *  notify AND gameplay registered a matching named target, so owning it costs nothing per frame.
	 *  Mirrors the Chalkie side (AAZ_PawnMoverInfectedCharacter). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<class UMotionWarpingComponent> MotionWarpingComponent;

	/** Default team id for this pawn class. Player pawns default to 0; AI subclasses
	 *  set their team in the constructor or at spawn. Read by AI perception. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 0;

	/** Live team id, overridable at runtime via SetGenericTeamId (e.g. faction switch). */
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
