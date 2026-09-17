// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_Inv_AuthoringUtils.generated.h"

class UBlueprint;

/**
 * Item-authoring bridge, in the same spirit as UAZ_MontageUtils.
 *
 * A pickup's manifest lives on an ItemComponent TEMPLATE inside the Blueprint's SimpleConstructionScript, and
 * the SCS is reachable from neither Python nor Blueprint — Python can only spawn an INSTANCE, whose edits are
 * thrown away with the actor. So an item cannot be authored or even read by script without this.
 *
 * Text in / text out deliberately: the manifest is a struct of instanced-struct fragments, and round-tripping
 * it as T3D is the only representation a script can both read from an existing item and write to a new one
 * without enumerating every fragment type.
 */
UCLASS()
class AZ_API UAZ_Inv_AuthoringUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The pickup manifest of Blueprint's ItemComponent template, as T3D text. Empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory|Authoring")
	static FString GetPickupManifestText(UBlueprint* Blueprint);

	/**
	 * Replace that manifest from T3D text (as produced by GetPickupManifestText or Python's export_text).
	 *
	 * Marks the Blueprint modified but does NOT compile, refresh nodes, or save.
	 *
	 * ★ That restraint is the point. Compiling is what pushes the change out to instances, but a Blueprint
	 * recompile garbage-collects, and doing that while a Python script is on the stack crashes this editor
	 * (PyUtil::CollectGarbage against the Python plugin's pre-GC hook). So the caller compiles — from the
	 * editor UI, or from C++ — after this returns.
	 *
	 * @return true when the text parsed and was written (failures are logged to LogAZ).
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory|Authoring")
	static bool SetPickupManifestText(UBlueprint* Blueprint, const FString& ManifestText);
};
