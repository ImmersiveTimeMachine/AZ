#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Navigation/AZ_NavigationTypes.h"
#include "AZ_NavigationLibrary.generated.h"

class UAZ_MapDefinition;

UCLASS()
class AZ_API UAZ_NavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Navigation")
	static bool IsNavigationDescriptorValid(const FAZ_NavigationTargetDescriptor& Descriptor);

	UFUNCTION(BlueprintPure, Category="Navigation")
	static bool IsWaypointValid(const FAZ_MapWaypoint& Waypoint);

	/** Bounds-checked placement on the definition's explicit plane; does not infer ground height. */
	UFUNCTION(BlueprintPure, Category="Navigation")
	static bool MakeWaypointFromMap(const UAZ_MapDefinition* MapDefinition, const FVector2D& MapNormalized,
		FAZ_MapWaypoint& OutWaypoint);

	UFUNCTION(BlueprintPure, Category="Navigation")
	static bool WaypointToMapNormalized(const UAZ_MapDefinition* MapDefinition, const FAZ_MapWaypoint& Waypoint,
		FVector2D& OutMapNormalized);

	UFUNCTION(BlueprintPure, Category="Navigation")
	static FAZ_NavigationTargetDescriptor WaypointToTargetDescriptor(const FAZ_MapWaypoint& Waypoint);
};
