// Copyright Artur. AZ project.
#include "NGTriggerHand.h"

#include "NGJson.h"

#include <cmath>

// Python semantics used below (so the picks match bit for bit):
//   min(a, b) returns a unless b < a;  max(a, b) returns a unless b > a   (written as ternaries)
//   x ** 2 -> PyPow(x, 2.0);  `best is None or s > best[0]` -> strict '>' (the first of equal scores wins)
namespace ng
{
	namespace
	{
		const V3 kYAxis(0.0, 1.0, 0.0);

		/** Python float ** : CPython calls the CRT pow, so call it too (the exponent is read through a volatile so the
		 *  compiler cannot fold pow(x, 2.0) into x * x, which can differ from the CRT's result in the last bit). */
		double PyPow(double Base, double Exponent)
		{
			volatile double Opaque = Exponent;
			return std::pow(Base, Opaque);
		}

		/** Python 3.12+ sum() of floats starting from the int 0: a Neumaier compensated sum, not a plain running sum. */
		double PySum(const double* V, int Count)
		{
			double F = 0.0 + V[0];
			double Comp = 0.0;
			for (int I = 1; I < Count; ++I)
			{
				const double Item = V[I];
				const double Tot = F + Item;
				if (std::fabs(F) >= std::fabs(Item))
				{
					Comp += (F - Tot) + Item;
				}
				else
				{
					Comp += (Item - Tot) + F;
				}
				F = Tot;
			}
			if (Comp != 0.0 && std::isfinite(Comp))
			{
				F += Comp;
			}
			return F;
		}

		/** any(b in locs for b in chain(f)[1:]) */
		bool HasAnyLink(const HandModel& H, const Locals& Locs, int F)
		{
			const std::vector<int>& Ch = H.Chain[F];
			return Locs.Has(Ch[1]) || Locs.Has(Ch[2]) || Locs.Has(Ch[3]);
		}

		/** The capsules of the placed fingers (index..pinky) other than Skip1 / Skip2, in finger order. */
		void CollectOthers(const HandModel& H, const Pose& W, const Locals& Locs, int Skip1, int Skip2, std::vector<Seg>& Out)
		{
			Out.clear();
			for (int G = Index; G <= Pinky; ++G)
			{
				if (G != Skip1 && G != Skip2 && HasAnyLink(H, Locs, G))
				{
					Seg Sg[3];
					H.Segs(W, G, Sg);
					Out.insert(Out.end(), Sg, Sg + 3);
				}
			}
		}

		// --- index_nat.py ---

		/** metrics with caller-provided scratch (the finger's locals are overwritten, nothing else is touched). */
		IndexMetrics MetricsWith(const HandModel& H, const TriggerSettings& Set, const X& HandW, const double Xv[4], Locals& L, Pose& W)
		{
			H.FingerLocals(Index, 0.0, Xv[0], Xv[1], Xv[2], Xv[3], L);
			H.World(HandW, L, W);
			IndexMetrics M;
			M.Err = Length(Sub(H.PadPoint(W, Index), Set.Trigger));
			const V3 Nrm = W[H.LastBone(Index)].Vec(kYAxis);   // palm side of the tip phalanx
			M.Face = Dot(Nrm, Set.Pull);
			M.Pen = H.MaxPen(W, Index);
			return M;
		}

		double CostOf(const IndexMetrics& M, const double Xv[4])
		{
			const double PenPos = M.Pen > 0.0 ? M.Pen : 0.0;   // max(0.0, pen)
			return PyPow(M.Err / 0.08, 2.0) + 60.0 * PyPow(PenPos, 2.0) * 100.0 + TriggerUnnatural(Xv) + 3.0 * (1.0 - M.Face);
		}

		// --- place_nat.py ---

		struct NatItem
		{
			double Xv[4];
			double U;
			V3 PadH;        // pad point in hand space (identity hand)
			V3 NormH;       // palm side of the tip phalanx in hand space
		};

		/** place_nat.natural_set(), built with the identity hand. */
		std::vector<NatItem> BuildNat(const HandModel& H)
		{
			std::vector<NatItem> Out;
			const X One;
			Locals L;
			L.Init(H.Bones.size());
			Pose W;
			const double Phis[6] = {-4.0, -2.0, 0.0, 2.0, 4.0, 6.0};
			const double Rs[3] = {0.55, 0.65, 0.75};
			for (int Pi = 0; Pi < 6; ++Pi)
			{
				for (int A0 = 10; A0 <= 50; A0 += 5)
				{
					for (int A1 = 35; A1 <= 85; A1 += 5)
					{
						for (int Ri = 0; Ri < 3; ++Ri)
						{
							NatItem It;
							It.Xv[0] = Phis[Pi];
							It.Xv[1] = static_cast<double>(A0);
							It.Xv[2] = static_cast<double>(A1);
							It.Xv[3] = Rs[Ri] * static_cast<double>(A1);
							It.U = TriggerUnnatural(It.Xv);
							if (It.U > 1.5)
							{
								continue;
							}
							H.FingerLocals(Index, 0.0, It.Xv[0], It.Xv[1], It.Xv[2], It.Xv[3], L);
							H.World(One, L, W);
							It.PadH = H.PadPoint(W, Index);
							It.NormH = W[H.LastBone(Index)].Vec(kYAxis);
							Out.push_back(It);
						}
					}
				}
			}
			return Out;
		}

		/** place_nat.eval_p. Returns false for None. */
		bool EvalP(const Setup& Su, const TriggerSettings& Set, const std::vector<NatItem>& Nat, const double P[6], Locals& L, Pose& W,
		           PlaceNatEntry& Out)
		{
			const HandModel& H = Su.Hand;
			const X C = Su.Plc.Corr(P);
			const X Hand = Su.Plc.HandNew(C);
			bool bHas = false;
			size_t BestI = 0;
			double BestS = 0.0, BestE = 0.0, BestFace = 0.0;
			for (size_t I = 0; I < Nat.size(); ++I)
			{
				const NatItem& N = Nat[I];
				const double E = Length(Sub(Hand.Pos(N.PadH), Set.Trigger));
				const double Face = Dot(Hand.Vec(N.NormH), Set.Pull);
				const double Sc = PyPow(E / 0.1, 2.0) + N.U + 3.0 * (1.0 - Face);
				if (!bHas || Sc < BestS)           // first minimum
				{
					bHas = true;
					BestS = Sc;
					BestI = I;
					BestE = E;
					BestFace = Face;
				}
			}
			if (!bHas || BestE > 0.6)
			{
				return false;
			}
			const double Palm = Su.Plc.PalmWorst(C);
			if (Palm < -0.15)
			{
				return false;
			}
			const double* Xi = Nat[BestI].Xv;
			H.FingerLocals(Index, 0.0, Xi[0], Xi[1], Xi[2], Xi[3], L);
			H.World(Hand, L, W);
			const double Pen = H.MaxPen(W, Index);
			const double Dev = PlaceNatDev(Set, P);
			const double PalmGap = 0.05 - Palm;
			const double PalmTerm = (PalmGap > 0.0 ? PalmGap : 0.0) * 20.0;      // max(0.0, 0.05 - palm) * 20
			const double PenPos = Pen > 0.0 ? Pen : 0.0;                          // max(0.0, pen)
			Out.J = BestS + PyPow(PalmTerm, 2.0) + PenPos * 30.0 + 0.8 * Dev;
			for (int K = 0; K < 6; ++K)
			{
				Out.P[K] = P[K];
			}
			for (int K = 0; K < 4; ++K)
			{
				Out.Idx[K] = Xi[K];
			}
			Out.Err = BestE;
			Out.Face = BestFace;
			Out.Palm = Palm;
			Out.Pen = Pen;
			Out.Dev = Dev;
			return true;
		}

		// --- rsolve2.py ---

		/** finger_locals(f, CUP[f] * max(0, a[0]) / 90, phi, *a) */
		void PutFingerLocals(const HandModel& H, int F, double Phi, const double A[3], Locals& L)
		{
			const double M0 = A[0] > 0.0 ? A[0] : 0.0;           // max(0, a[0])
			H.FingerLocals(F, H.Cup[F] * M0 / 90.0, Phi, A[0], A[1], A[2], L);
		}

		/** rsolve2._opt_job: the best pose of finger F for one side angle Phi. bPlaced = "not None". */
		TriggerFinger OptJob(const HandModel& H, const TriggerSettings& Set, const X& HandW, const Locals& Locs, int F, double Phi,
		                     int PrevF, const Exec& Ex)
		{
			TriggerFinger Best;
			Pose Wp;
			H.World(HandW, Locs, Wp);
			Seg Ps[3];
			H.Segs(Wp, PrevF, Ps);
			std::vector<Seg> Others;
			CollectOthers(H, Wp, Locs, F, PrevF, Others);
			const bool bTight = PrevF != Index;
			const std::vector<int>& Ch = H.Chain[F];
			const int Link0 = Ch[1], Link1 = Ch[2], Link2 = Ch[3];
			const double Ratios[4] = {0.5, 0.65, 0.8, 0.95};

			Locals L = Locs;
			Pose W;
			double A0 = H.LimMcp[0];
			while (A0 <= 90.0)
			{
				if (Ex.Cancelled())
				{
					break;
				}
				double A1 = 0.0;
				while (A1 <= 105.0)
				{
					for (int Ri = 0; Ri < 4; ++Ri)
					{
						const double R = Ratios[Ri];
						const double Ra1 = R * A1;
						const double Av[3] = {A0, A1, Ra1 < H.LimDip[1] ? Ra1 : H.LimDip[1]};   // min(80.0, r * a1)
						PutFingerLocals(H, F, Phi, Av, L);
						H.World(HandW, L, W);
						if (H.MaxPen(W, F) > 0.0)
						{
							continue;
						}
						Seg Sf[3];
						H.Segs(W, F, Sf);
						if (FingerOverlap(Sf, Others) > 0.0)
						{
							continue;
						}
						const double Gm = SegSeg(Sf[1].A, Sf[1].B, Ps[1].A, Ps[1].B) - Sf[1].R - Ps[1].R;
						const double Gp = SegSeg(Sf[0].A, Sf[0].B, Ps[0].A, Ps[0].B) - Sf[0].R - Ps[0].R;
						const double Gd = SegSeg(Sf[2].A, Sf[2].B, Ps[2].A, Ps[2].B) - Sf[2].R - Ps[2].R;
						double Mn = Gm;                          // min(gm, gp, gd)
						if (Gp < Mn)
						{
							Mn = Gp;
						}
						if (Gd < Mn)
						{
							Mn = Gd;
						}
						if (Mn < -0.05)
						{
							continue;
						}
						const double G[3] = {H.LinkGap(W, Link0), H.LinkGap(W, Link1), H.LinkGap(W, Link2)};
						const double Wts[3] = {1.0, 1.5, 1.0};
						double RestTerms[3];                     // sum(w * max(0.0, x - CONTACT) ...)
						for (int K = 0; K < 3; ++K)
						{
							const double Over = G[K] - Set.Contact;
							RestTerms[K] = Wts[K] * (Over > 0.0 ? Over : 0.0);
						}
						const double Rest = PySum(RestTerms, 3);
						const double Adj = (PyPow(Gm - 0.15, 2.0) + 0.5 * PyPow(Gd - 0.15, 2.0)) * (bTight ? 6.0 : 0.3);
						// WRAP: the pad must come round to the far side of the grip
						const V3 Tip = W[Link2].Pos(Mul(H.MRef[Link2].t, 0.6));
						const double Wrap = (Tip.x - Set.GripX) * Set.ThumbSide;
						const double Behind = Set.WrapFrontY - Tip.y;
						const double WrapCl = 1.2 < Wrap ? 1.2 : Wrap;                 // min(wrap, 1.2)
						const double BehindLo = -2.0 > Behind ? -2.0 : Behind;         // max(behind, -2.0)
						const double BehindCl = 1.0 < BehindLo ? 1.0 : BehindLo;       // min(., 1.0)
						const double Around = 2.0 * WrapCl + 2.0 * BehindCl;
						const double Sc = -Rest * 3.0 - Adj - std::fabs(Phi) / 30.0 - (PyPow(R - 0.7, 2.0)) * 2.0 + Around;
						if (!Best.bPlaced || Sc > Best.Score)
						{
							Best.bPlaced = true;
							Best.Score = Sc;
							Best.Phi = Phi;
							for (int K = 0; K < 3; ++K)
							{
								Best.A[K] = Av[K];
								Best.Gaps[K] = G[K];
							}
							Best.Adj[0] = Gp;
							Best.Adj[1] = Gm;
							Best.Adj[2] = Gd;
						}
					}
					A1 += 3.0;
				}
				A0 += 3.0;
			}
			return Best;
		}

		/** rsolve2.next_finger_opt: _opt_job over the phi list (parallel, one slot per phi), the first maximum wins. */
		bool NextFingerOpt(const HandModel& H, const TriggerSettings& Set, const X& HandW, const Locals& Locs, int F, int PrevF,
		                   const std::vector<double>& Phis, const Exec& Ex, TriggerFinger& Out)
		{
			std::vector<TriggerFinger> Slots(Phis.size());
			Ex.For(static_cast<int>(Phis.size()), [&](int I)
			{
				Slots[static_cast<size_t>(I)] = OptJob(H, Set, HandW, Locs, F, Phis[static_cast<size_t>(I)], PrevF, Ex);
			});
			bool bAny = false;
			for (const TriggerFinger& R : Slots)
			{
				if (R.bPlaced && (!bAny || R.Score > Out.Score))
				{
					Out = R;
					bAny = true;
				}
			}
			return bAny;
		}

		/** rsolve2.thumb_rest: parallel over (abd, fl) slots, reduced in the Python loop order with strict '>'. */
		TriggerThumb ThumbRest(const Setup& Su, const TriggerSettings& Set, const X& HandW, const std::vector<Seg>& Others, const Exec& Ex)
		{
			const HandModel& H = Su.Hand;
			const int T0 = H.Chain[Thumb][0], T1 = H.Chain[Thumb][1], T2 = H.Chain[Thumb][2];
			const int NumAbd = 10, NumFl = 9;                 // range(-60, 31, 10), range(-40, 41, 10)
			const double Ks[3] = {0.5, 0.8, 1.1};
			std::vector<TriggerThumb> Slots(static_cast<size_t>(NumAbd * NumFl));
			Ex.For(NumAbd * NumFl, [&](int Slot)
			{
				if (Ex.Cancelled())
				{
					return;
				}
				const double Abd = static_cast<double>(-60 + 10 * (Slot / NumFl));
				const double Fl = static_cast<double>(-40 + 10 * (Slot % NumFl));
				TriggerThumb& Best = Slots[static_cast<size_t>(Slot)];
				Locals L;
				L.Init(H.Bones.size());
				Pose W;
				for (int Mi = 0; Mi <= 60; Mi += 5)
				{
					const double M = static_cast<double>(Mi);
					for (int Ki = 0; Ki < 3; ++Ki)
					{
						const double Ip = Ks[Ki] * M;
						H.ThumbLocals(Su.Plc.Clip, M, Ip, true, Abd, Fl, L);
						H.World(HandW, L, W);
						const double Pen0 = H.LinkPen(W, T0) - 0.3;
						const double Pen1 = H.LinkPen(W, T1);
						const double Pen2 = H.LinkPen(W, T2);
						double PenMax = Pen0;                    // max(a, b, c)
						if (Pen1 > PenMax)
						{
							PenMax = Pen1;
						}
						if (Pen2 > PenMax)
						{
							PenMax = Pen2;
						}
						if (PenMax > 0.0)
						{
							continue;
						}
						Seg St[3];
						H.Segs(W, Thumb, St);
						if (FingerOverlap(St, Others) > 0.0)
						{
							continue;
						}
						const double G1 = H.LinkGap(W, T1), G2 = H.LinkGap(W, T2);
						const V3 Tip = W[T2].Pos(Mul(H.MRef[T2].t, 0.9));
						const bool bFar = (Tip.x - Set.GripX) * Set.ThumbSide > 0.0;
						const bool bLow = Tip.z <= Set.ThumbZMax;
						const double Over1 = G1 - 0.2;
						const double Sc = -std::fabs(G2 - 0.05) * 5.0 - (Over1 > 0.0 ? Over1 : 0.0) * 2.0 + (bFar ? 1.5 : 0.0)
							+ (bLow ? 1.0 : -2.0) - (Abd * Abd + Fl * Fl) / 3000.0;
						if (!Best.bPlaced || Sc > Best.Score)
						{
							Best.bPlaced = true;
							Best.Score = Sc;
							Best.Abd = Abd;
							Best.Flex = Fl;
							Best.Mcp = M;
							Best.Ip = Ip;
							Best.Gaps[0] = G1;
							Best.Gaps[1] = G2;
							Best.bFar = bFar;
							Best.bLow = bLow;
						}
					}
				}
			});
			TriggerThumb Out;
			for (const TriggerThumb& R : Slots)
			{
				if (R.bPlaced && (!Out.bPlaced || R.Score > Out.Score))
				{
					Out = R;
				}
			}
			return Out;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// index_nat.py

	double TriggerUnnatural(const double Xv[4])
	{
		const double Phi = Xv[0], A0 = Xv[1], A1 = Xv[2], A2 = Xv[3];
		const double Den = 1.0 > A1 ? 1.0 : A1;                    // max(a1, 1.0)
		const double R = A2 / Den;
		double C = PyPow((R - 0.65) / 0.15, 2.0);               // DIP follows PIP
		const double Hook = 0.5 * A1 - A0;                         // the knuckle closes too (no hook)
		C += PyPow((Hook > 0.0 ? Hook : 0.0) / 10.0, 2.0);
		const double Flat = A0 - 1.1 * A1;                         // ... and not a flat knuckle-only bend
		C += PyPow((Flat > 0.0 ? Flat : 0.0) / 15.0, 2.0);
		C += PyPow(Phi / 12.0, 2.0);
		return C;
	}

	IndexMetrics TriggerMetrics(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, const double Xv[4])
	{
		Locals L;
		L.Init(Hand.Bones.size());
		Pose W;
		return MetricsWith(Hand, Set, HandW, Xv, L, W);
	}

	double TriggerCost(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, const double Xv[4])
	{
		return CostOf(TriggerMetrics(Hand, Set, HandW, Xv), Xv);
	}

	double TriggerRefine(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, double Xv[4])
	{
		const double Lo[4] = {-20.0, -15.0, 0.0, 0.0};
		const double Hi[4] = {20.0, 90.0, 105.0, 80.0};
		const int Pairs[3][2] = {{1, 2}, {1, 3}, {2, 3}};
		const double Steps[5] = {4.0, 2.0, 1.0, 0.5, 0.25};
		Locals L;
		L.Init(Hand.Bones.size());
		Pose W;
		auto CostAt = [&](const double Yv[4])
		{
			return CostOf(MetricsWith(Hand, Set, HandW, Yv, L, W), Yv);
		};
		double C = CostAt(Xv);
		for (int Si = 0; Si < 5; ++Si)
		{
			const double Step = Steps[Si];
			bool bImp = true;
			while (bImp)
			{
				bImp = false;
				for (int K = 0; K < 4; ++K)
				{
					for (int Sg = 0; Sg < 2; ++Sg)
					{
						const double Sv = Sg == 0 ? -Step : Step;
						double Yv[4] = {Xv[0], Xv[1], Xv[2], Xv[3]};
						const double Moved = Yv[K] + Sv;
						const double AboveLo = Moved > Lo[K] ? Moved : Lo[K];       // max(lo[k], y[k] + s)
						Yv[K] = AboveLo < Hi[K] ? AboveLo : Hi[K];                  // min(hi[k], .)
						const double Cy = CostAt(Yv);
						if (Cy < C - 1e-7)
						{
							for (int J = 0; J < 4; ++J)
							{
								Xv[J] = Yv[J];
							}
							C = Cy;
							bImp = true;
						}
					}
				}
				// paired moves along the family (MCP up, PIP down) so the search can slide along the solution curve
				for (int Sg = 0; Sg < 2; ++Sg)
				{
					const double Sv = Sg == 0 ? -Step : Step;
					for (int Pk = 0; Pk < 3; ++Pk)
					{
						double Yv[4] = {Xv[0], Xv[1], Xv[2], Xv[3]};
						Yv[Pairs[Pk][0]] += Sv;
						Yv[Pairs[Pk][1]] -= Sv;
						for (int K = 0; K < 4; ++K)
						{
							const double AboveLo = Yv[K] > Lo[K] ? Yv[K] : Lo[K];
							Yv[K] = AboveLo < Hi[K] ? AboveLo : Hi[K];
						}
						const double Cy = CostAt(Yv);
						if (Cy < C - 1e-7)
						{
							for (int J = 0; J < 4; ++J)
							{
								Xv[J] = Yv[J];
							}
							C = Cy;
							bImp = true;
						}
					}
				}
			}
		}
		return C;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// place_nat.py

	std::vector<std::array<double, 6>> PlaceNatGrid()
	{
		const double Yaw[6] = {0, 5, 10, 15, 20, 25};
		const double Pitch[7] = {-25, -20, -15, -10, -5, 0, 5};
		const double Roll[7] = {-5, 0, 5, 10, 15, 20, 25};
		const double Dx[5] = {-3, -2, -1, 0, 1};
		const double Dy[5] = {-3, -2, -1, 0, 1};
		const double Dz[7] = {-3, -2.25, -1.5, -0.75, 0, 0.75, 1.5};
		std::vector<std::array<double, 6>> Out;
		Out.reserve(6 * 7 * 7 * 5 * 5 * 7);
		for (double A : Yaw)
		{
			for (double B : Pitch)
			{
				for (double C : Roll)
				{
					for (double D : Dx)
					{
						for (double E : Dy)
						{
							for (double F : Dz)
							{
								Out.push_back({A, B, C, D, E, F});
							}
						}
					}
				}
			}
		}
		return Out;
	}

	double PlaceNatDev(const TriggerSettings& Set, const double P[6])
	{
		double LinTerms[3], RotTerms[3];
		for (int K = 0; K < 3; ++K)
		{
			LinTerms[K] = PyPow(P[K] - Set.P0[K], 2.0);
			RotTerms[K] = PyPow(P[K + 3] - Set.P0[K + 3], 2.0);
		}
		return std::sqrt(PySum(LinTerms, 3)) / 10.0 + std::sqrt(PySum(RotTerms, 3));   // math.sqrt(sum(...)) / 10.0 + math.sqrt(sum(...))
	}

	bool PlaceNat(const Setup& Su, const TriggerSettings& Set, const Exec& Ex, std::vector<PlaceNatEntry>& Out)
	{
		Out.clear();
		const std::vector<NatItem> Nat = BuildNat(Su.Hand);
		std::vector<std::array<double, 6>> Grid = PlaceNatGrid();
		// the grid was laid out around the M16's approved re-grip P0 = (10, -10, 10, -1, -1, -1); another weapon's search
		// is centred on its own P0 (its clip hold = zeros). For the M16 the shift is exactly 0 (parity untouched).
		{
			static const double M16P0[6] = {10.0, -10.0, 10.0, -1.0, -1.0, -1.0};
			double Shift[6];
			bool bShift = false;
			for (int K = 0; K < 6; ++K)
			{
				Shift[K] = Set.P0[K] - M16P0[K];
				bShift = bShift || Shift[K] != 0.0;
			}
			if (bShift)
			{
				for (std::array<double, 6>& G : Grid)
				{
					for (int K = 0; K < 6; ++K)
					{
						G[static_cast<size_t>(K)] += Shift[K];
					}
				}
			}
		}
		const int RowLen = 5 * 5 * 7;                                         // dx, dy, dz
		const int NumRows = static_cast<int>(Grid.size()) / RowLen;
		std::vector<PlaceNatEntry> Slots(Grid.size());
		std::vector<char> Has(Grid.size(), 0);
		Ex.For(NumRows, [&](int Row)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			Locals L;
			L.Init(Su.Hand.Bones.size());
			Pose W;
			for (int J = 0; J < RowLen; ++J)
			{
				const size_t Slot = static_cast<size_t>(Row) * static_cast<size_t>(RowLen) + static_cast<size_t>(J);
				Has[Slot] = EvalP(Su, Set, Nat, Grid[Slot].data(), L, W, Slots[Slot]) ? 1 : 0;
			}
		});
		if (Ex.Cancelled())
		{
			return false;
		}
		for (size_t I = 0; I < Slots.size(); ++I)
		{
			if (Has[I])
			{
				Out.push_back(Slots[I]);
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// rsolve3.py

	bool SolveTrigger(const Setup& Su, const TriggerSettings& Set, const double P[6], const double X0[4], const Exec& Ex,
	                  TriggerSolution& Out)
	{
		const HandModel& H = Su.Hand;
		const X C = Su.Plc.Corr(P);
		const X Hand = Su.Plc.HandNew(C);
		Out = TriggerSolution();
		for (int K = 0; K < 6; ++K)
		{
			Out.P[K] = P[K];
		}
		Out.Palm = Su.Plc.PalmWorst(C);

		double Xv[4] = {X0[0], X0[1], X0[2], X0[3]};
		Out.IndexCost = TriggerRefine(H, Set, Hand, Xv);
		if (Ex.Cancelled())
		{
			return false;
		}
		const IndexMetrics M = TriggerMetrics(H, Set, Hand, Xv);
		Out.IndexPhi = Xv[0];
		Out.IndexA[0] = Xv[1];
		Out.IndexA[1] = Xv[2];
		Out.IndexA[2] = Xv[3];
		Out.IndexErr = M.Err;
		Out.IndexFace = M.Face;
		Out.IndexPen = M.Pen;
		Out.IndexUnnatural = TriggerUnnatural(Xv);

		Locals& Locs = Out.Locs;
		Locs.Init(H.Bones.size());
		H.FingerLocals(Index, 0.0, Xv[0], Xv[1], Xv[2], Xv[3], Locs);
		int PrevF = Index;
		std::vector<double> Phis;                                             // [v * 2.5 for v in range(-6, 11)]
		for (int V = -6; V < 11; ++V)
		{
			Phis.push_back(static_cast<double>(V) * 2.5);
		}
		for (int F = Middle; F <= Pinky; ++F)
		{
			TriggerFinger Best;
			const bool bFound = NextFingerOpt(H, Set, Hand, Locs, F, PrevF, Phis, Ex, Best);
			if (Ex.Cancelled())
			{
				return false;
			}
			if (!bFound)
			{
				continue;                                                     // rec[f] = None
			}
			Out.Fingers[F - Middle] = Best;
			PutFingerLocals(H, F, Best.Phi, Best.A, Locs);
			PrevF = F;
		}

		Pose W;
		H.World(Hand, Locs, W);
		std::vector<Seg> Others;
		CollectOthers(H, W, Locs, -1, -1, Others);
		Out.Thumb = ThumbRest(Su, Set, Hand, Others, Ex);
		if (Ex.Cancelled())
		{
			return false;
		}
		if (Out.Thumb.bPlaced)
		{
			H.ThumbLocals(Su.Plc.Clip, Out.Thumb.Mcp, Out.Thumb.Ip, true, Out.Thumb.Abd, Out.Thumb.Flex, Locs);
		}
		Out.HandInWeapon = Hand;
		return true;
	}

	std::string TriggerSolutionJson(const Setup& Su, const TriggerSolution& Sol)
	{
		const HandModel& H = Su.Hand;
		JWriter W;
		W.BeginObject();
		W.Key("p").Numbers(Sol.P, 6);
		W.Key("palm").Value(Sol.Palm);
		W.Key("index").BeginObject();
		W.Key("phi").Value(Sol.IndexPhi);
		W.Key("a").Numbers(Sol.IndexA, 3);
		W.Key("err").Value(Sol.IndexErr);
		W.Key("face").Value(Sol.IndexFace);
		W.Key("pen").Value(Sol.IndexPen);
		W.Key("unnatural").Value(Sol.IndexUnnatural);
		W.EndObject();
		static const char* Names[3] = {"middle", "ring", "pinky"};
		for (int I = 0; I < 3; ++I)
		{
			const TriggerFinger& Fg = Sol.Fingers[I];
			W.Key(Names[I]);
			if (!Fg.bPlaced)
			{
				W.Null();
				continue;
			}
			W.BeginObject();
			W.Key("phi").Value(Fg.Phi);
			W.Key("a").Numbers(Fg.A, 3);
			W.Key("gaps").Numbers(Fg.Gaps, 3);
			W.Key("adj").Numbers(Fg.Adj, 3);
			W.EndObject();
		}
		W.Key("thumb");
		if (!Sol.Thumb.bPlaced)
		{
			W.Null();
		}
		else
		{
			const double Cmc[2] = {Sol.Thumb.Abd, Sol.Thumb.Flex};
			W.BeginObject();
			W.Key("cmc").Numbers(Cmc, 2);
			W.Key("mcp").Value(Sol.Thumb.Mcp);
			W.Key("ip").Value(Sol.Thumb.Ip);
			W.Key("gaps").Numbers(Sol.Thumb.Gaps, 2);
			W.Key("far").Value(Sol.Thumb.bFar);
			W.Key("low").Value(Sol.Thumb.bLow);
			W.EndObject();
		}
		const double HandF7[7] = {Sol.HandInWeapon.t.x, Sol.HandInWeapon.t.y, Sol.HandInWeapon.t.z,
		                          Sol.HandInWeapon.q.x, Sol.HandInWeapon.q.y, Sol.HandInWeapon.q.z, Sol.HandInWeapon.q.w};
		W.Key("hand_in_weapon").Numbers(HandF7, 7);
		W.Key("locals").BeginObject();
		const int Order[5] = {Index, Middle, Ring, Pinky, Thumb};            // the dict's insertion order
		for (int Fi : Order)
		{
			for (int Bone : H.Chain[Fi])
			{
				if (Sol.Locs.Has(Bone))
				{
					const Q& Rot = Sol.Locs.L[Bone].q;
					const double Quat[4] = {Rot.x, Rot.y, Rot.z, Rot.w};
					W.Key(H.Bones[Bone]).Numbers(Quat, 4);
				}
			}
		}
		W.EndObject();
		W.EndObject();
		return W.Str();
	}
}
