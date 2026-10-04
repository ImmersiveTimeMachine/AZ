// Copyright Artur. AZ project.

#include "AZNaturalGripLibrary.h"

#include "AZNaturalGripProfile.h"
#include "NGApply.h"
#include "NGJobRunner.h"
#include "NGJobs.h"

FString UAZNaturalGripLibrary::RunStage(UAZNaturalGripProfile* Profile, const FString& Stage, int32 Threads)
{
	if (!Profile)
	{
		return TEXT("ERROR no profile");
	}
	FNGProfileData Data;
	FString Error;
	if (!NGJobs::Snapshot(*Profile, Data, Error))
	{
		return FString::Printf(TEXT("ERROR %s"), *Error);
	}
	const ng::Exec Ex = FNGJobRunner::MakeExec(Threads, nullptr);
	FString Log;
	try
	{
		Log = NGJobs::Run(Data, Stage, Ex);
	}
	catch (const std::exception& E)
	{
		Log = FString::Printf(TEXT("ERROR %hs"), E.what());
	}
	UE_LOG(LogTemp, Display, TEXT("AZ Natural Grip:\n%s"), *Log);
	return Log;
}

FString UAZNaturalGripLibrary::Apply(UAZNaturalGripProfile* Profile, bool bDryRun)
{
	if (!Profile)
	{
		return TEXT("ERROR no profile");
	}
	FString Log;
	NGApply::Apply(*Profile, bDryRun, Log);
	UE_LOG(LogTemp, Display, TEXT("AZ Natural Grip:\n%s"), *Log);
	return Log;
}
