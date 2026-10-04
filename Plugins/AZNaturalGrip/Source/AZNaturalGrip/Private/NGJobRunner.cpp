// Copyright Artur. AZ project.

#include "NGJobRunner.h"

#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformTime.h"
#include "Tasks/Task.h"

ng::Exec FNGJobRunner::MakeExec(int32 Threads, const std::atomic<bool>* InCancel)
{
	ng::Exec Ex;
	Ex.Cancel = InCancel;
	if (Threads == 1)
	{
		Ex.For = ng::SerialFor;
	}
	else
	{
		Ex.For = [](int Num, const ng::ForBody& Body)
		{
			ParallelFor(Num, [&Body](int32 I) { Body(I); });
		};
	}
	return Ex;
}

bool FNGJobRunner::Start(const FString& Label, int32 Threads, FWork&& Work, FDone&& Done)
{
	bool bExpected = false;
	if (!bRunning.compare_exchange_strong(bExpected, true))
	{
		return false;
	}
	bCancel = false;
	Fraction = 0.f;
	StartTime = FPlatformTime::Seconds();
	{
		FScopeLock ScopeLock(&Lock);
		Label_ = Label;
		Stage_ = Label;
	}
	TWeakPtr<FNGJobRunner> WeakThis = AsShared();
	UE::Tasks::Launch(UE_SOURCE_LOCATION,
		[WeakThis, Threads, Work = MoveTemp(Work), Done = MoveTemp(Done)]() mutable
		{
			TSharedPtr<FNGJobRunner> This = WeakThis.Pin();
			if (!This.IsValid())
			{
				return;
			}
			ng::Exec Ex = MakeExec(Threads, &This->bCancel);
			FNGJobRunner* Raw = This.Get();
			Ex.Progress = [Raw](const char* Stage, double InFraction)
			{
				Raw->Fraction = static_cast<float>(InFraction);
				FScopeLock ScopeLock(&Raw->Lock);
				Raw->Stage_ = UTF8_TO_TCHAR(Stage);
			};
			FString Log;
			try
			{
				Log = Work(Ex);
			}
			catch (const std::exception& E)
			{
				Log = FString::Printf(TEXT("ERROR %hs"), E.what());
			}
			const bool bWasCancelled = This->bCancel.load();
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Log = MoveTemp(Log), bWasCancelled, Done = MoveTemp(Done)]() mutable
			{
				if (TSharedPtr<FNGJobRunner> Pinned = WeakThis.Pin())
				{
					Pinned->bRunning = false;
					Pinned->Fraction = 1.f;
				}
				Done(Log, bWasCancelled);
			});
		});
	return true;
}

FString FNGJobRunner::GetLabel() const
{
	FScopeLock ScopeLock(&Lock);
	return Label_;
}

FString FNGJobRunner::GetStage() const
{
	FScopeLock ScopeLock(&Lock);
	return Stage_;
}

double FNGJobRunner::GetElapsed() const
{
	return bRunning ? FPlatformTime::Seconds() - StartTime : 0.0;
}
