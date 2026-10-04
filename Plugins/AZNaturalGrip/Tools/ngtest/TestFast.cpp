// Copyright Artur. AZ project.
// Part B check: the fast search (coarse grid + pattern search) against the exhaustive grid (part A, = Python) on the SAME
// scores, over many placements.  ngtest fast support [count]
#include "NGTest.h"

#include "NGSetup.h"
#include "NGSupportHand.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		double Secs(std::chrono::steady_clock::time_point T0)
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		}
	}

	int RunFastSupport(int Count)
	{
		SetupConfig Cfg;
		Cfg.Side = 'l';
		Cfg.Weapon = "m16";
		Cfg.LeftPick = "aim";
		Cfg.Thin = 2;
		Cfg.bFine = false;
		std::string Err;
		auto S1 = std::make_unique<Setup>();
		auto Sf = std::make_unique<Setup>();
		SupportSettings St;
		if (!S1->Load(Cfg, Err) || !LoadSupportElbows(Cfg.SavedDir, Cfg.Weapon, St, Err))
		{
			std::printf("ERROR %s\n", Err.c_str());
			return 2;
		}
		Cfg.Thin = 1;
		Cfg.bFine = true;
		if (!Sf->Load(Cfg, Err))
		{
			std::printf("ERROR %s\n", Err.c_str());
			return 2;
		}
		const Exec Ex = HarnessExec();
		// placements: the applied L2, the stage-1 best, then random ones from the valid stage-1 set
		std::vector<SupportPlacement> All;
		SupportStage1(*S1, St, Ex, All);
		std::vector<std::array<double, 6>> Ps;
		Ps.push_back({0, 10, 15, 1, 0, 0});
		for (size_t I = 0; I < All.size() && Ps.size() < static_cast<size_t>(Count) / 2; ++I)
		{
			Ps.push_back({All[I].P[0], All[I].P[1], All[I].P[2], All[I].P[3], All[I].P[4], All[I].P[5]});
		}
		std::mt19937 Rng(7);
		std::uniform_real_distribution<double> Ang(-20.0, 20.0), Rol(-30.0, 30.0), Off(-2.0, 2.0);
		while (Ps.size() < static_cast<size_t>(Count))
		{
			Ps.push_back({Ang(Rng), Ang(Rng), Rol(Rng), Off(Rng), Off(Rng), Off(Rng)});
		}

		std::vector<double> Phis;
		for (int V = -4; V < 7; ++V)
		{
			Phis.push_back(V * 2.5);
		}
		const std::vector<double> Ratios = {0.5, 0.65, 0.8, 0.95};
		const long long GridEvals = 4LL * 11 * 36 * 36 * 4 + 9 * 9 * 13 * 3;
		SupportFastOptions O;
		if (const char* E = std::getenv("NG_FAST_STEP"))
		{
			O.CoarseStep = std::atof(E);
		}
		if (const char* E = std::getenv("NG_FAST_SEEDS"))
		{
			O.Seeds = std::atoi(E);
		}
		if (const char* E = std::getenv("NG_FAST_PHI"))         // coarse phi spacing
		{
			const double Dp = std::atof(E);
			O.CoarsePhis.clear();
			for (double V = O.PhiLo; V <= O.PhiHi + 1e-9; V += Dp)
			{
				O.CoarsePhis.push_back(V);
			}
		}
		if (const char* E = std::getenv("NG_FAST_THUMB_SEEDS"))
		{
			O.ThumbSeeds = std::atoi(E);
		}
		std::printf("B options: coarse step %g, phis %zu, seeds %d, thumb seeds %d\n", O.CoarseStep, O.CoarsePhis.size(), O.Seeds, O.ThumbSeeds);

		int Better = 0, Equal = 0, Worse = 0, IdxWorse = 0;
		double SumDiff = 0.0, WorstDiff = 0.0, SumEvals = 0.0, TA = 0.0, TB = 0.0;
		std::printf("  %-44s %10s %10s %9s | %8s %8s\n", "placement", "A total", "B total", "B - A", "A index", "B index");
		for (size_t I = 0; I < Ps.size(); ++I)
		{
			SupportFinalResult A, B;
			auto T0 = std::chrono::steady_clock::now();
			SolveSupportFinal(*Sf, St, Ps[I].data(), Phis, 3.0, Ratios, Ex, A);
			TA += Secs(T0);
			T0 = std::chrono::steady_clock::now();
			long long Evals = 0;
			SolveSupportFinalFast(*Sf, St, Ps[I].data(), O, Ex, B, &Evals);
			TB += Secs(T0);
			SumEvals += static_cast<double>(Evals);
			const double D = B.Solve.Total - A.Solve.Total;
			const double Ia = A.Solve.Finger[Index].bValid ? A.Solve.Finger[Index].Score : -99.0;
			const double Ib = B.Solve.Finger[Index].bValid ? B.Solve.Finger[Index].Score : -99.0;
			Better += D > 1e-9 ? 1 : 0;
			Equal += std::fabs(D) <= 1e-9 ? 1 : 0;
			Worse += D < -1e-9 ? 1 : 0;
			IdxWorse += Ib < Ia - 1e-9 ? 1 : 0;          // the first finger has the same context in A and B
			SumDiff += D;
			WorstDiff = std::min(WorstDiff, D);
			if (I < 12 || D < -0.05)
			{
				char Buf[64];
				std::snprintf(Buf, sizeof(Buf), "[%.1f, %.1f, %.1f, %.2f, %.2f, %.2f]", Ps[I][0], Ps[I][1], Ps[I][2], Ps[I][3], Ps[I][4], Ps[I][5]);
				std::printf("  %-44s %10.3f %10.3f %+9.3f | %8.3f %8.3f\n", Buf, A.Solve.Total, B.Solve.Total, D, Ia, Ib);
			}
		}
		const double N = static_cast<double>(Ps.size());
		std::printf("placements %zu: B better %d, equal %d, worse %d (index alone worse %d); mean B-A %+.3f, worst %+.3f\n", Ps.size(), Better,
		            Equal, Worse, IdxWorse, SumDiff / N, WorstDiff);
		std::printf("configurations per placement: A %lld, B %.0f on average; time A %.2f s, B %.2f s in total\n", GridEvals, SumEvals / N, TA, TB);
		std::printf("fast support: %s\n", Worse <= static_cast<int>(N * 0.01) ? "PASS (B >= A in 99%+)" : "CHECK");
		return 0;
	}

	int RunFast(int Argc, char** Argv)
	{
		const std::string Which = Argc > 0 ? Argv[0] : "support";
		const int Count = Argc > 1 ? std::atoi(Argv[1]) : 60;
		if (Which == "support")
		{
			return RunFastSupport(Count);
		}
		std::printf("unknown fast mode %s\n", Which.c_str());
		return 2;
	}
}
