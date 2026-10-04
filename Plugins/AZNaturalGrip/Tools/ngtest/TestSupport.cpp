// Copyright Artur. AZ project.
// Support (left) hand solver parity: NGSupportHand against the Python goldens in Tools/wgs/natgrip.
//   ngtest support stage1|stage2|final|all [--out DIR] [--golden DIR]
// stage1: lsolve_stage1.json (top 400 of the 6-D grid, Thin 2, coarse field)
// stage2: lsolve_stage2_0.json (24 candidates, Thin 2, fine field, step 8, phis -5..10, ratios .55 .75)
// final : lsolve_m16_L2.json (p = 0,10,15,1,0,0, Thin 1, fine field, step 3, 11 phis, 4 ratios) + the gap report
// The C++ results are also written to DIR (default Intermediate/support/out) so runs with different NG_THREADS can be
// compared byte for byte.
#include "NGTest.h"

#include "NGSetup.h"
#include "NGSupportHand.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		const double kTol = 1e-9;
		const char* kSaved = "C:/UnrealEngine/Games/AZ/Saved/wgs";

		struct Ctx
		{
			std::string GoldenDir = "C:/UnrealEngine/Games/AZ/Tools/wgs/natgrip";
			std::string OutDir = "C:/UnrealEngine/Games/AZ/Plugins/AZNaturalGrip/Tools/ngtest/Intermediate/support/out";
		};

		double Seconds(std::chrono::steady_clock::time_point T0)
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		}

		bool MakeSetup(std::unique_ptr<Setup>& Su, int Thin, bool bFine)
		{
			SetupConfig Cfg;
			Cfg.SavedDir = kSaved;
			Cfg.Side = 'l';
			Cfg.Weapon = "m16";
			Cfg.LeftPick = "aim";
			Cfg.Thin = Thin;
			Cfg.bFine = bFine;
			Su = std::make_unique<Setup>();
			std::string Err;
			if (!Su->Load(Cfg, Err))
			{
				std::printf("ERROR setup (thin %d fine %d): %s\n", Thin, bFine ? 1 : 0, Err.c_str());
				return false;
			}
			if (Su->bFineLoaded != bFine)
			{
				std::printf("ERROR setup: fine field loaded=%d expected %d (%s)\n", Su->bFineLoaded ? 1 : 0, bFine ? 1 : 0, Su->FinePath.c_str());
				return false;
			}
			std::printf("setup: side l, weapon m16, thin %d, fine %d (%s)\n", Thin, bFine ? 1 : 0, Su->bFineLoaded ? Su->FinePath.c_str() : "coarse only");
			return true;
		}

		bool MakeSettings(SupportSettings& St)
		{
			std::string Err;
			if (!LoadSupportElbows(kSaved, "m16", St, Err))
			{
				std::printf("ERROR elbows: %s\n", Err.c_str());
				return false;
			}
			return true;
		}

		bool LoadGolden(const Ctx& C, const char* Name, JValue& J)
		{
			std::string Err;
			if (!LoadJsonFile(C.GoldenDir + "/" + Name, J, Err))
			{
				std::printf("ERROR golden %s: %s\n", Name, Err.c_str());
				return false;
			}
			return true;
		}

		void EnsureDir(const std::string& Dir)
		{
			std::error_code Ec;
			std::filesystem::create_directories(Dir, Ec);
		}

		bool SaveText(const std::string& Path, const std::string& Text)
		{
			std::ofstream F(Path, std::ios::binary);
			F << Text;
			return static_cast<bool>(F);
		}

		void PrintGroups(std::vector<Diff>& Ds, bool& bPass)
		{
			for (const Diff& D : Ds)
			{
				D.Print(kTol);
				bPass = bPass && D.Ok(kTol);
			}
		}

		const char* FingerKey(int F)
		{
			static const char* Names[NumFingers] = {"thumb", "index", "middle", "ring", "pinky"};
			return Names[F];
		}

		/** Groups shared by the stage-2 records and the final report (the Python "rec" dict). */
		struct RecDiffs
		{
			Diff Presence{"finger/thumb present"};
			Diff Angles{"angles + phi (exact)"};
			Diff Gaps{"finger gaps"};
			Diff Adj{"adj (presence exact)"};
			Diff Wrap{"wrap"};
			Diff Thumb{"thumb cmc/mcp/ip (exact)"};
			Diff Pad{"thumb pad_gap"};
			Diff PalmAngle{"palm_angle"};

			void Collect(std::vector<Diff>& Out) const
			{
				Out.push_back(Presence);
				Out.push_back(Angles);
				Out.push_back(Gaps);
				Out.push_back(Adj);
				Out.push_back(Wrap);
				Out.push_back(Thumb);
				Out.push_back(Pad);
				Out.push_back(PalmAngle);
			}
		};

		/** Compares a Python rec (fingers + thumb + palm_angle) with the C++ solve. */
		void CompareRec(RecDiffs& D, const JValue& Rec, const SupportSolve& S, const std::string& At)
		{
			for (int F = Index; F <= Pinky; ++F)
			{
				const std::string AtF = At + " " + FingerKey(F);
				const JValue* Py = Rec.Find(FingerKey(F));
				const SupportFinger& C = S.Finger[F];
				const bool bPyNull = !Py || Py->IsNull();
				D.Presence.Exact(bPyNull == !C.bValid, AtF);
				if (bPyNull || !C.bValid)
				{
					continue;
				}
				D.Angles.Exact((*Py)["phi"].Number() == C.Phi, AtF + " phi");
				const JValue& A = (*Py)["a"];
				for (int K = 0; K < 3; ++K)
				{
					D.Angles.Exact(A.NumAt(static_cast<size_t>(K)) == C.A[K], AtF + " a" + std::to_string(K));
				}
				const JValue& G = (*Py)["gaps"];
				for (int K = 0; K < 3; ++K)
				{
					D.Gaps.Num(G.NumAt(static_cast<size_t>(K)), C.Gaps[K], AtF);
				}
				const JValue& Adj = (*Py)["adj"];
				D.Adj.Exact(Adj.IsNull() == !C.bHasAdj, AtF);
				if (!Adj.IsNull() && C.bHasAdj)
				{
					for (int K = 0; K < 3; ++K)
					{
						D.Adj.Num(Adj.NumAt(static_cast<size_t>(K)), C.Adj[K], AtF);
					}
				}
				D.Wrap.Num((*Py)["wrap"].Number(), C.Wrap, AtF);
			}
			const JValue* Tb = Rec.Find("thumb");
			const bool bTbNull = !Tb || Tb->IsNull();
			D.Presence.Exact(bTbNull == !S.Thumb.bValid, At + " thumb");
			if (!bTbNull && S.Thumb.bValid)
			{
				const JValue& Cmc = (*Tb)["cmc"];
				D.Thumb.Exact(Cmc.NumAt(0) == static_cast<double>(S.Thumb.Abd), At + " abd");
				D.Thumb.Exact(Cmc.NumAt(1) == static_cast<double>(S.Thumb.Fl), At + " fl");
				D.Thumb.Exact((*Tb)["mcp"].Number() == static_cast<double>(S.Thumb.M), At + " mcp");
				D.Thumb.Exact((*Tb)["ip"].Number() == S.Thumb.Ip, At + " ip");
				D.Pad.Num((*Tb)["pad_gap"].Number(), S.Thumb.PadGap, At);
			}
			D.PalmAngle.Num(Rec["palm_angle"].Number(), S.PalmAngle, At);
		}

		int TestStage1(const Ctx& C)
		{
			std::printf("== support stage1\n");
			std::unique_ptr<Setup> Su;
			SupportSettings St;
			if (!MakeSetup(Su, 2, false) || !MakeSettings(St))
			{
				return 2;
			}
			JValue G;
			if (!LoadGolden(C, "lsolve_stage1.json", G))
			{
				return 2;
			}
			const Exec Ex = HarnessExec();
			const auto T0 = std::chrono::steady_clock::now();
			std::vector<SupportPlacement> Res;
			SupportStage1(*Su, St, Ex, Res);
			std::printf("stage1: %zu valid placements in %.2f s\n", Res.size(), Seconds(T0));

			Diff Dn("valid count (1445)"), Dp("p (exact)"), Dpalm("palm"), Dbend("bend"), Ddev("dev"), Dj("J1");
			Dn.Exact(Res.size() == 1445, "count");
			Dn.Exact(Res.size() >= G.Size(), "golden size");
			for (size_t I = 0; I < G.Size() && I < Res.size(); ++I)
			{
				const JValue& Py = G.At(I);
				const SupportPlacement& R = Res[I];
				const std::string At = "rank " + std::to_string(I);
				const JValue& P = Py["p"];
				for (int K = 0; K < 6; ++K)
				{
					Dp.Exact(P.NumAt(static_cast<size_t>(K)) == R.P[K], At);
				}
				Dpalm.Num(Py["palm"].Number(), R.Palm, At);
				Dbend.Num(Py["bend"].NumAt(0), R.Bend[0], At);
				Dbend.Num(Py["bend"].NumAt(1), R.Bend[1], At);
				Ddev.Num(Py["dev"].Number(), R.Dev, At);
				Dj.Num(Py["J1"].Number(), R.J1, At);
			}
			std::vector<Diff> Ds = {Dn, Dp, Dpalm, Dbend, Ddev, Dj};
			bool bPass = true;
			PrintGroups(Ds, bPass);

			EnsureDir(C.OutDir);
			JWriter W(false);
			W.BeginArray();
			for (const SupportPlacement& R : Res)
			{
				W.BeginObject();
				W.Key("p").Numbers(R.P, 6);
				W.Key("palm").Value(R.Palm);
				W.Key("bend").Numbers(R.Bend, 2);
				W.Key("dev").Value(R.Dev);
				W.Key("J1").Value(R.J1);
				W.EndObject();
			}
			W.EndArray();
			SaveText(C.OutDir + "/support_stage1.json", W.Str());
			std::printf("support stage1: %s\n", bPass ? "PASS" : "FAIL");
			return bPass ? 0 : 1;
		}

		int TestStage2(const Ctx& C)
		{
			std::printf("== support stage2\n");
			std::unique_ptr<Setup> Su;
			SupportSettings St;
			if (!MakeSetup(Su, 2, true) || !MakeSettings(St))
			{
				return 2;
			}
			JValue G1, G2;
			if (!LoadGolden(C, "lsolve_stage1.json", G1) || !LoadGolden(C, "lsolve_stage2_0.json", G2))
			{
				return 2;
			}
			// the candidates = the first N of the stage-1 list, carrying the stage-1 numbers (coarse field) like the Python run
			std::vector<SupportPlacement> Cands;
			for (size_t I = 0; I < G2.Size() && I < G1.Size(); ++I)
			{
				const JValue& Py = G1.At(I);
				SupportPlacement P;
				for (int K = 0; K < 6; ++K)
				{
					P.P[K] = Py["p"].NumAt(static_cast<size_t>(K));
				}
				P.Palm = Py["palm"].Number();
				P.Bend[0] = Py["bend"].NumAt(0);
				P.Bend[1] = Py["bend"].NumAt(1);
				P.Dev = Py["dev"].Number();
				P.J1 = Py["J1"].Number();
				Cands.push_back(P);
			}
			const std::vector<double> Phis = {-5.0, 0.0, 5.0, 10.0};
			const std::vector<double> Ratios = {0.55, 0.75};
			const Exec Ex = HarnessExec();
			const auto T0 = std::chrono::steady_clock::now();
			std::vector<SupportStage2Result> Res;
			SupportStage2(*Su, St, Cands, Phis, 8.0, Ratios, Ex, Res);
			std::printf("stage2: %zu candidates in %.2f s\n", Res.size(), Seconds(T0));

			Diff Dn("candidates"), Dp("p (exact)"), Df("F"), Dj("J2");
			RecDiffs Rd;
			Dn.Exact(Res.size() == G2.Size(), "count");
			for (size_t I = 0; I < G2.Size() && I < Res.size(); ++I)
			{
				const JValue& Py = G2.At(I);
				const SupportStage2Result& R = Res[I];
				const std::string At = "cand " + std::to_string(I);
				for (int K = 0; K < 6; ++K)
				{
					Dp.Exact(Py["p"].NumAt(static_cast<size_t>(K)) == R.Cand.P[K], At);
				}
				CompareRec(Rd, Py["fingers"], R.Solve, At);
				Df.Num(Py["F"].Number(), R.Solve.Total, At);
				Dj.Num(Py["J2"].Number(), R.J2, At);
			}
			std::vector<Diff> Ds = {Dn, Dp};
			Rd.Collect(Ds);
			Ds.push_back(Df);
			Ds.push_back(Dj);
			bool bPass = true;
			PrintGroups(Ds, bPass);

			EnsureDir(C.OutDir);
			JWriter W(false);
			W.BeginArray();
			for (const SupportStage2Result& R : Res)
			{
				W.BeginObject();
				W.Key("p").Numbers(R.Cand.P, 6);
				W.Key("fingers").BeginObject();
				WriteSupportRecord(W, R.Solve);
				W.EndObject();
				W.Key("F").Value(R.Solve.Total);
				W.Key("J2").Value(R.J2);
				W.EndObject();
			}
			W.EndArray();
			SaveText(C.OutDir + "/support_stage2.json", W.Str());
			std::printf("support stage2: %s\n", bPass ? "PASS" : "FAIL");
			return bPass ? 0 : 1;
		}

		int TestFinal(const Ctx& C)
		{
			std::printf("== support final (L2)\n");
			std::unique_ptr<Setup> Su;
			SupportSettings St;
			if (!MakeSetup(Su, 1, true) || !MakeSettings(St))
			{
				return 2;
			}
			JValue G;
			if (!LoadGolden(C, "lsolve_m16_L2.json", G))
			{
				return 2;
			}
			const double P[6] = {0.0, 10.0, 15.0, 1.0, 0.0, 0.0};
			std::vector<double> Phis;
			for (int V = -4; V < 7; ++V)
			{
				Phis.push_back(static_cast<double>(V) * 2.5);
			}
			const std::vector<double> Ratios = {0.5, 0.65, 0.8, 0.95};
			const Exec Ex = HarnessExec();
			const auto T0 = std::chrono::steady_clock::now();
			SupportFinalResult R;
			SolveSupportFinal(*Su, St, P, Phis, 3.0, Ratios, Ex, R);
			std::printf("final: solved in %.2f s, F = %.17g\n", Seconds(T0), R.Solve.Total);

			Diff Dp("p (exact)"), Df("F"), Dpalm("palm"), Dbend("bend"), Dh("hand_in_weapon"), Dl("locals (quaternions)"), Dk("locals keys");
			RecDiffs Rd;
			for (int K = 0; K < 6; ++K)
			{
				Dp.Exact(G["p"].NumAt(static_cast<size_t>(K)) == R.P[K], "p");
			}
			Df.Num(G["F"].Number(), R.Solve.Total, "F");
			const JValue& Rep = G["report"];
			CompareRec(Rd, Rep, R.Solve, "report");
			Dpalm.Num(Rep["palm"].Number(), R.Palm, "palm");
			Dbend.Num(Rep["bend"].NumAt(0), R.Bend[0], "bend aim");
			Dbend.Num(Rep["bend"].NumAt(1), R.Bend[1], "bend relaxed");
			Dh.Xf(G["hand_in_weapon"], R.HandInWeapon, "hand_in_weapon");
			const HandModel& H = Su->Hand;
			size_t NumSet = 0;
			for (int B = 1; B < H.NumBones(); ++B)
			{
				if (R.Solve.Locs.Has(B))
				{
					++NumSet;
				}
			}
			Dk.Exact(NumSet == G["locals"].Obj.size(), "locals count");
			for (const auto& KV : G["locals"].Obj)
			{
				const int B = H.BoneIndex(KV.first);
				Dk.Exact(B >= 0 && R.Solve.Locs.Has(B), KV.first);
				if (B >= 0 && R.Solve.Locs.Has(B))
				{
					const Q& Qc = R.Solve.Locs.L[static_cast<size_t>(B)].q;
					const double Qv[4] = {Qc.x, Qc.y, Qc.z, Qc.w};
					for (int K = 0; K < 4; ++K)
					{
						Dl.Num(KV.second.NumAt(static_cast<size_t>(K)), Qv[K], KV.first);
					}
				}
			}
			std::vector<Diff> Ds = {Dp, Df};
			Rd.Collect(Ds);
			Ds.push_back(Dpalm);
			Ds.push_back(Dbend);
			Ds.push_back(Dh);
			Ds.push_back(Dk);
			Ds.push_back(Dl);
			bool bPass = true;
			PrintGroups(Ds, bPass);

			// finger_gaps.report text, compared with what the Python run printed (lsolve_L2.out)
			SupportGapReport Gap;
			SupportFingerGaps(*Su, R.HandInWeapon, R.Solve.Locs, Gap);
			const std::string Text = FormatSupportGapReport("M16 support hand L2", Gap);
			std::printf("%s", Text.c_str());
			{
				std::ifstream F(C.GoldenDir + "/lsolve_L2.out");
				std::stringstream Ss;
				Ss << F.rdbuf();
				const std::string Py = Ss.str();
				Diff Dg("gap report vs lsolve_L2.out");
				if (!F || Py.empty())
				{
					std::printf("  (lsolve_L2.out not readable: gap report text not compared)\n");
				}
				else
				{
					std::istringstream Lines(Text);
					std::string Line;
					while (std::getline(Lines, Line))
					{
						std::string Clean = Line;
						while (!Clean.empty() && (Clean.back() == '\r' || Clean.back() == '\n'))
						{
							Clean.pop_back();
						}
						Dg.Exact(Py.find(Clean) != std::string::npos, Clean);
					}
					std::vector<Diff> Dgs = {Dg};
					PrintGroups(Dgs, bPass);
				}
			}

			EnsureDir(C.OutDir);
			WriteSupportFinalJson(*Su, R, C.OutDir + "/support_final_L2.json");
			SaveText(C.OutDir + "/support_final_L2_gaps.txt", Text);
			std::printf("support final: %s\n", bPass ? "PASS" : "FAIL");
			return bPass ? 0 : 1;
		}
	}

	int RunSupport(int Argc, char** Argv)
	{
		Ctx C;
		std::string Mode = "all";
		for (int I = 0; I < Argc; ++I)
		{
			if (std::strcmp(Argv[I], "--out") == 0 && I + 1 < Argc)
			{
				C.OutDir = Argv[++I];
			}
			else if (std::strcmp(Argv[I], "--golden") == 0 && I + 1 < Argc)
			{
				C.GoldenDir = Argv[++I];
			}
			else
			{
				Mode = Argv[I];
			}
		}
		if (Mode != "stage1" && Mode != "stage2" && Mode != "final" && Mode != "all")
		{
			std::printf("usage: ngtest support stage1|stage2|final|all [--out DIR] [--golden DIR]\n");
			return 2;
		}
		const char* Env = std::getenv("NG_THREADS");
		std::printf("support %s: NG_THREADS=%s (hardware %u)\n", Mode.c_str(), Env ? Env : "(all)", std::thread::hardware_concurrency());
		int Code = 0;
		if (Mode == "stage1" || Mode == "all")
		{
			Code = Code | TestStage1(C);
		}
		if (Mode == "stage2" || Mode == "all")
		{
			Code = Code | TestStage2(C);
		}
		if (Mode == "final" || Mode == "all")
		{
			Code = Code | TestFinal(C);
		}
		return Code;
	}
}
