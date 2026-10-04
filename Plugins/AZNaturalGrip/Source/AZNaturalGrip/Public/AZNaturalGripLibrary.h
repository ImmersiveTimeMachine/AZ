// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZNaturalGripLibrary.generated.h"

class UAZNaturalGripProfile;

/** Script access to the AZ Natural Grip solver (Python: unreal.AZNaturalGripLibrary.run_stage(profile, "parity", 0)). */
UCLASS()
class AZNATURALGRIP_API UAZNaturalGripLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Runs one solver stage synchronously (the editor waits) and returns its log. Stages: fields, stage1, stage2, final,
	 *  support, placement, solve, trigger, parity. Threads: 0 = all worker threads, 1 = serial. Results are written under
	 *  <Project>/Saved/NaturalGrip/<Profile>/. */
	UFUNCTION(BlueprintCallable, Category = "AZ Natural Grip")
	static FString RunStage(UAZNaturalGripProfile* Profile, const FString& Stage, int32 Threads = 0);

	/** Writes the profile's last final solve into the weapon's assets (socket / correction, grip pose, baked flag) after a
	 *  backup, then reads it back. bDryRun: only the plan. Returns the log (ends with APPLIED on success). */
	UFUNCTION(BlueprintCallable, Category = "AZ Natural Grip")
	static FString Apply(UAZNaturalGripProfile* Profile, bool bDryRun = true);
};
