// Copyright Artur. AZ project.
// The solver jobs the panel and the Python library run. A job takes a plain-data snapshot of a profile (made on the game
// thread) and runs on any thread; results are written as JSON in the Python solver's schema under
// <Project>/Saved/NaturalGrip/<Profile>/ and summarised in the returned log text.

#pragma once

#include "AZNaturalGripProfile.h"
#include "CoreMinimal.h"
#include "NGGeometry.h"
#include "NGHandData.h"
#include "NGParallel.h"
#include "NGSetup.h"

#include <memory>

class UAZNaturalGripProfile;

/** A plain-data copy of UAZNaturalGripProfile (no UObject access off the game thread). */
struct FNGProfileData
{
	FString Name;
	bool bTrigger = false;             // trigger-hand rules (pistol grip / straight wrist)
	bool bHandle = false;              // handle rules (knife / melee): the power grasp on a general axis
	bool bNotYet = false;              // a role without a solver yet
	bool bPistolCup = false;           // the pistol second hand: the power grasp around the grip, on top of the right hand
	std::vector<ng::Capsule> Obstacles; // the other hand (pistol second hand)
	ng::V3 WeaponFwd = ng::V3(0.0, 1.0, 0.0);
	ng::V3 HandleBack, HandleFront;    // handle axis (markers)
	bool bHasHandleAxis = false;
	ng::SetupConfig Base;            // SavedDir, Side, Weapon, LeftPick (Thin / bFine set per stage)
	int32 SearchThin = 2;
	int32 FinalThin = 1;
	FString OutDir;                  // <Project>/Saved/NaturalGrip/<Profile>
	FString ReferenceDir;            // <Project>/Tools/wgs/natgrip (the Python goldens)

	double CoarseSpacing = 0.5;
	double CoarseMargin = 3.0;
	ng::V3 FineLo, FineHi;
	double FineSpacing = 0.25;
	double FineBand = 1.3;

	ng::V3 Trigger, Pull;
	double GripX = 0.0, ThumbSide = 1.0, ThumbZMax = 99.0, WrapFrontY = 99.0;
	double SearchCentre[6] = {0, 0, 0, 0, 0, 0};
	double TriggerP[6] = {0, 0, 0, 0, 0, 0};
	double IndexStart[4] = {0, 0, 0, 0};

	double AxX = 0.0, AxZ = 0.0;
	int32 Stage2Top = 24;
	double Stage2Step = 8.0;
	double SupportP[6] = {0, 0, 0, 0, 0, 0};

	/** Input = Assets: the weapon triangles and the hero's hand, read on the game thread. The fields are then baked from
	 *  the triangles and the hand comes from the mesh; only the clip hold still comes from the data files. */
	bool bAssets = false;
	ng::TriMesh WeaponTris;
	ng::HandData HeroHand;
	/** Input = Assets with a clip folder: the hold sampled from the weapon's clips (else the data files). */
	bool bHasHold = false;
	ng::HoldData Hold;
	/** Support hand: the power grasp (closure, thumb along the handguard) for "Solve weapon hand". */
	bool bPowerGrasp = true;
	int32 PowerCandidates = 54;
	/** What the snapshot read from the assets (marker sockets, the hold), for the log. */
	FString InputLog;
	/** Filled by NGJobs::Run when bAssets (fields baked from WeaponTris). */
	std::shared_ptr<const ng::Lattice> BakedCoarse;
	std::shared_ptr<const ng::Lattice> BakedFine;
};

namespace NGJobs
{
	/** Game thread: copy the profile (and read the weapon mesh when the profile says Assets). */
	bool Snapshot(const UAZNaturalGripProfile& Profile, FNGProfileData& Out, FString& Error);

	/** The stages: "fields", "stage1", "stage2", "final", "support" (stage1+2+final on the best stage-2 placement),
	 *  "placement", "solve", "trigger" (placement + solve), "parity" (the hand's stages vs the Python goldens),
	 *  "inputs" (Input = Assets: what was read from the assets vs the Python dump files), "power" (support hand: the
	 *  power grasp search + final). */
	FString Run(const FNGProfileData& P, const FString& Stage, const ng::Exec& Ex);

	/** Stage names offered for a role, in panel order. */
	TArray<FString> StagesFor(EAZGripHand Role);
}
