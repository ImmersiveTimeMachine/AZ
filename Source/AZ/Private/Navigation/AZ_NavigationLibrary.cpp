#include "Navigation/AZ_NavigationLibrary.h"

#include "Navigation/AZ_MapDefinition.h"

bool UAZ_NavigationLibrary::IsNavigationDescriptorValid(const FAZ_NavigationTargetDescriptor& Descriptor)
{
	return Descriptor.IsWellFormed();
}

bool UAZ_NavigationLibrary::IsWaypointValid(const FAZ_MapWaypoint& Waypoint)
{
	return Waypoint.IsWellFormed();
}

bool UAZ_NavigationLibrary::MakeWaypointFromMap(const UAZ_MapDefinition* MapDefinition,
	const FVector2D& MapNormalized, FAZ_MapWaypoint& OutWaypoint)
{
	OutWaypoint = FAZ_MapWaypoint();
	FVector WorldLocation;
	if (!IsValid(MapDefinition) || !MapDefinition->ContainsMapNormalized(MapNormalized)
		|| !MapDefinition->MapNormalizedToWorld(MapNormalized, WorldLocation))
	{
		return false;
	}
	OutWaypoint.bActive = true;
	OutWaypoint.MapId = MapDefinition->MapId;
	OutWaypoint.LayerId = MapDefinition->LayerId;
	OutWaypoint.WorldLocation = WorldLocation;
	return true;
}

bool UAZ_NavigationLibrary::WaypointToMapNormalized(const UAZ_MapDefinition* MapDefinition,
	const FAZ_MapWaypoint& Waypoint, FVector2D& OutMapNormalized)
{
	OutMapNormalized = FVector2D::ZeroVector;
	return IsValid(MapDefinition) && Waypoint.IsWellFormed()
		&& MapDefinition->MapId == Waypoint.MapId && MapDefinition->LayerId == Waypoint.LayerId
		&& MapDefinition->WorldToMapNormalized(Waypoint.WorldLocation, OutMapNormalized);
}

FAZ_NavigationTargetDescriptor UAZ_NavigationLibrary::WaypointToTargetDescriptor(const FAZ_MapWaypoint& Waypoint)
{
	FAZ_NavigationTargetDescriptor Result;
	if (Waypoint.IsWellFormed())
	{
		Result.MapId = Waypoint.MapId;
		Result.LayerId = Waypoint.LayerId;
		Result.bHasWorldLocation = true;
		Result.WorldLocation = Waypoint.WorldLocation;
	}
	return Result;
}
