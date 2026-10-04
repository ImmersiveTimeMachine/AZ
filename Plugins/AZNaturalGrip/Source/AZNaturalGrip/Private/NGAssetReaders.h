// Copyright Artur. AZ project.
// Reads the solver's inputs straight from assets (game thread only). Each reader follows the exact path of the Python
// dump script it replaces, so the numbers are the same:
//   ReadMeshTriangles  <- Tools/wgs/weapon_skm_dump.py (GeometryScript CopyMeshFromSkeletalMesh, LOD 0 source model)
//   ReadHand           <- Tools/wgs/natgrip/dump_hand_verts.py (+ the bone list of dump_hand_live.py)

#pragma once

#include "CoreMinimal.h"
#include "NGGeometry.h"
#include "NGHandData.h"

class USkeletalMesh;

namespace NGAssetReaders
{
	/** All vertex positions (vertex-ID gaps as zero, like GetAllVertexPositions(bSkipGaps=false)) and the triangles. */
	bool ReadMeshTriangles(USkeletalMesh* Mesh, ng::TriMesh& Out, FString& Error);

	/** The hero's hand: solver bone order + parents, finger ref locals and bind pose in hand space, and the hand's skin
	 *  vertices in hand space (box filter and 1e-3 rounding of dump_hand_verts.py). Side 'r' or 'l'. */
	bool ReadHand(USkeletalMesh* Hero, char Side, ng::HandData& Out, FString& Error);

	struct FHoldSampling
	{
		FString ClipFolder;                    // long package path, e.g. /Game/AZ/Assets/M16/Riffle_RTG_MH
		FName WeaponSocket;                    // the hero socket the weapon hangs on in these clips
		TArray<FString> AimFilters;            // a clip name containing any of these = an aim clip ...
		TArray<FString> AimExcludes;           // ... unless it contains one of these
		TArray<FString> RelaxedFilters;        // relaxed clips (support hand: the second elbow)
		int32 FramesPerClip = 8;
		char Side = 'l';
	};

	/** The clip hold (replaces dump_left_m16.py + the medoid picks): samples the weapon's clips, puts the hand in weapon
	 *  space (weapon = WeaponSocket on its bone in each frame) and takes the medoid frame of the aim clips (hand position
	 *  + 0.1 x rotation degrees); the elbows of the aim and the relaxed medoids. Log gets the counts and the picks. */
	bool SampleHold(USkeletalMesh* Hero, const FHoldSampling& O, ng::HoldData& Out, FString& Log, FString& Error);

	/** A marker socket's position in the weapon mesh's component space (ref pose). False when the socket is missing. */
	bool ReadMarker(USkeletalMesh* Weapon, FName Socket, ng::V3& OutPos);
}
