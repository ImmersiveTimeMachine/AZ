// Copyright Artur. AZ project.
// Natural Grip solver core: the support (left) hand solver.
// Port of Tools/wgs/natgrip/lsolve_m16.py (corr_hand, angle, bend, around, palm_angle, wrap_deg, finger_W, finger_opt,
// solve_fingers, stage1_eval and the stage1 / stage2 / final modes), left_solve.thumb_left and finger_gaps.report.
// Every formula is copied operation for operation (double precision, Python's min / max / floored modulo / compensated
// sum() rules) so the results match the Python solver exactly; see NGParallel.h for the determinism rule of the loops.
// Results are identical for any thread count. When Exec::Cancelled() turns true the solvers return early with incomplete
// results (the caller must check it).
#pragma once

#include "NGHand.h"
#include "NGJson.h"
#include "NGMath.h"
#include "NGParallel.h"
#include "NGSetup.h"

#include <string>
#include <vector>

namespace ng
{
	/** lsolve_m16.py module constants + the clips' elbows (Saved/wgs/<weapon>_left_picks.json). */
	struct SupportSettings
	{
		double AxX = -1.1;                 // handguard axis (x, z); it runs along +y
		double AxZ = 11.85;
		// a general axis (a knife handle, a pistol grip): origin + unit direction toward the front (blade, muzzle); the
		// angles around it use the basis E1, E2. Off = the handguard along +Y through (AxX, AxZ), as the Python solver.
		bool bGeneralAxis = false;
		V3 AxisOrigin, AxisDir = V3(0.0, 1.0, 0.0), AxisE1 = V3(1.0, 0.0, 0.0), AxisE2 = V3(0.0, 0.0, 1.0);

		/** A general axis from two points (A = back / butt end, B = front end). */
		void SetAxis(const V3& A, const V3& B)
		{
			bGeneralAxis = true;
			AxisOrigin = A;
			AxisDir = Norm(Sub(B, A));
			// E1 = weapon X made perpendicular to the axis (Z when the axis runs along X); E2 completes the frame
			const V3 Ref = std::fabs(AxisDir.x) < 0.9 ? V3(1.0, 0.0, 0.0) : V3(0.0, 0.0, 1.0);
			AxisE1 = Norm(Sub(Ref, Mul(AxisDir, Dot(Ref, AxisDir))));
			AxisE2 = Cross(AxisE1, AxisDir);
			if (Dot(AxisE2, V3(0.0, 0.0, 1.0)) < 0.0 && std::fabs(AxisDir.z) < 0.9)
			{
				AxisE2 = Mul(AxisE2, -1.0);
			}
		}
		V3 Origin() const { return bGeneralAxis ? AxisOrigin : V3(AxX, 0.0, AxZ); }
		/** The power thumb lies along this, pointing forward (default: the axis; the pistol second hand: the barrel). */
		bool bThumbFwd = false;
		V3 ThumbFwd = V3(0.0, 1.0, 0.0);
		V3 ThumbDir() const { return bThumbFwd ? ThumbFwd : Dir(); }
		V3 Dir() const { return bGeneralAxis ? AxisDir : V3(0.0, 1.0, 0.0); }
		double Contact = 0.08;             // CONTACT
		V3 ElbowAim;                       // PICKS["aim"]["elbow"]
		V3 ElbowRelaxed;                   // PICKS["relaxed"]["elbow"]
		// finger score weights (defaults = lsolve_m16.py; the power grasp seats the proximals harder and rewards the full wrap)
		double RestW[3] = {1.0, 1.5, 1.0}; // proximal, middle, distal phalanx resting
		double WrapCap = 160.0;            // min(wrap, cap) / div
		double WrapDiv = 40.0;
		double ThumbOpposeW = 2.0;         // power thumb: weight of the opposition reward

		/** The power grasp weights (2026-10-03, user: "when you shoot you hold it firmly"). */
		void SetPower()
		{
			RestW[0] = 2.5;
			RestW[1] = 1.5;
			RestW[2] = 1.0;
			WrapCap = 180.0;
			WrapDiv = 30.0;
		}
	};

	/** Reads the "aim" / "relaxed" elbows of <SavedDir>/<Weapon>_left_picks.json. */
	bool LoadSupportElbows(const std::string& SavedDir, const std::string& Weapon, SupportSettings& Out, std::string& Error);

	/** stage1_eval record: {p, palm, bend:[aim, relaxed], dev, J1}. */
	struct SupportPlacement
	{
		double P[6] = {0, 0, 0, 0, 0, 0};  // yaw, pitch, roll (deg), dx, dy, dz (cm)
		double Palm = 0.0;
		double Bend[2] = {0.0, 0.0};
		double Dev = 0.0;
		double J1 = 0.0;
	};

	/** stage1_eval for one placement; false when it is rejected (palm out of range). */
	bool SupportStage1Eval(const Setup& Su, const SupportSettings& St, const double P[6], SupportPlacement& Out);

	/** Stage 1: every valid placement of the 6-D grid, stable-sorted by J1 (the caller takes the top 400). */
	void SupportStage1(const Setup& Su, const SupportSettings& St, const Exec& Ex, std::vector<SupportPlacement>& Out);

	/** One long finger of solve_fingers' rec (null in Python = bValid false). */
	struct SupportFinger
	{
		bool bValid = false;
		double Phi = 0.0;
		double A[3] = {0.0, 0.0, 0.0};     // MCP, PIP, DIP absolute degrees
		double Gaps[3] = {0.0, 0.0, 0.0};
		bool bHasAdj = false;              // only when there is a previous finger
		double Adj[3] = {0.0, 0.0, 0.0};   // gaps to the previous finger (proximal, middle, distal)
		double Wrap = 0.0;
		double Score = 0.0;
	};

	/** left_solve.thumb_left: best = (s, (abd, fl, m, k * m), g3). */
	struct SupportThumb
	{
		bool bValid = false;
		double Abd = 0.0;                  // integers on the exhaustive grid, continuous in part B
		double Fl = 0.0;
		double M = 0.0;
		double Ip = 0.0;
		double PadGap = 0.0;
		double Score = 0.0;
	};

	/** solve_fingers result: (total, rec, locs). Finger[Index..Pinky] are used; Finger[Thumb] stays invalid. */
	struct SupportSolve
	{
		SupportFinger Finger[NumFingers];
		SupportThumb Thumb;
		double Total = 0.0;
		double PalmAngle = 0.0;
		Locals Locs;
	};

	/** solve_fingers: the four long fingers in turn, then the thumb. Parallel over phi (fingers) and abd (thumb). */
	void SolveSupportFingers(const Setup& Su, const SupportSettings& St, const X& Hand, const std::vector<double>& Phis,
	                         double Step, const std::vector<double>& Ratios, const Exec& Ex, SupportSolve& Out);

	/** left_solve.thumb_left over the capsules of the placed fingers. */
	void SupportThumbLeft(const Setup& Su, const X& Hand, const std::vector<Seg>& Others, const Exec& Ex, SupportThumb& Out);

	/** One stage-2 record: the stage-1 fields + the finger solve; J2 = J1 - F. */
	struct SupportStage2Result
	{
		SupportPlacement Cand;
		SupportSolve Solve;
		double J2 = 0.0;
	};

	/** Stage 2: solve_fingers for every candidate (parallel over the candidates). */
	void SupportStage2(const Setup& Su, const SupportSettings& St, const std::vector<SupportPlacement>& Cands,
	                   const std::vector<double>& Phis, double Step, const std::vector<double>& Ratios, const Exec& Ex,
	                   std::vector<SupportStage2Result>& Out);

	/** The "final" mode: one placement, fine solve. */
	struct SupportFinalResult
	{
		double P[6] = {0, 0, 0, 0, 0, 0};
		SupportSolve Solve;
		double Palm = 0.0;                 // rec["palm"]
		double Bend[2] = {0.0, 0.0};       // rec["bend"]
		X HandInWeapon;                    // hand_new(corr(p))
	};
	void SolveSupportFinal(const Setup& Su, const SupportSettings& St, const double P[6], const std::vector<double>& Phis,
	                       double Step, const std::vector<double>& Ratios, const Exec& Ex, SupportFinalResult& Out);

	/** Part B (docs/design-briefs/natural-grip-cpp-plugin.md): the same scores, searched by a coarse grid + pattern search
	 *  from the best distinct cells (NGOptim.h) instead of the exhaustive fine grid; answers are continuous. */
	struct SupportFastOptions
	{
		/** true (default): start from the exhaustive grid's best (part A) and polish it with the pattern search, so every
		 *  finger and the thumb come out at least as good as part A; false: coarse grid + pattern search from the best
		 *  distinct cells only (fewer evaluations, but it misses part A's best on rugged contact scores). */
		bool bPolishGrid = true;
		std::vector<double> GridPhis = {-10.0, -7.5, -5.0, -2.5, 0.0, 2.5, 5.0, 7.5, 10.0, 12.5, 15.0};
		double GridStep = 3.0;
		std::vector<double> GridRatios = {0.5, 0.65, 0.8, 0.95};
		std::vector<double> PolishSteps = {1.5, 0.75, 0.3, 0.1};
		std::vector<double> ThumbPolishSteps = {5.0, 2.5, 1.0, 0.5, 0.2};
		std::vector<double> CoarsePhis = {-10.0, -5.0, 0.0, 5.0, 10.0, 15.0};
		double CoarseStep = 15.0;                                   // MCP / PIP grid step (degrees)
		std::vector<double> CoarseRatios = {0.5, 0.65, 0.8, 0.95};  // DIP / PIP
		int Seeds = 6;                                              // refined starts per finger
		double PhiLo = -10.0, PhiHi = 15.0, RatioLo = 0.5, RatioHi = 0.95;
		std::vector<double> Steps = {7.5, 3.75, 1.5, 0.75, 0.3};   // pattern steps (degrees; ratio steps = /100)
		std::vector<double> ThumbAbd = {-40.0, -20.0, 0.0, 20.0, 40.0};
		std::vector<double> ThumbFlex = {-40.0, -20.0, 0.0, 20.0, 40.0};
		std::vector<double> ThumbMcp = {0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0};
		std::vector<double> ThumbIpRatio = {0.6, 0.8, 1.0};
		int ThumbSeeds = 3;
		std::vector<double> ThumbSteps = {10.0, 5.0, 2.5, 1.0, 0.5};
	};

	/** solve_fingers, part B. OutEvals (optional) = configurations evaluated. */
	void SolveSupportFingersFast(const Setup& Su, const SupportSettings& St, const X& Hand, const SupportFastOptions& O, const Exec& Ex,
	                             SupportSolve& Out, long long* OutEvals = nullptr);
	/** The final solve, part B. */
	void SolveSupportFinalFast(const Setup& Su, const SupportSettings& St, const double P[6], const SupportFastOptions& O, const Exec& Ex,
	                           SupportFinalResult& Out, long long* OutEvals = nullptr);

	/** Power grasp of the support hand (not in the Python solver): the handguard seated in the hand (palm and proximal
	 *  phalanges on it), the fingers wrapping up to the far side (wrap reward to 180 deg), the thumb pad OPPOSITE the finger
	 *  pads closing the grip, and the hand free to roll about the handguard axis (Theta) with a forearm-twist limit. */
	struct SupportPowerOptions
	{
		std::vector<double> Thetas = {-60.0, -45.0, -30.0, -15.0, 0.0, 15.0, 30.0, 45.0, 60.0};
		std::vector<double> YawPitch = {-20.0, -10.0, 0.0, 10.0, 20.0};
		std::vector<double> Roll = {-30.0, -15.0, 0.0, 15.0, 30.0};
		std::vector<double> Shift = {-1.0, 0.0, 1.0};
		double PalmLo = -0.1, PalmHi = 0.5, PalmTarget = 0.1;    // palm seated on the handguard (cm)
		double TwistFree = 35.0;                                 // forearm twist allowed without penalty (deg)
		std::vector<double> Phis = {-10.0, -7.5, -5.0, -2.5, 0.0, 2.5, 5.0, 7.5, 10.0, 12.5, 15.0};
		double Step = 3.0;
		std::vector<double> Ratios = {0.5, 0.65, 0.8, 0.95};
		bool bPolish = true;
		std::vector<double> ThumbAbd = {-80.0, -70.0, -60.0, -50.0, -40.0, -30.0, -20.0, -10.0, 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0};
		std::vector<double> ThumbFlex = {-60.0, -50.0, -40.0, -30.0, -20.0, -10.0, 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0};
		std::vector<double> ThumbMcp = {0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 55.0, 60.0};
		std::vector<double> ThumbIpRatio = {0.5, 0.8, 1.1};
	};

	struct PowerPlacement
	{
		double P[7] = {0, 0, 0, 0, 0, 0, 0};   // yaw, pitch, roll, dx, dy, dz, theta (roll about the handguard axis)
		double Palm = 0.0;
		double Bend[2] = {0.0, 0.0};
		double Twist = 0.0;
		double PalmAngle = 0.0;
		double J1 = 0.0;
	};

	/** Power stage 1: placements by palm seating, wrist bend, forearm twist and change; stable-sorted by J1. */
	void SupportPowerStage1(const Setup& Su, const SupportSettings& St, const SupportPowerOptions& O, const Exec& Ex, std::vector<PowerPlacement>& Out);
	/** The fingers (grid + polish, St weights) and the power thumb on one hand placement. Total = finger + thumb scores. */
	void SolveSupportFingersPower(const Setup& Su, const SupportSettings& St, const X& Hand, const SupportPowerOptions& O, const Exec& Ex,
	                              SupportSolve& Out, double* OutOppose = nullptr);
	/** One power placement (7 parameters). */
	void SolveSupportPower(const Setup& Su, const SupportSettings& St, const double P[7], const SupportPowerOptions& O, const Exec& Ex,
	                       SupportFinalResult& Out, double& Oppose, double& Twist);

	/** The writes of lsolve_m16.py final / stage2, in the Python JSON schema. */
	void WriteSupportRecord(JWriter& W, const SupportSolve& S);   // the keys of "rec" into an open object
	std::string SupportFinalJson(const Setup& Su, const SupportFinalResult& R);
	bool WriteSupportFinalJson(const Setup& Su, const SupportFinalResult& R, const std::string& Path);

	/** finger_gaps.report: neighbouring fingers' nearest skin points and capsule gaps (cm). */
	struct SupportGapRow
	{
		int FingerA = 0;
		int FingerB = 0;
		double Skin = 9.0;                 // skin_gap (phalanges only, every 2nd bound vertex)
		int BoneA = -1;                    // the nearest pair's bones (-1 = none closer than 9 cm)
		int BoneB = -1;
		double Mid = 0.0;                  // middle phalanges' capsule gap
		double Dist = 0.0;                 // distal phalanges' capsule gap
	};
	struct SupportGapReport
	{
		SupportGapRow Row[3];              // index-middle, middle-ring, ring-pinky
	};
	void SupportFingerGaps(const Setup& Su, const X& Hand, const Locals& Locs, SupportGapReport& Out);
	/** The text finger_gaps.report prints (label line + 3 indented lines, each ending in a newline). */
	std::string FormatSupportGapReport(const std::string& Label, const SupportGapReport& R);
}
