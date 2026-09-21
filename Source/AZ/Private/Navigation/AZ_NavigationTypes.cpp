#include "Navigation/AZ_NavigationTypes.h"

bool FAZ_NavigationTargetDescriptor::IsWellFormed() const
{
	if (!FMath::IsFinite(SearchRadius) || SearchRadius < 0.0f || (TargetId.IsNone() && !bHasWorldLocation))
	{
		return false;
	}
	// Location-only and fallback locations must identify the map/layer to which they belong.
	return !bHasWorldLocation || (!WorldLocation.ContainsNaN() && !MapId.IsNone() && !LayerId.IsNone());
}

bool FAZ_MapWaypoint::IsWellFormed() const
{
	return bActive && !MapId.IsNone() && !LayerId.IsNone() && !WorldLocation.ContainsNaN();
}
