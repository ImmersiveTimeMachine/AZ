// Copyright Artur. AZ project.
// The M16 support-hand POWER grasp (not a parity test): stage 1 placements with a roll about the handguard axis, stage 2
// coarse power fingers + opposing thumb for the best ones, the final fine solve on the best.  ngtest power [top]
// Writes Intermediate/power/power_final.json (lsolve_m16 schema, for Tools/wgs/natgrip/left_view3d.py) and
// power_L2.json (the applied L2 placement under the power scores, for comparison).
#include "NGTest.h"

#include "NGSetup.h"
#include "NGSupportHand.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		void Describe(const char* Label, const SupportFinalResult& R, double Oppose, double Twist, double Theta)
		{
			const SupportSolve& S = R.Solve;
			std::printf("%s: F %.2f palm %+.2f bend [%.0f, %.0f] twist %+.0f theta %+.0f\n", Label, S.Total, R.Palm, R.Bend[0], R.Bend[1], Twist, Theta);
			static const char* Names[] = {"thumb", "index", "middle", "ring", "pinky"};
			for (int F = Index; F <= Pinky; ++F)
			{
				const SupportFinger& Fg = S.Finger[F];
				if (Fg.bValid)
				{
					std::printf("   %-6s a [%3.0f %3.0f %3.0f] phi %+5.1f wrap %3.0f gaps [%.2f %.2f %.2f]\n", Names[F], Fg.A[0], Fg.A[1], Fg.A[2], Fg.Phi, Fg.Wrap,
					            Fg.Gaps[0], Fg.Gaps[1], Fg.Gaps[2]);
				}
				else
				{
					std::printf("   %-6s none\n", Names[F]);
				}
			}
			if (S.Thumb.bValid)
			{
				std::printf("   thumb  cmc (%.0f, %.0f) mcp %.0f ip %.0f pad gap %.2f, hand encloses %.0f deg of the handguard\n", S.Thumb.Abd, S.Thumb.Fl,
				            S.Thumb.M, S.Thumb.Ip, S.Thumb.PadGap, Oppose);
			}
			else
			{
				std::printf("   thumb  none\n");
			}
		}
	}

	int RunPower(int Argc, char** Argv)
	{
		const int Top = Argc > 0 ? std::atoi(Argv[0]) : 24;
		const std::string Out = "Intermediate/power";
		std::filesystem::create_directories(Out);
		SetupConfig Cfg;
		Cfg.Side = 'l';
		Cfg.Weapon = "m16";
		Cfg.LeftPick = "aim";
		std::string Err;
		auto S2 = std::make_unique<Setup>();
		auto Sf = std::make_unique<Setup>();
		Cfg.Thin = 2;
		Cfg.bFine = true;
		SupportSettings St;
		if (!S2->Load(Cfg, Err) || !LoadSupportElbows(Cfg.SavedDir, Cfg.Weapon, St, Err))
		{
			std::printf("ERROR %s\n", Err.c_str());
			return 2;
		}
		Cfg.Thin = 1;
		if (!Sf->Load(Cfg, Err))
		{
			std::printf("ERROR %s\n", Err.c_str());
			return 2;
		}
		St.SetPower();
		const Exec Ex = HarnessExec();
		SupportPowerOptions O;

		// the applied L2 under the power scores (theta 0)
		{
			const double L2[7] = {0, 10, 15, 1, 0, 0, 0};
			SupportFinalResult R;
			double Op = 0.0, Tw = 0.0;
			SolveSupportPower(*Sf, St, L2, O, Ex, R, Op, Tw);
			Describe("applied L2 under the power scores", R, Op, Tw, 0.0);
			WriteSupportFinalJson(*Sf, R, Out + "/power_L2.json");
		}

		std::vector<PowerPlacement> P1;
		SupportPowerStage1(*S2, St, O, Ex, P1);
		std::printf("stage 1: %zu placements\n", P1.size());
		for (size_t I = 0; I < P1.size() && I < 8; ++I)
		{
			const PowerPlacement& R = P1[I];
			std::printf("  J1 %.2f p [%g %g %g %g %g %g] theta %+g palm %+.2f bend [%.0f %.0f] twist %+.0f palm angle %.0f\n", R.J1, R.P[0], R.P[1], R.P[2], R.P[3],
			            R.P[4], R.P[5], R.P[6], R.Palm, R.Bend[0], R.Bend[1], R.Twist, R.PalmAngle);
		}

		SupportPowerOptions Coarse = O;
		Coarse.Phis = {-5.0, 0.0, 5.0, 10.0};
		Coarse.Step = 8.0;
		Coarse.Ratios = {0.55, 0.75};
		Coarse.bPolish = false;
		struct FCand
		{
			PowerPlacement P;
			double F = 0.0, J2 = 0.0, Oppose = 0.0;
		};
		// stage 1 does not see the fingers, so the candidates are the best Top/9 of EVERY roll angle, not the global top
		std::vector<PowerPlacement> Picks;
		const int PerTheta = std::max(1, Top / static_cast<int>(O.Thetas.size()));
		for (const double Th : O.Thetas)
		{
			int N = 0;
			for (const PowerPlacement& Pp : P1)
			{
				if (Pp.P[6] == Th && N < PerTheta)
				{
					Picks.push_back(Pp);
					++N;
				}
			}
		}
		std::vector<FCand> C2;
		for (const PowerPlacement& Pp : Picks)
		{
			SupportFinalResult R;
			double Op = 0.0, Tw = 0.0;
			SolveSupportPower(*S2, St, Pp.P, Coarse, Ex, R, Op, Tw);
			C2.push_back({Pp, R.Solve.Total, Pp.J1 - R.Solve.Total, Op});
		}
		std::stable_sort(C2.begin(), C2.end(), [](const FCand& A, const FCand& B) { return A.J2 < B.J2; });
		std::printf("stage 2 (best of %zu):\n", C2.size());
		for (size_t I = 0; I < C2.size() && I < 8; ++I)
		{
			const FCand& C = C2[I];
			std::printf("  J2 %6.2f J1 %.2f F %6.2f p [%g %g %g %g %g %g] theta %+g twist %+.0f oppose %.0f\n", C.J2, C.P.J1, C.F, C.P.P[0], C.P.P[1], C.P.P[2],
			            C.P.P[3], C.P.P[4], C.P.P[5], C.P.P[6], C.P.Twist, C.Oppose);
		}
		if (C2.empty())
		{
			return 1;
		}
		SupportFinalResult R;
		double Op = 0.0, Tw = 0.0;
		SolveSupportPower(*Sf, St, C2[0].P.P, O, Ex, R, Op, Tw);
		Describe("POWER final", R, Op, Tw, C2[0].P.P[6]);
		WriteSupportFinalJson(*Sf, R, Out + "/power_final.json");
		SupportGapReport Gaps;
		SupportFingerGaps(*Sf, R.HandInWeapon, R.Solve.Locs, Gaps);
		std::printf("%s", FormatSupportGapReport("finger gaps", Gaps).c_str());
		return 0;
	}
}
