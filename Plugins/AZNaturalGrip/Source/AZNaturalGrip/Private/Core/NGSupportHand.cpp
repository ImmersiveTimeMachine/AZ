// Copyright Artur. AZ project.
#include "NGSupportHand.h"

#include "NGOptim.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <functional>

namespace ng
{
	namespace
	{
		const char* FingerName(int F)
		{
			static const char* Names[NumFingers] = {"thumb", "index", "middle", "ring", "pinky"};
			return Names[F];
		}

		/** Python max(0.0, X): 0.0 unless X > 0.0. */
		inline double PyMax0(double V) { return V > 0.0 ? V : 0.0; }

		/** CPython float_rem: Python's floored modulo (the result has the sign of the divisor). */
		double PyFloorMod(double A, double B)
		{
			double Mod = std::fmod(A, B);
			if (Mod != 0.0)
			{
				if ((B < 0.0) != (Mod < 0.0))
				{
					Mod += B;
				}
			}
			else
			{
				Mod = std::copysign(0.0, B);
			}
			return Mod;
		}

		/** Python x ** 2 = CRT pow(x, 2.0). The exponent is read through a volatile so the compiler cannot fold it to x * x. */
		double PyPow2(double V)
		{
			static volatile double Two = 2.0;
			return std::pow(V, Two);
		}

		/** Python sum() of floats (CPython >= 3.12: Neumaier compensated summation, start value int 0). */
		double PySum(const double* V, size_t Count)
		{
			if (Count == 0)
			{
				return 0.0;
			}
			double Result = 0.0 + V[0];
			double Comp = 0.0;
			for (size_t I = 1; I < Count; ++I)
			{
				const double Item = V[I];
				const double T = Result + Item;
				if (std::fabs(Result) >= std::fabs(Item))
				{
					Comp += (Result - T) + Item;
				}
				else
				{
					Comp += (Item - T) + Result;
				}
				Result = T;
			}
			if (Comp != 0.0 && std::isfinite(Comp))
			{
				Result += Comp;
			}
			return Result;
		}

		/** lsolve_m16.angle. */
		double AngleDeg(const V3& A, const V3& B)
		{
			const double D = Dot(Norm(A), Norm(B));
			const double Mn = D < 1.0 ? D : 1.0;          // min(1.0, d)
			const double Mx = Mn > -1.0 ? Mn : -1.0;      // max(-1.0, ...)
			return Degrees(std::acos(Mx));
		}

		/** lsolve_m16.bend from the hand's ref-pose world pose (world(hand, {})). */
		double BendOf(const HandModel& H, const X& Hand, const Pose& WRef, const V3& Elbow)
		{
			return AngleDeg(Sub(Hand.t, Elbow), Sub(WRef[H.Chain[Middle][1]].t, Hand.t));
		}

		/** lsolve_m16.around: angle of a point around the handguard axis. */
		double AroundDeg(const SupportSettings& St, const V3& P)
		{
			if (St.bGeneralAxis)
			{
				const V3 V = Sub(P, St.AxisOrigin);
				return Degrees(std::atan2(Dot(V, St.AxisE2), Dot(V, St.AxisE1)));
			}
			return Degrees(std::atan2(P.z - St.AxZ, P.x - St.AxX));   // the handguard along +Y (lsolve_m16.around)
		}

		/** lsolve_m16.palm_angle. */
		double PalmAngleOf(const Setup& Su, const SupportSettings& St, const X& Hand)
		{
			const HandModel& H = Su.Hand;
			Locals None;
			None.Init(H.Bones.size());
			Pose W;
			H.World(Hand, None, W);
			V3 C(0.0, 0.0, 0.0);
			for (const auto& BO : Su.Plc.Palm)
			{
				C = Add(C, W[BO.first].Pos(BO.second));
			}
			return AroundDeg(St, Mul(C, 1.0 / static_cast<double>(Su.Plc.Palm.size())));
		}

		/** lsolve_m16.wrap_deg. */
		double WrapDeg(const HandModel& H, const SupportSettings& St, const Pose& W, int F, double Pa)
		{
			const int B3 = H.LastBone(F);
			const double D = AroundDeg(St, W[B3].Pos(Mul(H.MRef[B3].t, 0.6))) - Pa;
			return std::fabs(PyFloorMod(D + 180.0, 360.0) - 180.0);
		}

		/** The flexion-coupled metacarpal cup of finger_W / the solver: CUP[f] * max(0, a0) / 90. */
		inline double CupOf(const HandModel& H, int F, double A0)
		{
			return H.Cup[F] * PyMax0(A0) / 90.0;
		}

		struct FingerBest
		{
			bool bHas = false;
			double S = 0.0;
			double A[3] = {0.0, 0.0, 0.0};
			double Phi = 0.0;
			double G[3] = {0.0, 0.0, 0.0};
			bool bGG = false;
			double GG[3] = {0.0, 0.0, 0.0};
			double Wr = 0.0;
		};

		/** finger_opt's context for one finger: the hand, the fingers placed so far, the previous finger's capsules (adjacency)
		 *  and the other placed fingers' capsules (overlap). */
		struct FingerCtx
		{
			const Setup* Su = nullptr;
			const SupportSettings* St = nullptr;
			X Hand;
			int F = 0;
			bool bHasPs = false;
			Seg Ps[3];
			std::vector<Seg> Others;
			double Pa = 0.0;
		};

		FingerCtx MakeFingerCtx(const Setup& Su, const SupportSettings& St, const X& Hand, const Locals& Locs, int F, int PrevF, double Pa)
		{
			const HandModel& H = Su.Hand;
			FingerCtx C;
			C.Su = &Su;
			C.St = &St;
			C.Hand = Hand;
			C.F = F;
			C.Pa = Pa;
			Pose Wp;
			H.World(Hand, Locs, Wp);
			C.bHasPs = PrevF >= 0;
			if (C.bHasPs)
			{
				H.Segs(Wp, PrevF, C.Ps);
			}
			for (int G = Index; G <= Pinky; ++G)
			{
				if (G != F && G != PrevF
					&& (Locs.Has(H.Chain[G][1]) || Locs.Has(H.Chain[G][2]) || Locs.Has(H.Chain[G][3])))
				{
					Seg Sg[3];
					H.Segs(Wp, G, Sg);
					C.Others.push_back(Sg[0]);
					C.Others.push_back(Sg[1]);
					C.Others.push_back(Sg[2]);
				}
			}
			return C;
		}

		/** One configuration of finger_opt's loop body (phi, MCP, PIP, DIP ratio). L = a copy of the placed fingers' locals
		 *  (the finger's own bones are overwritten), W = scratch. False = rejected (penetration, overlap, adjacency). */
		bool EvalFingerCfg(const FingerCtx& C, double Phi, double A0, double A1, double R, Locals& L, Pose& W, FingerBest& Out)
		{
			const HandModel& H = C.Su->Hand;
			const int F = C.F;
			const std::vector<int>& Links = H.Chain[F];      // [1..3] = the three phalanges
			const double Ra = R * A1;
			const double A2 = Ra < H.LimDip[1] ? Ra : H.LimDip[1];     // min(LIM_ABS["dip"][1], r * a1)
			H.FingerLocals(F, CupOf(H, F, A0), Phi, A0, A1, A2, L);
			H.World(C.Hand, L, W);
			if (H.MaxPen(W, F) > 0)
			{
				return false;
			}
			Seg Sg[3];
			H.Segs(W, F, Sg);
			if (!C.Others.empty() && FingerOverlap(Sg, C.Others) > 0)
			{
				return false;
			}
			double Adj = 0.0;
			bool bGG = false;
			double GG[3] = {0.0, 0.0, 0.0};
			if (C.bHasPs)
			{
				const Seg* Ps = C.Ps;
				const double Gp = SegSeg(Sg[0].A, Sg[0].B, Ps[0].A, Ps[0].B) - Sg[0].R - Ps[0].R;
				const double Gm = SegSeg(Sg[1].A, Sg[1].B, Ps[1].A, Ps[1].B) - Sg[1].R - Ps[1].R;
				const double Gd = SegSeg(Sg[2].A, Sg[2].B, Ps[2].A, Ps[2].B) - Sg[2].R - Ps[2].R;
				double Mn = Gp;                                        // min(gp, gm, gd)
				if (Gm < Mn) Mn = Gm;
				if (Gd < Mn) Mn = Gd;
				if (Mn < -0.05)
				{
					return false;
				}
				Adj = (PyPow2(Gm - 0.15) + 0.5 * PyPow2(Gd - 0.15)) * 6.0;
				bGG = true;
				GG[0] = Gp;
				GG[1] = Gm;
				GG[2] = Gd;
			}
			double G[3];
			for (int K = 0; K < 3; ++K)
			{
				G[K] = H.LinkGap(W, Links[K + 1]);
			}
			const double Terms[3] = {C.St->RestW[0] * PyMax0(G[0] - C.St->Contact), C.St->RestW[1] * PyMax0(G[1] - C.St->Contact),
			                         C.St->RestW[2] * PyMax0(G[2] - C.St->Contact)};
			const double Rest = PySum(Terms, 3);
			const double Wr = WrapDeg(H, *C.St, W, F, C.Pa);
			// natural closing: no hook (MCP straight under a bent PIP) and no flat knuckle-only bend
			const double Nat = PyPow2((R - 0.65) / 0.15) * 0.5 + PyPow2(PyMax0(0.5 * A1 - A0) / 15.0)
				+ PyPow2(PyMax0(A0 - A1 - 15.0) / 12.0);
			const double WrCap = C.St->WrapCap < Wr ? C.St->WrapCap : Wr;      // min(wr, 160.0)
			Out.bHas = true;
			Out.S = -3.0 * Rest - Adj - std::fabs(Phi) / 30.0 - Nat + WrCap / C.St->WrapDiv;
			Out.A[0] = A0;
			Out.A[1] = A1;
			Out.A[2] = A2;
			Out.Phi = Phi;
			Out.G[0] = G[0];
			Out.G[1] = G[1];
			Out.G[2] = G[2];
			Out.bGG = bGG;
			Out.GG[0] = GG[0];
			Out.GG[1] = GG[1];
			Out.GG[2] = GG[2];
			Out.Wr = Wr;
			return true;
		}

		/** One phi of finger_opt's loops: the best (s, a, phi, g, gg, wr) in the Python loop order, strict '>'. */
		FingerBest FingerOptPhi(const FingerCtx& C, const Locals& Locs, double Phi, double Step, const std::vector<double>& Ratios,
		                        const Exec& Ex)
		{
			const HandModel& H = C.Su->Hand;
			FingerBest Best;
			Locals L = Locs;                                  // dict(locs); the finger's own bones are overwritten each time
			Pose W;
			double A0 = H.LimMcp[0];
			while (A0 <= 90.0)
			{
				if (Ex.Cancelled())
				{
					return Best;
				}
				double A1 = 0.0;
				while (A1 <= 105.0)
				{
					for (const double R : Ratios)
					{
						FingerBest Cand;
						if (EvalFingerCfg(C, Phi, A0, A1, R, L, W, Cand) && (!Best.bHas || Cand.S > Best.S))
						{
							Best = Cand;
						}
					}
					A1 += Step;
				}
				A0 += Step;
			}
			return Best;
		}

		/** lsolve_m16.finger_opt: parallel over phi, reduced in phi order with strict '>' (= Python's phi-outermost loop). */
		FingerBest FingerOpt(const Setup& Su, const SupportSettings& St, const X& Hand, const Locals& Locs, int F, int PrevF,
		                     double Pa, const std::vector<double>& Phis, double Step, const std::vector<double>& Ratios,
		                     const Exec& Ex)
		{
			const FingerCtx C = MakeFingerCtx(Su, St, Hand, Locs, F, PrevF, Pa);
			std::vector<FingerBest> Slots(Phis.size());
			Ex.For(static_cast<int>(Phis.size()), [&](int I)
			{
				Slots[static_cast<size_t>(I)] = FingerOptPhi(C, Locs, Phis[static_cast<size_t>(I)], Step, Ratios, Ex);
			});
			FingerBest Best;
			for (const FingerBest& S : Slots)
			{
				if (S.bHas && (!Best.bHas || S.S > Best.S))
				{
					Best = S;
				}
			}
			return Best;
		}

		/** One configuration of left_solve.thumb_left's loop body (CMC side, CMC flex, MCP, IP = k * MCP). */
		bool EvalThumbCfg(const Setup& Su, const X& Hand, const std::vector<Seg>& Others, double Abd, double Fl, double M, double K,
		                  Locals& Th, Pose& W, SupportThumb& Out)
		{
			const HandModel& H = Su.Hand;
			const std::vector<int>& T = H.Chain[Thumb];
			const double Ip = K * M;
			H.ThumbLocals(Su.Plc.Clip, M, Ip, true, Abd, Fl, Th);
			H.World(Hand, Th, W);
			double P = H.LinkPen(W, T[0]) - 0.3;                  // max(a, b, c)
			const double P1 = H.LinkPen(W, T[1]);
			const double P2 = H.LinkPen(W, T[2]);
			if (P1 > P) P = P1;
			if (P2 > P) P = P2;
			Seg Sg[3];
			H.Segs(W, Thumb, Sg);
			if (P > 0 || FingerOverlap(Sg, Others) > 0)
			{
				return false;
			}
			const double G3 = H.LinkGap(W, T[2]);
			const double G2 = H.LinkGap(W, T[1]);
			const V3 T2 = W[T[1]].t;
			const V3 T3 = W[T[2]].t;
			const double Fwd = Norm(Sub(T3, T2))[1];                // along the forend (+y)
			double S = -std::fabs(G3 - 0.05) * 5.0;
			S = S - PyMax0(G2 - 0.4) * 1.0;
			S = S + 1.0 * Fwd;
			S = S - (Abd * Abd + Fl * Fl) / 3000.0;
			S = S - ((M - 20) * (M - 20)) / 3000.0;
			Out.bValid = true;
			Out.Score = S;
			Out.Abd = Abd;
			Out.Fl = Fl;
			Out.M = M;
			Out.Ip = Ip;
			Out.PadGap = G3;
			return true;
		}

		/** One abd value of left_solve.thumb_left: the best over (fl, m, k) in the Python loop order. */
		SupportThumb ThumbAbd(const Setup& Su, const X& Hand, const std::vector<Seg>& Others, int Abd)
		{
			const HandModel& H = Su.Hand;
			static const double Ks[3] = {0.6, 0.8, 1.0};
			SupportThumb Best;
			Locals Th;
			Th.Init(H.Bones.size());
			Pose W;
			for (int Fl = -40; Fl <= 40; Fl += 10)
			{
				for (int M = 0; M <= 60; M += 5)
				{
					for (const double K : Ks)
					{
						SupportThumb Cand;
						if (EvalThumbCfg(Su, Hand, Others, static_cast<double>(Abd), static_cast<double>(Fl), static_cast<double>(M), K, Th, W, Cand)
							&& (!Best.bValid || Cand.Score > Best.Score))
						{
							Best = Cand;
						}
					}
				}
			}
			return Best;
		}

		// ---------------------------------------------------------------- part B: coarse grid + pattern search

		/** Picks up to MaxSeeds best candidates that are pairwise more than one coarse cell apart. */
		template <typename TCand, typename TScore, typename TFar>
		std::vector<TCand> PickSeeds(std::vector<TCand> All, int MaxSeeds, TScore Score, TFar Far)
		{
			std::stable_sort(All.begin(), All.end(), [&Score](const TCand& A, const TCand& B) { return Score(A) > Score(B); });
			std::vector<TCand> Seeds;
			for (const TCand& C : All)
			{
				bool bFar = true;
				for (const TCand& S : Seeds)
				{
					bFar = bFar && Far(C, S);
				}
				if (bFar)
				{
					Seeds.push_back(C);
					if (static_cast<int>(Seeds.size()) >= MaxSeeds)
					{
						break;
					}
				}
			}
			return Seeds;
		}

		struct FingerCand
		{
			double Xv[4] = {0, 0, 0, 0};   // phi, MCP, PIP, DIP ratio
			FingerBest B;
		};

		FingerBest FingerFast(const Setup& Su, const SupportSettings& St, const X& Hand, const Locals& Locs, int F, int PrevF, double Pa,
		                      const SupportFastOptions& O, const Exec& Ex, std::atomic<long long>& Evals)
		{
			const HandModel& H = Su.Hand;
			const FingerCtx C = MakeFingerCtx(Su, St, Hand, Locs, F, PrevF, Pa);
			if (O.bPolishGrid)
			{
				// the exhaustive grid's best (= part A), then the pattern search from it: never worse than A
				FingerBest Grid = FingerOpt(Su, St, Hand, Locs, F, PrevF, Pa, O.GridPhis, O.GridStep, O.GridRatios, Ex);
				Evals += static_cast<long long>(O.GridPhis.size()) * 36 * 36 * static_cast<long long>(O.GridRatios.size());
				if (!Grid.bHas)
				{
					return Grid;
				}
				PatternOptions P;
				P.Lo = {O.PhiLo, H.LimMcp[0], 0.0, O.RatioLo};
				P.Hi = {O.PhiHi, 90.0, 105.0, O.RatioHi};
				P.Scale = {1.0, 1.0, 1.0, 0.01};
				P.Steps = O.PolishSteps;
				P.Pairs = {{1, 2}, {2, 3}};
				Locals L = Locs;
				Pose W;
				const double A1 = Grid.A[1];
				std::vector<double> Xv = {Grid.Phi, Grid.A[0], A1, A1 > 0.0 ? Grid.A[2] / A1 : 0.65};
				// the grid's DIP is min(80, r * PIP); recover its ratio exactly from the grid list when PIP > 0
				for (const double R : O.GridRatios)
				{
					const double Ra = R * A1;
					if ((Ra < H.LimDip[1] ? Ra : H.LimDip[1]) == Grid.A[2])
					{
						Xv[3] = R;
						break;
					}
				}
				long long N = 0;
				const ScoreFn Fn = [&](const double* V, double& S)
				{
					FingerBest Fb;
					if (!EvalFingerCfg(C, V[0], V[1], V[2], V[3], L, W, Fb))
					{
						return false;
					}
					S = Fb.S;
					return true;
				};
				const double S0 = Grid.S;
				const double S1 = PatternMaximize(Fn, Xv, S0, P, N);
				Evals += N;
				if (S1 > S0)
				{
					FingerBest Fb;
					EvalFingerCfg(C, Xv[0], Xv[1], Xv[2], Xv[3], L, W, Fb);
					return Fb;
				}
				return Grid;
			}
			// 1. coarse grid (parallel over phi), every allowed configuration kept
			std::vector<std::vector<FingerCand>> Slots(O.CoarsePhis.size());
			Ex.For(static_cast<int>(O.CoarsePhis.size()), [&](int I)
			{
				Locals L = Locs;
				Pose W;
				long long N = 0;
				const double Phi = O.CoarsePhis[static_cast<size_t>(I)];
				for (double A0 = H.LimMcp[0]; A0 <= 90.0 + 1e-9; A0 += O.CoarseStep)
				{
					for (double A1 = 0.0; A1 <= 105.0 + 1e-9; A1 += O.CoarseStep)
					{
						for (const double R : O.CoarseRatios)
						{
							FingerCand Cd;
							++N;
							if (EvalFingerCfg(C, Phi, A0, A1, R, L, W, Cd.B))
							{
								Cd.Xv[0] = Phi;
								Cd.Xv[1] = A0;
								Cd.Xv[2] = A1;
								Cd.Xv[3] = R;
								Slots[static_cast<size_t>(I)].push_back(Cd);
							}
						}
					}
				}
				Evals += N;
			});
			std::vector<FingerCand> All;
			for (const std::vector<FingerCand>& S : Slots)
			{
				All.insert(All.end(), S.begin(), S.end());
			}
			const double PhiStep = O.CoarsePhis.size() > 1 ? std::fabs(O.CoarsePhis[1] - O.CoarsePhis[0]) : 5.0;
			const std::vector<FingerCand> Seeds = PickSeeds(All, O.Seeds, [](const FingerCand& Cd) { return Cd.B.S; },
				[&](const FingerCand& A, const FingerCand& B)
				{
					return std::fabs(A.Xv[0] - B.Xv[0]) > PhiStep || std::fabs(A.Xv[1] - B.Xv[1]) > O.CoarseStep
						|| std::fabs(A.Xv[2] - B.Xv[2]) > O.CoarseStep || std::fabs(A.Xv[3] - B.Xv[3]) > 0.15 + 1e-9;
				});
			// 2. pattern search from every seed (parallel over the seeds), the same score
			PatternOptions P;
			P.Lo = {O.PhiLo, H.LimMcp[0], 0.0, O.RatioLo};
			P.Hi = {O.PhiHi, 90.0, 105.0, O.RatioHi};
			P.Scale = {1.0, 1.0, 1.0, 0.01};
			P.Steps = O.Steps;
			P.Pairs = {{1, 2}, {2, 3}};
			std::vector<FingerBest> Refined(Seeds.size());
			Ex.For(static_cast<int>(Seeds.size()), [&](int I)
			{
				Locals L = Locs;
				Pose W;
				long long N = 0;
				const FingerCand& Sd = Seeds[static_cast<size_t>(I)];
				std::vector<double> Xv(Sd.Xv, Sd.Xv + 4);
				const ScoreFn Fn = [&](const double* V, double& S)
				{
					FingerBest Fb;
					if (!EvalFingerCfg(C, V[0], V[1], V[2], V[3], L, W, Fb))
					{
						return false;
					}
					S = Fb.S;
					return true;
				};
				PatternMaximize(Fn, Xv, Sd.B.S, P, N);
				FingerBest Fb;
				EvalFingerCfg(C, Xv[0], Xv[1], Xv[2], Xv[3], L, W, Fb);
				Refined[static_cast<size_t>(I)] = Fb;
				Evals += N + 1;
			});
			FingerBest Best;
			for (const FingerBest& B : Refined)
			{
				if (B.bHas && (!Best.bHas || B.S > Best.S))
				{
					Best = B;
				}
			}
			return Best;
		}

		void ThumbFast(const Setup& Su, const X& Hand, const std::vector<Seg>& Others, const SupportFastOptions& O, const Exec& Ex,
		               std::atomic<long long>& Evals, SupportThumb& Out)
		{
			const HandModel& H = Su.Hand;
			if (O.bPolishGrid)
			{
				SupportThumb Grid;
				SupportThumbLeft(Su, Hand, Others, Ex, Grid);
				Evals += 9 * 9 * 13 * 3;
				Out = Grid;
				if (!Grid.bValid)
				{
					return;
				}
				PatternOptions P;
				P.Lo = {-40.0, -40.0, 0.0, 0.6};
				P.Hi = {40.0, 40.0, 60.0, 1.0};
				P.Scale = {1.0, 1.0, 1.0, 0.01};
				P.Steps = O.ThumbPolishSteps;
				Locals Th;
				Th.Init(H.Bones.size());
				Pose W;
				std::vector<double> Xv = {Grid.Abd, Grid.Fl, Grid.M, Grid.M != 0.0 ? Grid.Ip / Grid.M : 0.8};
				long long N = 0;
				const ScoreFn Fn = [&](const double* V, double& S)
				{
					SupportThumb Tb;
					if (!EvalThumbCfg(Su, Hand, Others, V[0], V[1], V[2], V[3], Th, W, Tb))
					{
						return false;
					}
					S = Tb.Score;
					return true;
				};
				const double S1 = PatternMaximize(Fn, Xv, Grid.Score, P, N);
				Evals += N;
				if (S1 > Grid.Score)
				{
					EvalThumbCfg(Su, Hand, Others, Xv[0], Xv[1], Xv[2], Xv[3], Th, W, Out);
				}
				return;
			}
			std::vector<std::vector<SupportThumb>> Slots(O.ThumbAbd.size());
			Ex.For(static_cast<int>(O.ThumbAbd.size()), [&](int I)
			{
				Locals Th;
				Th.Init(H.Bones.size());
				Pose W;
				long long N = 0;
				for (const double Fl : O.ThumbFlex)
				{
					for (const double M : O.ThumbMcp)
					{
						for (const double K : O.ThumbIpRatio)
						{
							SupportThumb Cd;
							++N;
							if (EvalThumbCfg(Su, Hand, Others, O.ThumbAbd[static_cast<size_t>(I)], Fl, M, K, Th, W, Cd))
							{
								Slots[static_cast<size_t>(I)].push_back(Cd);
							}
						}
					}
				}
				Evals += N;
			});
			std::vector<SupportThumb> All;
			for (const std::vector<SupportThumb>& S : Slots)
			{
				All.insert(All.end(), S.begin(), S.end());
			}
			const std::vector<SupportThumb> Seeds = PickSeeds(All, O.ThumbSeeds, [](const SupportThumb& T) { return T.Score; },
				[](const SupportThumb& A, const SupportThumb& B)
				{
					return std::fabs(A.Abd - B.Abd) > 20.0 || std::fabs(A.Fl - B.Fl) > 20.0 || std::fabs(A.M - B.M) > 10.0
						|| std::fabs(A.Ip / (A.M != 0.0 ? A.M : 1.0) - B.Ip / (B.M != 0.0 ? B.M : 1.0)) > 0.2 + 1e-9;
				});
			PatternOptions P;
			P.Lo = {-40.0, -40.0, 0.0, 0.6};
			P.Hi = {40.0, 40.0, 60.0, 1.0};
			P.Scale = {1.0, 1.0, 1.0, 0.01};
			P.Steps = O.ThumbSteps;
			std::vector<SupportThumb> Refined(Seeds.size());
			Ex.For(static_cast<int>(Seeds.size()), [&](int I)
			{
				Locals Th;
				Th.Init(H.Bones.size());
				Pose W;
				long long N = 0;
				const SupportThumb& Sd = Seeds[static_cast<size_t>(I)];
				std::vector<double> Xv = {Sd.Abd, Sd.Fl, Sd.M, Sd.M != 0.0 ? Sd.Ip / Sd.M : 0.8};
				const ScoreFn Fn = [&](const double* V, double& S)
				{
					SupportThumb Tb;
					if (!EvalThumbCfg(Su, Hand, Others, V[0], V[1], V[2], V[3], Th, W, Tb))
					{
						return false;
					}
					S = Tb.Score;
					return true;
				};
				PatternMaximize(Fn, Xv, Sd.Score, P, N);
				SupportThumb Tb;
				EvalThumbCfg(Su, Hand, Others, Xv[0], Xv[1], Xv[2], Xv[3], Th, W, Tb);
				Refined[static_cast<size_t>(I)] = Tb;
				Evals += N + 1;
			});
			Out = SupportThumb();
			for (const SupportThumb& T : Refined)
			{
				if (T.bValid && (!Out.bValid || T.Score > Out.Score))
				{
					Out = T;
				}
			}
		}

		using FingerSearchFn = std::function<FingerBest(const Locals& /*Locs*/, int /*F*/, int /*PrevF*/, double /*Pa*/)>;
		using ThumbSearchFn = std::function<void(const std::vector<Seg>& /*Others*/, SupportThumb& /*Out*/)>;

		/** solve_fingers with a pluggable per-finger / thumb search (A: the exhaustive grids, B: grid + pattern search). */
		void SolveFingersWith(const Setup& Su, const SupportSettings& St, const X& Hand, const FingerSearchFn& FingerSearch,
		                      const ThumbSearchFn& ThumbSearch, const Exec& Ex, SupportSolve& Out)
		{
			const HandModel& H = Su.Hand;
			Out = SupportSolve();
			Out.Locs.Init(H.Bones.size());
			const double Pa = PalmAngleOf(Su, St, Hand);
			double Total = 0.0;
			int Prev = -1;
			for (int F = Index; F <= Pinky; ++F)
			{
				const FingerBest B = FingerSearch(Out.Locs, F, Prev, Pa);
				if (Ex.Cancelled())
				{
					return;
				}
				Ex.Report("support fingers", static_cast<double>(F - Index + 1) / 4.0);
				SupportFinger& Rec = Out.Finger[F];
				if (!B.bHas)
				{
					Rec = SupportFinger();
					Total -= 6.0;
					continue;
				}
				Rec.bValid = true;
				Rec.Phi = B.Phi;
				Rec.A[0] = B.A[0];
				Rec.A[1] = B.A[1];
				Rec.A[2] = B.A[2];
				Rec.Gaps[0] = B.G[0];
				Rec.Gaps[1] = B.G[1];
				Rec.Gaps[2] = B.G[2];
				Rec.bHasAdj = B.bGG;
				Rec.Adj[0] = B.GG[0];
				Rec.Adj[1] = B.GG[1];
				Rec.Adj[2] = B.GG[2];
				Rec.Wrap = B.Wr;
				Rec.Score = B.S;
				Total += B.S;
				H.FingerLocals(F, CupOf(H, F, B.A[0]), B.Phi, B.A[0], B.A[1], B.A[2], Out.Locs);
				Prev = F;
			}
			Pose W;
			H.World(Hand, Out.Locs, W);
			std::vector<Seg> Others;
			for (int G = Index; G <= Pinky; ++G)
			{
				if (Out.Locs.Has(H.Chain[G][1]) || Out.Locs.Has(H.Chain[G][2]) || Out.Locs.Has(H.Chain[G][3]))
				{
					Seg Sg[3];
					H.Segs(W, G, Sg);
					Others.push_back(Sg[0]);
					Others.push_back(Sg[1]);
					Others.push_back(Sg[2]);
				}
			}
			ThumbSearch(Others, Out.Thumb);
			if (Out.Thumb.bValid)
			{
				const SupportThumb& Tb = Out.Thumb;
				H.ThumbLocals(Su.Plc.Clip, Tb.M, Tb.Ip, true, Tb.Abd, Tb.Fl, Out.Locs);
				Total += -3.0 * std::fabs(Tb.PadGap - 0.05);
			}
			else
			{
				Total -= 4.0;
			}
			Out.Total = Total;
			Out.PalmAngle = Pa;
		}

		/** finger_gaps.skin_gap: the closest skin-to-skin distance of two fingers' phalanges. */
		void SkinGap(const HandModel& H, const Pose& W, int Fa, int Fb, SupportGapRow& Row)
		{
			auto Collect = [&](int F, std::vector<std::pair<int, V3>>& Pts)
			{
				const std::vector<int>& Ch = H.Chain[F];
				size_t Count = 0;
				for (const HandModel::FBind& B : H.Bind)
				{
					if (B.Bone == Ch[1] || B.Bone == Ch[2] || B.Bone == Ch[3])
					{
						if (Count % 2 == 0)                                    // [::2]
						{
							Pts.emplace_back(B.Bone, W[B.Bone].Pos(B.Off));
						}
						++Count;
					}
				}
			};
			std::vector<std::pair<int, V3>> Pa, Pb;
			Collect(Fa, Pa);
			Collect(Fb, Pb);
			double Best = 9.0;
			int Ba = -1, Bb = -1;
			for (const auto& A : Pa)
			{
				for (const auto& B : Pb)
				{
					const double D = Length(Sub(A.second, B.second));
					if (D < Best)
					{
						Best = D;
						Ba = A.first;
						Bb = B.first;
					}
				}
			}
			Row.Skin = Best;
			Row.BoneA = Ba;
			Row.BoneB = Bb;
		}
	}

	bool LoadSupportElbows(const std::string& SavedDir, const std::string& Weapon, SupportSettings& Out, std::string& Error)
	{
		JValue J;
		if (!LoadJsonFile(SavedDir + "/" + Weapon + "_left_picks.json", J, Error))
		{
			return false;
		}
		const char* Keys[2] = {"aim", "relaxed"};
		V3* Dst[2] = {&Out.ElbowAim, &Out.ElbowRelaxed};
		for (int K = 0; K < 2; ++K)
		{
			const JValue* Pick = J.Find(Keys[K]);
			const JValue* Elb = Pick ? Pick->Find("elbow") : nullptr;
			if (!Elb || !Elb->IsArray() || Elb->Size() != 3)
			{
				Error = std::string("left picks: no ") + Keys[K] + " elbow";
				return false;
			}
			*Dst[K] = V3(Elb->NumAt(0), Elb->NumAt(1), Elb->NumAt(2));
		}
		return true;
	}

	bool SupportStage1Eval(const Setup& Su, const SupportSettings& St, const double P[6], SupportPlacement& Out)
	{
		const HandModel& H = Su.Hand;
		const X C = Su.Plc.Corr(P);
		const X Hand = Su.Plc.HandNew(C);
		const double Palm = Su.Plc.PalmWorst(C);
		if (Palm < -0.1 || Palm > 0.6)
		{
			return false;
		}
		Locals None;
		None.Init(H.Bones.size());
		Pose W;
		H.World(Hand, None, W);
		const double Ba = BendOf(H, Hand, W, St.ElbowAim);
		const double Br = BendOf(H, Hand, W, St.ElbowRelaxed);
		const double Sq0[3] = {P[0] * P[0], P[1] * P[1], P[2] * P[2]};
		const double Sq1[3] = {P[3] * P[3], P[4] * P[4], P[5] * P[5]};
		const double Dev = std::sqrt(PySum(Sq0, 3)) / 10.0 + std::sqrt(PySum(Sq1, 3));
		const double J = Dev + PyMax0(Ba - 25.0) / 5.0 + PyMax0(Br - 30.0) / 5.0 + std::fabs(Palm - 0.15) * 3.0;
		for (int K = 0; K < 6; ++K)
		{
			Out.P[K] = P[K];
		}
		Out.Palm = Palm;
		Out.Bend[0] = Ba;
		Out.Bend[1] = Br;
		Out.Dev = Dev;
		Out.J1 = J;
		return true;
	}

	void SupportStage1(const Setup& Su, const SupportSettings& St, const Exec& Ex, std::vector<SupportPlacement>& Out)
	{
		static const double AxYawPitch[5] = {-20.0, -10.0, 0.0, 10.0, 20.0};
		static const double AxRoll[5] = {-30.0, -15.0, 0.0, 15.0, 30.0};
		static const double AxShift[5] = {-2.0, -1.0, 0.0, 1.0, 2.0};
		const int Num = 5 * 5 * 5 * 5 * 5 * 5;
		std::vector<SupportPlacement> Slots(static_cast<size_t>(Num));
		std::vector<char> Valid(static_cast<size_t>(Num), 0);
		std::atomic<int> Done{0};
		Ex.For(Num, [&](int I)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			int Idx[6];
			int Rem = I;                                   // itertools.product: the last axis varies fastest
			for (int K = 5; K >= 0; --K)
			{
				Idx[K] = Rem % 5;
				Rem /= 5;
			}
			const double P[6] = {AxYawPitch[Idx[0]], AxYawPitch[Idx[1]], AxRoll[Idx[2]], AxShift[Idx[3]], AxShift[Idx[4]], AxShift[Idx[5]]};
			Valid[static_cast<size_t>(I)] = SupportStage1Eval(Su, St, P, Slots[static_cast<size_t>(I)]) ? 1 : 0;
			const int N = Done.fetch_add(1) + 1;
			if (N % 1024 == 0)
			{
				Ex.Report("support stage 1", static_cast<double>(N) / static_cast<double>(Num));
			}
		});
		Out.clear();
		for (int I = 0; I < Num; ++I)
		{
			if (Valid[static_cast<size_t>(I)])
			{
				Out.push_back(Slots[static_cast<size_t>(I)]);
			}
		}
		// list.sort(key=J1) is stable: equal J1 keep the grid order
		std::stable_sort(Out.begin(), Out.end(), [](const SupportPlacement& A, const SupportPlacement& B) { return A.J1 < B.J1; });
	}

	void SupportThumbLeft(const Setup& Su, const X& Hand, const std::vector<Seg>& Others, const Exec& Ex, SupportThumb& Out)
	{
		const int NumAbd = 9;                              // range(-40, 41, 10)
		std::vector<SupportThumb> Slots(static_cast<size_t>(NumAbd));
		Ex.For(NumAbd, [&](int I)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			Slots[static_cast<size_t>(I)] = ThumbAbd(Su, Hand, Others, -40 + 10 * I);
		});
		Out = SupportThumb();
		for (const SupportThumb& S : Slots)
		{
			if (S.bValid && (!Out.bValid || S.Score > Out.Score))
			{
				Out = S;
			}
		}
	}

	void SolveSupportFingers(const Setup& Su, const SupportSettings& St, const X& Hand, const std::vector<double>& Phis,
	                         double Step, const std::vector<double>& Ratios, const Exec& Ex, SupportSolve& Out)
	{
		SolveFingersWith(Su, St, Hand,
			[&](const Locals& Locs, int F, int PrevF, double Pa) { return FingerOpt(Su, St, Hand, Locs, F, PrevF, Pa, Phis, Step, Ratios, Ex); },
			[&](const std::vector<Seg>& Others, SupportThumb& Tb) { SupportThumbLeft(Su, Hand, Others, Ex, Tb); },
			Ex, Out);
	}

	void SolveSupportFingersFast(const Setup& Su, const SupportSettings& St, const X& Hand, const SupportFastOptions& O, const Exec& Ex,
	                             SupportSolve& Out, long long* OutEvals)
	{
		std::atomic<long long> Evals{0};
		SolveFingersWith(Su, St, Hand,
			[&](const Locals& Locs, int F, int PrevF, double Pa) { return FingerFast(Su, St, Hand, Locs, F, PrevF, Pa, O, Ex, Evals); },
			[&](const std::vector<Seg>& Others, SupportThumb& Tb) { ThumbFast(Su, Hand, Others, O, Ex, Evals, Tb); },
			Ex, Out);
		if (OutEvals)
		{
			*OutEvals = Evals.load();
		}
	}

	void SupportStage2(const Setup& Su, const SupportSettings& St, const std::vector<SupportPlacement>& Cands,
	                   const std::vector<double>& Phis, double Step, const std::vector<double>& Ratios, const Exec& Ex,
	                   std::vector<SupportStage2Result>& Out)
	{
		Out.assign(Cands.size(), SupportStage2Result());
		Exec Inner = Ex;                                   // the candidates are the parallel axis; each solve runs serially
		Inner.For = SerialFor;
		Inner.Progress = nullptr;
		std::atomic<int> Done{0};
		const int Num = static_cast<int>(Cands.size());
		Ex.For(Num, [&](int I)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			SupportStage2Result& R = Out[static_cast<size_t>(I)];
			R.Cand = Cands[static_cast<size_t>(I)];
			const X Hand = Su.Plc.HandNew(Su.Plc.Corr(R.Cand.P));
			SolveSupportFingers(Su, St, Hand, Phis, Step, Ratios, Inner, R.Solve);
			R.J2 = R.Cand.J1 - R.Solve.Total;
			Ex.Report("support stage 2", static_cast<double>(Done.fetch_add(1) + 1) / static_cast<double>(Num));
		});
	}

	namespace
	{
		void FinishFinal(const Setup& Su, const SupportSettings& St, const double P[6], const X& C, const X& Hand, SupportFinalResult& Out)
		{
			const HandModel& H = Su.Hand;
			for (int K = 0; K < 6; ++K)
			{
				Out.P[K] = P[K];
			}
			Out.Palm = Su.Plc.PalmWorst(C);
			Locals None;
			None.Init(H.Bones.size());
			Pose W;
			H.World(Hand, None, W);
			Out.Bend[0] = BendOf(H, Hand, W, St.ElbowAim);
			Out.Bend[1] = BendOf(H, Hand, W, St.ElbowRelaxed);
			Out.HandInWeapon = Hand;
		}
	}

	void SolveSupportFinal(const Setup& Su, const SupportSettings& St, const double P[6], const std::vector<double>& Phis,
	                       double Step, const std::vector<double>& Ratios, const Exec& Ex, SupportFinalResult& Out)
	{
		Out = SupportFinalResult();
		const X C = Su.Plc.Corr(P);
		const X Hand = Su.Plc.HandNew(C);
		SolveSupportFingers(Su, St, Hand, Phis, Step, Ratios, Ex, Out.Solve);
		FinishFinal(Su, St, P, C, Hand, Out);
	}

	void SolveSupportFinalFast(const Setup& Su, const SupportSettings& St, const double P[6], const SupportFastOptions& O, const Exec& Ex,
	                           SupportFinalResult& Out, long long* OutEvals)
	{
		Out = SupportFinalResult();
		const X C = Su.Plc.Corr(P);
		const X Hand = Su.Plc.HandNew(C);
		SolveSupportFingersFast(Su, St, Hand, O, Ex, Out.Solve, OutEvals);
		FinishFinal(Su, St, P, C, Hand, Out);
	}

	// ================================================================== power grasp (support hand)

	namespace
	{
		/** -180..180 */
		double WrapPm180(double D)
		{
			return PyFloorMod(D + 180.0, 360.0) - 180.0;
		}

		/** The power placement: place2.corr from the clip hold, then a roll Theta about the handguard axis. */
		X PowerHandOf(const Setup& Su, const SupportSettings& St, const double P[7])
		{
			const X Hand = Su.Plc.HandNew(Su.Plc.Corr(P));
			const Q Rot = QAxis(St.Dir(), Radians(P[6]));
			const V3 Cc = St.Origin();
			return Hand * X(Rot, Sub(Cc, QRot(Rot, Cc)));
		}

		double PalmWorstHand(const Setup& Su, const X& Hand)
		{
			Pose W;
			Su.Hand.FK(Hand, Su.Plc.Clip, W);
			double Best = 0.0;
			bool bFirst = true;
			for (const auto& BO : Su.Plc.Palm)
			{
				const double D = Su.Hand.Fld->Sample(W[BO.first].Pos(BO.second));
				if (bFirst || D < Best)
				{
					Best = D;
					bFirst = false;
				}
			}
			return Best;
		}

		/** Forearm twist (pronation / supination) of the new hand vs the clip hold, about the aim elbow -> wrist axis. */
		double TwistDeg(const Setup& Su, const SupportSettings& St, const X& Hand)
		{
			const Q Rel = QMul(Hand.q, QInv(Su.Plc.HandW.q));
			const V3 Axis = Norm(Sub(Su.Plc.HandW.t, St.ElbowAim));
			const double T = 2.0 * Degrees(std::atan2(Rel.x * Axis.x + Rel.y * Axis.y + Rel.z * Axis.z, Rel.w));
			return WrapPm180(T);
		}

		/** The fingers' pad angle around the handguard axis (circular mean of the placed fingers' tip capsules). */
		double FingerPadAngle(const SupportSettings& St, const std::vector<Seg>& Others)
		{
			double Sx = 0.0, Sy = 0.0;
			for (size_t I = 2; I < Others.size(); I += 3)
			{
				const double A = Radians(AroundDeg(St, Others[I].B));
				Sx += std::cos(A);
				Sy += std::sin(A);
			}
			return Degrees(std::atan2(Sy, Sx));
		}

		/** Power thumb: the pad resting on the handguard OPPOSITE the finger pads (it closes the grip), both phalanges near the
		 *  surface, a moderate CMC change. Oppose = the angle between the thumb pad and the finger pads around the axis. */
		/** The arc around the handguard axis the hand covers: from the finger pads through the palm to the thumb pad (deg).
		 *  > 180 = the handguard is enclosed (it cannot slip out without opening the hand); the open arc 360 - it stays on
		 *  the far side of the palm. Fingers and thumb on the same side of the palm = no enclosure (0). */
		double EnclosureDeg(double PalmAng, double FingerAng, double ThumbAng)
		{
			const double Df = WrapPm180(FingerAng - PalmAng);
			const double Dt = WrapPm180(ThumbAng - PalmAng);
			return Df * Dt < 0.0 ? std::fabs(Df) + std::fabs(Dt) : 0.0;
		}

		bool EvalThumbPowerCfg(const Setup& Su, const SupportSettings& St, const X& Hand, const std::vector<Seg>& Others, double FingerAng,
		                       double PalmAng, double Abd, double Fl, double M, double K, Locals& Th, Pose& W, SupportThumb& Out, double& Oppose)
		{
			const HandModel& H = Su.Hand;
			const std::vector<int>& T = H.Chain[Thumb];
			const double Ip = K * M;
			H.ThumbLocals(Su.Plc.Clip, M, Ip, true, Abd, Fl, Th);
			H.World(Hand, Th, W);
			double P = H.LinkPen(W, T[0]) - 0.3;
			const double P1 = H.LinkPen(W, T[1]);
			const double P2 = H.LinkPen(W, T[2]);
			if (P1 > P) P = P1;
			if (P2 > P) P = P2;
			Seg Sg[3];
			H.Segs(W, Thumb, Sg);
			if (P > 0 || FingerOverlap(Sg, Others) > 0)
			{
				return false;
			}
			const double G3 = H.LinkGap(W, T[2]);
			const double G2 = H.LinkGap(W, T[1]);
			const V3 Pad = W[T[2]].Pos(Mul(H.MRef[T[2]].t, 0.6));
			Oppose = EnclosureDeg(PalmAng, FingerAng, AroundDeg(St, Pad));
			// the pad's palm side = where the tip moves when the IP joint flexes (rotation about the bone's local -Z); it must
			// face the handguard axis: the thumb presses with its pad, not with its tip (PIE 2026-10-03: a curled thumb
			// stood on its tip - "стоит кончиком пальца на автомате")
			const V3 FlexAxis = W[T[2]].Vec(V3(0.0, 0.0, -1.0));
			const V3 PalmSide = Norm(Cross(FlexAxis, Sub(Pad, W[T[2]].t)));
			const V3 Ao = St.Origin(), Ad = St.Dir();
			const V3 ToAxis = Norm(Sub(Add(Ao, Mul(Ad, Dot(Sub(Pad, Ao), Ad))), Pad));
			const double Face = Dot(PalmSide, ToAxis);
			double S = -std::fabs(G3 - 0.05) * 5.0;
			S -= PyMax0(G2 - 0.12) * 6.0;                                   // the proximal phalanx lies on it too
			S += 2.0 * Face;
			// the thumb lies ALONG the handguard pointing forward (+Y, toward the muzzle), not across it (render P3: a thumb
			// standing up across the handguard "enclosed" 267 deg by reaching over the top)
			const double Fwd = Dot(Norm(Sub(W[T[2]].Pos(Mul(H.MRef[T[2]].t, 0.9)), W[T[1]].t)), St.ThumbDir());
			S += 2.0 * Fwd - PyPow2(PyMax0(0.75 - Fwd) / 0.15);
			S += (Oppose < 260.0 ? Oppose : 260.0) / 50.0 * St.ThumbOpposeW;   // closure: up to 260 deg of the handguard held
			S -= (Abd * Abd + Fl * Fl) / 4000.0;
			S -= PyPow2(PyMax0(M - 30.0) / 10.0) + PyPow2(PyMax0(Ip - 30.0) / 10.0);   // no curled (hooked) thumb
			Out.bValid = true;
			Out.Score = S;
			Out.Abd = Abd;
			Out.Fl = Fl;
			Out.M = M;
			Out.Ip = Ip;
			Out.PadGap = G3;
			return true;
		}

		void ThumbPower(const Setup& Su, const SupportSettings& St, const X& Hand, const std::vector<Seg>& Others, const SupportPowerOptions& O,
		                const Exec& Ex, SupportThumb& Out, double& OutOppose)
		{
			const HandModel& H = Su.Hand;
			const double FingerAng = FingerPadAngle(St, Others);
			const double PalmAng = PalmAngleOf(Su, St, Hand);
			struct FSlot
			{
				SupportThumb T;
				double Oppose = 0.0;
			};
			std::vector<FSlot> Slots(O.ThumbAbd.size());
			Ex.For(static_cast<int>(O.ThumbAbd.size()), [&](int I)
			{
				Locals Th;
				Th.Init(H.Bones.size());
				Pose W;
				FSlot& Best = Slots[static_cast<size_t>(I)];
				for (const double Fl : O.ThumbFlex)
				{
					for (const double M : O.ThumbMcp)
					{
						for (const double K : O.ThumbIpRatio)
						{
							SupportThumb Cd;
							double Op = 0.0;
							if (EvalThumbPowerCfg(Su, St, Hand, Others, FingerAng, PalmAng, O.ThumbAbd[static_cast<size_t>(I)], Fl, M, K, Th, W, Cd, Op)
								&& (!Best.T.bValid || Cd.Score > Best.T.Score))
							{
								Best.T = Cd;
								Best.Oppose = Op;
							}
						}
					}
				}
			});
			FSlot Best;
			for (const FSlot& Sl : Slots)
			{
				if (Sl.T.bValid && (!Best.T.bValid || Sl.T.Score > Best.T.Score))
				{
					Best = Sl;
				}
			}
			if (Best.T.bValid && O.bPolish)
			{
				PatternOptions P;
				P.Lo = {O.ThumbAbd.front(), O.ThumbFlex.front(), O.ThumbMcp.front(), O.ThumbIpRatio.front()};
				P.Hi = {O.ThumbAbd.back(), O.ThumbFlex.back(), O.ThumbMcp.back(), O.ThumbIpRatio.back()};
				P.Scale = {1.0, 1.0, 1.0, 0.01};
				P.Steps = {5.0, 2.5, 1.0, 0.5, 0.2};
				Locals Th;
				Th.Init(H.Bones.size());
				Pose W;
				std::vector<double> Xv = {Best.T.Abd, Best.T.Fl, Best.T.M, Best.T.M != 0.0 ? Best.T.Ip / Best.T.M : 0.8};
				long long N = 0;
				const ScoreFn Fn = [&](const double* V, double& S)
				{
					SupportThumb Tb;
					double Op = 0.0;
					if (!EvalThumbPowerCfg(Su, St, Hand, Others, FingerAng, PalmAng, V[0], V[1], V[2], V[3], Th, W, Tb, Op))
					{
						return false;
					}
					S = Tb.Score;
					return true;
				};
				if (PatternMaximize(Fn, Xv, Best.T.Score, P, N) > Best.T.Score)
				{
					EvalThumbPowerCfg(Su, St, Hand, Others, FingerAng, PalmAng, Xv[0], Xv[1], Xv[2], Xv[3], Th, W, Best.T, Best.Oppose);
				}
			}
			Out = Best.T;
			OutOppose = Best.Oppose;
		}
	}

	void SolveSupportFingersPower(const Setup& Su, const SupportSettings& St, const X& Hand, const SupportPowerOptions& O, const Exec& Ex,
	                              SupportSolve& Out, double* OutOppose)
	{
		SupportFastOptions F;
		F.bPolishGrid = true;
		F.GridPhis = O.Phis;
		F.GridStep = O.Step;
		F.GridRatios = O.Ratios;
		if (!O.bPolish)
		{
			F.PolishSteps.clear();
		}
		std::atomic<long long> Evals{0};
		double Oppose = 0.0;
		SolveFingersWith(Su, St, Hand,
			[&](const Locals& Locs, int Fi, int PrevF, double Pa) { return FingerFast(Su, St, Hand, Locs, Fi, PrevF, Pa, F, Ex, Evals); },
			[&](const std::vector<Seg>& Others, SupportThumb& Tb) { ThumbPower(Su, St, Hand, Others, O, Ex, Tb, Oppose); },
			Ex, Out);
		// the power total: every finger's score + the thumb's own score (closure counts as much as a finger)
		double Total = 0.0;
		for (int Fi = Index; Fi <= Pinky; ++Fi)
		{
			Total += Out.Finger[Fi].bValid ? Out.Finger[Fi].Score : -6.0;
		}
		Total += Out.Thumb.bValid ? Out.Thumb.Score : -6.0;
		Out.Total = Total;
		if (OutOppose)
		{
			*OutOppose = Oppose;
		}
	}

	void SupportPowerStage1(const Setup& Su, const SupportSettings& St, const SupportPowerOptions& O, const Exec& Ex, std::vector<PowerPlacement>& Out)
	{
		const HandModel& H = Su.Hand;
		std::vector<std::array<double, 7>> Grid;
		for (const double Th : O.Thetas)
			for (const double Yw : O.YawPitch)
				for (const double Pt : O.YawPitch)
					for (const double Rl : O.Roll)
						for (const double Dx : O.Shift)
							for (const double Dy : O.Shift)
								for (const double Dz : O.Shift)
								{
									Grid.push_back({Yw, Pt, Rl, Dx, Dy, Dz, Th});
								}
		std::vector<PowerPlacement> Slots(Grid.size());
		std::vector<char> Valid(Grid.size(), 0);
		Ex.For(static_cast<int>(Grid.size()), [&](int I)
		{
			const double* P = Grid[static_cast<size_t>(I)].data();
			const X Hand = PowerHandOf(Su, St, P);
			const double Palm = PalmWorstHand(Su, Hand);
			if (Palm < O.PalmLo || Palm > O.PalmHi)
			{
				return;
			}
			Locals None;
			None.Init(H.Bones.size());
			Pose W;
			H.World(Hand, None, W);
			PowerPlacement& R = Slots[static_cast<size_t>(I)];
			for (int K = 0; K < 7; ++K)
			{
				R.P[K] = P[K];
			}
			R.Palm = Palm;
			R.Bend[0] = BendOf(H, Hand, W, St.ElbowAim);
			R.Bend[1] = BendOf(H, Hand, W, St.ElbowRelaxed);
			R.Twist = TwistDeg(Su, St, Hand);
			R.PalmAngle = PalmAngleOf(Su, St, Hand);
			const double Dev = std::sqrt(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]) / 20.0 + std::sqrt(P[3] * P[3] + P[4] * P[4] + P[5] * P[5]) * 0.5;
			R.J1 = Dev + PyMax0(R.Bend[0] - 25.0) / 5.0 + PyMax0(R.Bend[1] - 30.0) / 5.0 + std::fabs(Palm - O.PalmTarget) * 3.0
				+ PyMax0(std::fabs(R.Twist) - O.TwistFree) / 5.0;
			Valid[static_cast<size_t>(I)] = 1;
		});
		Out.clear();
		for (size_t I = 0; I < Grid.size(); ++I)
		{
			if (Valid[I])
			{
				Out.push_back(Slots[I]);
			}
		}
		std::stable_sort(Out.begin(), Out.end(), [](const PowerPlacement& A, const PowerPlacement& B) { return A.J1 < B.J1; });
	}

	void SolveSupportPower(const Setup& Su, const SupportSettings& St, const double P[7], const SupportPowerOptions& O, const Exec& Ex,
	                       SupportFinalResult& Out, double& Oppose, double& Twist)
	{
		Out = SupportFinalResult();
		const X Hand = PowerHandOf(Su, St, P);
		SolveSupportFingersPower(Su, St, Hand, O, Ex, Out.Solve, &Oppose);
		for (int K = 0; K < 6; ++K)
		{
			Out.P[K] = P[K];
		}
		Out.Palm = PalmWorstHand(Su, Hand);
		Locals None;
		None.Init(Su.Hand.Bones.size());
		Pose W;
		Su.Hand.World(Hand, None, W);
		Out.Bend[0] = BendOf(Su.Hand, Hand, W, St.ElbowAim);
		Out.Bend[1] = BendOf(Su.Hand, Hand, W, St.ElbowRelaxed);
		Out.HandInWeapon = Hand;
		Twist = TwistDeg(Su, St, Hand);
	}

	void WriteSupportRecord(JWriter& W, const SupportSolve& S)
	{
		for (int F = Index; F <= Pinky; ++F)
		{
			const SupportFinger& R = S.Finger[F];
			W.Key(FingerName(F));
			if (!R.bValid)
			{
				W.Null();
				continue;
			}
			W.BeginObject();
			W.Key("phi").Value(R.Phi);
			W.Key("a").Numbers(R.A, 3);
			W.Key("gaps").Numbers(R.Gaps, 3);
			W.Key("adj");
			if (R.bHasAdj)
			{
				W.Numbers(R.Adj, 3);
			}
			else
			{
				W.Null();
			}
			W.Key("wrap").Value(R.Wrap);
			W.EndObject();
		}
		W.Key("thumb");
		if (!S.Thumb.bValid)
		{
			W.Null();
		}
		else
		{
			W.BeginObject();
			W.Key("cmc").BeginArray().Value(S.Thumb.Abd).Value(S.Thumb.Fl).EndArray();
			W.Key("mcp").Value(S.Thumb.M);
			W.Key("ip").Value(S.Thumb.Ip);
			W.Key("pad_gap").Value(S.Thumb.PadGap);
			W.EndObject();
		}
		W.Key("palm_angle").Value(S.PalmAngle);
	}

	std::string SupportFinalJson(const Setup& Su, const SupportFinalResult& R)
	{
		const HandModel& H = Su.Hand;
		JWriter W(true);
		W.BeginObject();
		W.Key("p").Numbers(R.P, 6);
		W.Key("F").Value(R.Solve.Total);
		W.Key("report").BeginObject();
		WriteSupportRecord(W, R.Solve);
		W.Key("palm").Value(R.Palm);
		W.Key("bend").Numbers(R.Bend, 2);
		W.EndObject();
		const double Hw[7] = {R.HandInWeapon.t.x, R.HandInWeapon.t.y, R.HandInWeapon.t.z,
		                      R.HandInWeapon.q.x, R.HandInWeapon.q.y, R.HandInWeapon.q.z, R.HandInWeapon.q.w};
		W.Key("hand_in_weapon").Numbers(Hw, 7);
		W.Key("locals").BeginObject();
		auto PutBone = [&](int Bone)
		{
			const Q& Qb = R.Solve.Locs.L[static_cast<size_t>(Bone)].q;
			const double Qv[4] = {Qb.x, Qb.y, Qb.z, Qb.w};
			W.Key(H.Bones[static_cast<size_t>(Bone)]).Numbers(Qv, 4);
		};
		for (int F = Index; F <= Pinky; ++F)               // the Python dict's insertion order
		{
			if (R.Solve.Finger[F].bValid)
			{
				for (const int Bone : H.Chain[F])
				{
					PutBone(Bone);
				}
			}
		}
		if (R.Solve.Thumb.bValid)
		{
			for (const int Bone : H.Chain[Thumb])
			{
				PutBone(Bone);
			}
		}
		W.EndObject();
		W.EndObject();
		return W.Str();
	}

	bool WriteSupportFinalJson(const Setup& Su, const SupportFinalResult& R, const std::string& Path)
	{
		const std::string Text = SupportFinalJson(Su, R);
		std::ofstream F(Path, std::ios::binary);
		F << Text;
		return static_cast<bool>(F);
	}

	void SupportFingerGaps(const Setup& Su, const X& Hand, const Locals& Locs, SupportGapReport& Out)
	{
		const HandModel& H = Su.Hand;
		Pose W;
		H.World(Hand, Locs, W);
		static const int Pairs[3][2] = {{Index, Middle}, {Middle, Ring}, {Ring, Pinky}};
		for (int K = 0; K < 3; ++K)
		{
			SupportGapRow& Row = Out.Row[K];
			Row = SupportGapRow();
			Row.FingerA = Pairs[K][0];
			Row.FingerB = Pairs[K][1];
			SkinGap(H, W, Row.FingerA, Row.FingerB, Row);
			Seg Sa[3], Sb[3];
			H.Segs(W, Row.FingerA, Sa);
			H.Segs(W, Row.FingerB, Sb);
			Row.Mid = SegSeg(Sa[1].A, Sa[1].B, Sb[1].A, Sb[1].B) - Sa[1].R - Sb[1].R;
			Row.Dist = SegSeg(Sa[2].A, Sa[2].B, Sb[2].A, Sb[2].B) - Sa[2].R - Sb[2].R;
		}
	}

	std::string FormatSupportGapReport(const std::string& Label, const SupportGapReport& R)
	{
		std::string Out = Label + "\n";
		for (const SupportGapRow& Row : R.Row)
		{
			char Buf[256];
			std::snprintf(Buf, sizeof(Buf), "    %s-%s: nearest skin %.2f cm, middle phalanges %.2f, distal %.2f\n",
			              FingerName(Row.FingerA), FingerName(Row.FingerB), Row.Skin, Row.Mid, Row.Dist);
			Out += Buf;
		}
		return Out;
	}
}
