// Copyright Artur. AZ project.
// Natural Grip solver core: the trigger (right) hand solver.
// Port of Tools/wgs/natgrip: index_nat.py (natural trigger finger: metrics, unnatural, cost, refine), place_nat.py
// (natural_set, dev, eval_p, grid), rsolve.py (TRIG / GRIP_X / THUMB_SIDE), rsolve2.py (_opt_job, next_finger_opt,
// thumb_rest) and rsolve3.py (solve). The formulas are copied operation for operation (double, left to right) so the
// results match the Python solver; parallel loops write one slot per work item and reduce in the Python loop order
// (see NGParallel.h), so the result does not depend on the thread count.
#pragma once

#include "NGHand.h"
#include "NGMath.h"
#include "NGParallel.h"
#include "NGSetup.h"

#include <array>
#include <string>
#include <vector>

namespace ng
{
	/** What the Python reads from its NATGRIP_* environment (defaults = the M16 runs). */
	struct TriggerSettings
	{
		V3 Trigger = V3(-1.08, 6.65, 5.0);               // NATGRIP_TRIGGER: pad centre target (weapon space)
		double GripX = -1.08;                            // NATGRIP_GRIP_X
		double ThumbSide = 1.0;                          // NATGRIP_THUMB_SIDE
		double ThumbZMax = 4.0;                          // NATGRIP_THUMB_ZMAX
		double WrapFrontY = 2.5;                         // NATGRIP_WRAP_FRONT_Y
		V3 Pull = Norm(V3(0.0, -1.0, 0.0));              // NATGRIP_PULL, already normalised (index_nat.PULL)
		double P0[6] = {10.0, -10.0, 10.0, -1.0, -1.0, -1.0};   // place_nat.P0: the applied re-grip
		double Contact = 0.08;                           // rsolve2.CONTACT: a phalanx "rests" within this of the surface (cm)
	};

	// --- index_nat.py ---

	/** index_nat.metrics: the pad's distance to the trigger, how squarely it faces the pull, the deepest penetration. */
	struct IndexMetrics
	{
		double Err = 0.0, Face = 0.0, Pen = 0.0;
	};

	/** index_nat.unnatural for Xv = {phi, mcp, pip, dip}. */
	double TriggerUnnatural(const double Xv[4]);
	IndexMetrics TriggerMetrics(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, const double Xv[4]);
	double TriggerCost(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, const double Xv[4]);
	/** index_nat.refine: Xv is refined in place, the final cost is returned. */
	double TriggerRefine(const HandModel& Hand, const TriggerSettings& Set, const X& HandW, double Xv[4]);

	// --- place_nat.py ---

	/** place_nat.eval_p result (a placement whose natural index reaches the trigger). */
	struct PlaceNatEntry
	{
		double P[6] = {0, 0, 0, 0, 0, 0};
		double J = 0.0;
		double Idx[4] = {0, 0, 0, 0};
		double Err = 0.0, Face = 0.0, Palm = 0.0, Pen = 0.0, Dev = 0.0;
	};

	/** place_nat.grid(): 6 x 7 x 7 x 5 x 5 x 7 = 51,450 placements in itertools.product order. */
	std::vector<std::array<double, 6>> PlaceNatGrid();
	/** place_nat.dev. */
	double PlaceNatDev(const TriggerSettings& Set, const double P[6]);
	/**
	 * Every non-None eval_p over the grid, in grid order. The grid is parallel (one slot per placement).
	 * Returns false when cancelled (Out is then incomplete).
	 */
	bool PlaceNat(const Setup& Su, const TriggerSettings& Set, const Exec& Ex, std::vector<PlaceNatEntry>& Out);

	// --- rsolve3.py ---

	/** One of middle / ring / pinky: {phi, a, gaps, adj} (adj = the gaps to the previous finger: proximal, middle, distal). */
	struct TriggerFinger
	{
		bool bPlaced = false;
		double Score = 0.0;
		double Phi = 0.0;
		double A[3] = {0, 0, 0};
		double Gaps[3] = {0, 0, 0};
		double Adj[3] = {0, 0, 0};
	};

	/** rsolve2.thumb_rest result. */
	struct TriggerThumb
	{
		bool bPlaced = false;
		double Score = 0.0;
		double Abd = 0.0, Flex = 0.0;                    // the CMC change
		double Mcp = 0.0, Ip = 0.0;
		double Gaps[2] = {0, 0};
		bool bFar = false, bLow = false;
	};

	struct TriggerSolution
	{
		double P[6] = {0, 0, 0, 0, 0, 0};
		double Palm = 0.0;
		double IndexPhi = 0.0;
		double IndexA[3] = {0, 0, 0};
		double IndexErr = 0.0, IndexFace = 0.0, IndexPen = 0.0, IndexUnnatural = 0.0;
		double IndexCost = 0.0;
		TriggerFinger Fingers[3];                        // middle, ring, pinky
		TriggerThumb Thumb;
		X HandInWeapon;                                  // hand_in_weapon (placed hand)
		Locals Locs;                                     // final local transforms of every solved finger and the thumb
	};

	/** rsolve3.solve(p, x0): x0 = {phi, mcp, pip, dip} start of the index refine. Returns false when cancelled. */
	bool SolveTrigger(const Setup& Su, const TriggerSettings& Set, const double P[6], const double X0[4], const Exec& Ex,
	                  TriggerSolution& Out);

	/** The record as JSON in the schema rsolve3.py writes (locals = quaternions only, in the dict's insertion order). */
	std::string TriggerSolutionJson(const Setup& Su, const TriggerSolution& Sol);
}
