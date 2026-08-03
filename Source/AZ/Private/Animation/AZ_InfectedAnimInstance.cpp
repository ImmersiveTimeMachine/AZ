// Copyright Artur. AZ project.

#include "Animation/AZ_InfectedAnimInstance.h"

#include "AZ_GameplayTags.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Character/AZ_PawnMovementMode_Walking.h"
#include "Components/SkeletalMeshComponent.h"   // grab hand-IK reads the prey's grip sockets
#include "MoverDataModelTypes.h"

void UAZ_InfectedAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

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

	Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
	if (Cached_Pawn)
	{
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}
}

void UAZ_InfectedAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Re-resolve lazily: the pawn / Mover may not exist on the first init (editor preview) or right after spawn.
	if (!Cached_Pawn)
	{
		Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
	}
	if (Cached_Pawn && !Cached_MoverComponent)
	{
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}

	// ============================== GRAB HAND-IK GATHER ==============================
	// Mirror of the hero's block in UAZ_MoverAnimInstance. Runs BEFORE the Mover early-out below: a grab
	// pins the body anyway, and the hands must keep their grip even if the movement component is missing.
	// Cross-actor socket reads are game-thread only, which is why this is here and not in a thread-safe update.
	{
		float TargetAlpha = 0.f;
		if (const AActor* Prey = Cached_Pawn ? Cached_Pawn->GetGrabTarget() : nullptr)
		{
			if (const USkeletalMeshComponent* PreyMesh = Prey->FindComponentByClass<USkeletalMeshComponent>())
			{
				TargetAlpha = 1.f;
				GrabIKTarget_HandL = PreyMesh->GetSocketLocation(GrabIKPreySocketForHandL);
				GrabIKTarget_HandR = PreyMesh->GetSocketLocation(GrabIKPreySocketForHandR);
			}
		}
		// Ramp rather than snap: a hard 0->1 on the catch frame reads as the hands teleporting onto the prey.
		GrabIKAlpha = FMath::FInterpTo(GrabIKAlpha, TargetAlpha, DeltaSeconds, GrabIKBlendSpeed);
	}

	if (!Cached_MoverComponent)
	{
		GroundSpeed = 0.f;
		bIsMoving   = false;
		return;
	}

	// Physics -> anim: the Mover owns translation. GroundSpeed stays MEASURED (turn detection + IsMoving
	// consume it); the BS axis gets the COMMANDED speed below.
	const FVector Velocity = Cached_MoverComponent->GetVelocity();
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
	if (const FObjectProperty* SetProperty = CastField<FObjectProperty>(
		Cached_Pawn->GetClass()->FindPropertyByName(TEXT("AnimSet"))))
	{
		if (UObject* AnimSet = SetProperty->GetObjectPropertyValue_InContainer(Cached_Pawn))
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
	// clients; the AI controller (server-only) is deliberately never consulted here.
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	bIsAlerted    = Cached_Pawn->HasMatchingGameplayTag(Tags.State_Infected_Alerted);
	bIsAggressive = Cached_Pawn->HasMatchingGameplayTag(Tags.State_Infected_Aggressive);
}
