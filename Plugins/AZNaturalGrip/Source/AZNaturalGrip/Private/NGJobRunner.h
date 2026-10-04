// Copyright Artur. AZ project.
// Runs one solver job on a background task: the solver core's parallel loops go to Unreal's ParallelFor, the UI polls
// progress and can cancel. Inputs must be gathered on the game thread BEFORE Start (no UObject access in the job).

#pragma once

#include "CoreMinimal.h"
#include "NGParallel.h"

#include <atomic>

class FNGJobRunner : public TSharedFromThis<FNGJobRunner>
{
public:
	/** The job: gets the Exec (ParallelFor, cancel flag, progress) and returns its log text. Runs off the game thread. */
	using FWork = TUniqueFunction<FString(const ng::Exec&)>;
	/** Called on the game thread when the job ends (Cancelled = the user pressed Cancel). */
	using FDone = TUniqueFunction<void(const FString& /*Log*/, bool /*bCancelled*/)>;

	/** Threads: 0 = all worker threads, 1 = serial. False when a job is already running. */
	bool Start(const FString& Label, int32 Threads, FWork&& Work, FDone&& Done);
	void Cancel() { bCancel = true; }

	bool IsRunning() const { return bRunning; }
	FString GetLabel() const;
	FString GetStage() const;
	float GetFraction() const { return Fraction.load(); }
	double GetElapsed() const;

	/** An Exec for a synchronous run on the calling thread (Python access, tests). */
	static ng::Exec MakeExec(int32 Threads, const std::atomic<bool>* InCancel);

private:
	std::atomic<bool> bRunning{false};
	std::atomic<bool> bCancel{false};
	std::atomic<float> Fraction{0.f};
	double StartTime = 0.0;
	mutable FCriticalSection Lock;
	FString Label_;
	FString Stage_;
};
