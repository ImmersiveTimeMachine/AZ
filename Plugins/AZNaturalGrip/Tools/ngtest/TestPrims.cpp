// Copyright Artur. AZ project.
// Primitive parity: the C++ core against py/dump_prims.py output for the same random inputs.
#include "NGTest.h"

#include "NGSetup.h"

#include <cstdio>
#include <memory>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		int FingerOf(const std::string& Name)
		{
			static const char* Names[NumFingers] = {"thumb", "index", "middle", "ring", "pinky"};
			for (int F = 0; F < NumFingers; ++F)
			{
				if (Name == Names[F])
				{
					return F;
				}
			}
			return -1;
		}

		void CompareSegs(Diff& D, const JValue& Py, const Seg S[3], const std::string& At)
		{
			for (int I = 0; I < 3; ++I)
			{
				const JValue& P = Py.At(I);
				D.Num(P.NumAt(0), S[I].A.x, At); D.Num(P.NumAt(1), S[I].A.y, At); D.Num(P.NumAt(2), S[I].A.z, At);
				D.Num(P.NumAt(3), S[I].B.x, At); D.Num(P.NumAt(4), S[I].B.y, At); D.Num(P.NumAt(5), S[I].B.z, At);
				D.Num(P.NumAt(6), S[I].R, At);
			}
		}
	}

	int RunPrims(const std::string& DumpPath)
	{
		JValue J;
		std::string Err;
		if (!LoadJsonFile(DumpPath, J, Err))
		{
			std::printf("ERROR %s\n", Err.c_str());
			return 2;
		}
		const JValue& St = J["setting"];
		SetupConfig Cfg;
		Cfg.Side = St["side"].S[0];
		Cfg.Weapon = St["weapon"].S;
		Cfg.Thin = static_cast<int>(St["thin"].Number());
		Cfg.bFine = St["fine"].B;
		Cfg.LeftPick = St["left_pick"].S;
		auto Su = std::make_unique<Setup>();
		if (!Su->Load(Cfg, Err))
		{
			std::printf("ERROR setup: %s\n", Err.c_str());
			return 2;
		}
		const HandModel& Hm = Su->Hand;
		std::printf("prims %s: side %c weapon %s thin %d fine %d (loaded %d / python %d)\n", DumpPath.c_str(), Cfg.Side,
		            Cfg.Weapon.c_str(), Cfg.Thin, Cfg.bFine ? 1 : 0, Su->bFineLoaded ? 1 : 0, St["fine_loaded"].B ? 1 : 0);

		std::vector<Diff> Out;

		// skin binding
		{
			Diff D("skin bind");
			const JValue& B = J["bind"];
			D.Exact(B.Size() == Hm.Bind.size(), "count");
			for (size_t I = 0; I < B.Size() && I < Hm.Bind.size(); ++I)
			{
				const JValue& E = B.At(I);
				const std::string At = "vertex " + std::to_string(I);
				D.Exact(E.At(0).S == Hm.Bones[Hm.Bind[I].Bone], At);
				D.Num(E.At(1).Number(), Hm.Bind[I].Off.x, At);
				D.Num(E.At(2).Number(), Hm.Bind[I].Off.y, At);
				D.Num(E.At(3).Number(), Hm.Bind[I].Off.z, At);
			}
			for (const auto& KV : J["group_sizes"].Obj)
			{
				const int Bn = Hm.BoneIndex(KV.first);
				D.Exact(Bn >= 0 && static_cast<double>(Hm.Group[Bn].size()) == KV.second.Number(), "group " + KV.first);
			}
			Out.push_back(D);
		}
		// placement frame
		{
			Diff D("pivot + palm samples");
			D.Vec(J["pivot"], Su->Plc.Pivot, "pivot");
			const JValue& P = J["palm"];
			D.Exact(P.Size() == Su->Plc.Palm.size(), "palm count");
			for (size_t I = 0; I < P.Size() && I < Su->Plc.Palm.size(); ++I)
			{
				const JValue& E = P.At(I);
				D.Exact(E.At(0).S == Hm.Bones[Su->Plc.Palm[I].first], "palm " + std::to_string(I));
				D.Num(E.At(1).Number(), Su->Plc.Palm[I].second.x);
				D.Num(E.At(2).Number(), Su->Plc.Palm[I].second.y);
				D.Num(E.At(3).Number(), Su->Plc.Palm[I].second.z);
			}
			Out.push_back(D);
		}
		// field
		{
			Diff Df("field (fine/coarse)"), Dc("field (coarse)");
			const JValue& F = J["field"];
			for (size_t I = 0; I < F.Size(); ++I)
			{
				const JValue& E = F.At(I);
				const double X0 = E.NumAt(0), Y0 = E.NumAt(1), Z0 = E.NumAt(2);
				Df.Num(E.NumAt(3), Su->Fld.Sample(X0, Y0, Z0), "point " + std::to_string(I));
				Dc.Num(E.NumAt(4), Su->Fld.CoarseSample(X0, Y0, Z0), "point " + std::to_string(I));
			}
			Out.push_back(Df);
			Out.push_back(Dc);
		}
		// placements
		std::vector<X> Hands;
		{
			Diff D("placement corr/hand/palm");
			const JValue& P = J["place"];
			for (size_t I = 0; I < P.Size(); ++I)
			{
				const JValue& E = P.At(I);
				const JValue& Pv = E["p"];
				const double Pp[6] = {Pv.NumAt(0), Pv.NumAt(1), Pv.NumAt(2), Pv.NumAt(3), Pv.NumAt(4), Pv.NumAt(5)};
				const X C = Su->Plc.Corr(Pp);
				const std::string At = "placement " + std::to_string(I);
				D.Xf(E["C"], C, At);
				D.Xf(E["hand"], Su->Plc.HandNew(C), At);
				D.Num(E["palm"].Number(), Su->Plc.PalmWorst(C), At);
				Hands.push_back(F7Of(E["hand"]));     // the Python hand feeds the finger tests (isolates each check)
			}
			Out.push_back(D);
		}
		// fingers
		{
			Diff Dw("finger FK"), Dp("finger pens/gaps"), Dd("finger pad + capsules");
			const JValue& Fs = J["fingers"];
			Pose W;
			for (size_t I = 0; I < Fs.Size(); ++I)
			{
				const JValue& E = Fs.At(I);
				const int F = FingerOf(E["f"].S);
				const JValue& A = E["a"];
				Locals L;
				L.Init(Hm.Bones.size());
				Hm.FingerLocals(F, E["cup"].Number(), E["phi"].Number(), A.NumAt(0), A.NumAt(1), A.NumAt(2), L);
				Hm.World(Hands[static_cast<size_t>(E["pi"].Number())], L, W);
				const std::string At = "finger " + std::to_string(I);
				for (const auto& KV : E["W"].Obj)
				{
					Dw.Xf(KV.second, W[Hm.BoneIndex(KV.first)], At + " " + KV.first);
				}
				double P[3];
				Hm.Pens(W, F, P);
				for (int K = 0; K < 3; ++K)
				{
					Dp.Num(E["pens"].NumAt(K), P[K], At);
					Dp.Num(E["gaps"].NumAt(K), Hm.LinkGap(W, Hm.Chain[F][K + 1]), At);
				}
				Dd.Vec(E["pad"], Hm.PadPoint(W, F), At);
				Seg S[3];
				Hm.Segs(W, F, S);
				CompareSegs(Dd, E["segs"], S, At);
			}
			Out.push_back(Dw);
			Out.push_back(Dp);
			Out.push_back(Dd);
		}
		// thumbs
		{
			Diff Dw("thumb FK"), Dp("thumb pens/gaps/capsules");
			const JValue& Ts = J["thumbs"];
			Pose W;
			for (size_t I = 0; I < Ts.Size(); ++I)
			{
				const JValue& E = Ts.At(I);
				Locals L;
				L.Init(Hm.Bones.size());
				Hm.ThumbLocals(Su->Plc.Clip, E["m"].Number(), E["ip"].Number(), true, E["cmc"].NumAt(0), E["cmc"].NumAt(1), L);
				Hm.World(Hands[static_cast<size_t>(E["pi"].Number())], L, W);
				const std::string At = "thumb " + std::to_string(I);
				for (const auto& KV : E["W"].Obj)
				{
					Dw.Xf(KV.second, W[Hm.BoneIndex(KV.first)], At + " " + KV.first);
				}
				for (int K = 0; K < 3; ++K)
				{
					Dp.Num(E["pen"].NumAt(K), Hm.LinkPen(W, Hm.Chain[Thumb][K]), At);
					Dp.Num(E["gap"].NumAt(K), Hm.LinkGap(W, Hm.Chain[Thumb][K]), At);
				}
				Seg S[3];
				Hm.Segs(W, Thumb, S);
				CompareSegs(Dp, E["segs"], S, At);
			}
			Out.push_back(Dw);
			Out.push_back(Dp);
		}
		// segment distance and overlap
		{
			Diff Ds("seg_seg"), Do("finger_overlap");
			const JValue& Ss = J["segseg"];
			for (size_t I = 0; I < Ss.Size(); ++I)
			{
				const JValue& E = Ss.At(I);
				V3 P[4];
				for (int K = 0; K < 4; ++K)
				{
					const JValue& V = E.At(static_cast<size_t>(K));
					P[K] = V3(V.NumAt(0), V.NumAt(1), V.NumAt(2));
				}
				Ds.Num(E.At(4).Number(), SegSeg(P[0], P[1], P[2], P[3]), "pair " + std::to_string(I));
			}
			const JValue& Fs = J["fingers"];
			auto SegsOf = [&](size_t I, Seg S[3])
			{
				const JValue& Py = Fs.At(I)["segs"];
				for (int K = 0; K < 3; ++K)
				{
					const JValue& P = Py.At(static_cast<size_t>(K));
					S[K] = {V3(P.NumAt(0), P.NumAt(1), P.NumAt(2)), V3(P.NumAt(3), P.NumAt(4), P.NumAt(5)), P.NumAt(6)};
				}
			};
			const JValue& Ov = J["overlap"];
			for (size_t I = 0; I < Ov.Size(); ++I)
			{
				const JValue& E = Ov.At(I);
				Seg A[3], B[3];
				SegsOf(static_cast<size_t>(E.NumAt(0)), A);
				SegsOf(static_cast<size_t>(E.NumAt(1)), B);
				const std::vector<Seg> Others(B, B + 3);
				Do.Num(E.NumAt(2), FingerOverlap(A, Others), "pair " + std::to_string(I));
			}
			Out.push_back(Ds);
			Out.push_back(Do);
		}

		const double Tol = 1e-9;
		bool bOk = true;
		for (const Diff& D : Out)
		{
			D.Print(Tol);
			bOk = bOk && D.Ok(Tol);
		}
		std::printf("prims: %s\n", bOk ? "PASS" : "FAIL");
		return bOk ? 0 : 1;
	}
}
