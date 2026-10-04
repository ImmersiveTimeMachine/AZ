// Copyright Artur. AZ project.
// Natural Grip solver core: one solve setup = hand model + weapon field + the clip hold + the placement frame.
// Port of Tools/wgs/natgrip/hand.py (which files per side / weapon), views.py (which field files) and place2.py
// (placement = rotation about the hold's middle knuckle + translation, palm clearance).
#pragma once

#include "NGField.h"
#include "NGHand.h"
#include "NGHandData.h"
#include "NGJson.h"

#include <string>
#include <utility>
#include <vector>

namespace ng
{
	/** What the Python reads from its NATGRIP_* environment. */
	struct SetupConfig
	{
		std::string SavedDir = "C:/UnrealEngine/Games/AZ/Saved/wgs";
		char Side = 'r';                     // NATGRIP_SIDE
		std::string Weapon = "winchester";   // NATGRIP_WEAPON
		int Thin = 2;                        // NATGRIP_THIN
		bool bFine = true;                   // NATGRIP_FINE
		std::string LeftPick = "aim";        // NATGRIP_LEFT_PICK
	};

	/** place2.py: hand placements around the clip hold. */
	struct Placement
	{
		const HandModel* H = nullptr;
		X HandW;                             // the clip hold: hand in weapon space
		Locals Clip;                         // the clip's finger locals (all bones)
		V3 Pivot;                            // the hold's middle_01 joint (weapon space)
		double FwdSign = -1.0;
		std::vector<std::pair<int, V3>> Palm;   // palm skin samples (bone, offset)

		void Init(const HandModel& Hand, const X& InHandW, const Locals& InClip);
		/** place2.corr: rotation yaw (z), pitch (x), roll (y) degrees about Pivot, then the translation D. */
		X Corr(double Yaw, double Pitch, double Roll, const V3& D) const;
		X Corr(const double P[6]) const { return Corr(P[0], P[1], P[2], V3(P[3], P[4], P[5])); }
		/** place2.hand_new: the re-gripped hand. */
		X HandNew(const X& C) const { return HandW * C; }
		/** place2.palm_worst: the palm's smallest field distance with the clip's fingers. */
		double PalmWorst(const X& C) const;
	};

	/** Inputs given directly (read from assets / baked in C++) instead of the Python data files. Null = from the files.
	 *  With a given Coarse lattice the fine lattice is only Fine (never the files). The hold always comes from the files. */
	struct SetupInputs
	{
		const HandData* Hand = nullptr;
		const Lattice* Coarse = nullptr;
		const Lattice* Fine = nullptr;
		const HoldData* Hold = nullptr;        // the clip hold sampled from the clips (else the pick files)
		const std::vector<Capsule>* Obstacles = nullptr;   // extra obstacles in the field (the other hand)
	};

	/** The hand inputs from the Python dump files: win_hand_live[_l].json + hand_<side>_verts.json. */
	bool ReadHandDataJson(const std::string& SavedDir, char Side, HandData& Out, std::string& Error);

	/** Holds pointers into itself (Hand -> Fld, Plc -> Hand): not copyable. */
	struct Setup
	{
		Setup() = default;
		Setup(const Setup&) = delete;
		Setup& operator=(const Setup&) = delete;

		SetupConfig Cfg;
		Field Fld;
		HandModel Hand;
		Placement Plc;
		std::string CoarsePath, FinePath;
		bool bFineLoaded = false;

		bool Load(const SetupConfig& InCfg, std::string& Error, const SetupInputs& Inputs = SetupInputs());
		/** Locals from a {bone: [tx,ty,tz,qx,qy,qz,qw]} JSON object. */
		bool ReadLocals(const JValue& Obj, Locals& Out, std::string& Error) const;
	};

	bool FileExists(const std::string& Path);
}
