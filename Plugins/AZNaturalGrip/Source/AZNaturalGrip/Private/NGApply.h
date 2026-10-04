// Copyright Artur. AZ project.
// Apply: writes the profile's last final solve into the weapon's assets (game thread only).
//   support hand: the LeftHandSocket transform (= the solved hand in weapon space; the socket may live on the mesh or on its
//                 skeleton - whichever package owns it is backed up and saved), the grip pose's left finger rotations,
//                 the "baked left grasp" flag;
//   trigger hand: the weapon BP's right-hand correction (solved hand vs the clip hold, hand-local), the grip pose's right
//                 finger rotations, the "baked right grasp" flag.
// Every write: refused while PIE runs; dry-run plan first; the owning .uasset files copied to the backup folder with a
// manifest of the old values; saved; read back and compared with the targets.

#pragma once

#include "CoreMinimal.h"

class UAZNaturalGripProfile;

namespace NGApply
{
	/** bDryRun: only the plan (no writes). Returns false when refused or a check fails; Log gets the details. */
	bool Apply(UAZNaturalGripProfile& Profile, bool bDryRun, FString& Log);

	/** The result file Apply uses for this profile (<Project>/Saved/NaturalGrip/<Profile>/support_final.json | trigger_solve.json). */
	FString ResultPath(const UAZNaturalGripProfile& Profile);
}
