// Copyright Artur. AZ project.
#include "Character/AZ_MeleeAlignmentMove.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAZMeleeAlignmentMovePriorityTest, "AZ.Melee.Alignment.Priority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAZMeleeAlignmentMovePriorityTest::RunTest(const FString& Parameters)
{
	UMovementMixer* Mixer = NewObject<UMovementMixer>();
	FLayeredMove_RootMotionAttribute LocomotionRootMotion;
	LocomotionRootMotion.StartSimTimeMs = 100.0;
	FProposedMove LocomotionStep;
	LocomotionStep.MixMode = LocomotionRootMotion.MixMode;
	LocomotionStep.LinearVelocity = FVector(200.f, 0.f, 0.f);
	LocomotionStep.AngularVelocityDegrees = FVector(0.f, 0.f, 90.f);

	FLayeredMove_AZ_MeleeAlignment Hold;
	Hold.StartSimTimeMs = 200.0;
	Hold.DurationMs = 225.f;
	FProposedMove HoldStep;
	HoldStep.MixMode = Hold.MixMode;
	HoldStep.LinearVelocity = Hold.Velocity;

	// Exercise the real mixer with both visitation orders. A newer priority-0 hold loses
	// to an existing BlendStack transition in both orders, which is the regression here.
	auto MixWithHold = [&](const FLayeredMoveBase& HoldMove, bool bLocomotionFirst)
	{
		Mixer->ResetMixerState();
		FProposedMove Result;
		if (bLocomotionFirst)
		{
			Mixer->MixLayeredMove(LocomotionRootMotion, LocomotionStep, Result);
			Mixer->MixLayeredMove(HoldMove, HoldStep, Result);
		}
		else
		{
			Mixer->MixLayeredMove(HoldMove, HoldStep, Result);
			Mixer->MixLayeredMove(LocomotionRootMotion, LocomotionStep, Result);
		}
		return Result;
	};

	FLayeredMove_AZ_MeleeAlignment UnprioritizedHold = Hold;
	UnprioritizedHold.Priority = LocomotionRootMotion.Priority;
	for (bool bLocomotionFirst : {true, false})
	{
		const FString Order = bLocomotionFirst ? TEXT("locomotion first") : TEXT("hold first");
		TestEqual(*FString::Printf(TEXT("Regression control: older root motion wins equal priority (%s)"), *Order),
			MixWithHold(UnprioritizedHold, bLocomotionFirst).LinearVelocity, LocomotionStep.LinearVelocity);
		const FProposedMove Result = MixWithHold(Hold, bLocomotionFirst);
		TestEqual(*FString::Printf(TEXT("Blocked recoil remains stationary (%s)"), *Order),
			Result.LinearVelocity, FVector::ZeroVector);
		TestEqual(*FString::Printf(TEXT("Transition root rotation cannot turn blocked recoil (%s)"), *Order),
			Result.AngularVelocityDegrees, FVector::ZeroVector);
	}

	TUniquePtr<FLayeredMoveBase> Clone(Hold.Clone());
	TestEqual(TEXT("Simulation clone retains combat priority"), Clone->Priority, Hold.Priority);
	TestEqual(TEXT("Simulation clone retains scoped move type"), Clone->GetScriptStruct(), Hold.GetScriptStruct());
	TestEqual(TEXT("Cloned hold still overrides existing locomotion root motion"),
		MixWithHold(*Clone, true).LinearVelocity, FVector::ZeroVector);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	Hold.NetSerialize(SaveArchive);
	TestFalse(TEXT("Move serialization succeeds"), SaveArchive.IsError());

	FLayeredMove_AZ_MeleeAlignment Restored;
	Restored.Priority = 0; // Ensure the archive, rather than the constructor, supplies the priority.
	Restored.MixMode = EMoveMixMode::AdditiveVelocity;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, false);
	Restored.NetSerialize(LoadArchive);
	TestFalse(TEXT("Move deserialization succeeds"), LoadArchive.IsError());
	TestEqual(TEXT("Network serialization retains combat priority"), Restored.Priority, Hold.Priority);
	TestTrue(TEXT("Network serialization retains override mode"), Restored.MixMode == Hold.MixMode);
	TestEqual(TEXT("Network serialization retains simulation start time"), Restored.StartSimTimeMs, Hold.StartSimTimeMs);
	TestEqual(TEXT("Deserialized hold still overrides existing locomotion root motion"),
		MixWithHold(Restored, false).LinearVelocity, FVector::ZeroVector);
	return true;
}
#endif
