// Copyright Artur. AZ project.

#include "Character/AZ_PawnMovementMode_Walking.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "Animation/AnimInstance.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"   // FMoverEventContext
#include "DefaultMovementSet/Settings/StanceSettings.h"

// [v2 Facing] SIM-SIDE branch trace. The game-thread [v2 Cam] line shows WHAT the body did; this shows WHY —
// which facing branch the sim took and the smoothing time it handed the critically-damped spring. Added
// 2026-09-11 after the logs caught the body covering 95 deg in 0.25 s (373 deg/s) on aim entry, where the aim
// turn-in-place pacing should have capped it near 67: the pacing did not run, and only the sim can say whether
// that is the RotationMode, a missing custom-input block, held move input, or a delta the mode disagrees with.
static TAutoConsoleVariable<int32> CVarAZFacingDebug(
	TEXT("az.Facing.Debug"), 0,
	TEXT("1 = log [v2 Facing] sim-side facing-branch telemetry (which branch, smoothing time, delta, angular velocity)."),
	ECVF_Default);

UAZ_PawnMovementMode_Walking::UAZ_PawnMovementMode_Walking()
{
	// GASP CDO overrides — engine defaults are bSmoothFacingWithDoubleSpring=true, FacingSmoothingTime=0.25.
	// Double-spring adds an S-curve lead-out (cinematic feel); GASP turns it off for crisp locomotion
	// response. FacingSmoothingTime baseline is overridden per-tick by the gait map, but the CDO value
	// is used on the first tick before the override runs.
	bSmoothFacingWithDoubleSpring = false;
	FacingSmoothingTime = 0.5f;
	SharedSettingsClasses.Add(UStanceSettings::StaticClass());
}

FQuat UAZ_PawnMovementMode_Walking::ResolveRotationTarget(const FMoverTickStartData& StartState,
	const FQuat& DesiredFacing, const FQuat& CurrentFacing) const
{
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();

	const double RotationOffsetIn = CustomInputs ? CustomInputs->RotationOffset : 0.0;
	const double ClampedOffset = FMath::Clamp(RotationOffsetIn,
		CachedRotationOffsetDegrees - RotationOffsetClampDegrees,
		CachedRotationOffsetDegrees + RotationOffsetClampDegrees);
	return (FRotator(0.0, ClampedOffset, 0.0).Quaternion() * DesiredFacing);
}

EAZ_Gait UAZ_PawnMovementMode_Walking::ResolveGait(const FMoverTickStartData& StartState) const
{
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();
	return CustomInputs ? CustomInputs->Gait : EAZ_Gait::Walk;
}

void UAZ_PawnMovementMode_Walking::Activate(const FMoverEventContext& Context, FName PrevModeName,
	const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
	FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	// Sticky-landing: record the landing SIM time when entering from the Falling mode. The window itself
	// is derived in GenerateMove — sim-time math, no wall-clock timer, valid under time dilation and resim.
	if (PrevModeName == FallingModeName)
	{
		LandedSimTimeMs = Context.EventTimeMs;
	}
}

void UAZ_PawnMovementMode_Walking::GenerateMove_Implementation(const FMoverSimContext& SimContext,
	const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Derive the sticky-landing window in sim time, BEFORE Super runs the walk-move math (which calls our
	// GenerateWalkMove — the Deceleration pick below reads bJustLanded).
	bJustLanded = (LandedSimTimeMs >= 0.0) &&
		((TimeStep.BaseSimTimeMs - LandedSimTimeMs) < static_cast<double>(JustLandedDuration) * 1000.0);

	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
}

void UAZ_PawnMovementMode_Walking::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
	const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
	const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	// Cache the engine default inputs for the MoveInput read below. Custom-input reads go through
	// the ResolveRotationTarget / ResolveGait virtuals so subclasses can swap rotation/gait policy
	// without touching this function.
	const FCharacterDefaultInputs* DefaultInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	// Apply the Rotation Offset input to the desired facing rotation, and save as the OverridenDesiredFacing.
	// The offset is clamped to not be greater than the current offset + 179, or less than the current offset - 179.
	// This means the pawn will always rotate TOWARD the Desired Facing direction, even if the shortest path
	// would've been away from it.
	const FQuat OverridenDesiredFacing = ResolveRotationTarget(StartState, DesiredFacing, CurrentFacing);

	// Cache the current offset between the current pawn orientation and desired facing orientation.
	{
		const FRotator Delta = (CurrentFacing.Inverse() * OverridenDesiredFacing).Rotator();
		CachedRotationOffsetDegrees = FRotator::NormalizeAxis(Delta.Yaw);
	}

	// Set the Max Speed depending on the Gait Input or Crouching state.
	const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent<UMoverComponent>());
	const bool bIsCrouching = CharacterMover && CharacterMover->IsCrouching();
	const EAZ_Gait Gait = ResolveGait(StartState);

	if (bIsCrouching)
	{
		MaxSpeedOverride = CrouchSpeed;
	}
	else
	{
		switch (Gait)
		{
		case EAZ_Gait::Walk:   MaxSpeedOverride = WalkSpeed;   break;
		case EAZ_Gait::Run:    MaxSpeedOverride = RunSpeed;    break;
		case EAZ_Gait::Sprint: MaxSpeedOverride = SprintSpeed; break;
		default:               MaxSpeedOverride = RunSpeed;    break;
		}

		// RM-lite curve-follow (infected only): the zombie IPC clips carry the original root motion baked
		// into a "fwd vel" float curve (per-frame cm/s; walk 3-101 around avg 41, chase -16-186 around 77).
		// When the playing anim exposes it, the ANIM is the metronome and the capsule FOLLOWS its lurch —
		// override the gait constant with the blended curve value. The BS axis is fed the COMMANDED gait
		// speed (AnimInstance side), never this pulsing measured speed, so play rate stays authored and the
		// curve phase can't feed back into itself. No curve active (hero, idle, turn states) = constant gaits.
		// GLUE: anim read inside the sim — same class as the pawn's Turning read; both migrate to
		// FAZ_MoverCustomInputs in the turn-controller-v2 batch.
		const UMoverComponent* OwnerMover = GetMoverComponent<UMoverComponent>();
		const AAZ_PawnMoverInfectedCharacter* Infected = OwnerMover
			? Cast<AAZ_PawnMoverInfectedCharacter>(OwnerMover->GetOwner()) : nullptr;
		if (Infected)
		{
			const USkeletalMeshComponent* InfectedMesh = Infected->GetMesh();
			if (const UAnimInstance* InfectedAnim = InfectedMesh ? InfectedMesh->GetAnimInstance() : nullptr)
			{
				float CurveSpeed = 0.f;
				if (InfectedAnim->GetCurveValue(TEXT("fwd vel"), CurveSpeed))
				{
					// Floor 2: the chase clip dips negative (backstep) — a capsule reversing against its
					// nav path would fight the path-follower, so bottom out at a creep instead.
					// Ceiling 500: fastest measured set clip (HyperChase_3) averages 421 with peaks near it.
				MaxSpeedOverride = FMath::Clamp(CurveSpeed, 2.f, 500.f);
				}
			}
		}
	}

	// Set the Acceleration depending on the Gait Input. The Sprint Acceleration is only applied
	// when moving faster than the run speed, effectively allowing us to tune how fast you can get
	// from stationary to the max run speed separately from how fast we can get from the run to
	// max sprint speed.
	const float CurrentSpeedXY = static_cast<float>(InOutVelocity.Size2D());
	float BaseAccel;
	switch (Gait)
	{
	case EAZ_Gait::Walk:   BaseAccel = WalkAcceleration; break;
	case EAZ_Gait::Run:    BaseAccel = RunAcceleration;  break;
	case EAZ_Gait::Sprint: BaseAccel = RunAcceleration;  break;
	default:               BaseAccel = RunAcceleration;  break;
	}
	const float SprintAdd = (CurrentSpeedXY > RunSpeed) ? SprintAcceleration : 0.f;
	Acceleration = BaseAccel + SprintAdd;

	// Set the Deceleration based on if there is Movement Input or not. This allows us to tune
	// stopping deceleration separately from Gait change deceleration, like when going from
	// sprint to run or run to walk. Also, we greatly increase the deceleration if we've just
	// landed which makes the landings feel sticky and responsive.
	const FVector WorldMoveInput = DefaultInputs ? DefaultInputs->GetMoveInput_WorldSpace() : FVector::ZeroVector;
	const bool bMoveInputZero = WorldMoveInput.IsNearlyZero();

	if (bJustLanded)
	{
		Deceleration = JustLandedDeceleration;
	}
	else
	{
		Deceleration = bMoveInputZero ? StoppingDeceleration : GaitChangeDeceleration;
	}

	// Set the Turning Strength, which controls how quickly the pawn can change its velocity
	// direction (not its rotation). This essentially controls how responsive the pawn is when
	// cornering. A low turning strength will make the pawn feel sluggish, and a high turning
	// strength will make the pawn feel snappy.
	TurningStrength = static_cast<float>(FMath::GetMappedRangeValueClamped(
		FVector2D(RunSpeed, SprintSpeed),
		FVector2D(WalkRunTurnStrength, SprintTurnStrength),
		CurrentSpeedXY));

	// Set the Facing Smoothing Time, which controls how fast the pawn can rotate to get to the
	// desired facing direction. Strafe (combat-ready) overrides it with a tight aim-lock.
	const FAZ_MoverCustomInputs* FacingInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();
	// AIMING gets its own FLAT facing time and skips the strafe ramp entirely — see AimFacingTime's comment: the
	// ramp paces the body against a turn-start clip that never plays while aiming, so it was pure lag. Checked
	// first because Aiming used to fall into the strafe branch below.
	const bool bAimFacing = FacingInputs && FacingInputs->RotationMode == EAZ_RotationMode::Aiming;
	const bool bStrafeFacing = FacingInputs && FacingInputs->RotationMode == EAZ_RotationMode::Strafe;
	const bool bHadFacingInputs = (FacingInputs != nullptr);   // [v2 Facing] diagnostic only

	if (bAimFacing)
	{
		// Flat, fast spring. The aim turn-in-place is a yaw-rate CLAMP applied after the spring below (from the
		// InputCmd's AimTurnYawRateLimit), never state kept on this mode object: the trajectory predictor runs this
		// function ~60 times a frame and any latch here is overwritten by those steps.
		bAimTurningInPlace = FacingInputs->AimTurnYawRateLimit > KINDA_SMALL_NUMBER;   // [v2 Facing] 'tip' = a yaw-rate limit is active (turn-in-place OR the plain aim cap)
		FacingSmoothingTime = AimFacingTime;
	}
	else if (bStrafeFacing)
	{
		// Strafe (combat-ready): aim-lock spring, but RAMP the spring time by how far the body is off its target
		// so a big move-start turn matches the turn-start CLIP instead of outrunning it. The angle band mirrors the
		// SM turn-start buckets: <=45deg is the Fwd bucket (no turn clip) -> snappy StrafeFacingTime; >=135deg is the
		// 135/180 bucket -> slow StrafeTurnFacingTime so the body turns over ~the clip's length; lerp between. As the
		// body closes on the target the angle shrinks and the spring tightens back to the snappy aim-lock, so steady
		// camera-tracking and small adjustments stay rigid. |delta| is already cached above (CachedRotationOffsetDegrees,
		// signed current->desired facing yaw). The idle HOLD + latched/frozen move-start target are produced by
		// OrientationIntent in ProduceInput, NOT here — this only sets HOW FAST we spring toward that target.
		constexpr float TurnRampStartDeg = 45.f;    // == SM TurnBucketFwd_90Deg: below this there is no turn-start clip
		constexpr float TurnRampFullDeg  = 135.f;   // == SM 135/180 bucket onset: at/above this use the full slow time
		FacingSmoothingTime = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(TurnRampStartDeg, TurnRampFullDeg),
			FVector2D(StrafeFacingTime, StrafeTurnFacingTime),
			FMath::Abs(CachedRotationOffsetDegrees)));
	}
	else
	{
		if (bMoveInputZero)
		{
			FacingSmoothingTime = IdleFacingTime;
		}
		else
		{
			FacingSmoothingTime = static_cast<float>(FMath::GetMappedRangeValueClamped(
				FVector2D(RunSpeed, SprintSpeed),
				FVector2D(WalkRunFacingTime, SprintFacingTime),
				CurrentSpeedXY));
		}

		// GASP BP_MovementMode_Walking parity: when |Δfacing| > 90°, subtract up to 0.2s
		// from FacingSmoothingTime so the capsule's angular velocity ramps up to at least
		// the control rotation's rate (which is high when the camera is turning quickly).
		// Without this, the spring damper under-rotates past ~135° and the mesh lags.
		// EXPLORE ONLY — in strafe the body already tracks the camera tightly, and this snap is exactly
		// what produced the catch-up spin when the camera was whipped around.
		const float FacingDeltaAbs = static_cast<float>(FMath::Abs(
			FRotator::NormalizeAxis((CurrentFacing.Inverse() * DesiredFacing).Rotator().Yaw)));
		const float SnapShorten = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(CameraSnapShortenStartAngle, CameraSnapShortenFullAngle),
			FVector2D(0.f, CameraSnapShortenMaxSeconds),
			FacingDeltaAbs));
		FacingSmoothingTime = FMath::Max(0.f, FacingSmoothingTime - SnapShorten);
	}

	if (CVarAZFacingDebug.GetValueOnAnyThread() != 0)
	{
		const float DbgDelta = static_cast<float>(CachedRotationOffsetDegrees);
		if (FMath::Abs(DbgDelta) > 3.f || FMath::Abs(InOutAngularVelocityDegrees.Z) > 10.f)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[v2 Facing] inputs=%d mode=%d aim=%d strafe=%d moveZero=%d tip=%d delta=%+.1f T=%.3f angVelZ=%+.0f dt=%.4f"),
				bHadFacingInputs ? 1 : 0,
				bHadFacingInputs ? static_cast<int32>(FacingInputs->RotationMode) : -1,
				bAimFacing ? 1 : 0, bStrafeFacing ? 1 : 0, bMoveInputZero ? 1 : 0,
				bAimTurningInPlace ? 1 : 0, DbgDelta, FacingSmoothingTime,
				InOutAngularVelocityDegrees.Z, DeltaSeconds);
		}
	}

	// GRABBED overrides both branches above: the held body must reach the grabber's line inside the catch
	// close-in from ANY start angle. ProduceInput points OrientationIntent at the grabber; this only makes the
	// spring fast enough to matter before the paired clips' first frame (see GrabbedFacingTime).
	if (FacingInputs && FacingInputs->bGrabbed)
	{
		FacingSmoothingTime = GrabbedFacingTime;
	}

	// Phase 4: pass OverridenDesiredFacing (raw DesiredFacing + aim offset) to the parent spring-damper.
	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity,
		OverridenDesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);

	// AIM TURN-IN-PLACE: clamp the yaw rate the spring asked for. Full turn speed from the first tick, a spring-shaped
	// tail only in the last few degrees, identical in the live tick and in every predictor step. The clamped value
	// is also next tick's spring velocity state, so the spring never carries a faster rate into the next tick.
	if (FacingInputs && FacingInputs->AimTurnYawRateLimit > KINDA_SMALL_NUMBER)
	{
		const double Limit = FacingInputs->AimTurnYawRateLimit;
		InOutAngularVelocityDegrees.Z = FMath::Clamp(InOutAngularVelocityDegrees.Z, -Limit, Limit);
	}

	// GRABBED = ROOTED. The input layer zeroes the move intent at the catch, but a running body keeps its
	// momentum and brakes at StoppingDeceleration — at sprint that is on the order of the whole PSI catch
	// spacing (~86cm), so a hero caught mid-run slid INTO the grabber the search had just placed in front
	// of him (measured 2026-09-02: facing squared to -3° by 0.3s, bodies still coincident — and only when
	// caught moving). Kill the planar velocity here, in the sim, the frame the hold starts. Z stays with
	// the floor snap; layered moves (the v1 socket anchor, hit-react root motion) still mix in after this.
	if (FacingInputs && FacingInputs->bGrabbed)
	{
		InOutVelocity.X = 0.f;
		InOutVelocity.Y = 0.f;

		// Diagnostic ([GrabFace], sim-side, only while the held body is still off its target): proves the
		// grabbed flag reached the sim, which smoothing time the spring got, and WHAT it is chasing — the
		// raw intent (the grabber) vs the offset-composed target the parent is actually handed. Measured
		// 2026-09-02: the @0.3s error stalled at -19/+18/+77 with GrabbedFacingTime=0.04 — either the flag is
		// not here or the target is not the grabber.
		const float ErrToIntent = static_cast<float>(FRotator::NormalizeAxis((CurrentFacing.Inverse() * DesiredFacing).Rotator().Yaw));
		const float ErrToTarget = static_cast<float>(FRotator::NormalizeAxis((CurrentFacing.Inverse() * OverridenDesiredFacing).Rotator().Yaw));
		if (FMath::Abs(ErrToIntent) > 5.f)
		{
			UE_LOG(LogTemp, Display, TEXT("[GrabFace] smooth=%.3f errIntent=%+.0f errTarget=%+.0f rotOffset=%+.0f angVelZ=%+.0f strafe=%d"),
				FacingSmoothingTime, ErrToIntent, ErrToTarget, CachedRotationOffsetDegrees, InOutAngularVelocityDegrees.Z, bStrafeFacing ? 1 : 0);
		}
	}
}
