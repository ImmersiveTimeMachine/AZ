// Copyright Artur. AZ project.

#include "NGJobs.h"

#include "AZNaturalGripProfile.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "NGAssetReaders.h"
#include "NGJson.h"
#include "NGJsonDiff.h"
#include "NGSupportHand.h"
#include "NGTriggerHand.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
	std::string U8(const FString& S)
	{
		return std::string(TCHAR_TO_UTF8(*S));
	}

	FString FromU8(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	FString PStr(const double P[6])
	{
		return FString::Printf(TEXT("[%g, %g, %g, %g, %g, %g]"), P[0], P[1], P[2], P[3], P[4], P[5]);
	}

	/** Log collector with a timestamp per stage. */
	struct FLog
	{
		FString Text;
		double T0 = FPlatformTime::Seconds();

		void Line(const FString& S)
		{
			Text += S;
			Text += TEXT("\n");
		}
		void Stage(const FString& S)
		{
			Line(FString::Printf(TEXT("== %s"), *S));
			T0 = FPlatformTime::Seconds();
		}
		double Seconds() const { return FPlatformTime::Seconds() - T0; }
	};

	bool MakeSetup(const FNGProfileData& P, int32 Thin, bool bFine, std::unique_ptr<ng::Setup>& Out, FLog& Log)
	{
		ng::SetupConfig Cfg = P.Base;
		Cfg.Thin = Thin;
		Cfg.bFine = bFine;
		Out = std::make_unique<ng::Setup>();
		std::string Err;
		ng::SetupInputs In;
		if (P.bAssets)                    // the hand from the hero mesh, the fields baked from the weapon mesh
		{
			In.Hand = &P.HeroHand;
			In.Coarse = P.BakedCoarse.get();
			In.Fine = P.BakedFine.get();
		}
		if (P.bHasHold)
		{
			In.Hold = &P.Hold;
		}
		if (!P.Obstacles.empty())
		{
			In.Obstacles = &P.Obstacles;
		}
		if (!Out->Load(Cfg, Err, In))
		{
			Log.Line(FString::Printf(TEXT("ERROR setup: %s"), *FromU8(Err)));
			return false;
		}
		Log.Line(FString::Printf(TEXT("setup: side %c, weapon %s, thin %d, inputs %s, fine %s"), Cfg.Side, *FromU8(Cfg.Weapon), Thin,
			P.bAssets ? TEXT("assets") : TEXT("data files"), Out->bFineLoaded ? *FromU8(Out->FinePath) : TEXT("off")));
		return true;
	}

	bool SaveJson(const ng::JWriter& W, const FString& Path, FLog& Log)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!W.Save(U8(Path)))
		{
			Log.Line(FString::Printf(TEXT("ERROR cannot write %s"), *Path));
			return false;
		}
		Log.Line(FString::Printf(TEXT("-> %s"), *Path));
		return true;
	}

	// ------------------------------------------------------------------ support hand

	bool SupportSettingsOf(const FNGProfileData& P, ng::SupportSettings& St, FLog& Log)
	{
		St.AxX = P.AxX;
		St.AxZ = P.AxZ;
		if (P.bHandle || P.bPistolCup)
		{
			St.SetAxis(P.HandleBack, P.HandleFront);
		}
		if (P.bPistolCup)
		{
			St.bThumbFwd = true;
			St.ThumbFwd = P.WeaponFwd;
		}
		if (P.bHasHold && P.Hold.bHasElbows)
		{
			St.ElbowAim = P.Hold.ElbowAim;
			St.ElbowRelaxed = P.Hold.ElbowRelaxed;
			return true;
		}
		std::string Err;
		if (!ng::LoadSupportElbows(P.Base.SavedDir, P.Base.Weapon, St, Err))
		{
			Log.Line(FString::Printf(TEXT("ERROR elbows: %s"), *FromU8(Err)));
			return false;
		}
		return true;
	}

	void WritePlacementFields(ng::JWriter& W, const ng::SupportPlacement& R)
	{
		W.Key("p").Numbers(R.P, 6);
		W.Key("palm").Value(R.Palm);
		W.Key("bend").Numbers(R.Bend, 2);
		W.Key("dev").Value(R.Dev);
		W.Key("J1").Value(R.J1);
	}

	bool SupportStage1(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log, std::vector<ng::SupportPlacement>& Out, FString& OutPath)
	{
		Log.Stage(TEXT("support stage 1: placements (palm clearance, wrist bend, change)"));
		std::unique_ptr<ng::Setup> Su;
		ng::SupportSettings St;
		if (!MakeSetup(P, P.SearchThin, false, Su, Log) || !SupportSettingsOf(P, St, Log))
		{
			return false;
		}
		ng::SupportStage1(*Su, St, Ex, Out);
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok %d valid placements (%.2f s)"), static_cast<int32>(Out.size()), Log.Seconds()));
		for (size_t I = 0; I < Out.size() && I < 15; ++I)
		{
			const ng::SupportPlacement& R = Out[I];
			Log.Line(FString::Printf(TEXT("J1 %.2f p %s palm %+.2f bend [%.0f, %.0f] dev %.2f"), R.J1, *PStr(R.P), R.Palm, R.Bend[0], R.Bend[1], R.Dev));
		}
		ng::JWriter W(false);
		W.BeginArray();
		for (size_t I = 0; I < Out.size() && I < 400; ++I)       // the Python file keeps the top 400
		{
			W.BeginObject();
			WritePlacementFields(W, Out[I]);
			W.EndObject();
		}
		W.EndArray();
		OutPath = P.OutDir / TEXT("support_stage1.json");
		return SaveJson(W, OutPath, Log);
	}

	FString FingerLine(const ng::SupportSolve& S)
	{
		static const TCHAR* Names[] = {TEXT("t"), TEXT("i"), TEXT("m"), TEXT("r"), TEXT("p")};
		FString L;
		for (int F = ng::Index; F <= ng::Pinky; ++F)
		{
			const ng::SupportFinger& Fg = S.Finger[F];
			L += Fg.bValid ? FString::Printf(TEXT("%s:[%.0f, %.0f, %.0f] w%.0f "), Names[F], Fg.A[0], Fg.A[1], Fg.A[2], Fg.Wrap)
			               : FString::Printf(TEXT("%s:x "), Names[F]);
		}
		L += S.Thumb.bValid ? FString::Printf(TEXT("| thumb cmc (%.1f, %.1f) mcp %.1f ip %.1f pad %.2f"), S.Thumb.Abd, S.Thumb.Fl, S.Thumb.M, S.Thumb.Ip, S.Thumb.PadGap)
		                    : FString(TEXT("| thumb x"));
		return L;
	}

	bool SupportStage2(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log, const std::vector<ng::SupportPlacement>& Stage1,
	                   std::vector<ng::SupportStage2Result>& Out, FString& OutPath)
	{
		Log.Stage(FString::Printf(TEXT("support stage 2: coarse fingers for the best %d placements (step %g)"), P.Stage2Top, P.Stage2Step));
		std::unique_ptr<ng::Setup> Su;
		ng::SupportSettings St;
		if (!MakeSetup(P, P.SearchThin, true, Su, Log) || !SupportSettingsOf(P, St, Log))
		{
			return false;
		}
		const std::vector<ng::SupportPlacement> Cands(Stage1.begin(), Stage1.begin() + FMath::Min<size_t>(Stage1.size(), static_cast<size_t>(P.Stage2Top)));
		ng::SupportStage2(*Su, St, Cands, {-5.0, 0.0, 5.0, 10.0}, P.Stage2Step, {0.55, 0.75}, Ex, Out);
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok %d candidates (%.2f s)"), static_cast<int32>(Out.size()), Log.Seconds()));
		std::vector<size_t> Order(Out.size());
		for (size_t I = 0; I < Order.size(); ++I)
		{
			Order[I] = I;
		}
		std::stable_sort(Order.begin(), Order.end(), [&Out](size_t A, size_t B) { return Out[A].J2 < Out[B].J2; });
		for (size_t K = 0; K < Order.size() && K < 10; ++K)
		{
			const ng::SupportStage2Result& R = Out[Order[K]];
			Log.Line(FString::Printf(TEXT("J2 %6.2f J1 %.2f F %6.2f p %s palm %+.2f bend [%.0f, %.0f] | %s"), R.J2, R.Cand.J1, R.Solve.Total, *PStr(R.Cand.P),
				R.Cand.Palm, R.Cand.Bend[0], R.Cand.Bend[1], *FingerLine(R.Solve)));
		}
		ng::JWriter W(false);
		W.BeginArray();
		for (const ng::SupportStage2Result& R : Out)
		{
			W.BeginObject();
			WritePlacementFields(W, R.Cand);
			W.Key("fingers").BeginObject();
			ng::WriteSupportRecord(W, R.Solve);
			W.EndObject();
			W.Key("F").Value(R.Solve.Total);
			W.Key("J2").Value(R.J2);
			W.EndObject();
		}
		W.EndArray();
		OutPath = P.OutDir / TEXT("support_stage2.json");
		return SaveJson(W, OutPath, Log);
	}

	/** The final solve. bPolish: also the part-B solve (the exhaustive grid's best per finger polished by the pattern search)
	 *  and keep whichever whole hand scores higher, so the result is never worse than the Python-parity one. */
	bool SupportFinal(const FNGProfileData& P, const double Pl[6], const ng::Exec& Ex, FLog& Log, FString& OutPath, bool bPolish = false)
	{
		Log.Stage(FString::Printf(TEXT("support final: fine solve at p %s%s"), *PStr(Pl), bPolish ? TEXT(" + continuous polish") : TEXT("")));
		std::unique_ptr<ng::Setup> Su;
		ng::SupportSettings St;
		if (!MakeSetup(P, P.FinalThin, true, Su, Log) || !SupportSettingsOf(P, St, Log))
		{
			return false;
		}
		std::vector<double> Phis;
		for (int V = -4; V < 7; ++V)
		{
			Phis.push_back(V * 2.5);
		}
		ng::SupportFinalResult R;
		ng::SolveSupportFinal(*Su, St, Pl, Phis, 3.0, {0.5, 0.65, 0.8, 0.95}, Ex, R);
		if (Ex.Cancelled())
		{
			return false;
		}
		if (bPolish)
		{
			ng::SupportFinalResult B;
			ng::SolveSupportFinalFast(*Su, St, Pl, ng::SupportFastOptions(), Ex, B);
			if (Ex.Cancelled())
			{
				return false;
			}
			const bool bUseB = B.Solve.Total > R.Solve.Total;
			Log.Line(FString::Printf(TEXT("grid (A) total %.3f, polished (B) total %.3f -> keep %s"), R.Solve.Total, B.Solve.Total, bUseB ? TEXT("B") : TEXT("A")));
			if (bUseB)
			{
				R = B;
			}
		}
		Log.Line(FString::Printf(TEXT("ok F %.3f palm %+.2f bend [%.0f, %.0f] (%.2f s)"), R.Solve.Total, R.Palm, R.Bend[0], R.Bend[1], Log.Seconds()));
		Log.Line(FingerLine(R.Solve));
		ng::SupportGapReport Gaps;
		ng::SupportFingerGaps(*Su, R.HandInWeapon, R.Solve.Locs, Gaps);
		Log.Line(FromU8(ng::FormatSupportGapReport("finger gaps", Gaps)).TrimEnd());
		OutPath = P.OutDir / TEXT("support_final.json");
		IFileManager::Get().MakeDirectory(*P.OutDir, true);
		if (!ng::WriteSupportFinalJson(*Su, R, U8(OutPath)))
		{
			Log.Line(FString::Printf(TEXT("ERROR cannot write %s"), *OutPath));
			return false;
		}
		Log.Line(FString::Printf(TEXT("-> %s"), *OutPath));
		return true;
	}

	// ------------------------------------------------------------------ trigger hand

	ng::TriggerSettings TriggerSettingsOf(const FNGProfileData& P)
	{
		ng::TriggerSettings St;
		St.Trigger = P.Trigger;
		St.Pull = ng::Norm(P.Pull);
		St.GripX = P.GripX;
		St.ThumbSide = P.ThumbSide;
		St.ThumbZMax = P.ThumbZMax;
		St.WrapFrontY = P.WrapFrontY;
		for (int K = 0; K < 6; ++K)
		{
			St.P0[K] = P.SearchCentre[K];
		}
		return St;
	}

	bool TriggerPlacement(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log, std::vector<ng::PlaceNatEntry>& Out, FString& OutPath)
	{
		Log.Stage(TEXT("trigger placement: placements x natural index shapes (pad on the trigger, palm out, small change)"));
		std::unique_ptr<ng::Setup> Su;
		if (!MakeSetup(P, P.SearchThin, true, Su, Log))
		{
			return false;
		}
		if (!ng::PlaceNat(*Su, TriggerSettingsOf(P), Ex, Out) || Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok %d candidates (%.2f s)"), static_cast<int32>(Out.size()), Log.Seconds()));
		std::vector<size_t> Order(Out.size());
		for (size_t I = 0; I < Order.size(); ++I)
		{
			Order[I] = I;
		}
		std::stable_sort(Order.begin(), Order.end(), [&Out](size_t A, size_t B) { return Out[A].J < Out[B].J; });
		for (size_t K = 0; K < Order.size() && K < 20; ++K)
		{
			const ng::PlaceNatEntry& R = Out[Order[K]];
			Log.Line(FString::Printf(TEXT("J %5.2f p %s idx [%.0f, %.0f, %.0f, %.0f] err %.2f face %.2f palm %+.2f pen %+.2f dev %.2f"), R.J, *PStr(R.P),
				R.Idx[0], R.Idx[1], R.Idx[2], R.Idx[3], R.Err, R.Face, R.Palm, R.Pen, R.Dev));
		}
		ng::JWriter W(false);
		W.BeginArray();
		for (const ng::PlaceNatEntry& R : Out)                  // every candidate in grid order (pn_all.json)
		{
			W.BeginObject();
			W.Key("p").Numbers(R.P, 6);
			W.Key("J").Value(R.J);
			W.Key("idx").Numbers(R.Idx, 4);
			W.Key("err").Value(R.Err);
			W.Key("face").Value(R.Face);
			W.Key("palm").Value(R.Palm);
			W.Key("pen").Value(R.Pen);
			W.Key("dev").Value(R.Dev);
			W.EndObject();
		}
		W.EndArray();
		OutPath = P.OutDir / TEXT("trigger_placement_all.json");
		return SaveJson(W, OutPath, Log);
	}

	bool TriggerSolve(const FNGProfileData& P, const double Pl[6], const double X0[4], const ng::Exec& Ex, FLog& Log, FString& OutPath)
	{
		Log.Stage(FString::Printf(TEXT("trigger solve at p %s, index start [%g, %g, %g, %g]"), *PStr(Pl), X0[0], X0[1], X0[2], X0[3]));
		std::unique_ptr<ng::Setup> Su;
		if (!MakeSetup(P, P.FinalThin, true, Su, Log))
		{
			return false;
		}
		ng::TriggerSolution Sol;
		if (!ng::SolveTrigger(*Su, TriggerSettingsOf(P), Pl, X0, Ex, Sol) || Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok palm %.2f index phi %+.1f a [%.1f, %.1f, %.1f] err %.3f face %.2f pen %+.3f unnatural %.2f (%.2f s)"),
			Sol.Palm, Sol.IndexPhi, Sol.IndexA[0], Sol.IndexA[1], Sol.IndexA[2], Sol.IndexErr, Sol.IndexFace, Sol.IndexPen, Sol.IndexUnnatural, Log.Seconds()));
		static const TCHAR* Names[] = {TEXT("middle"), TEXT("ring"), TEXT("pinky")};
		for (int F = 0; F < 3; ++F)
		{
			const ng::TriggerFinger& Fg = Sol.Fingers[F];
			Log.Line(Fg.bPlaced
				? FString::Printf(TEXT("  %-6s phi %+.1f a [%.0f, %.0f, %.0f] gaps [%.2f, %.2f, %.2f] adj [%.2f, %.2f, %.2f]"), Names[F], Fg.Phi, Fg.A[0], Fg.A[1], Fg.A[2],
					Fg.Gaps[0], Fg.Gaps[1], Fg.Gaps[2], Fg.Adj[0], Fg.Adj[1], Fg.Adj[2])
				: FString::Printf(TEXT("  %-6s none"), Names[F]));
		}
		Log.Line(Sol.Thumb.bPlaced
			? FString::Printf(TEXT("  thumb  cmc (%.0f, %.0f) mcp %.0f ip %.1f gaps [%.2f, %.2f] far %d low %d"), Sol.Thumb.Abd, Sol.Thumb.Flex, Sol.Thumb.Mcp, Sol.Thumb.Ip,
				Sol.Thumb.Gaps[0], Sol.Thumb.Gaps[1], Sol.Thumb.bFar ? 1 : 0, Sol.Thumb.bLow ? 1 : 0)
			: FString(TEXT("  thumb  none")));
		OutPath = P.OutDir / TEXT("trigger_solve.json");
		IFileManager::Get().MakeDirectory(*P.OutDir, true);
		std::ofstream F(U8(OutPath), std::ios::binary);
		F << ng::TriggerSolutionJson(*Su, Sol);
		if (!F)
		{
			Log.Line(FString::Printf(TEXT("ERROR cannot write %s"), *OutPath));
			return false;
		}
		Log.Line(FString::Printf(TEXT("-> %s"), *OutPath));
		return true;
	}

	// ------------------------------------------------------------------ support hand: power grasp

	bool SupportPowerChain(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log, FString& OutPath)
	{
		Log.Stage(TEXT("support POWER grasp: placements (palm seated, wrist bend, twist, roll about the handguard)"));
		std::unique_ptr<ng::Setup> S2, Sf;
		ng::SupportSettings St;
		if (!MakeSetup(P, P.SearchThin, true, S2, Log) || !MakeSetup(P, P.FinalThin, true, Sf, Log) || !SupportSettingsOf(P, St, Log))
		{
			return false;
		}
		St.SetPower();
		const ng::SupportPowerOptions O;
		std::vector<ng::PowerPlacement> P1;
		ng::SupportPowerStage1(*S2, St, O, Ex, P1);
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok %d placements (%.2f s)"), static_cast<int32>(P1.size()), Log.Seconds()));
		// stage 1 does not see the fingers: take the best of EVERY roll angle, not the global top
		std::vector<ng::PowerPlacement> Picks;
		const int32 PerTheta = FMath::Max(1, P.PowerCandidates / static_cast<int32>(O.Thetas.size()));
		for (const double Th : O.Thetas)
		{
			int32 N = 0;
			for (const ng::PowerPlacement& Pp : P1)
			{
				if (Pp.P[6] == Th && N < PerTheta)
				{
					Picks.push_back(Pp);
					++N;
				}
			}
		}
		Log.Stage(FString::Printf(TEXT("power stage 2: coarse fingers + closing thumb for %d placements"), static_cast<int32>(Picks.size())));
		ng::SupportPowerOptions Coarse = O;
		Coarse.Phis = {-5.0, 0.0, 5.0, 10.0};
		Coarse.Step = 8.0;
		Coarse.Ratios = {0.55, 0.75};
		Coarse.bPolish = false;
		size_t Best = 0;
		double BestJ2 = 0.0;
		for (size_t I = 0; I < Picks.size(); ++I)
		{
			ng::SupportFinalResult R;
			double Enc = 0.0, Tw = 0.0;
			ng::SolveSupportPower(*S2, St, Picks[I].P, Coarse, Ex, R, Enc, Tw);
			if (Ex.Cancelled())
			{
				return false;
			}
			const double J2 = Picks[I].J1 - R.Solve.Total;
			if (I == 0 || J2 < BestJ2)
			{
				Best = I;
				BestJ2 = J2;
			}
			Ex.Report("power stage 2", static_cast<double>(I + 1) / static_cast<double>(Picks.size()));
		}
		const ng::PowerPlacement& B = Picks[Best];
		Log.Line(FString::Printf(TEXT("best J2 %.2f: p [%g, %g, %g, %g, %g, %g] roll about the handguard %+g deg (%.2f s)"), BestJ2, B.P[0], B.P[1], B.P[2], B.P[3],
			B.P[4], B.P[5], B.P[6], Log.Seconds()));
		Log.Stage(TEXT("power final: fine fingers + polish + closing thumb"));
		ng::SupportFinalResult R;
		double Enc = 0.0, Tw = 0.0;
		ng::SolveSupportPower(*Sf, St, B.P, O, Ex, R, Enc, Tw);
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("ok F %.2f palm %+.2f bend [%.0f, %.0f] forearm twist %+.0f, the hand encloses %.0f deg of the handguard (%.2f s)"),
			R.Solve.Total, R.Palm, R.Bend[0], R.Bend[1], Tw, Enc, Log.Seconds()));
		Log.Line(FingerLine(R.Solve));
		OutPath = P.OutDir / TEXT("support_final.json");
		IFileManager::Get().MakeDirectory(*P.OutDir, true);
		if (!ng::WriteSupportFinalJson(*Sf, R, U8(OutPath)))
		{
			Log.Line(FString::Printf(TEXT("ERROR cannot write %s"), *OutPath));
			return false;
		}
		Log.Line(FString::Printf(TEXT("-> %s (Apply writes this)"), *OutPath));
		return true;
	}

	// ------------------------------------------------------------------ fields

	bool LoadPartsJson(const std::string& Path, ng::SdfWeapon& Out, FLog& Log)
	{
		ng::JValue J;
		std::string Err;
		if (!ng::LoadJsonFile(Path, J, Err))
		{
			Log.Line(FString::Printf(TEXT("ERROR %s"), *FromU8(Err)));
			return false;
		}
		for (const auto& KV : J.Obj)
		{
			std::vector<ng::V3> Verts;
			const ng::JValue& Vs = KV.second["verts"];
			for (size_t I = 0; I < Vs.Size(); ++I)
			{
				const ng::JValue& V = Vs.At(I);
				Verts.emplace_back(V.NumAt(0), V.NumAt(1), V.NumAt(2));
			}
			std::vector<int> Tris;
			for (const double T : KV.second["tris"].Numbers())
			{
				Tris.push_back(static_cast<int>(T));
			}
			Out.Parts.emplace_back();
			Out.Parts.back().Build(KV.first, Verts, Tris);
		}
		return !Out.Parts.empty();
	}

	void WriteLattice(const ng::Lattice& L, ng::JWriter& W)
	{
		W.BeginObject();
		const double O[3] = {L.Origin.x, L.Origin.y, L.Origin.z};
		W.Key("origin").Numbers(O, 3);
		W.Key("spacing").Value(L.Spacing);
		W.Key("dims").BeginArray().Value(L.Dims[0]).Value(L.Dims[1]).Value(L.Dims[2]).EndArray();
		W.Key("d").Numbers(L.D.data(), L.D.size());
		W.EndObject();
	}

	FString CompareLattice(const ng::Lattice& Got, const std::string& RefPath)
	{
		ng::Lattice Ref;
		std::string Err;
		if (!Ref.LoadJson(RefPath, Err))
		{
			return FString::Printf(TEXT("(no reference: %s)"), *FromU8(Err));
		}
		if (Ref.D.size() != Got.D.size() || Ref.Dims[0] != Got.Dims[0] || Ref.Dims[1] != Got.Dims[1] || Ref.Dims[2] != Got.Dims[2])
		{
			return TEXT("vs reference: DIFFERENT grid");
		}
		double Max = 0.0;
		int64 Diff = 0;
		for (size_t I = 0; I < Ref.D.size(); ++I)
		{
			const double D = FMath::Abs(Ref.D[I] - Got.D[I]);
			Max = FMath::Max(Max, D);
			Diff += D > 0.0 ? 1 : 0;
		}
		return FString::Printf(TEXT("vs reference: %lld of %d nodes differ, max |diff| %g"), Diff, static_cast<int32>(Ref.D.size()), Max);
	}

	bool BakeFields(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log)
	{
		Log.Stage(TEXT("fields: weapon parts, coarse lattice, fine narrow band"));
		ng::SdfWeapon Weapon;
		if (P.bAssets)
		{
			for (ng::NamedPart& Part : ng::SplitParts(P.WeaponTris))
			{
				Weapon.Parts.emplace_back();
				Weapon.Parts.back().Build(Part.Name, Part.Verts, Part.Tris);
			}
			Log.Line(FString::Printf(TEXT("weapon mesh: %d vertices, %d triangles -> %d parts"), static_cast<int32>(P.WeaponTris.Verts.size()),
				static_cast<int32>(P.WeaponTris.Tris.size() / 3), static_cast<int32>(Weapon.Parts.size())));
		}
		else if (!LoadPartsJson(P.Base.SavedDir + "/weapons/" + P.Base.Weapon + "_parts.json", Weapon, Log))
		{
			return false;
		}
		const ng::Lattice Coarse = ng::BakeCoarse(Weapon, P.CoarseSpacing, P.CoarseMargin, Ex);
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("coarse %d x %d x %d (%.2f s) %s"), Coarse.Dims[0], Coarse.Dims[1], Coarse.Dims[2], Log.Seconds(),
			*CompareLattice(Coarse, P.Base.SavedDir + "/fields/" + P.Base.Weapon + "_field.json")));
		ng::JWriter Wc(false);
		WriteLattice(Coarse, Wc);
		SaveJson(Wc, P.OutDir / TEXT("field_coarse.json"), Log);

		ng::Field CoarseOnly;
		CoarseOnly.Coarse = Coarse;
		const double T1 = FPlatformTime::Seconds();
		const ng::Lattice Fine = ng::BakeFine(Weapon, CoarseOnly, P.FineLo, P.FineHi, P.FineSpacing, P.FineBand, Ex);
		if (Ex.Cancelled())
		{
			return false;
		}
		const std::string FineRef = P.Base.SavedDir + "/fine_field_" + (P.bTrigger ? "right" : "left") + "_" + P.Base.Weapon + ".json";
		Log.Line(FString::Printf(TEXT("fine %d x %d x %d (%.2f s) %s"), Fine.Dims[0], Fine.Dims[1], Fine.Dims[2], FPlatformTime::Seconds() - T1,
			*CompareLattice(Fine, FineRef)));
		ng::JWriter Wf(false);
		WriteLattice(Fine, Wf);
		return SaveJson(Wf, P.OutDir / (P.bTrigger ? TEXT("field_fine_right.json") : TEXT("field_fine_left.json")), Log);
	}

	// ------------------------------------------------------------------ assets

	/** Input = Assets: bake the coarse field and this hand's fine field from the weapon mesh's triangles. */
	bool PrepareAssetFields(FNGProfileData& P, const ng::Exec& Ex, FLog& Log)
	{
		Log.Stage(TEXT("assets: fields from the weapon mesh"));
		ng::SdfWeapon Weapon;
		for (ng::NamedPart& Part : ng::SplitParts(P.WeaponTris))
		{
			Weapon.Parts.emplace_back();
			Weapon.Parts.back().Build(Part.Name, Part.Verts, Part.Tris);
		}
		if (Weapon.Parts.empty())
		{
			Log.Line(TEXT("ERROR the weapon mesh has no triangles"));
			return false;
		}
		auto Coarse = std::make_shared<ng::Lattice>(ng::BakeCoarse(Weapon, P.CoarseSpacing, P.CoarseMargin, Ex));
		ng::Field CoarseOnly;
		CoarseOnly.Coarse = *Coarse;
		auto Fine = std::make_shared<ng::Lattice>(ng::BakeFine(Weapon, CoarseOnly, P.FineLo, P.FineHi, P.FineSpacing, P.FineBand, Ex));
		if (Ex.Cancelled())
		{
			return false;
		}
		Log.Line(FString::Printf(TEXT("%d parts, coarse %d x %d x %d, fine %d x %d x %d (%.2f s); hand: %d skin vertices"),
			static_cast<int32>(Weapon.Parts.size()), Coarse->Dims[0], Coarse->Dims[1], Coarse->Dims[2], Fine->Dims[0], Fine->Dims[1],
			Fine->Dims[2], Log.Seconds(), static_cast<int32>(P.HeroHand.VertsHandSpace.size())));
		P.BakedCoarse = Coarse;
		P.BakedFine = Fine;
		return true;
	}

	double MaxXDiff(const std::vector<std::pair<std::string, ng::X>>& L, const std::vector<std::pair<std::string, ng::X>>& R, int32& Missing)
	{
		double Max = 0.0;
		for (const auto& KV : R)
		{
			const auto It = std::find_if(L.begin(), L.end(), [&KV](const std::pair<std::string, ng::X>& E) { return E.first == KV.first; });
			if (It == L.end())
			{
				++Missing;
				continue;
			}
			const ng::X& X0 = It->second;
			const ng::X& X1 = KV.second;
			const double D[7] = {X0.t.x - X1.t.x, X0.t.y - X1.t.y, X0.t.z - X1.t.z, X0.q.x - X1.q.x, X0.q.y - X1.q.y, X0.q.z - X1.q.z, X0.q.w - X1.q.w};
			for (const double V : D)
			{
				Max = FMath::Max(Max, FMath::Abs(V));
			}
		}
		return Max;
	}

	/** Input = Assets: what was read from the assets vs the Python dump files (they must be identical). */
	bool CompareInputs(const FNGProfileData& P, const ng::Exec& Ex, FLog& Log)
	{
		Log.Stage(TEXT("inputs: assets vs the Python dump files"));
		ng::HandData Py;
		std::string Err;
		Log.Line(FString::Printf(TEXT("hand: %d skin vertices read from the hero mesh; weapon mesh: %d vertices, %d triangles"),
			static_cast<int32>(P.HeroHand.VertsHandSpace.size()), static_cast<int32>(P.WeaponTris.Verts.size()), static_cast<int32>(P.WeaponTris.Tris.size() / 3)));
		if (P.bHasHold)
		{
			Log.Line(FString::Printf(TEXT("hold from the clips: hand at (%.2f, %.2f, %.2f) in weapon space"), P.Hold.HandInWeapon.t.x, P.Hold.HandInWeapon.t.y,
				P.Hold.HandInWeapon.t.z));
		}
		const bool bHasDumps = ng::ReadHandDataJson(P.Base.SavedDir, P.Base.Side, Py, Err)
			&& ng::FileExists(P.Base.SavedDir + "/weapons/" + P.Base.Weapon + "_parts.json");
		if (!bHasDumps)
		{
			// a new weapon: no Python dumps to compare with - the fields are baked to show they build
			FNGProfileData Pd = P;
			const bool bFields = PrepareAssetFields(Pd, Ex, Log);
			Log.Line(bFields ? TEXT("INPUTS: read (no Python dumps for this weapon to compare with)") : TEXT("INPUTS: the fields did not build"));
			return bFields;
		}
		const ng::HandData& A = P.HeroHand;
		const bool bBones = A.Bones == Py.Bones && A.Parents == Py.Parents;
		int32 Missing = 0;
		const double DRef = MaxXDiff(A.RefLocalMesh, Py.RefLocalMesh, Missing);
		const double DBind = MaxXDiff(A.BoneHandSpace, Py.BoneHandSpace, Missing);
		double DVerts = A.VertsHandSpace.size() == Py.VertsHandSpace.size() ? 0.0 : 1e300;
		for (size_t I = 0; I < A.VertsHandSpace.size() && I < Py.VertsHandSpace.size(); ++I)
		{
			DVerts = FMath::Max(DVerts, FMath::Max3(FMath::Abs(A.VertsHandSpace[I].x - Py.VertsHandSpace[I].x),
				FMath::Abs(A.VertsHandSpace[I].y - Py.VertsHandSpace[I].y), FMath::Abs(A.VertsHandSpace[I].z - Py.VertsHandSpace[I].z)));
		}
		Log.Line(FString::Printf(TEXT("hand bones + parents: %s"), bBones ? TEXT("identical") : TEXT("DIFFERENT")));
		Log.Line(FString::Printf(TEXT("finger ref locals max |diff| %g, bind pose max |diff| %g, missing %d"), DRef, DBind, Missing));
		Log.Line(FString::Printf(TEXT("skin vertices: %d (dump %d), max |diff| %g"), static_cast<int32>(A.VertsHandSpace.size()),
			static_cast<int32>(Py.VertsHandSpace.size()), DVerts));
		bool bOk = bBones && Missing == 0 && DRef <= 1e-9 && DBind <= 1e-9 && DVerts <= 1e-9;

		// weapon parts
		const std::vector<ng::NamedPart> Parts = ng::SplitParts(P.WeaponTris);
		ng::JValue J;
		if (!ng::LoadJsonFile(P.Base.SavedDir + "/weapons/" + P.Base.Weapon + "_parts.json", J, Err))
		{
			Log.Line(FString::Printf(TEXT("ERROR %s"), *FromU8(Err)));
			return false;
		}
		bool bParts = J.Obj.size() == Parts.size();
		double DParts = 0.0;
		for (size_t K = 0; bParts && K < Parts.size(); ++K)
		{
			const ng::JValue& Pj = J.Obj[K].second;
			const ng::JValue& Vs = Pj["verts"];
			bParts = J.Obj[K].first == Parts[K].Name && Vs.Size() == Parts[K].Verts.size() && Pj["tris"].Size() == Parts[K].Tris.size();
			for (size_t I = 0; bParts && I < Vs.Size(); ++I)
			{
				const ng::JValue& V = Vs.At(I);
				for (int C = 0; C < 3; ++C)
				{
					DParts = FMath::Max(DParts, FMath::Abs(V.NumAt(static_cast<size_t>(C)) - Parts[K].Verts[I][C]));
				}
			}
		}
		Log.Line(FString::Printf(TEXT("weapon mesh: %d vertices, %d triangles -> %d parts: %s, max |diff| %g"), static_cast<int32>(P.WeaponTris.Verts.size()),
			static_cast<int32>(P.WeaponTris.Tris.size() / 3), static_cast<int32>(Parts.size()),
			bParts ? TEXT("same parts as the dump") : TEXT("DIFFERENT parts"), DParts));
		bOk = bOk && bParts && DParts <= 1e-9;

		// fields baked from the asset vs the Python fields
		FNGProfileData Pd = P;
		if (!PrepareAssetFields(Pd, Ex, Log))
		{
			return false;
		}
		const FString CoarseCmp = CompareLattice(*Pd.BakedCoarse, P.Base.SavedDir + "/fields/" + P.Base.Weapon + "_field.json");
		const FString FineCmp = CompareLattice(*Pd.BakedFine, P.Base.SavedDir + "/fine_field_" + (P.bTrigger ? "right" : "left") + "_" + P.Base.Weapon + ".json");
		Log.Line(TEXT("coarse field ") + CoarseCmp);
		Log.Line(TEXT("fine field ") + FineCmp);
		bOk = bOk && CoarseCmp.StartsWith(TEXT("vs reference: 0 of")) && FineCmp.StartsWith(TEXT("vs reference: 0 of"));
		Log.Line(bOk ? TEXT("INPUTS: identical to the Python dump files") : TEXT("INPUTS: DIFFERENT"));
		return bOk;
	}

	// ------------------------------------------------------------------ parity

	void ParityLine(FLog& Log, const FString& Label, const std::string& RefPath, const FString& GotPath, bool bPrefix, bool& bAllOk)
	{
		ng::JValue Ref, Got;
		std::string Err;
		if (!ng::LoadJsonFile(RefPath, Ref, Err) || !ng::LoadJsonFile(U8(GotPath), Got, Err))
		{
			Log.Line(FString::Printf(TEXT("%-28s ERROR %s"), *Label, *FromU8(Err)));
			bAllOk = false;
			return;
		}
		ng::JsonDiffReport R;
		ng::JsonCompare(Ref, Got, R, bPrefix);
		Log.Line(FString::Printf(TEXT("%-28s %s"), *Label, *FromU8(R.Summary(1e-9))));
		bAllOk = bAllOk && R.Ok(1e-9);
	}
}

bool NGJobs::Snapshot(const UAZNaturalGripProfile& Profile, FNGProfileData& Out, FString& Error)
{
	Out = FNGProfileData();
	Out.Name = Profile.GetName();
	Out.bTrigger = Profile.IsTriggerRole();
	Out.bHandle = Profile.Hand == EAZGripHand::Handle;
	Out.bPistolCup = Profile.Hand == EAZGripHand::PistolCup;
	Out.WeaponFwd = ng::Norm(ng::V3(Profile.WeaponForward.X, Profile.WeaponForward.Y, Profile.WeaponForward.Z));
	Out.Base.SavedDir = U8(Profile.ResolveDataDir());
	Out.Base.Side = Profile.SideChar();
	Out.Base.Weapon = U8(Profile.WeaponKey);
	Out.Base.LeftPick = U8(Profile.LeftPick);
	Out.SearchThin = Profile.SearchThin;
	Out.FinalThin = Profile.FinalThin;
	Out.OutDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaturalGrip"), Out.Name));
	Out.ReferenceDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/wgs/natgrip")));
	Out.CoarseSpacing = Profile.CoarseSpacing;
	Out.CoarseMargin = Profile.CoarseMargin;
	Out.FineLo = ng::V3(Profile.FineBoxMin.X, Profile.FineBoxMin.Y, Profile.FineBoxMin.Z);
	Out.FineHi = ng::V3(Profile.FineBoxMax.X, Profile.FineBoxMax.Y, Profile.FineBoxMax.Z);
	Out.FineSpacing = Profile.FineSpacing;
	Out.FineBand = Profile.FineBand;
	Out.Trigger = ng::V3(Profile.TriggerPoint.X, Profile.TriggerPoint.Y, Profile.TriggerPoint.Z);
	Out.Pull = ng::V3(Profile.PullDirection.X, Profile.PullDirection.Y, Profile.PullDirection.Z);
	Out.GripX = Profile.GripX;
	Out.ThumbSide = Profile.ThumbSide;
	Out.ThumbZMax = Profile.ThumbZMax;
	Out.WrapFrontY = Profile.WrapFrontY;
	if (Profile.Hand == EAZGripHand::TriggerStraight)
	{
		// a straight wrist has no pistol-grip front face and the thumb wraps over the top: no limits unless marked
		Out.WrapFrontY = 99.0;
		Out.ThumbZMax = 99.0;
	}
	Profile.SearchCentre.ToArray(Out.SearchCentre);
	Profile.TriggerPlacement.ToArray(Out.TriggerP);
	Out.IndexStart[0] = Profile.IndexStart.X;
	Out.IndexStart[1] = Profile.IndexStart.Y;
	Out.IndexStart[2] = Profile.IndexStart.Z;
	Out.IndexStart[3] = Profile.IndexStart.W;
	Out.AxX = Profile.HandguardAxisXZ.X;
	Out.AxZ = Profile.HandguardAxisXZ.Y;
	Out.Stage2Top = Profile.Stage2Top;
	Out.Stage2Step = Profile.Stage2Step;
	Profile.SupportPlacement.ToArray(Out.SupportP);
	Out.bPowerGrasp = Profile.bPowerGrasp;
	Out.PowerCandidates = Profile.PowerCandidates;
	if (Profile.Input == EAZGripInputSource::Assets)
	{
		USkeletalMesh* Weapon = Profile.WeaponMesh.LoadSynchronous();
		if (!Weapon)
		{
			Error = TEXT("Input = Assets but the profile has no weapon mesh");
			return false;
		}
		if (!NGAssetReaders::ReadMeshTriangles(Weapon, Out.WeaponTris, Error))
		{
			return false;
		}
		USkeletalMesh* Hero = Profile.HeroMesh.LoadSynchronous();
		if (!Hero)
		{
			Error = TEXT("Input = Assets but the profile has no hero mesh");
			return false;
		}
		if (!NGAssetReaders::ReadHand(Hero, Out.Base.Side, Out.HeroHand, Error))
		{
			return false;
		}
		Out.bAssets = true;

		// marker sockets on the weapon mesh replace the typed numbers
		ng::V3 M;
		TArray<FString> MissingMarkers;            // markers this role reads that the weapon mesh lacks
		if (NGAssetReaders::ReadMarker(Weapon, Profile.TriggerMarker, M))
		{
			Out.Trigger = M;
			Out.GripX = M.x;
			Out.InputLog += FString::Printf(TEXT("marker %s: trigger (%.2f, %.2f, %.2f), grip X %.2f\n"), *Profile.TriggerMarker.ToString(), M.x, M.y, M.z, M.x);
		}
		else if (Out.bTrigger)
		{
			MissingMarkers.Add(Profile.TriggerMarker.ToString());
		}
		if (NGAssetReaders::ReadMarker(Weapon, Profile.GripFrontMarker, M))
		{
			Out.WrapFrontY = M.y;
			Out.InputLog += FString::Printf(TEXT("marker %s: grip front Y %.2f\n"), *Profile.GripFrontMarker.ToString(), M.y);
		}
		else if (Profile.Hand == EAZGripHand::Trigger)
		{
			MissingMarkers.Add(Profile.GripFrontMarker.ToString());
		}
		if (NGAssetReaders::ReadMarker(Weapon, Profile.ThumbLimitMarker, M))
		{
			Out.ThumbZMax = M.z;
			Out.InputLog += FString::Printf(TEXT("marker %s: thumb limit Z %.2f\n"), *Profile.ThumbLimitMarker.ToString(), M.z);
		}
		else if (Profile.Hand == EAZGripHand::Trigger)
		{
			MissingMarkers.Add(Profile.ThumbLimitMarker.ToString());
		}
		ng::V3 Hb, Hf;
		if (NGAssetReaders::ReadMarker(Weapon, Profile.HandleBackMarker, Hb) && NGAssetReaders::ReadMarker(Weapon, Profile.HandleFrontMarker, Hf))
		{
			Out.HandleBack = Hb;
			Out.HandleFront = Hf;
			Out.bHasHandleAxis = true;
			Out.InputLog += FString::Printf(TEXT("markers %s / %s: handle axis (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n"), *Profile.HandleBackMarker.ToString(),
				*Profile.HandleFrontMarker.ToString(), Hb.x, Hb.y, Hb.z, Hf.x, Hf.y, Hf.z);
		}
		if (NGAssetReaders::ReadMarker(Weapon, Profile.HandguardMarker, M))
		{
			Out.AxX = M.x;
			Out.AxZ = M.z;
			Out.InputLog += FString::Printf(TEXT("marker %s: handguard axis X %.2f, Z %.2f\n"), *Profile.HandguardMarker.ToString(), M.x, M.z);
		}
		else if (Profile.Hand == EAZGripHand::Support)
		{
			MissingMarkers.Add(Profile.HandguardMarker.ToString());
		}
		// a weapon whose hold comes from its clips has no typed numbers of its own (the defaults are the M16's): every
		// marker its role reads is required, or the M16's geometry would silently solve it
		if (!Profile.ClipFolder.Path.IsEmpty() && MissingMarkers.Num() > 0)
		{
			Error = FString::Printf(TEXT("the weapon mesh %s has no marker socket %s - add it on the mesh (or its skeleton) where the hand meets the weapon; without it the M16's numbers would be used"),
				*Weapon->GetName(), *FString::Join(MissingMarkers, TEXT(", ")));
			return false;
		}

		// the pistol second hand lies on the right hand: its last solve, as capsules
		if (Out.bPistolCup)
		{
			UAZNaturalGripProfile* Other = Profile.OtherHand.LoadSynchronous();
			const FString OtherPath = Other ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaturalGrip"), Other->GetName(), TEXT("trigger_solve.json")) : FString();
			ng::JValue J;
			std::string Err;
			if (Other && ng::LoadJsonFile(U8(OtherPath), J, Err))
			{
				ng::HandData RightHand;
				if (!NGAssetReaders::ReadHand(Hero, 'r', RightHand, Error))
				{
					return false;
				}
				ng::HoldData RightHold;
				const ng::JValue& H = J["hand_in_weapon"];
				RightHold.HandInWeapon = ng::X(ng::Q(H.NumAt(3), H.NumAt(4), H.NumAt(5), H.NumAt(6)), ng::V3(H.NumAt(0), H.NumAt(1), H.NumAt(2)));
				for (const auto& KV : J["locals"].Obj)
				{
					// the solve keeps rotations only: the translations are the mesh ref pose's
					ng::X L;
					L.q = ng::Q(KV.second.NumAt(0), KV.second.NumAt(1), KV.second.NumAt(2), KV.second.NumAt(3));
					for (const auto& Ref : RightHand.RefLocalMesh)
					{
						if (Ref.first == KV.first)
						{
							L.t = Ref.second.t;
						}
					}
					RightHold.Locals.emplace_back(KV.first, L);
				}
				ng::Lattice Dummy;
				Dummy.Dims[0] = Dummy.Dims[1] = Dummy.Dims[2] = 2;
				Dummy.D.assign(8, 10.0);
				ng::SetupConfig Cfg = Out.Base;
				Cfg.Side = 'r';
				Cfg.bFine = false;
				ng::SetupInputs In;
				In.Hand = &RightHand;
				In.Coarse = &Dummy;
				In.Hold = &RightHold;
				auto Su = std::make_unique<ng::Setup>();
				if (!Su->Load(Cfg, Err, In))
				{
					Error = FString::Printf(TEXT("the right hand of %s: %s"), *Other->GetName(), *FromU8(Err));
					return false;
				}
				Out.Obstacles = ng::HandCapsules(Su->Hand, Su->Plc.HandW, Su->Plc.Clip);
				Out.InputLog += FString::Printf(TEXT("the right hand (%s, %d capsules) is the surface for this hand\n"), *OtherPath, static_cast<int32>(Out.Obstacles.size()));
			}
			else
			{
				Out.InputLog += TEXT("no right-hand solve found (set OtherHand and solve that profile first)\n");
			}
		}

		// the hold from the weapon's clips
		if (!Profile.ClipFolder.Path.IsEmpty() && Profile.WeaponSocketOnHero != NAME_None)
		{
			NGAssetReaders::FHoldSampling Hs;
			Hs.ClipFolder = Profile.ClipFolder.Path.StartsWith(TEXT("/")) ? Profile.ClipFolder.Path : TEXT("/Game/") + Profile.ClipFolder.Path;
			Hs.WeaponSocket = Profile.WeaponSocketOnHero;
			Profile.AimClipFilter.ParseIntoArray(Hs.AimFilters, TEXT(","));
			Profile.AimClipExclude.ParseIntoArray(Hs.AimExcludes, TEXT(","));
			Profile.RelaxedClipFilter.ParseIntoArray(Hs.RelaxedFilters, TEXT(","));
			Hs.FramesPerClip = Profile.FramesPerClip;
			Hs.Side = Out.Base.Side;
			if (!NGAssetReaders::SampleHold(Hero, Hs, Out.Hold, Out.InputLog, Error))
			{
				return false;
			}
			Out.bHasHold = true;
			if (Profile.bAutoFineBox)
			{
				const ng::V3& C = Out.Hold.HandInWeapon.t;
				const double R = Profile.FineBoxHalfSize;
				Out.FineLo = ng::V3(C.x - R, C.y - R, C.z - R);
				Out.FineHi = ng::V3(C.x + R, C.y + R, C.z + R);
				Out.InputLog += FString::Printf(TEXT("fine field box = the hand (%.1f, %.1f, %.1f) +- %.0f cm\n"), C.x, C.y, C.z, R);
			}
		}
	}
	return true;
}

TArray<FString> NGJobs::StagesFor(EAZGripHand Role)
{
	if (Role == EAZGripHand::Handle || Role == EAZGripHand::PistolCup)
	{
		return {TEXT("fields"), TEXT("power"), TEXT("inputs")};
	}
	if (Role == EAZGripHand::Trigger || Role == EAZGripHand::TriggerStraight)
	{
		return {TEXT("fields"), TEXT("placement"), TEXT("solve"), TEXT("parity"), TEXT("inputs")};
	}
	return {TEXT("fields"), TEXT("power"), TEXT("stage1"), TEXT("stage2"), TEXT("final"), TEXT("polish"), TEXT("parity"), TEXT("inputs")};
}

FString NGJobs::Run(const FNGProfileData& P, const FString& Stage, const ng::Exec& Ex)
{
	FLog Log;
	Log.Line(FString::Printf(TEXT("[%s] %s, %s hand"), *P.Name, *Stage, P.bTrigger ? TEXT("trigger") : (P.bHandle ? TEXT("handle") : TEXT("support"))));
	if (!P.InputLog.IsEmpty() && !P.BakedCoarse)
	{
		Log.Text += P.InputLog;
	}
	const double TStart = FPlatformTime::Seconds();
	bool bOk = true;
	if (Stage == TEXT("fields"))
	{
		bOk = BakeFields(P, Ex, Log);
	}
	else if (Stage == TEXT("inputs"))
	{
		if (!P.bAssets)
		{
			Log.Line(TEXT("ERROR 'inputs' compares what is read from the assets: set the profile's Input to Assets"));
			bOk = false;
		}
		else
		{
			bOk = CompareInputs(P, Ex, Log);
		}
	}
	else if (P.bAssets && !P.BakedCoarse)
	{
		FNGProfileData Pd = P;
		if (!PrepareAssetFields(Pd, Ex, Log))
		{
			Log.Line(TEXT("stopped"));
			return Log.Text;
		}
		return Log.Text + Run(Pd, Stage, Ex);
	}
	else if (P.bNotYet)
	{
		Log.Line(TEXT("ERROR this role (the pistol second hand) has no solver yet"));
		bOk = false;
	}
	else if (P.bHandle || P.bPistolCup)
	{
		if (!P.bAssets || !P.bHasHold || !P.bHasHandleAxis)
		{
			Log.Line(TEXT("ERROR this role needs Input = Assets, a clip folder + weapon socket (the hold) and the marker sockets NG_HandleBack / NG_HandleFront (the handle / grip axis) on the weapon mesh"));
			bOk = false;
		}
		else if (P.bPistolCup && P.Obstacles.empty())
		{
			Log.Line(TEXT("ERROR the pistol second hand needs OtherHand = the right hand's profile, solved first (its trigger_solve.json)"));
			bOk = false;
		}
		else if (Stage == TEXT("power") || Stage == TEXT("support"))
		{
			FString FinalPath;
			bOk = SupportPowerChain(P, Ex, Log, FinalPath);
		}
		else
		{
			Log.Line(FString::Printf(TEXT("ERROR unknown stage '%s' for the handle role"), *Stage));
			bOk = false;
		}
	}
	else if (!P.bTrigger)
	{
		std::vector<ng::SupportPlacement> S1;
		std::vector<ng::SupportStage2Result> S2;
		FString S1Path, S2Path, FinalPath;
		if (Stage == TEXT("stage1"))
		{
			bOk = SupportStage1(P, Ex, Log, S1, S1Path);
		}
		else if (Stage == TEXT("stage2"))
		{
			bOk = SupportStage1(P, Ex, Log, S1, S1Path) && SupportStage2(P, Ex, Log, S1, S2, S2Path);
		}
		else if (Stage == TEXT("final"))
		{
			bOk = SupportFinal(P, P.SupportP, Ex, Log, FinalPath);
		}
		else if (Stage == TEXT("polish"))
		{
			bOk = SupportFinal(P, P.SupportP, Ex, Log, FinalPath, true);
		}
		else if (Stage == TEXT("power") || (Stage == TEXT("support") && P.bPowerGrasp))
		{
			bOk = SupportPowerChain(P, Ex, Log, FinalPath);
		}
		else if (Stage == TEXT("support"))
		{
			bOk = SupportStage1(P, Ex, Log, S1, S1Path) && SupportStage2(P, Ex, Log, S1, S2, S2Path);
			if (bOk && !S2.empty())
			{
				size_t Best = 0;
				for (size_t I = 1; I < S2.size(); ++I)
				{
					Best = S2[I].J2 < S2[Best].J2 ? I : Best;
				}
				bOk = SupportFinal(P, S2[Best].Cand.P, Ex, Log, FinalPath, true);
			}
		}
		else if (Stage == TEXT("parity"))
		{
			// the Python runs: stage 1 (thin 2, coarse field), stage 2 (top 24, step 8), final at the applied L2 placement
			bOk = SupportStage1(P, Ex, Log, S1, S1Path) && SupportStage2(P, Ex, Log, S1, S2, S2Path) && SupportFinal(P, P.SupportP, Ex, Log, FinalPath);
			if (bOk)
			{
				Log.Stage(TEXT("parity vs the Python solver"));
				const std::string Ref = U8(P.ReferenceDir);
				bool bAll = true;
				ParityLine(Log, TEXT("stage 1 (top 400)"), Ref + "/lsolve_stage1.json", S1Path, true, bAll);
				ParityLine(Log, TEXT("stage 2"), Ref + "/lsolve_stage2_0.json", S2Path, false, bAll);
				ParityLine(Log, TEXT("final (L2)"), Ref + "/lsolve_m16_L2.json", FinalPath, false, bAll);
				Log.Line(bAll ? TEXT("PARITY: PASS (identical to the Python solver)") : TEXT("PARITY: FAIL"));
				bOk = bAll;
			}
		}
		else
		{
			Log.Line(FString::Printf(TEXT("ERROR unknown stage '%s' for the support hand"), *Stage));
			bOk = false;
		}
	}
	else
	{
		std::vector<ng::PlaceNatEntry> Places;
		FString PlacePath, SolvePath;
		if (Stage == TEXT("placement"))
		{
			bOk = TriggerPlacement(P, Ex, Log, Places, PlacePath);
		}
		else if (Stage == TEXT("solve"))
		{
			bOk = TriggerSolve(P, P.TriggerP, P.IndexStart, Ex, Log, SolvePath);
		}
		else if (Stage == TEXT("trigger"))
		{
			bOk = TriggerPlacement(P, Ex, Log, Places, PlacePath);
			if (bOk && !Places.empty())
			{
				size_t Best = 0;
				for (size_t I = 1; I < Places.size(); ++I)
				{
					Best = Places[I].J < Places[Best].J ? I : Best;
				}
				bOk = TriggerSolve(P, Places[Best].P, Places[Best].Idx, Ex, Log, SolvePath);
			}
		}
		else if (Stage == TEXT("parity"))
		{
			// the Python runs: place_nat over the whole grid, rsolve3 at the applied N2 placement
			bOk = TriggerPlacement(P, Ex, Log, Places, PlacePath) && TriggerSolve(P, P.TriggerP, P.IndexStart, Ex, Log, SolvePath);
			if (bOk)
			{
				Log.Stage(TEXT("parity vs the Python solver"));
				const std::string Ref = U8(P.ReferenceDir);
				bool bAll = true;
				ParityLine(Log, TEXT("placement (all candidates)"), Ref + "/pn_all.json", PlacePath, false, bAll);
				ParityLine(Log, TEXT("solve (N2)"), Ref + "/rsolve3_m16_N2.json", SolvePath, false, bAll);
				Log.Line(bAll ? TEXT("PARITY: PASS (identical to the Python solver)") : TEXT("PARITY: FAIL"));
				bOk = bAll;
			}
		}
		else
		{
			Log.Line(FString::Printf(TEXT("ERROR unknown stage '%s' for the trigger hand"), *Stage));
			bOk = false;
		}
	}
	if (Ex.Cancelled())
	{
		Log.Line(TEXT("CANCELLED"));
	}
	Log.Line(FString::Printf(TEXT("%s in %.2f s"), bOk ? TEXT("done") : TEXT("stopped"), FPlatformTime::Seconds() - TStart));
	return Log.Text;
}
