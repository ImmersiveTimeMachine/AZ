// Copyright Artur. AZ project.
#include "NGHand.h"

namespace ng
{
	namespace
	{
		const V3 ZFlex(0.0, 0.0, -1.0);   // flexion axis (bone local)
		const V3 YAbd(0.0, 1.0, 0.0);     // MCP side axis, + = toward the index side

		double SegDist(const V3& P, const V3& A, const V3& B)
		{
			const V3 Ab = Sub(B, A);
			const double Den = Dot(Ab, Ab);
			const double T = Clamp01Py(Dot(Sub(P, A), Ab) / (Den > 1e-9 ? Den : 1e-9));
			return Length(Sub(P, Add(A, Mul(Ab, T))));
		}

		const char* FingerName(int F)
		{
			static const char* Names[NumFingers] = {"thumb", "index", "middle", "ring", "pinky"};
			return Names[F];
		}
	}

	int HandModel::BoneIndex(const std::string& Name) const
	{
		for (int I = 0; I < NumBones(); ++I)
		{
			if (Bones[I] == Name)
			{
				return I;
			}
		}
		return -1;
	}

	void HandModel::BuildSkin(const std::vector<V3>& VertsHandSpace, int Thin)
	{
		const std::string S(1, Side);
		for (int F = 0; F < NumFingers; ++F)
		{
			Chain[F].clear();
			if (F != Thumb)
			{
				Chain[F].push_back(BoneIndex(std::string(FingerName(F)) + "_metacarpal_" + S));
			}
			for (int K = 1; K <= 3; ++K)
			{
				Chain[F].push_back(BoneIndex(std::string(FingerName(F)) + "_0" + std::to_string(K) + "_" + S));
			}
		}

		// bone segments in hand space (skin.SEGS order: per finger its chain, then the hand's wrist-to-base segment)
		struct FSeg
		{
			int Bone;
			V3 A, B;
		};
		std::vector<FSeg> Segments;
		for (int F = 0; F < NumFingers; ++F)
		{
			const std::vector<int>& Ch = Chain[F];
			for (size_t I = 0; I < Ch.size(); ++I)
			{
				const int B = Ch[I];
				const V3 A = BCS[B].t;
				const V3 E = I + 1 < Ch.size() ? BCS[Ch[I + 1]].t : BCS[B].Pos(Mul(MRef[B].t, 0.9));
				Segments.push_back({B, A, E});
			}
			Segments.push_back({0, V3(0.0, 0.0, 0.0), BCS[Ch[0]].t});
		}

		Bind.clear();
		Bind.reserve(VertsHandSpace.size());
		for (const V3& V : VertsHandSpace)
		{
			size_t Best = 0;
			double BestD = SegDist(V, Segments[0].A, Segments[0].B);
			for (size_t I = 1; I < Segments.size(); ++I)
			{
				const double D = SegDist(V, Segments[I].A, Segments[I].B);
				if (D < BestD)                      // min(): the first of equal keys wins
				{
					BestD = D;
					Best = I;
				}
			}
			const int B = Segments[Best].Bone;
			Bind.push_back({B, BCS[B].IPos(V)});
		}

		std::vector<std::vector<V3>> All(Bones.size());
		for (const FBind& Bd : Bind)
		{
			All[Bd.Bone].push_back(Bd.Off);
		}
		Group.assign(Bones.size(), {});
		const size_t Step = Thin > 0 ? static_cast<size_t>(Thin) : 1;
		for (size_t B = 0; B < All.size(); ++B)
		{
			for (size_t I = 0; I < All[B].size(); I += Step)
			{
				Group[B].push_back(All[B][I]);
			}
		}
	}

	void HandModel::World(const X& Hand, const Locals& Locs, Pose& W) const
	{
		W.resize(Bones.size());
		W[0] = Hand;
		for (int B = 1; B < NumBones(); ++B)
		{
			W[B] = (Locs.Has(B) ? Locs.L[B] : MRef[B]) * W[Parent[B]];
		}
	}

	void HandModel::FK(const X& Hand, const Locals& Full, Pose& W) const
	{
		W.resize(Bones.size());
		W[0] = Hand;
		for (int B = 1; B < NumBones(); ++B)
		{
			W[B] = Full.L[B] * W[Parent[B]];
		}
	}

	void HandModel::FingerLocals(int F, double CupDeg, double Phi, double A0, double A1, double A2, Locals& Out) const
	{
		const std::vector<int>& Ch = Chain[F];
		const double* R = RefAbs[F];
		const int M = Ch[0], P1 = Ch[1], P2 = Ch[2], P3 = Ch[3];
		Out.Put(M, X(QMul(MRef[M].q, QDeg(ZFlex, CupDeg)), MRef[M].t));
		Out.Put(P1, X(QMul(QMul(MRef[P1].q, QDeg(YAbd, Phi)), QDeg(ZFlex, A0 - R[0])), MRef[P1].t));
		Out.Put(P2, X(QMul(MRef[P2].q, QDeg(ZFlex, A1 - R[1])), MRef[P2].t));
		Out.Put(P3, X(QMul(MRef[P3].q, QDeg(ZFlex, A2 - R[2])), MRef[P3].t));
	}

	void HandModel::ThumbLocals(const Locals& Base, double A1, double A2, bool bCmc, double CmcAbd, double CmcFlex, Locals& Out) const
	{
		const int T1 = Chain[Thumb][0], T2 = Chain[Thumb][1], T3 = Chain[Thumb][2];
		Q Q1 = Base.L[T1].q;
		if (bCmc)
		{
			Q1 = QMul(QMul(Q1, QDeg(YAbd, CmcAbd)), QDeg(ZFlex, CmcFlex));
		}
		Out.Put(T1, X(Q1, MRef[T1].t));
		Out.Put(T2, X(QMul(MRef[T2].q, QDeg(ZFlex, A1)), MRef[T2].t));
		Out.Put(T3, X(QMul(MRef[T3].q, QDeg(ZFlex, A2)), MRef[T3].t));
	}

	double HandModel::LinkPen(const Pose& W, int Bone) const
	{
		const X& Wb = W[Bone];
		double Worst = -9.0;
		for (const V3& O : Group[Bone])
		{
			const double D = Fld->Sample(Wb.Pos(O));
			if (-D > Worst)
			{
				Worst = -D;
			}
		}
		return Worst - Tol;
	}

	double HandModel::LinkGap(const Pose& W, int Bone) const
	{
		const X& Wb = W[Bone];
		const std::vector<V3>& G = Group[Bone];
		if (G.empty())
		{
			return Fld->Sample(Wb.Pos(V3(0.0, 0.0, 0.0)));
		}
		double Best = Fld->Sample(Wb.Pos(G[0]));
		for (size_t I = 1; I < G.size(); ++I)
		{
			const double D = Fld->Sample(Wb.Pos(G[I]));
			if (D < Best)
			{
				Best = D;
			}
		}
		return Best;
	}

	void HandModel::Pens(const Pose& W, int F, double Out[3]) const
	{
		for (int K = 0; K < 3; ++K)
		{
			Out[K] = LinkPen(W, Chain[F][K + 1]);
		}
	}

	double HandModel::MaxPen(const Pose& W, int F) const
	{
		double P[3];
		Pens(W, F, P);
		double M = P[0];
		if (P[1] > M) M = P[1];
		if (P[2] > M) M = P[2];
		return M;
	}

	V3 HandModel::PadPoint(const Pose& W, int F) const
	{
		const int B3 = LastBone(F);
		return W[B3].Pos(Add(Mul(MRef[B3].t, 0.55), V3(0.0, 0.35, 0.0)));
	}

	void HandModel::Segs(const Pose& W, int F, Seg Out[3]) const
	{
		const std::vector<int>& Ch = Chain[F];
		const size_t First = F == Thumb ? 0 : 1;
		V3 Pts[4];
		int N = 0;
		for (size_t I = First; I < Ch.size(); ++I)
		{
			Pts[N++] = W[Ch[I]].t;
		}
		const int Last = Ch.back();
		Pts[N++] = W[Last].Pos(Mul(MRef[Last].t, 0.9));
		for (int I = 0; I < 3; ++I)
		{
			Out[I] = {Pts[I], Pts[I + 1], Rad[F][I < 2 ? I : 2]};
		}
	}

	std::vector<Capsule> HandCapsules(const HandModel& H, const X& Hand, const Locals& Locs)
	{
		Pose W;
		H.World(Hand, Locs, W);
		std::vector<Capsule> Out;
		for (int F = Thumb; F <= Pinky; ++F)
		{
			Seg S[3];
			H.Segs(W, F, S);
			for (const Seg& Sg : S)
			{
				Out.push_back({Sg.A, Sg.B, Sg.R});
			}
			if (F != Thumb)
			{
				// the palm: wrist -> knuckle along each metacarpal, ~1.4 cm half thickness
				Out.push_back({W[0].t, W[H.Chain[F][1]].t, 1.4});
			}
		}
		return Out;
	}

	double SegSeg(const V3& P1, const V3& Q1, const V3& P2, const V3& Q2)
	{
		const V3 D1 = Sub(Q1, P1), D2 = Sub(Q2, P2), R = Sub(P1, P2);
		const double A = Dot(D1, D1), E = Dot(D2, D2), F = Dot(D2, R);
		const double C = Dot(D1, R), B = Dot(D1, D2);
		const double Den = A * E - B * B;
		double S = Den > 1e-9 ? Clamp01Py((B * F - C * E) / Den) : 0.0;
		double T = E > 1e-9 ? (B * S + F) / E : 0.0;
		if (T < 0)
		{
			T = 0.0;
			S = Clamp01Py(-C / A);
		}
		else if (T > 1)
		{
			T = 1.0;
			S = Clamp01Py((B - C) / A);
		}
		return Length(Sub(Add(P1, Mul(D1, S)), Add(P2, Mul(D2, T))));
	}

	double FingerOverlap(const Seg Sa[3], const std::vector<Seg>& Others)
	{
		double Worst = 0.0;
		for (int I = 0; I < 3; ++I)
		{
			for (const Seg& O : Others)
			{
				const double V = (Sa[I].R + O.R - 0.25) - SegSeg(Sa[I].A, Sa[I].B, O.A, O.B);
				if (V > Worst)
				{
					Worst = V;
				}
			}
		}
		return Worst;
	}
}
