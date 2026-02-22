// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/AZ_AT_WaitChangeFOV.h"

#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"

UAZ_AT_WaitChangeFOV::UAZ_AT_WaitChangeFOV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bIsFinished = false;
}

UAZ_AT_WaitChangeFOV* UAZ_AT_WaitChangeFOV::WaitChangeFOV(UGameplayAbility* OwningAbility, FName TaskInstanceName, class UCameraComponent* CameraComponent, float TargetFOV, float Duration, UCurveFloat* OptionalInterpolationCurve)
{
	auto* NewTask = NewAbilityTask<UAZ_AT_WaitChangeFOV>(OwningAbility, TaskInstanceName);
	NewTask->CameraComponent = CameraComponent;
	if (CameraComponent != nullptr)
	{
		NewTask->StartFOV = NewTask->CameraComponent->FieldOfView;
	}
	NewTask->TargetFOV = TargetFOV;
	NewTask->Duration = FMath::Max(Duration, 0.001f);
	NewTask->TimeChangeStarted = NewTask->GetWorld()->GetTimeSeconds();
	NewTask->TimeChangeWillEnd = NewTask->TimeChangeStarted + NewTask->Duration;
	NewTask->LerpCurve = OptionalInterpolationCurve;
	return NewTask;
}

void UAZ_AT_WaitChangeFOV::Activate()
{
}

void UAZ_AT_WaitChangeFOV::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	if (CameraComponent)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (CurrentTime >= TimeChangeWillEnd)
		{
			bIsFinished = true;

			CameraComponent->SetFieldOfView(TargetFOV);
			
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				OnTargetFOVReached.Broadcast();
			}
			EndTask();
		}
		else
		{
			float NewFOV;

			float MoveFraction = (CurrentTime - TimeChangeStarted) / Duration;
			if (LerpCurve)
			{
				MoveFraction = LerpCurve->GetFloatValue(MoveFraction);
			}

			NewFOV = FMath::Lerp<float, float>(StartFOV, TargetFOV, MoveFraction);

			CameraComponent->SetFieldOfView(NewFOV);
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UAZ_AT_WaitChangeFOV::OnDestroy(bool AbilityIsEnding)
{
	Super::OnDestroy(AbilityIsEnding);
}
