// Copyright Artur. AZ project.
// Parity: the C++ trigger (right) hand solver against the Python goldens in Tools/wgs/natgrip.
//   ngtest trigger placenat|solve|all [golden dir] [-saved dir] [-dump solve.json]
// placenat: place_nat.py slice 0 1 -> pn_all.json;  solve: rsolve3.py N2 -> rsolve3_m16_N2.json (M16, thin 1, fine field).
// Each stage prints one line per compared group, a checksum of every result bit (equal for any NG_THREADS) and PASS/FAIL.
#include "NGTest.h"

#include "NGSetup.h"
#include "NGTriggerHand.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		const double kTol = 1e-9;

		/** FNV-1a over the bit patterns of every result number: equal checksums = bit-identical results. */
		struct Checksum
		{
			uint64_t H = 1469598103934665603ULL;
			void Add(double V)
			{
				uint64_t Bits = 0;
				std::memcpy(&Bits, &V, sizeof(Bits));
				for (int B = 0; B < 8; ++B)
				{
					H ^= (Bits >> (8 * B)) & 0xFFu;
					H *= 1099511628211ULL;
				}
			}
			void Add(const double* V, int N)
			{
				for (int I = 0; I < N; ++I)
				{
					Add(V[I]);
				}
			}
		};

		struct Args
		{
			std::string Mode = "all";
			std::string GoldenDir = "C:/UnrealEngine/Games/AZ/Tools/wgs/natgrip";
			std::string SavedDir = "C:/UnrealEngine/Games/AZ/Saved/wgs";
			std::string DumpPath;
		};

		bool ParseArgs(int Argc, char** Argv, Args& Out)
		{
			int Pos = 0;
			for (int I = 0; I < Argc; ++I)
			{
				const std::string A = Argv[I];
				if (A == "-saved" && I + 1 < Argc)
				{
					Out.SavedDir = Argv[++I];
				}
				else if (A == "-dump" && I + 1 < Argc)
				{
					Out.DumpPath = Argv[++I];
				}
				else if (Pos == 0)
				{
					Out.Mode = A;
					++Pos;
				}
				else if (Pos == 1)
				{
					Out.GoldenDir = A;
					++Pos;
				}
				else
				{
					return false;
				}
			}
			return Out.Mode == "placenat" || Out.Mode == "solve" || Out.Mode == "all";
		}

		bool LoadM16Setup(const Args& A, std::unique_ptr<Setup>& Su)
		{
			SetupConfig Cfg;
			Cfg.SavedDir = A.SavedDir;
			Cfg.Side = 'r';
			Cfg.Weapon = "m16";
			Cfg.Thin = 1;
			Cfg.bFine = true;
			Su = std::make_unique<Setup>();
			std::string Err;
			if (!Su->Load(Cfg, Err))
			{
				std::printf("ERROR setup: %s\n", Err.c_str());
				return false;
			}
			if (!Su->bFineLoaded)
			{
				std::printf("ERROR the fine field %s was not loaded\n", Su->FinePath.c_str());
				return false;
			}
			return true;
		}

		double MsSince(const std::chrono::steady_clock::time_point& T0)
		{
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		}

		bool Report(const std::vector<Diff>& Diffs, const char* Stage)
		{
			bool bOk = true;
			for (const Diff& D : Diffs)
			{
				D.Print(kTol);
				bOk = bOk && D.Ok(kTol);
			}
			std::printf("trigger %s: %s\n", Stage, bOk ? "PASS" : "FAIL");
			return bOk;
		}

		// --- place_nat ---

		bool RunPlaceNat(const Setup& Su, const TriggerSettings& Set, const Args& A)
		{
			JValue Gold;
			std::string Err;
			if (!LoadJsonFile(A.GoldenDir + "/pn_all.json", Gold, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return false;
			}
			std::vector<PlaceNatEntry> Res;
			const auto T0 = std::chrono::steady_clock::now();
			if (!PlaceNat(Su, Set, HarnessExec(), Res))
			{
				std::printf("ERROR place_nat cancelled\n");
				return false;
			}
			std::printf("placenat: %zu placements in %.0f ms\n", Res.size(), MsSince(T0));

			Diff DCount("placenat count"), DPick("placenat p + idx (exact)"), DNum("placenat J err face palm pen dev");
			DCount.Exact(Gold.Size() == Res.size(), "python " + std::to_string(Gold.Size()) + " cpp " + std::to_string(Res.size()));
			Checksum Sum;
			const size_t N = Gold.Size() < Res.size() ? Gold.Size() : Res.size();
			for (size_t I = 0; I < N; ++I)
			{
				const JValue& G = Gold.At(I);
				const PlaceNatEntry& R = Res[I];
				const std::string At = "entry " + std::to_string(I);
				for (int K = 0; K < 6; ++K)
				{
					DPick.Exact(G["p"].NumAt(static_cast<size_t>(K)) == R.P[K], At + " p" + std::to_string(K));
				}
				for (int K = 0; K < 4; ++K)
				{
					DPick.Exact(G["idx"].NumAt(static_cast<size_t>(K)) == R.Idx[K], At + " idx" + std::to_string(K));
				}
				DNum.Num(G["J"].Number(), R.J, At + " J");
				DNum.Num(G["err"].Number(), R.Err, At + " err");
				DNum.Num(G["face"].Number(), R.Face, At + " face");
				DNum.Num(G["palm"].Number(), R.Palm, At + " palm");
				DNum.Num(G["pen"].Number(), R.Pen, At + " pen");
				DNum.Num(G["dev"].Number(), R.Dev, At + " dev");
			}
			for (const PlaceNatEntry& R : Res)
			{
				Sum.Add(R.P, 6);
				Sum.Add(R.J);
				Sum.Add(R.Idx, 4);
				Sum.Add(R.Err);
				Sum.Add(R.Face);
				Sum.Add(R.Palm);
				Sum.Add(R.Pen);
				Sum.Add(R.Dev);
			}
			std::printf("placenat checksum %016llx\n", static_cast<unsigned long long>(Sum.H));
			return Report({DCount, DPick, DNum}, "placenat");
		}

		// --- rsolve3 ---

		void CompareFinger(Diff& DPick, Diff& DNum, const JValue* G, const TriggerFinger& R, const std::string& At)
		{
			const bool bGoldNull = G == nullptr || G->IsNull();
			DPick.Exact(bGoldNull == !R.bPlaced, At + " placed");
			if (bGoldNull || !R.bPlaced)
			{
				return;
			}
			DPick.Exact((*G)["phi"].Number() == R.Phi, At + " phi");
			for (int K = 0; K < 3; ++K)
			{
				DPick.Exact((*G)["a"].NumAt(static_cast<size_t>(K)) == R.A[K], At + " a" + std::to_string(K));
				DNum.Num((*G)["gaps"].NumAt(static_cast<size_t>(K)), R.Gaps[K], At + " gap" + std::to_string(K));
				DNum.Num((*G)["adj"].NumAt(static_cast<size_t>(K)), R.Adj[K], At + " adj" + std::to_string(K));
			}
		}

		bool RunSolve(const Setup& Su, const TriggerSettings& Set, const Args& A)
		{
			JValue Gold;
			std::string Err;
			if (!LoadJsonFile(A.GoldenDir + "/rsolve3_m16_N2.json", Gold, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return false;
			}
			const double P[6] = {15.0, -5.0, 0.0, -2.0, -1.0, -1.5};
			const double X0[4] = {2.0, 20.0, 40.0, 26.0};
			TriggerSolution R;
			const auto T0 = std::chrono::steady_clock::now();
			if (!SolveTrigger(Su, Set, P, X0, HarnessExec(), R))
			{
				std::printf("ERROR solve cancelled\n");
				return false;
			}
			std::printf("solve: N2 in %.0f ms\n", MsSince(T0));

			Diff DHead("solve p + palm"), DIdx("solve index"), DPick("solve fingers/thumb picks (exact)"), DNum("solve gaps/adj"),
				DHand("solve hand_in_weapon"), DLoc("solve locals");
			for (int K = 0; K < 6; ++K)
			{
				DHead.Exact(Gold["p"].NumAt(static_cast<size_t>(K)) == R.P[K], "p" + std::to_string(K));
			}
			DHead.Num(Gold["palm"].Number(), R.Palm, "palm");

			const JValue& GI = Gold["index"];
			DIdx.Exact(GI["phi"].Number() == R.IndexPhi, "index phi");
			for (int K = 0; K < 3; ++K)
			{
				DIdx.Exact(GI["a"].NumAt(static_cast<size_t>(K)) == R.IndexA[K], "index a" + std::to_string(K));
			}
			DIdx.Num(GI["err"].Number(), R.IndexErr, "index err");
			DIdx.Num(GI["face"].Number(), R.IndexFace, "index face");
			DIdx.Num(GI["pen"].Number(), R.IndexPen, "index pen");
			DIdx.Num(GI["unnatural"].Number(), R.IndexUnnatural, "index unnatural");

			static const char* Names[3] = {"middle", "ring", "pinky"};
			for (int I = 0; I < 3; ++I)
			{
				CompareFinger(DPick, DNum, Gold.Find(Names[I]), R.Fingers[I], Names[I]);
			}
			const JValue* GT = Gold.Find("thumb");
			const bool bGoldThumbNull = GT == nullptr || GT->IsNull();
			DPick.Exact(bGoldThumbNull == !R.Thumb.bPlaced, "thumb placed");
			if (!bGoldThumbNull && R.Thumb.bPlaced)
			{
				DPick.Exact((*GT)["cmc"].NumAt(0) == R.Thumb.Abd, "thumb abd");
				DPick.Exact((*GT)["cmc"].NumAt(1) == R.Thumb.Flex, "thumb flex");
				DPick.Exact((*GT)["mcp"].Number() == R.Thumb.Mcp, "thumb mcp");
				DPick.Exact((*GT)["ip"].Number() == R.Thumb.Ip, "thumb ip");
				DNum.Num((*GT)["gaps"].NumAt(0), R.Thumb.Gaps[0], "thumb gap1");
				DNum.Num((*GT)["gaps"].NumAt(1), R.Thumb.Gaps[1], "thumb gap2");
				DPick.Exact((*GT)["far"].B == R.Thumb.bFar, "thumb far");
				DPick.Exact((*GT)["low"].B == R.Thumb.bLow, "thumb low");
			}

			DHand.Xf(Gold["hand_in_weapon"], R.HandInWeapon, "hand_in_weapon");
			const JValue& GL = Gold["locals"];
			size_t NumLocals = 0;
			for (int B = 1; B < Su.Hand.NumBones(); ++B)
			{
				NumLocals += R.Locs.Has(B) ? 1 : 0;
			}
			DLoc.Exact(GL.Obj.size() == NumLocals, "locals count python " + std::to_string(GL.Obj.size()) + " cpp " + std::to_string(NumLocals));
			for (const auto& KV : GL.Obj)
			{
				const int B = Su.Hand.BoneIndex(KV.first);
				const bool bHas = B >= 0 && R.Locs.Has(B);
				DLoc.Exact(bHas, KV.first);
				if (bHas)
				{
					const Q& Rot = R.Locs.L[B].q;
					const double Quat[4] = {Rot.x, Rot.y, Rot.z, Rot.w};
					for (int K = 0; K < 4; ++K)
					{
						DLoc.Num(KV.second.NumAt(static_cast<size_t>(K)), Quat[K], KV.first);
					}
				}
			}

			Checksum Sum;
			Sum.Add(R.P, 6);
			Sum.Add(R.Palm);
			Sum.Add(R.IndexPhi);
			Sum.Add(R.IndexA, 3);
			Sum.Add(R.IndexErr);
			Sum.Add(R.IndexFace);
			Sum.Add(R.IndexPen);
			Sum.Add(R.IndexUnnatural);
			Sum.Add(R.IndexCost);
			for (const TriggerFinger& Fg : R.Fingers)
			{
				Sum.Add(Fg.bPlaced ? 1.0 : 0.0);
				Sum.Add(Fg.Score);
				Sum.Add(Fg.Phi);
				Sum.Add(Fg.A, 3);
				Sum.Add(Fg.Gaps, 3);
				Sum.Add(Fg.Adj, 3);
			}
			Sum.Add(R.Thumb.Score);
			Sum.Add(R.Thumb.Abd);
			Sum.Add(R.Thumb.Flex);
			Sum.Add(R.Thumb.Mcp);
			Sum.Add(R.Thumb.Ip);
			Sum.Add(R.Thumb.Gaps, 2);
			Sum.Add(R.Thumb.bFar ? 1.0 : 0.0);
			Sum.Add(R.Thumb.bLow ? 1.0 : 0.0);
			const double Hf[7] = {R.HandInWeapon.t.x, R.HandInWeapon.t.y, R.HandInWeapon.t.z,
			                      R.HandInWeapon.q.x, R.HandInWeapon.q.y, R.HandInWeapon.q.z, R.HandInWeapon.q.w};
			Sum.Add(Hf, 7);
			for (int B = 1; B < Su.Hand.NumBones(); ++B)
			{
				if (R.Locs.Has(B))
				{
					const Q& Rot = R.Locs.L[B].q;
					const double Quat[4] = {Rot.x, Rot.y, Rot.z, Rot.w};
					Sum.Add(Quat, 4);
				}
			}
			std::printf("solve checksum %016llx\n", static_cast<unsigned long long>(Sum.H));

			if (!A.DumpPath.empty())
			{
				const std::string Json = TriggerSolutionJson(Su, R);
				JValue Back;
				if (!ParseJson(Json, Back, Err))
				{
					std::printf("ERROR the solution JSON does not parse back: %s\n", Err.c_str());
					return false;
				}
				std::FILE* F = std::fopen(A.DumpPath.c_str(), "wb");
				if (F)
				{
					std::fwrite(Json.data(), 1, Json.size(), F);
					std::fclose(F);
					std::printf("solve: JSON written to %s\n", A.DumpPath.c_str());
				}
			}
			return Report({DHead, DIdx, DPick, DNum, DHand, DLoc}, "solve");
		}
	}

	int RunTrigger(int Argc, char** Argv)
	{
		Args A;
		if (!ParseArgs(Argc, Argv, A))
		{
			std::printf("usage: ngtest trigger placenat|solve|all [golden dir] [-saved dir] [-dump solve.json]\n");
			return 2;
		}
		std::unique_ptr<Setup> Su;
		if (!LoadM16Setup(A, Su))
		{
			return 2;
		}
		const char* Threads = std::getenv("NG_THREADS");
		std::printf("trigger: M16 right hand, thin 1, fine field loaded, NG_THREADS=%s\n", Threads ? Threads : "(all)");
		const TriggerSettings Set;
		bool bOk = true;
		if (A.Mode == "placenat" || A.Mode == "all")
		{
			bOk = RunPlaceNat(*Su, Set, A) && bOk;
		}
		if (A.Mode == "solve" || A.Mode == "all")
		{
			bOk = RunSolve(*Su, Set, A) && bOk;
		}
		if (A.Mode == "all")
		{
			std::printf("trigger all: %s\n", bOk ? "PASS" : "FAIL");
		}
		return bOk ? 0 : 1;
	}
}
