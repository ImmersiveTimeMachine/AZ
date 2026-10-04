// Copyright Artur. AZ project.
// Parity harness for the Natural Grip core: shared helpers.
#pragma once

#include "NGJson.h"
#include "NGMath.h"
#include "NGParallel.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace ngtest
{
	/** Largest absolute difference over a group of compared numbers. */
	struct Diff
	{
		std::string Name;
		double Max = 0.0;
		size_t Count = 0;
		size_t Mismatch = 0;           // exact-match comparisons that failed (names, counts, picks)
		std::string Where;

		explicit Diff(const std::string& N) : Name(N) {}

		void Num(double Py, double Cpp, const std::string& At = std::string())
		{
			++Count;
			const double D = std::isnan(Py) && std::isnan(Cpp) ? 0.0 : std::fabs(Py - Cpp);
			if (!(D <= Max))           // NaN on one side counts as infinite
			{
				Max = std::isnan(D) ? 1e300 : D;
				Where = At;
			}
		}
		void Vec(const ng::JValue& Py, const ng::V3& Cpp, const std::string& At = std::string())
		{
			for (int K = 0; K < 3; ++K)
			{
				Num(Py.NumAt(K), Cpp[K], At);
			}
		}
		void Xf(const ng::JValue& Py, const ng::X& Cpp, const std::string& At = std::string())
		{
			Vec(Py, Cpp.t, At);
			const double Q[4] = {Cpp.q.x, Cpp.q.y, Cpp.q.z, Cpp.q.w};
			for (int K = 0; K < 4; ++K)
			{
				Num(Py.NumAt(3 + K), Q[K], At);
			}
		}
		void Exact(bool bSame, const std::string& At = std::string())
		{
			++Count;
			if (!bSame)
			{
				if (Mismatch == 0)
				{
					Where = At;
				}
				++Mismatch;
			}
		}
		bool Ok(double Tol) const { return Mismatch == 0 && Max <= Tol; }
		void Print(double Tol) const
		{
			std::printf("  %-26s %-4s n=%-8zu max|diff|=%-10.3g mismatches=%zu %s\n", Name.c_str(), Ok(Tol) ? "OK" : "FAIL",
			            Count, Max, Mismatch, Where.empty() ? "" : ("worst at " + Where).c_str());
		}
	};

	inline ng::X F7Of(const ng::JValue& A)
	{
		double V[7];
		for (int K = 0; K < 7; ++K)
		{
			V[K] = A.NumAt(K);
		}
		return ng::X::F7(V);
	}

	/** std::thread pool loop (the harness's stand-in for Unreal's ParallelFor). NG_THREADS=1 forces serial. */
	inline void ThreadPoolFor(int Num, const ng::ForBody& Body)
	{
		unsigned NumThreads = std::thread::hardware_concurrency();
		if (const char* Env = std::getenv("NG_THREADS"))
		{
			NumThreads = static_cast<unsigned>(std::atoi(Env));
		}
		if (NumThreads <= 1 || Num <= 1)
		{
			ng::SerialFor(Num, Body);
			return;
		}
		std::atomic<int> Next{0};
		std::vector<std::thread> Threads;
		const unsigned Count = NumThreads < static_cast<unsigned>(Num) ? NumThreads : static_cast<unsigned>(Num);
		for (unsigned T = 0; T < Count; ++T)
		{
			Threads.emplace_back([&]()
			{
				for (;;)
				{
					const int I = Next.fetch_add(1);
					if (I >= Num)
					{
						break;
					}
					Body(I);
				}
			});
		}
		for (std::thread& T : Threads)
		{
			T.join();
		}
	}

	inline ng::Exec HarnessExec()
	{
		ng::Exec E;
		E.For = ThreadPoolFor;
		return E;
	}

	int RunPrims(const std::string& DumpPath);
	/** Support (left) hand solver parity: TestSupport.cpp. Args after the mode name. */
	int RunSupport(int Argc, char** Argv);
	/** Trigger (right) hand solver parity: TestTrigger.cpp. Args after the mode name. */
	int RunTrigger(int Argc, char** Argv);
	/** Geometry / field bake parity: TestGeometry.cpp. Args after the mode name. */
	int RunGeometry(int Argc, char** Argv);
	/** Part B vs part A on equal scores: TestFast.cpp. Args after the mode name. */
	int RunFast(int Argc, char** Argv);
	/** M16 support-hand power grasp: TestPower.cpp. */
	int RunPower(int Argc, char** Argv);
}
