// Copyright Artur. AZ project.

#include "Character/Cmc/AZ_CmcHeroCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"
#include "AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.h"
#include "Animation/AZ_CmcAnimInstance.h"
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "Animation/AZ_LocomotionStateMachine.h"   // BucketStartDirection — one owner for the turn thresholds
#include "AZ_GameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Player/AZ_PlayerState.h"

AAZ_CmcHeroCharacter::AAZ_CmcHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// --- CMC feel (starting point = v2 gait speeds + GASP-flavored accel; P1 tunes against the Mover
	// build side-by-side; further tuning belongs in the BP child's CharacterMovement panel, NOT here
	// once children exist — doctrine rule 1). ---
	// NOTE: MaxAcceleration / BrakingDecelerationWalking / GroundFriction / RotationRate are deliberately
	// NOT set here — ApplyMovementFeelParams() recomputes all four every frame and is their single owner.
	// Setting them here too would look authoritative in the BP details panel while being overwritten on
	// the first tick.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = 420.f;
	Move->GravityScale = 1.5f;
	Move->AirControl = 0.2f;
	Move->MaxStepHeight = 30.f;
	Move->SetWalkableFloorAngle(38.f);

	// --- Camera (P0 framing ≈ v2 explore stance; per-mode stances land in P2) ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 220.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->CameraRotationLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->SetFieldOfView(80.f);
}

namespace AZ::RmMontage
{
	// RM MONTAGE HAND-BACK. The first version committed the player for the CLIP'S WHOLE LENGTH
	// (0.77-1.4s), which reads exactly as reported: "as soon as we start RM animation inputs are
	// prohibited unless it ends". Root motion itself was working perfectly throughout (measured
	// 2026-08-25: mtgHasRM=1 charHasRM=1, rmDelta 0.10 -> 2.22, vel 5 -> 113) — the fault was the
	// commitment, not the mechanism.
	//
	// Measured: every start clip delivers 95-100% of its ROTATION by 50-60% of its length; the rest is
	// the run-out into the loop, which CMC can do itself and which the player should own. RM therefore
	// keeps the plant and the turn — the part only it can deliver — and hands back at ReleaseFraction
	// carrying whatever velocity it built.
	//
	// Two interrupts on top, because a start that ignores a changed mind is the same lockout in
	// miniature: a hard direction change and a released stick both blend out immediately.
	// File-scope so this is testable under Live Coding; single locally-controlled hero.
	// TODO promote to EditDefaultsOnly members at the next editor-closed build.
	static constexpr float ReleaseFraction   = 0.60f;
	static constexpr float InterruptAngleDeg = 60.f;
	static constexpr float BlendOutSeconds   = 0.15f;

	static bool    bActive = false;
	static double  LaunchTime = -1.0;
	static bool    bIsStop = false;
	static FVector LaunchDir = FVector::ZeroVector;
}

void AAZ_CmcHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// GASP's PreCMCTick ordering: the derived-parameter pass must land BEFORE the movement component
	// consumes it, otherwise CMC spends the frame on last frame's braking/accel values and the feel lags
	// input by exactly one tick. Declaring the actor a prerequisite of CMC is what guarantees that.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
	// The pose is evaluated from the post-move transform, so the mesh follows the actor as well.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void AAZ_CmcHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Order matters: the gait resolves this frame's MaxWalkSpeed, and the feel pass tapers acceleration
	// against the speed that gait implies.
	ResolveGaitAndStanceFromTags();
	UpdateSelectionGait();   // owns the stop-band latch that braking and pool selection both read
	// RM STOP on the stop EDGE. UpdateSelectionGait latched bStopActive this frame if the stick was
	// released while moving; that edge is the only moment a stop clip can start at its own frame 0.
	if (bStopActive && !bStopActive_LastFrameHero)
	{
		TryPlayRootMotionStop();
	}
	bStopActive_LastFrameHero = bStopActive;

	// RM HAND-BACK: release the montage once it has delivered what only it can deliver, or the moment
	// the player asks for something else. Without this the clip owns the character to its last frame.
	if (AZ::RmMontage::bActive)
	{
		USkeletalMeshComponent* RmMesh = GetMesh();
		UAnimInstance* RmAnim = RmMesh ? RmMesh->GetAnimInstance() : nullptr;
		UAnimMontage* Mtg = RmAnim ? RmAnim->GetCurrentActiveMontage() : nullptr;
		if (!RmAnim || !Mtg)
		{
			AZ::RmMontage::bActive = false;
		}
		else
		{
			const float Len = Mtg->GetPlayLength();
			const float Frac = (Len > KINDA_SMALL_NUMBER) ? (RmAnim->Montage_GetPosition(Mtg) / Len) : 1.f;
			const float Now2 = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
			const bool bInputHeld = ((Now2 - LastMoveInputTime) <= 0.1f) && !LastMoveInputDir.IsNearlyZero();

			const TCHAR* Why = nullptr;
			if (Frac >= AZ::RmMontage::ReleaseFraction)
			{
				Why = TEXT("delivered");
			}
			else if (!AZ::RmMontage::bIsStop && !bInputHeld)
			{
				Why = TEXT("input released");
			}
			else if (bInputHeld && !AZ::RmMontage::LaunchDir.IsNearlyZero()
				&& FMath::Abs(FRotator::NormalizeAxis(static_cast<float>(
					LastMoveInputDir.Rotation().Yaw - AZ::RmMontage::LaunchDir.Rotation().Yaw)))
					> AZ::RmMontage::InterruptAngleDeg)
			{
				Why = TEXT("redirected");
			}
			else if (AZ::RmMontage::bIsStop && bInputHeld
				&& LastMoveInputTime > (AZ::RmMontage::LaunchTime + 0.05))
			{
				// The input must have arrived AFTER the stop began. Without that test the 0.1s input
				// staleness window still counts the frames from just BEFORE the release, so every RM
				// stop cancelled itself on its first frame (measured 2026-08-25: 19 of 19 at 0%).
				Why = TEXT("stop cancelled");
			}

			if (Why)
			{
				RmAnim->Montage_Stop(AZ::RmMontage::BlendOutSeconds, Mtg);
				AZ::RmMontage::bActive = false;
				bRootMotionStopActive = false;
				UE_LOG(LogTemp, Display, TEXT("[CmcRmEnd] %s at %.0f%% vel=%.0f"), Why, Frac * 100.f,
					GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : -1.f);
			}
		}
	}

	// Clear the RM-stop flag once the montage is gone, so the contract resumes ownership next time.
	if (bRootMotionStopActive)
	{
		const UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!Anim || !Anim->IsAnyMontagePlaying())
		{
			bRootMotionStopActive = false;
		}
	}

	// [CmcRmDbg] Is montage root motion actually REACHING the capsule? The clips carry real root motion
	// (WalkFwdStart travels 73cm in 0.77s, verified), the montage plays, and yet the body reads spd=0
	// during it. Four things have to line up and this prints all four rather than assuming any of them:
	// the montage exists and claims root motion, the character reports HasAnimRootMotion, CMC's
	// RootMotionParams carry a delta, and the resulting velocity is non-zero.
	if (const USkeletalMeshComponent* DbgMesh = GetMesh())
	{
		if (UAnimInstance* DbgAnim = DbgMesh->GetAnimInstance())
		{
			if (UAnimMontage* Mtg = DbgAnim->GetCurrentActiveMontage())
			{
				const UCharacterMovementComponent* M = GetCharacterMovement();
				UE_LOG(LogTemp, Display,
					TEXT("[CmcRmDbg] %s pos=%.2f mtgHasRM=%d charHasRM=%d rmParams=%d rmDelta=%.2f vel=%.0f mode=%d"),
					*GetNameSafe(Mtg), DbgAnim->Montage_GetPosition(Mtg),
					Mtg->HasRootMotion() ? 1 : 0,
					IsPlayingRootMotion() ? 1 : 0,
					M ? (M->RootMotionParams.bHasRootMotion ? 1 : 0) : -1,
					M ? M->RootMotionParams.GetRootMotionTransform().GetTranslation().Size2D() : -1.f,
					M ? M->Velocity.Size2D() : -1.f,
					static_cast<int32>(DbgAnim->RootMotionMode));
			}
		}
	}

	ApplyMovementFeelParams(DeltaSeconds);
	UpdateTurnInPlaceLock(DeltaSeconds);
}

void AAZ_CmcHeroCharacter::ResolveGaitAndStanceFromTags()
{
	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();

	// Movement.* is domain state (granted by the movement abilities via ActivationOwnedTags); we read it
	// rather than Ability.State.*, which is ability-to-ability coordination. Same source as the v2 pawn.
	EAZ_Gait DesiredGait = EAZ_Gait::Walk;
	if (HasMatchingGameplayTag(AZTags.Movement_Sprinting))
	{
		DesiredGait = EAZ_Gait::Sprint;
	}
	else if (HasMatchingGameplayTag(AZTags.Movement_Running))
	{
		DesiredGait = EAZ_Gait::Run;
	}

	if (DesiredGait != GetCurrentGait())
	{
		SetGait(DesiredGait);   // the one write point for MaxWalkSpeed
	}

	// Crouch is native here — no Mover mode, no custom input struct. Guarded on bIsCrouched so we issue
	// the request on the transition only; CMC handles the capsule resize and its replication.
	const bool bWantsCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	if (bWantsCrouch && !bIsCrouched)
	{
		Crouch();
	}
	else if (!bWantsCrouch && bIsCrouched)
	{
		UnCrouch();
	}
}

void AAZ_CmcHeroCharacter::FillAnimContract(FAZ_CmcAnimContract& Out) const
{
	Super::FillAnimContract(Out);

	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();

	// Rotation mode drives which locomotion SET the graph searches (orient-to-movement forward clips vs
	// the 8-way directional strafe set), so it has to be resolved before OrientationIntent is meaningful.
	// Aiming outranks strafe: both face the camera, but aiming additionally locks the upper body.
	if (HasMatchingGameplayTag(AZTags.Ability_State_Aiming))
	{
		Out.RotationMode = EAZ_RotationMode::Aiming;
	}
	else if (HasMatchingGameplayTag(AZTags.Movement_Strafe))
	{
		Out.RotationMode = EAZ_RotationMode::Strafe;
	}

	// Raw desires, kept separate from the RESOLVED gait/stance the base already published. The graph
	// needs both: "sprinting" is what the body is doing, "wants to sprint" is what the player asked for,
	// and they differ for exactly as long as a transition takes — which is the window the start clips fill.
	Out.InputState.bWantsToSprint = HasMatchingGameplayTag(AZTags.Movement_Sprinting);
	Out.InputState.bWantsToWalk = !Out.InputState.bWantsToSprint && !HasMatchingGameplayTag(AZTags.Movement_Running);
	Out.InputState.bWantsToCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	Out.InputState.bWantsToStrafe = HasMatchingGameplayTag(AZTags.Movement_Strafe);
	Out.InputState.bWantsToAim = HasMatchingGameplayTag(AZTags.Ability_State_Aiming);

	// Recompute now that RotationMode is known — the base filled it assuming orient-to-movement, which is
	// wrong the moment we are strafing (the body faces the camera, not the stick).
	if (Out.RotationMode == EAZ_RotationMode::OrientToMovement && !Out.InputAcceleration.IsNearlyZero())
	{
		Out.OrientationIntent = Out.InputAcceleration.Rotation();
	}
	else
	{
		Out.OrientationIntent = FRotator(0.f, Out.AimingRotation.Yaw, 0.f);
	}

	// TIP lock last, because it overrides the intent the branches above just derived: with input zeroed
	// InputAcceleration is zero, so the else-branch would publish the CAMERA yaw — and the whole point
	// of the lock is that the body is turning to the LATCHED STICK direction, camera untouched.
	Out.bTurnInPlaceActive = bTipLockActive;
	Out.TurnInPlaceTargetYaw = TipTargetYaw;
	if (bTipLockActive)
	{
		Out.OrientationIntent = FRotator(0.f, TipTargetYaw, 0.f);
	}
}

void AAZ_CmcHeroCharacter::ApplyMovementFeelParams(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const float Speed2D = Move->Velocity.Size2D();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero();

	// Released-stick braking follows GAIT, because the stop CLIPS disagree by gait and they are what the
	// stop has to match. Foot slide on a stop is exactly clipStopTime - (Speed / braking), so a single
	// shared value cannot serve three gaits: measured 2026-08-23 at a shared 500, walk still slid 0.59s
	// while sprint OVERSHOT by 0.22s. Each per-gait default is that gait's speed / the clip's own ~0.95s
	// stop time. (The old "released brakes 4x harder than held" framing is gone: at 2000 the capsule
	// stopped in 0.19s from a run against a clip depicting 0.95s, which is what the 0.76s slide WAS.)
	// Keyed off ACTUAL SPEED, not CurrentGait. Gait comes from gameplay tags
	// (ResolveGaitAndStanceFromTags), so releasing the sprint input drops the tag to Walk on that frame
	// while the body is still travelling at sprint speed — measured 2026-08-23: spd=558 with
	// braking=190 (the walk value), a 2.9s stop. Rotation rate legitimately follows INTENT, which is why
	// bGaitScaledRotationRate keys off the gait; braking must follow MOMENTUM, which only speed knows.
	float NoInputBraking = BrakingDecelNoInput;

	// ===================== CURVE-DRIVEN STOP: the capsule tracks the CLIP =====================
	// Highest precedence, because it is the only option here that needs no tuning at all. The stop clips
	// carry MoveData_Speed, authored from their own root motion at 30Hz, so the animation already knows
	// the deceleration profile it depicts. Brake exactly hard enough to reach that speed THIS frame and
	// the capsule tracks the authored curve — zero slide by construction at any release speed, and
	// stopping distance becomes a property of the content instead of a number fitted to it.
	//
	// Every constant below this line (StopTimeSeconds, the three-band table) exists only to APPROXIMATE
	// this curve with a scalar. They stay as fallbacks for clips that carry no curve — the crouch stop's
	// 9 cm/s peak, and anything not yet authored.
	bool bCurveDriven = false;
	if (bStopCurveBraking && bStopActive && bStopIsAnimated && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		if (const UAZ_CmcAnimInstance* CmcAnim =
			GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
		{
			const float ClipSpeed = CmcAnim->GetStopClipDepictedSpeed();

			// HANDOVER GATE — decided once, on the first frame a stop clip is available, and then held.
			// Re-testing per frame would let a stop that failed the gate flicker into curve driving later
			// (the clip advances, the body slows, and eventually they agree by coincidence), which
			// reintroduces the step the gate exists to prevent.
			if (!bStopCurveEngaged && !bStopCurveRejected && CmcAnim->IsStopClipSelected())
			{
				const float HandoverStep = FMath::Abs(ClipSpeed - Speed2D);
				const float ClipTime = CmcAnim->GetStopClipSampleTime();
				const bool bAgrees = ClipSpeed > KINDA_SMALL_NUMBER
					&& HandoverStep <= StopCurveMaxHandoverStep
					&& ClipTime <= StopCurveMaxHandoverClipTime;

				if (!bAgrees)
				{
					bStopCurveRejected = true;
					UE_LOG(LogTemp, Display,
						TEXT("[CmcStop] curve REJECTED: body=%.0f clip=%.0f step=%.0f clipTime=%.2fs ")
						TEXT("-> falling back to v0/T (clip is describing a different stop)"),
						Speed2D, ClipSpeed, HandoverStep, ClipTime);
				}
			}

			// ONCE ENGAGED, STAY ENGAGED FOR THE REST OF THE STOP — including the plant, where the curve
			// legitimately reads zero.
			//
			// The first cut bailed out on a non-positive reading and handed back to the latched contract.
			// But StopBrakingDecel is EntrySpeed/StopTimeSeconds, sized for the WHOLE stop, so it is far
			// too soft for a leftover 100 cm/s at the end: measured 2026-08-23, entry=234 handed back at
			// braking=251 and took 1.18s to travel 138cm against a 109cm plan. Everything after the clip
			// plants is pure slide — the feet are down and the body is still moving.
			//
			// A zero reading can only mean "no curve on this clip" BEFORE engagement; once we have
			// engaged we know the clip carries one, so zero can only mean the plant. Targeting zero
			// through the same convergence then kills the residual in StopCurveConvergenceTime.
			const bool bCurveUsable = ClipSpeed > KINDA_SMALL_NUMBER || bStopCurveEngaged;
			if (!bStopCurveRejected && CmcAnim->IsStopClipSelected() && bCurveUsable)
			{
				// CONVERGE onto the curve over a window, do not snap onto it in one frame. Dividing by
				// DeltaSeconds closes the whole gap in a single tick, which turns every legitimate
				// foot-phase offset into a visible velocity step. Over StopCurveConvergenceTime the same
				// gap reads as weight instead. Floored at DeltaSeconds so the correction can never be
				// asked to act faster than one frame.
				//
				// Negative when the clip depicts MORE speed than the body has: coast, never accelerate.
				// A stop that pushed the character forward would be worse than any slide.
				const float ConvergeOver = FMath::Max(StopCurveConvergenceTime, DeltaSeconds);
				const float Needed = (Speed2D - ClipSpeed) / ConvergeOver;
				NoInputBraking = FMath::Clamp(Needed, 0.f, StopCurveMaxBraking);
				bCurveDriven = true;

				// Report the moment it takes over, once per stop. Without this we cannot tell a working
				// curve path from one that never engaged — exactly how the sub-floor gate went unnoticed
				// for a whole session. The step is the velocity discontinuity at handover: if MM entered
				// the clip at a frame whose speed is far from ours, this is where it shows.
				if (!bStopCurveEngaged)
				{
					bStopCurveEngaged = true;
					UE_LOG(LogTemp, Display,
						TEXT("[CmcStop] curve ENGAGED: body=%.0f clip=%.0f step=%+.0f cm/s braking=%.0f  clipTime=%.2fs"),
						Speed2D, ClipSpeed, ClipSpeed - Speed2D, NoInputBraking,
						CmcAnim->GetStopClipSampleTime());
				}
			}
		}
	}

	if (!bStopActive)
	{
		bStopCurveEngaged = false;
		bStopCurveRejected = false;
	}

	// RM stop owns the deceleration outright — the clip's root motion IS the stop. Two controllers
	// solving the same deceleration fight, and the montage wins on velocity anyway, so the contract's
	// braking would only corrupt the prediction the trajectory is built from.
	if (bRootMotionStopActive)
	{
		NoInputBraking = 0.f;
	}
	else if (!bCurveDriven && bStopTimeBraking && bStopActive)
	{
		// THE STOP CONTRACT WINS. UpdateSelectionGait solved braking = EntrySpeed / StopTimeSeconds once,
		// at the stop edge, so every stop takes the same ~0.93s the content depicts regardless of release
		// speed. The three-band table below is exact at exactly three speeds (165/375/585) and degrades to
		// the band edges — releasing one cm/s under the run threshold latched the WALK value and took
		// 1.42s over 190cm against a clip depicting 0.92s / 68cm.
		NoInputBraking = StopBrakingDecel;
	}
	else if (bGaitScaledBraking)
	{
		// SelectionGait, not Speed2D directly: it is latched at the instant the stop began, so the value
		// stays CONSTANT for the whole stop. Recomputing per-frame from the current speed made the
		// deceleration decay as the character slowed (615 -> 375 -> 190 in one stop from sprint = 493cm
		// over 2.15s against a 167cm / 0.95s clip). The stop clips decelerate linearly; so must we.
		switch (SelectionGait)
		{
		case EAZ_Gait::Sprint: NoInputBraking = SprintBrakingDecel; break;
		case EAZ_Gait::Run:    NoInputBraking = RunBrakingDecel;    break;
		default:               NoInputBraking = WalkBrakingDecel;   break;
		}
	}
	Move->BrakingDecelerationWalking = bHasInput ? BrakingDecelWithInput : NoInputBraking;

	// Acceleration and friction both fall off with speed, so the last stretch to top speed is gradual.
	const float AccelAlpha = FMath::Clamp(
		(Speed2D - AccelTaperSpeedMin) / FMath::Max(1.f, AccelTaperSpeedMax - AccelTaperSpeedMin), 0.f, 1.f);
	Move->MaxAcceleration = FMath::Lerp(MaxAccelerationBase, MaxAccelerationAtTopSpeed, AccelAlpha);

	const float FrictionAlpha = FMath::Clamp(Speed2D / FMath::Max(1.f, FrictionTaperSpeedMax), 0.f, 1.f);
	Move->GroundFriction = FMath::Lerp(GroundFrictionMax, GroundFrictionMin, FrictionAlpha);

	// Turn rate follows GAIT, because the arc loops disagree by gait and they are what a held turn has to
	// match: walk arcs carve at 180 deg/s, run arcs at 93-116. One rate cannot serve both — at the walk
	// rate a running turn outruns its arcs and the search falls through to pivots and 180-degree starts;
	// at the run rate walking crawls (a 180 takes 1.6s). Sprint has no arc content, so it gets the widest
	// carve by choice. Airborne stays finite regardless or the turn reads as a snap.
	float GroundedYawRate = GroundedRotationRateYaw;
	if (bGaitScaledRotationRate)
	{
		switch (CurrentGait)
		{
		case EAZ_Gait::Run:    GroundedYawRate = RunRotationRateYaw;    break;
		case EAZ_Gait::Sprint: GroundedYawRate = SprintRotationRateYaw; break;
		default:               break;   // Walk keeps GroundedRotationRateYaw
		}
	}
	const float NewYawRate = Move->IsFalling() ? FallingRotationRateYaw : GroundedYawRate;

	// Logged on CHANGE only, and deliberately with no static or member to hold the previous value: we
	// write RotationRate every frame, so comparing against what is already on the component IS the
	// change test, and it works per-instance. This exists because a CDO edit made while PIE is running
	// never reaches the spawned instance, which reads as "I changed the value and nothing happened" —
	// three separate times on 2026-08-24. Now the applied number is in the log and there is no doubt.
	if (!FMath::IsNearlyEqual(Move->RotationRate.Yaw, NewYawRate))
	{
		UE_LOG(LogTemp, Display, TEXT("[CmcRot] RotationRate.Yaw %.0f -> %.0f | gaitScaled=%d gait=%d falling=%d"),
			Move->RotationRate.Yaw, NewYawRate, bGaitScaledRotationRate ? 1 : 0,
			static_cast<int32>(CurrentGait), Move->IsFalling() ? 1 : 0);
	}

	Move->RotationRate = FRotator(0.f, NewYawRate, 0.f);

	// [CmcTurn] GROUND TRUTH. Every turn number so far has come from a simulation of CalcVelocity, and
	// on 2026-08-24 that simulation predicted a 90-degree turn bottoming at 281 cm/s while the live log
	// showed selections at 24-34. One of the two is wrong and only the game can say which. Logs the
	// APPLIED values (after the writes above), gated so it only speaks during an actual turn.
	{
		const FVector V = Move->Velocity;
		const FVector A = Move->GetCurrentAcceleration();
		const float   Sp = V.Size2D();
		if (Sp > 40.f && !A.IsNearlyZero())
		{
			const float Ang = FMath::Abs(FRotator::NormalizeAxis(
				static_cast<float>(A.Rotation().Yaw - V.Rotation().Yaw)));
			if (Ang > 25.f)
			{
				UE_LOG(LogTemp, Display,
					TEXT("[CmcTurn] ang=%5.1f spd=%6.1f fric=%4.1f accel=%5.0f maxspd=%5.0f capsuleYaw=%7.1f"),
					Ang, Sp, Move->GroundFriction, Move->MaxAcceleration, Move->GetMaxSpeed(),
					GetActorRotation().Yaw);
			}
		}
	}
}

void AAZ_CmcHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	// BP-defaults sanity checks (v2 lesson): a silent skip reads as "input doesn't work" with no error.
	ensureMsgf(DefaultMappingContext,
		TEXT("%s: DefaultMappingContext is null. Set it in the BP child defaults — no IAs will trigger."),
		*GetName());
	ensureMsgf(MoveInputAction,
		TEXT("%s: MoveInputAction is null. Set it in the BP child defaults — movement won't work."),
		*GetName());
	ensureMsgf(LookInputAction,
		TEXT("%s: LookInputAction is null. Set it in the BP child defaults — look won't work."),
		*GetName());

	if (MoveInputAction)
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_CmcHeroCharacter::OnMoveTriggered);
	}
	if (LookInputAction)
	{
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_CmcHeroCharacter::OnLookTriggered);
	}
	// Jump stays GAS-routed (InputConfig maps the Jump IA to Input.Action.Jump; UAZ_GA_PawnJump calls
	// IAZ_JumpRequester::SetJumpPressed, which the base forwards to native Jump()). Not bound here.
}

void AAZ_CmcHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Self-contained IMC push: the pawn owns its input surface and doesn't depend on the PC knowing this
	// pawn generation. Remove-then-add keeps the call idempotent across restarts.
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AAZ_CmcHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void AAZ_CmcHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

UAbilitySystemComponent* AAZ_CmcHeroCharacter::GetAbilitySystemComponent() const
{
	const AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void AAZ_CmcHeroCharacter::InitAbilitySystem()
{
	// Mirrors the v2 grant site verbatim — the pattern is proven and doctrine-compliant (grant the
	// RESOLVED class, patch ITS CDO, idempotent by spec lookup).
	AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	if (!PS)
	{
		return;   // clients retry from OnRep_PlayerState when it replicates in
	}

	UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(PS, this);

	if (HasAuthority())
	{
		ASC->GrantAbilitiesWithInputTag(StartupAbilities);

		UClass* GrabbedClass = *GrabbedAbilityClass ? *GrabbedAbilityClass : UAZ_GA_PlayerGrabbed::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(GrabbedClass))
		{
			UAZ_GA_PlayerGrabbed::ConfigureCDO(GrabbedClass);
			FGameplayAbilitySpec GrabbedSpec(GrabbedClass, 1, INDEX_NONE, this);
			GrabbedSpec.GetDynamicSpecSourceTags().AddTag(FAZ_GameplayTags::Get().Input_Action_Interact);
			ASC->GiveAbility(GrabbedSpec);
			UE_LOG(LogTemp, Display, TEXT("[CmcHero] %s granted to hero ASC"), *GrabbedClass->GetName());
		}

		UClass* HitReactClass = *HitReactAbilityClass ? *HitReactAbilityClass : UAZ_GA_HitReact::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(HitReactClass))
		{
			UAZ_GA_HitReact::ConfigureOnCDO(HitReactClass);
			ASC->GiveAbility(FGameplayAbilitySpec(HitReactClass, 1, INDEX_NONE, this));
			UE_LOG(LogTemp, Display, TEXT("[CmcHero] %s granted to hero ASC"), *HitReactClass->GetName());
		}
	}
}


namespace AZ::InputRamp
{
	// INPUT RAMP ("the corrector", user design 2026-08-24 night).
	//
	// Scale the movement input down on a fresh press or a hard direction change, then ease back to 1
	// over RampSeconds. The character no longer snaps to full speed the instant intent changes; it
	// builds, which is what a body does and what the start/turn clips depict.
	//
	// WHY INPUT AND NOT MaxWalkSpeed: AddMovementInput's magnitude becomes CMC's AnalogInputModifier,
	// and the PoseSearch predictor reads exactly that — MaxSpeed * AnalogInputModifier, floored by
	// GetMinAnalogSpeed() (PoseSearchTrajectoryLibrary.cpp:73). So the predicted trajectory ramps with
	// the body for free and clip selection stays honest. A MaxWalkSpeed write has no such guarantee.
	//
	// THE FLOOR MATTERS: MinAnalogWalkSpeed is 150 against a 172.6 walk, so at scale 0.45 the walk
	// target would clamp 78 -> 150 and the ramp would be worth 13% instead of 55%. The floor is scaled
	// by the same factor here, in the same place, so the ramp means what it says at every gait.
	//
	// File-scope statics, not members: Live Coding can patch function bodies but not add UPROPERTYs,
	// and this has to be testable without closing the editor. Single locally-controlled hero, so the
	// sharing is safe. TODO promote to EditDefaultsOnly members at the next editor-closed build.
	// OFF 2026-08-24 night. It made the artifact WORSE and the reason is instructive: cutting input cuts
	// ACCELERATION, and acceleration is what redirects velocity (Velocity += Acceleration*dt). The capsule
	// still rotated at the full 165 deg/s toward the (unchanged) input DIRECTION, so the body turned on
	// time while the velocity took longer to come round than before — "slide, then turn". Reducing input
	// widens the capsule-vs-velocity gap; it cannot close it.
	// Tunables moved to EditDefaultsOnly UPROPERTYs on the character (AZ|Movement|InputRamp) so they
	// can be dragged live instead of round-tripping through a code patch. Only RUNTIME state stays
	// here, and it stays file-scope for the reason documented above (Live Coding cannot add members).

	static double RampStartTime   = -1.0;
	static double LastInputTime   = -1.0;
	static FVector LastInputDir   = FVector::ZeroVector;
	static float  LastLoggedScale = 1.f;
}

void AAZ_CmcHeroCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	// IA_Move axis convention (same asset as v2): X = Right/Left, Y = Forward/Back. Camera-relative.
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller ? Controller->GetControlRotation().Yaw : 0.f, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Raw-input facts for the TIP lock. Kept regardless of the lock state: release detection is
	// "this timestamp went stale" (there is no Completed binding to hook), and the re-latch test needs
	// the live direction while input is suppressed.
	LastMoveInputDir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();
	LastMoveInputTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : -1.f;

	// Lock held: translation is the thing being suppressed, so the input stops HERE. Everything else
	// about the frame (camera, abilities, the contract) proceeds normally.
	if (bTipLockActive)
	{
		return;
	}

	// Latch test — idle, on the ground, orienting to movement, input pointing far off the facing.
	// Strafe/aiming are excluded because there the body is camera-owned and sidesteps are legitimate;
	// that path keeps GASP's aim-TIP behaviour, which needs no lock (no input to suppress).
	if (bTurnInPlaceLock && !LastMoveInputDir.IsNearlyZero())
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const bool bEligible =
			Move && Move->IsMovingOnGround()
			&& Move->Velocity.Size2D() < TurnInPlaceEnterMaxSpeed
			&& !HasMatchingGameplayTag(AZTags.Ability_State_Aiming)
			&& !HasMatchingGameplayTag(AZTags.Movement_Strafe);

		const float InputYaw = static_cast<float>(LastMoveInputDir.Rotation().Yaw);
		const float EnterDelta = FMath::Abs(FRotator::NormalizeAxis(
			InputYaw - static_cast<float>(GetActorRotation().Yaw)));

		if (bEligible && EnterDelta >= TurnInPlaceEnterAngle)
		{
			// Debounce: the frames spent counting also swallow the input, so a latch never begins with
			// a two-frame twitch of translation. A tap that never reaches the count costs an invisible
			// ~35 ms hole; a small-angle press never enters this branch at all.
			if (++TipEnterCandidateFrames >= TurnInPlaceMinHoldFrames)
			{
				bTipLockActive = true;
				TipTargetYaw = InputYaw;
				TipLockElapsed = 0.f;
				TipEnterCandidateFrames = 0;
				UE_LOG(LogTemp, Display,
					TEXT("[CmcTip] LATCH target=%.0f delta=%.0f spd=%.0f"),
					TipTargetYaw, EnterDelta, Move->Velocity.Size2D());
			}
			return;
		}
		TipEnterCandidateFrames = 0;
	}

	float RampScale = 1.f;
	if (bInputRampEnabled && GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const FVector Dir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();

		// Two things restart the ramp, and both are INTENT changes rather than body facts: a fresh
		// press after letting go, and a hard change of direction while held. Deliberately NOT keyed on
		// speed — a speed test would re-trigger every frame while the character is still slow, and the
		// ramp would never advance past its own start.
		const bool bFreshPress = (AZ::InputRamp::LastInputTime < 0.0)
			|| ((Now - AZ::InputRamp::LastInputTime) > InputRampReleaseSeconds);
		const bool bHardTurn = !AZ::InputRamp::LastInputDir.IsNearlyZero() && !Dir.IsNearlyZero()
			&& FMath::Abs(FRotator::NormalizeAxis(static_cast<float>(
				Dir.Rotation().Yaw - AZ::InputRamp::LastInputDir.Rotation().Yaw))) > InputRampRetriggerAngle;

		if (bFreshPress || bHardTurn || AZ::InputRamp::RampStartTime < 0.0)
		{
			AZ::InputRamp::RampStartTime = Now;
		}

		const float Elapsed = static_cast<float>(Now - AZ::InputRamp::RampStartTime);
		const float Alpha = FMath::Clamp(Elapsed / FMath::Max(InputRampSeconds, 0.01f), 0.f, 1.f);
		RampScale = FMath::Lerp(InputRampStartScale, 1.f, Alpha);

		AZ::InputRamp::LastInputTime = Now;
		AZ::InputRamp::LastInputDir = Dir;

		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MinAnalogWalkSpeed = InputRampBaseMinAnalog * RampScale;
		}

		if (FMath::Abs(RampScale - AZ::InputRamp::LastLoggedScale) > 0.15f)
		{
			AZ::InputRamp::LastLoggedScale = RampScale;
			UE_LOG(LogTemp, Display, TEXT("[CmcRamp] scale=%.2f t=%.2f fresh=%d hardTurn=%d spd=%.0f"),
				RampScale, Elapsed, bFreshPress ? 1 : 0, bHardTurn ? 1 : 0,
				GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : -1.f);
		}
	}

	// RM START. Attempted BEFORE the input is applied: if a start launches, its root motion owns the
	// capsule for the clip's duration and adding input on top would fight it (CMC ignores input for
	// translation while HasAnimRootMotion(), but the acceleration would still steer orient-to-movement
	// and desync the facing the clip is authoring). Returns false in every case it does not apply —
	// already moving, montage playing, cooldown, no content for this bucket — and normal input flows.
	const FVector WorldInputDir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();
	if (TryPlayRootMotionStart(WorldInputDir))
	{
		return;
	}

	AddMovementInput(Forward, Axis.Y * RampScale);
	AddMovementInput(Right, Axis.X * RampScale);
}

bool AAZ_CmcHeroCharacter::TryPlayRootMotionStart(const FVector& WorldInputDir)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bRootMotionStarts || !Move || !MeshComp || WorldInputDir.IsNearlyZero())
	{
		return false;
	}

	// From (near) rest, on the ground, and not already inside a start. Above the speed threshold this is
	// a pivot or a gait change — different events, different content, not this code's business.
	if (!Move->IsMovingOnGround() || Move->Velocity.Size2D() > RootMotionStartMaxSpeed)
	{
		return false;
	}

	UAnimInstance* Anim = MeshComp->GetAnimInstance();
	if (!Anim)
	{
		return false;
	}

	// Already playing one: do not restart. Montage RM owns the capsule while it runs, and relaunching
	// mid-motion would teleport the character back to the clip's frame 0 root.
	if (Anim->IsAnyMontagePlaying())
	{
		return false;
	}

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (LastRootMotionStartTime >= 0.f && (Now - LastRootMotionStartTime) < RootMotionStartCooldown)
	{
		return false;
	}

	// Signed yaw from where the body faces to where the stick points, bucketed with the SAME helper the
	// anim layer uses — one owner for the thresholds, so movement and animation can never disagree
	// about which start this is.
	const float SignedAngle = static_cast<float>(FRotator::NormalizeAxis(
		WorldInputDir.Rotation().Yaw - GetActorRotation().Yaw));
	const EAZ_StartDirection Direction = UAZ_LocomotionStateMachine::BucketStartDirection(SignedAngle);
	const EAZ_Gait Gait = GetCurrentGait();

	// Chooser first when assigned; the hardcoded table is the fallback. Same clip either way, so the
	// migration is reversible and the playback below never changes.
	ChooserDirection = Direction;
	UAnimSequence* Clip = EvaluateLocomotionChooser(RootMotionStartChooser);
	if (!Clip)
	{
		const FAZ_RootMotionStartClip* Row = RootMotionStartClips.FindByPredicate(
			[Gait, Direction](const FAZ_RootMotionStartClip& Entry)
			{
				return Entry.Gait == Gait && Entry.Direction == Direction && Entry.Clip != nullptr;
			});
		if (Row) { Clip = Row->Clip; }
	}

	if (!Clip)
	{
		// No content for this combination is a legitimate, common case (sprint has no start set at all).
		// Fall through silently to the MM path rather than forcing a wrong clip.
		return false;
	}

	UAnimMontage* Played = Anim->PlaySlotAnimationAsDynamicMontage(
		Clip, RootMotionStartSlot,
		RootMotionStartBlendIn, RootMotionStartBlendOut,
		/*InPlayRate*/ 1.f, /*LoopCount*/ 1);

	if (!Played)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CmcRmStart] FAILED to play %s on slot %s"),
			*GetNameSafe(Clip), *RootMotionStartSlot.ToString());
		return false;
	}

	LastRootMotionStartTime = Now;
	AZ::RmMontage::bActive = true;
	AZ::RmMontage::LaunchTime = Now;
	AZ::RmMontage::bIsStop = false;
	AZ::RmMontage::LaunchDir = WorldInputDir;

	const UEnum* DirEnum = StaticEnum<EAZ_StartDirection>();
	UE_LOG(LogTemp, Display,
		TEXT("[CmcRmStart] %s | angle=%+.0f bucket=%s gait=%d spd=%.0f dur=%.2fs"),
		*GetNameSafe(Clip), SignedAngle,
		DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(Direction)) : TEXT("?"),
		static_cast<int32>(Gait), Move->Velocity.Size2D(), Clip->GetPlayLength());

	return true;
}

UAnimSequence* AAZ_CmcHeroCharacter::EvaluateLocomotionChooser(UChooserTable* Table)
{
	if (!Table)
	{
		return nullptr;
	}

	// Publish the axes the chooser's columns bind to. Written immediately before evaluation so a column
	// can never read a stale frame's value — the whole point of keeping them here rather than letting
	// each column reach into live state on its own.
	ChooserGait = GetCurrentGait();
	ChooserStance = bIsCrouched ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	ChooserSpeed = GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : 0.f;

	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
	ChooserRotationMode = HasMatchingGameplayTag(AZTags.Ability_State_Aiming) ? EAZ_RotationMode::Aiming
		: (HasMatchingGameplayTag(AZTags.Movement_Strafe) ? EAZ_RotationMode::Strafe
		                                                  : EAZ_RotationMode::OrientToMovement);

	if (const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		bChooserLeftFootDown = CmcAnim->bLeftFootDown;
	}

	// `this` is the context object: chooser property columns resolve against the character's reflected
	// properties by name (the Chooser* fields above).
	UObject* Result = UChooserFunctionLibrary::EvaluateChooser(this, Table, UAnimSequence::StaticClass());
	return Cast<UAnimSequence>(Result);
}

bool AAZ_CmcHeroCharacter::TryPlayRootMotionStop()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bRootMotionStops || !Move || !MeshComp)
	{
		return false;
	}

	UAnimInstance* Anim = MeshComp->GetAnimInstance();
	if (!Anim || Anim->IsAnyMontagePlaying() || !Move->IsMovingOnGround())
	{
		return false;
	}

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (LastRootMotionStopTime >= 0.f && (Now - LastRootMotionStopTime) < RootMotionStartCooldown)
	{
		return false;
	}

	const float EntrySpeed = Move->Velocity.Size2D();

	// Read the foot from the ANIM INSTANCE, not from bChooserLeftFootDown. That staging field is only
	// written inside EvaluateLocomotionChooser, which returns early when no chooser is assigned - so
	// with the array path in use it kept its default forever and every stop logged leftFoot=1 (measured
	// 2026-08-25: 15 of 15). The chooser path sets it too, so both routes now agree.
	bool bLeftDown = true;
	if (const UAZ_CmcAnimInstance* FootAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		bLeftDown = FootAnim->bLeftFootDown;
	}
	bChooserLeftFootDown = bLeftDown;

	UAnimSequence* Clip = nullptr;

	// Chooser first when one is assigned; the array is the fallback and the two never both run.
	ChooserDirection = EAZ_StartDirection::Fwd;   // a stop has no turn bucket; column can ignore it
	if (UAnimSequence* Chosen = EvaluateLocomotionChooser(RootMotionStopChooser))
	{
		Clip = Chosen;
	}
	else
	{
		const EAZ_Gait Gait = GetCurrentGait();
		const FAZ_RootMotionStopClip* Row = RootMotionStopClips.FindByPredicate(
			[Gait, bLeftDown, EntrySpeed](const FAZ_RootMotionStopClip& E)
			{
				return E.Clip && E.Gait == Gait && E.bLeftFootDown == bLeftDown
					&& EntrySpeed >= E.MinEntrySpeed && EntrySpeed <= E.MaxEntrySpeed;
			});
		if (Row) { Clip = Row->Clip; }
	}

	if (!Clip)
	{
		// Out of band, or no content: the curve-driven stop contract owns this stop, unchanged.
		return false;
	}

	UAnimMontage* Played = Anim->PlaySlotAnimationAsDynamicMontage(
		Clip, RootMotionStartSlot, RootMotionStartBlendIn, RootMotionStartBlendOut, 1.f, 1);
	if (!Played)
	{
		return false;
	}

	LastRootMotionStopTime = Now;
	bRootMotionStopActive = true;
	AZ::RmMontage::bActive = true;
	AZ::RmMontage::LaunchTime = Now;
	AZ::RmMontage::bIsStop = true;
	AZ::RmMontage::LaunchDir = FVector::ZeroVector;

	UE_LOG(LogTemp, Display,
		TEXT("[CmcRmStop] %s | entry=%.0f leftFoot=%d gait=%d dur=%.2fs"),
		*GetNameSafe(Clip), EntrySpeed, bLeftDown ? 1 : 0,
		static_cast<int32>(GetCurrentGait()), Clip->GetPlayLength());
	return true;
}

void AAZ_CmcHeroCharacter::UpdateTurnInPlaceLock(float DeltaSeconds)
{
	if (!bTipLockActive)
	{
		return;
	}
	TipLockElapsed += DeltaSeconds;

	// Input released mid-lock: back to plain idle, nothing to align to. Detected by staleness because
	// only Triggered is bound; two missed input ticks is unambiguous.
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (Now - LastMoveInputTime > 0.1f)
	{
		ReleaseTurnInPlaceLock(TEXT("input released"), /*bSnapCapsule*/ false);
		return;
	}

	// Direction changed hard mid-lock: it is a NEW turn — re-latch and restart the clock. Below the
	// threshold it is stick noise and the original target stands.
	const float LiveYaw = static_cast<float>(LastMoveInputDir.Rotation().Yaw);
	if (FMath::Abs(FRotator::NormalizeAxis(LiveYaw - TipTargetYaw)) > TurnInPlaceRetargetAngle)
	{
		UE_LOG(LogTemp, Display, TEXT("[CmcTip] RELATCH %.0f -> %.0f (t=%.2f)"),
			TipTargetYaw, LiveYaw, TipLockElapsed);
		TipTargetYaw = LiveYaw;
		TipLockElapsed = 0.f;
	}

	const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
	if (!CmcAnim)
	{
		ReleaseTurnInPlaceLock(TEXT("no anim instance"), false);
		return;
	}

	// Remaining rotation is measured against the MESH ROOT — the capsule deliberately does not rotate
	// during the lock, so actor yaw would read "not started" forever.
	const float RemainingYaw = FMath::Abs(FRotator::NormalizeAxis(TipTargetYaw - CmcAnim->GetTipRootYaw()));
	const bool bTipSelected = CmcAnim->IsTurnInPlaceClipSelected();

	if (RemainingYaw <= TurnInPlaceExitAngle)
	{
		ReleaseTurnInPlaceLock(TEXT("root arrived"), true);
	}
	else if (bTipSelected && CmcAnim->GetTurnInPlaceClipFraction() >= 0.7f)
	{
		// The clip is nearly spent but the root has not arrived — authored rotation ran out (a 90 clip
		// serving a 170 request). Snap anyway: the offset absorbs it and the follow-up start covers it.
		ReleaseTurnInPlaceLock(TEXT("clip done"), true);
	}
	else if (!bTipSelected && TipLockElapsed > 0.35f)
	{
		// Selection never happened — the search kept Idle despite the bent trajectory. Do not hold a
		// lock nothing is animating; release WITHOUT the snap (nothing rotated).
		ReleaseTurnInPlaceLock(TEXT("no selection"), false);
	}
	else if (TipLockElapsed >= TurnInPlaceTimeout)
	{
		ReleaseTurnInPlaceLock(TEXT("WATCHDOG"), true);
	}
}

void AAZ_CmcHeroCharacter::ReleaseTurnInPlaceLock(const TCHAR* Reason, bool bSnapCapsule)
{
	const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;

	if (bSnapCapsule && bTurnInPlaceSnapCapsuleOnRelease && CmcAnim)
	{
		// Align the capsule to where the mesh already visibly faces. OffsetRootBone recomputes its
		// offset to ~0 (Accumulate mode keeps the root world-stable through capsule rotation — the
		// same mechanism that lets GASP rotate its capsule instantly), so nothing on screen moves; and
		// the resumed input now reads as a small-angle start instead of a second 180.
		SetActorRotation(FRotator(0.f, CmcAnim->GetTipRootYaw(), 0.f));
	}

	UE_LOG(LogTemp, Display,
		TEXT("[CmcTip] RELEASE (%s) t=%.2f remaining=%.0f clipFrac=%.2f snap=%d"),
		Reason, TipLockElapsed,
		CmcAnim ? FMath::Abs(FRotator::NormalizeAxis(TipTargetYaw - CmcAnim->GetTipRootYaw())) : -1.f,
		CmcAnim ? CmcAnim->GetTurnInPlaceClipFraction() : -1.f,
		bSnapCapsule ? 1 : 0);

	bTipLockActive = false;
	TipLockElapsed = 0.f;
	TipEnterCandidateFrames = 0;
}

void AAZ_CmcHeroCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	// Grabbed = camera locked (TLOU-style hold; the grab camera owns the shot). The IMC stays pushed —
	// removing it would also kill the E-mash IA.
	if (HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookRateYaw);
	AddControllerPitchInput(-Axis.Y * LookRatePitch);
}
