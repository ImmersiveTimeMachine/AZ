// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverComponent.h"

#include "Animation/AZ_LocomotionTypes.h"   // FAZ_MoverCustomInputs, EAZ_Gait
#include "Character/AZ_PawnMovementMode_RMAction.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"   // FJumpImpulseEffect

UAZ_PawnMoverComponent::UAZ_PawnMoverComponent()
{
	// Intentionally empty. RefreshSharedSettings NewObjects each settings instance,
	// and NewObject is forbidden inside a UObject ctor. We do the work in OnRegister.
}

void UAZ_PawnMoverComponent::OnRegister()
{
	// UMoverComponent::OnRegister calls FindAndRegisterDefaults, which fires every
	// mode's OnRegistered. Those handlers do
	//   FindSharedSettings<UCommonLegacyMovementSettings>()
	// and ensure-fail if the SharedSettings array is empty. PostInitProperties /
	// PostLoad / PostCDOCompiled in the base class populate SharedSettings, but a
	// BP CDO can persistently serialize an empty SharedSettings override that survives
	// those hooks. RefreshSharedSettings here guarantees the array is populated for
	// each running instance before the modes are registered.

	// Register the RM-action mode (gravity-free, no floor-snap; follows the jump clip's root-motion
	// attribute incl. the vertical Z — fixes Walking's floor-snap eating the jump lift). Added here,
	// BEFORE Super::OnRegister → FindAndRegisterDefaults, so it is registered alongside Walking/Falling.
	// Code-managed rather than in the BP CDO mode map: it is never the starting mode, only entered on
	// demand by the AnimInstance (QueueNextMode). Idempotent — FindMovementModeByName guards re-adds.
	static const FName RMActionModeName(TEXT("RMAction"));
	if (!FindMovementModeByName(RMActionModeName))
	{
		AddMovementModeFromClass(RMActionModeName, UAZ_PawnMovementMode_RMAction::StaticClass());
	}

	RefreshSharedSettings();
	Super::OnRegister();
}

bool UAZ_PawnMoverComponent::Jump()
{
	// ---- HYBRID branch: RM rise → physics fall ----
	// Enter the RMAction mode instead of applying an impulse. The AnimInstance reacts to the mode change
	// (RMAction maps to InAir → SM TransitionToInAir): it pushes the gait/foot Start clip from t=0 and
	// queues the OverrideAll root-motion layered move that actually lifts the capsule along the clip's
	// baked arc — anticipation included. RMAction detects the clip apex (vertical RM delta turns negative)
	// and hands itself to Falling; the AnimInstance cancels the RM move on that observed edge. Until the
	// RM move arrives (~1 frame), RMAction holds the capsule in place — which IS the anticipation pose.
	// CanActorJump() was already checked by the caller (OnMoverPreSimulationTick).
	if (bUseHybridJump)
	{
		QueueNextMode(TEXT("RMAction"));
		return true;
	}

	// ---- PHYSICS branch: gait-scaled impulse ----
	// Gait-scaled jump (v2). The gait is read from the deterministic InputCmd — the same
	// FAZ_MoverCustomInputs.Gait the walking mode's ResolveGait consumes — so every machine
	// (owning client prediction, server, replay) computes the identical impulse. Do NOT read
	// the ASC/tags here: this runs on the simulation path and must stay InputCmd-pure.
	EAZ_Gait Gait = EAZ_Gait::Run;
	if (const FAZ_MoverCustomInputs* CustomInputs =
			GetLastInputCmd().InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
	{
		Gait = CustomInputs->Gait;
	}

	float UpwardsSpeed = JumpUpwardsSpeedRun;
	switch (Gait)
	{
	case EAZ_Gait::Walk:   UpwardsSpeed = JumpUpwardsSpeedWalk;   break;
	case EAZ_Gait::Run:    UpwardsSpeed = JumpUpwardsSpeedRun;    break;
	case EAZ_Gait::Sprint: UpwardsSpeed = JumpUpwardsSpeedSprint; break;
	default:               break;
	}

	TSharedPtr<FJumpImpulseEffect> JumpMove = MakeShared<FJumpImpulseEffect>();
	JumpMove->UpwardsSpeed = UpwardsSpeed;
	QueueInstantMovementEffect(JumpMove);
	return true;
}
