// Copyright Artur. AZ project.

#include "Animation/AZ_InfectedAnimInstance.h"

#include "Animation/AZ_MoverAnimInstance.h"   // ResolveGrabIKTarget — one owner for the reach-clamp math
#include "AZ_GameplayTags.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Character/Cmc/AZ_CmcInfectedCharacter.h"   // [SPIKE: spike/cmc-backport] CMC (v3) Chalkie branch
#include "Components/SkeletalMeshComponent.h"   // grab hand-IK reads the prey's grip sockets
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagAssetInterface.h"
#include "MoverDataModelTypes.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"   // PSI: direct SetTrajectory push into our collector
#include "PoseSearch/PoseSearchLibrary.h"                     // PSI: FindPoseHistoryNode

// ★ IK MASTER SWITCH (user decision 2026-08-31) — mirror of the hero's in AZ_MoverAnimInstance.cpp.
// Fights move to a different technique, so the Chalkie's grab hand-IK (two TwoBoneIK nodes gated by
// GrabIKAlpha) is OFF. Holding TargetAlpha at 0 is the single owner: the alpha decays, the nodes become
// pass-throughs. The reach-clamp instrumentation ([GrabIK] frames/deficit) goes quiet with it.
static constexpr bool bGrabIKEnabled = false;

void UAZ_InfectedAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
	if (Cached_Pawn)
	{
		// RM bridge (anim side) — MUST be set here. This class derives from the ENGINE UAnimInstance, not from
		// UAZ_AnimInstance, so it never inherited that class's identical line and was sitting on the engine
		// default of RootMotionFromMontagesOnly. UAnimInstance::ShouldExtractRootMotion() is true ONLY for
		// RootMotionFromEverything / IgnoreRootMotion, so on the default the "RootMotionDelta" attribute is
		// never written at all. Everything downstream is gated on that attribute existing:
		//   - FLayeredMove_RootMotionAttribute checks bDidAttrHaveRootMotion and bails, so NO montage could
		//     ever move the Chalkie's capsule (knockback stumble, step-back, death slide).
		//   - Motion warping hooks ConvertLocalRootMotionToWorld, which is called INSIDE that same check —
		//     so every warp window on a Chalkie montage was silently inert too.
		// It hid for so long because the Rotter's old hit-react (AM_Zombie_KB_Chase_1) has 0.0cm of root
		// travel: there was nothing to see missing. Swapping to a clip with real travel exposed it.
		RootMotionMode = ERootMotionMode::RootMotionFromEverything;
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}
	else
	{
		Cached_CmcPawn = Cast<AAZ_CmcInfectedCharacter>(TryGetPawnOwner());
		// EXPLICIT RootMotionMode on the CMC branch too (doctrine rule 8 — the block above is what one
		// silent default cost us). MONTAGES-ONLY here: CMC consumes montage root motion natively, and
		// FromEverything would ALSO extract the locomotion sequence players' RM into the capsule — the
		// classic ABP's clips must stay speed-driven (MaxWalkSpeed is the one speed owner on this pawn).
		RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	}
}

void UAZ_InfectedAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Re-resolve lazily: the pawn may not exist on the first init (editor preview) or right after spawn.
	// RootMotionMode rides along on the LATE resolve as well — resolving here with init's mode still set
	// for the other backend would silently recreate the bDidAttrHaveRootMotion bug class.
	if (!Cached_Pawn && !Cached_CmcPawn)
	{
		Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
		if (Cached_Pawn)
		{
			RootMotionMode = ERootMotionMode::RootMotionFromEverything;
		}
		else if ((Cached_CmcPawn = Cast<AAZ_CmcInfectedCharacter>(TryGetPawnOwner())) != nullptr)
		{
			RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
		}
	}
	if (Cached_Pawn && !Cached_MoverComponent)
	{
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}

	APawn* AnyPawn = Cached_Pawn ? static_cast<APawn*>(Cached_Pawn.Get()) : static_cast<APawn*>(Cached_CmcPawn.Get());

	// ============================== PSI TRAJECTORY ==============================
	// Mesh-component world-space samples for the PoseHistory collector's TransformTrajectory pin. The
	// collector's own bGenerateTrajectory is ACharacter+CMC-only (inert on Mover — see the header
	// comment on Trajectory), so this member is the ONE trajectory owner for the Chalkie. History =
	// ~1s ring at ≤30Hz; future = constant-velocity extrapolation — the interaction schema and
	// MotionMatchMulti's ActorRootTransforms both read placement at t<=0, so this is sufficient.
	if (const USkeletalMeshComponent* OwnMesh = GetSkelMeshComponent(); OwnMesh && AnyPawn && GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastTrajectorySampleTime >= 0.03)
		{
			LastTrajectorySampleTime = Now;
			FTransformTrajectorySample Sample;
			Sample.Position = OwnMesh->GetComponentLocation();
			Sample.Facing = OwnMesh->GetComponentQuat();
			Sample.TimeInSeconds = static_cast<float>(Now);   // absolute until emission (rebased below)
			TrajectoryHistoryRing.Add(Sample);
			while (TrajectoryHistoryRing.Num() > 40
				|| (TrajectoryHistoryRing.Num() > 0 && Now - TrajectoryHistoryRing[0].TimeInSeconds > 1.05))
			{
				TrajectoryHistoryRing.RemoveAt(0);
			}
		}

		Trajectory.Samples.Reset(TrajectoryHistoryRing.Num() + 5);
		for (const FTransformTrajectorySample& Hist : TrajectoryHistoryRing)
		{
			FTransformTrajectorySample& Out = Trajectory.Samples.Add_GetRef(Hist);
			Out.TimeInSeconds = static_cast<float>(Hist.TimeInSeconds - Now);   // history: <= 0
		}
		// Guarantee a t=0 sample even on a frame the throttle skipped (the contract: exactly one current).
		if (Trajectory.Samples.IsEmpty() || Trajectory.Samples.Last().TimeInSeconds < 0.f)
		{
			FTransformTrajectorySample& Current = Trajectory.Samples.AddDefaulted_GetRef();
			Current.Position = OwnMesh->GetComponentLocation();
			Current.Facing = OwnMesh->GetComponentQuat();
			Current.TimeInSeconds = 0.f;
		}
		const FVector Velocity = AnyPawn->GetVelocity();
		const FVector BasePos = Trajectory.Samples.Last().Position;
		const FQuat BaseFacing = Trajectory.Samples.Last().Facing;
		for (int32 Step = 1; Step <= 4; ++Step)
		{
			const float FutureTime = 0.1f * Step;
			FTransformTrajectorySample& Future = Trajectory.Samples.AddDefaulted_GetRef();
			Future.Position = BasePos + Velocity * FutureTime;
			Future.Facing = BaseFacing;
			Future.TimeInSeconds = FutureTime;
		}

		// Push DIRECTLY into our own collector (game thread, pre-parallel-eval). The TransformTrajectory
		// pin binding is kept as documentation, but the util-made binding never reached the compiled
		// property-access path (probe 2026-08-31: hero's editor-made binding = 21 samples, ours = 0), and
		// the node's own SetTrajectory(empty pin default) is a no-op — so this push is the ONE writer.
		if (const FAnimNode_PoseSearchHistoryCollector_Base* Collector =
			UPoseSearchLibrary::FindPoseHistoryNode(FName(TEXT("PoseHistory")), this))
		{
			if (UE::PoseSearch::FPoseHistory* Hist =
				const_cast<UE::PoseSearch::FPoseHistory*>(Collector->GetPoseHistoryPtr()))
			{
				Hist->SetTrajectory(Trajectory, 1.f);
			}
		}
	}

	// ============================== GRAB HAND-IK GATHER ==============================
	// Mirror of the hero's block in UAZ_MoverAnimInstance. Runs BEFORE the movement early-out below: a grab
	// pins the body anyway, and the hands must keep their grip even if the movement component is missing.
	// Cross-actor socket reads are game-thread only, which is why this is here and not in a thread-safe update.
	// Backend-agnostic: the prey comes off whichever pawn generation owns this instance (same seam,
	// IAZ_CombatAvatar semantics), and everything below it is pure mesh-space math.
	{
		float TargetAlpha = 0.f;
		const AActor* Prey = nullptr;
		if (Cached_Pawn)
		{
			Prey = Cached_Pawn->GetGrabTarget();
		}
		else if (Cached_CmcPawn)
		{
			Prey = Cached_CmcPawn->GetGrabTarget();
		}
		const USkeletalMeshComponent* PreyMesh = Prey ? Prey->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (bGrabIKEnabled && PreyMesh)   // bGrabIKEnabled: file-scope master switch, see top of file
		{
			TargetAlpha = 1.f;
			// Reach-clamped grips (shared math, see ResolveGrabIKTarget): the authored socket when the
			// arm reaches, else the nearest reachable point ON the prey's body — this is what stops
			// the "hands sometimes don't touch the hero" air-grab. Smoothed because the clamp hands
			// off between socket and surface as the wrestle pose oscillates; snapped on frame one.
			float DeficitL = 0.f, DeficitR = 0.f;
			const FVector NewL = UAZ_MoverAnimInstance::ResolveGrabIKTarget(GetSkelMeshComponent(),
				TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
				PreyMesh, PreyMesh->GetSocketLocation(GrabIKPreySocketForHandL), GrabIKReachScale, &DeficitL);
			const FVector NewR = UAZ_MoverAnimInstance::ResolveGrabIKTarget(GetSkelMeshComponent(),
				TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
				PreyMesh, PreyMesh->GetSocketLocation(GrabIKPreySocketForHandR), GrabIKReachScale, &DeficitR);
			const bool bSnap = GrabIKAlpha < 0.05f;
			GrabIKTarget_HandL = bSnap ? NewL : FMath::VInterpTo(GrabIKTarget_HandL, NewL, DeltaSeconds, GrabIKTargetInterpSpeed);
			GrabIKTarget_HandR = bSnap ? NewR : FMath::VInterpTo(GrabIKTarget_HandR, NewR, DeltaSeconds, GrabIKTargetInterpSpeed);
			if (DeficitL > 0.f || DeficitR > 0.f)
			{
				++GrabIKClampedFrames;
				GrabIKMaxDeficit = FMath::Max(GrabIKMaxDeficit, FMath::Max(DeficitL, DeficitR));
			}
		}
		else if (GrabIKClampedFrames > 0)
		{
			// Grab just released — report the clamp's workload once, with numbers (measure rule): how
			// often a grip was out of reach and by how much. "0 frames" after a retune means the sockets
			// themselves now sit inside reach and the clamp is a no-op.
			UE_LOG(LogTemp, Display, TEXT("[GrabIK] %s: %d frames beyond reach, worst %.1fcm — clamped onto the prey's body surface"),
				*GetNameSafe(AnyPawn), GrabIKClampedFrames, GrabIKMaxDeficit);
			GrabIKClampedFrames = 0;
			GrabIKMaxDeficit = 0.f;
		}
		// Ramp rather than snap: a hard 0->1 on the catch frame reads as the hands teleporting onto the prey.
		GrabIKAlpha = FMath::FInterpTo(GrabIKAlpha, TargetAlpha, DeltaSeconds, GrabIKBlendSpeed);
	}

	// Physics -> anim: the movement backend owns translation. GroundSpeed stays MEASURED (turn detection +
	// IsMoving consume it); the BS axis gets the COMMANDED speed below.
	if (!Cached_MoverComponent && !Cached_CmcPawn)
	{
		GroundSpeed = 0.f;
		bIsMoving   = false;
		return;
	}
	const FVector Velocity = Cached_MoverComponent
		? Cached_MoverComponent->GetVelocity()
		: Cached_CmcPawn->GetVelocity();   // ACharacter::GetVelocity == CMC velocity
	GroundSpeed = Velocity.Size2D();
	bIsMoving   = GroundSpeed > MoveSpeedThreshold;

	// Loco SM driver — COMMANDED stage, not measured speed: the Locomotion SM's transition rules compare
	// the ABP int variable CommandedGait (0=Idle, 1=Walk, 2=Shamble/Run, 3=Chase/Sprint), written here by
	// reflection. Commanded means "what the sim was told" (last produced input: intent nonzero -> gait,
	// post turn-hold, post nav-consume), so the anim is the metronome playing clips at authored rate 1.0
	// while RM-lite curve-follow makes the capsule track the clip's baked "fwd vel" lurch. Measured
	// GroundSpeed above stays the input for turn detection / IsMoving only.
	// GLUE: reflection write onto a BP var — becomes a native UPROPERTY in the turn-controller-v2 batch.
	int32 CommandedStage = 0;
	if (Cached_MoverComponent)
	{
		const FMoverInputCmdContext& LastCmd = Cached_MoverComponent->GetLastInputCmd();
		const FCharacterDefaultInputs* DefaultInputs = LastCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
		if (DefaultInputs && !DefaultInputs->GetMoveInput_WorldSpace().IsNearlyZero())
		{
			EAZ_Gait Gait = EAZ_Gait::Walk;
			if (const FAZ_MoverCustomInputs* CustomInputs = LastCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
			{
				Gait = CustomInputs->Gait;
			}
			switch (Gait)
			{
			case EAZ_Gait::Walk:   CommandedStage = 1; break;
			case EAZ_Gait::Run:    CommandedStage = 2; break;
			case EAZ_Gait::Sprint: CommandedStage = 3; break;
			default:               CommandedStage = 2; break;
			}
		}
	}
	else if (Cached_CmcPawn)
	{
		// CMC: "commanded" = the nav/AI acceleration intent (PathFollowing writes CMC acceleration), and
		// the gait is the pawn's SetGait state — the same one owner that wrote MaxWalkSpeed. No RM-lite
		// here: clips play at authored rate and the capsule moves at the gait speed; P1 measures the slide.
		const UCharacterMovementComponent* Move = Cached_CmcPawn->GetCharacterMovement();
		if (Move && !Move->GetCurrentAcceleration().IsNearlyZero())
		{
			switch (Cached_CmcPawn->GetCurrentGait())
			{
			case EAZ_Gait::Walk:   CommandedStage = 1; break;
			case EAZ_Gait::Run:    CommandedStage = 2; break;
			case EAZ_Gait::Sprint: CommandedStage = 3; break;
			default:               CommandedStage = 2; break;
			}
		}
	}
	if (FProperty* StageProperty = GetClass()->FindPropertyByName(TEXT("CommandedGait")))
	{
		if (const FIntProperty* AsInt = CastField<FIntProperty>(StageProperty))
		{
			AsInt->SetPropertyValue_InContainer(this, CommandedStage);
		}
	}

	// Per-variant anim set: pawn's instance-editable AnimSet DataAsset (BP_ChalkieAnimSet) carries
	// IdleClip/WalkClip/ShambleClip/ChaseClip; push them into the ABP's like-named variables, which the
	// Locomotion SM sequence players are pin-BOUND to. Speed comes free: RM-lite follows whichever clip's
	// "fwd vel" curve is playing, so a set swap changes look AND locomotion speed with zero retuning.
	// No set assigned (or a hole in the set) -> that write is skipped and the ABP class default (set A)
	// stays. Per-frame reflection pushes of identical pointers are trivially cheap for a horde-scale count.
	// GLUE: whole block goes native (UAZ_ChalkieAnimSet UCLASS + UPROPERTYs) in the session-end batch.
	// Property lookup by NAME ("AnimSet") — works on the v2 BP child's variable AND the v3 native UPROPERTY.
	if (const FObjectProperty* SetProperty = CastField<FObjectProperty>(
		AnyPawn->GetClass()->FindPropertyByName(TEXT("AnimSet"))))
	{
		if (UObject* AnimSet = SetProperty->GetObjectPropertyValue_InContainer(AnyPawn))
		{
			static const FName ClipNames[] = { TEXT("IdleClip"), TEXT("WalkClip"), TEXT("ShambleClip"), TEXT("ChaseClip") };
			for (const FName& ClipName : ClipNames)
			{
				const FObjectProperty* Source = CastField<FObjectProperty>(AnimSet->GetClass()->FindPropertyByName(ClipName));
				FObjectProperty* Target = CastField<FObjectProperty>(GetClass()->FindPropertyByName(ClipName));
				if (Source && Target)
				{
					if (UObject* Clip = Source->GetObjectPropertyValue_InContainer(AnimSet))
					{
						Target->SetObjectPropertyValue_InContainer(this, Clip);
					}
				}
			}
		}
	}

	// AI phase from the pawn's ASC (IGameplayTagAssetInterface routes through it). Replicated tags — valid on
	// clients; the AI controller (server-only) is deliberately never consulted here. Interface query, so
	// every pawn generation answers uniformly.
	if (const IGameplayTagAssetInterface* TagOwner = Cast<IGameplayTagAssetInterface>(AnyPawn))
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		bIsAlerted    = TagOwner->HasMatchingGameplayTag(Tags.State_Infected_Alerted);
		bIsAggressive = TagOwner->HasMatchingGameplayTag(Tags.State_Infected_Aggressive);
	}
}
