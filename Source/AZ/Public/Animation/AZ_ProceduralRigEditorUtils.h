#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_ProceduralRigEditorUtils.generated.h"

/** Editor authoring helpers. These never compile, reconstruct, or save an AnimBlueprint. */
UCLASS()
class AZ_API UAZ_ProceduralRigEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Create a fresh Control Rig node, configuring its asset and custom pins before allocation. */
	UFUNCTION(BlueprintCallable, Category="AZ|Animation|Authoring")
	static FString AddControlRigNode(const FString& BlueprintPath, const FString& RigAssetPath,
		const TArray<FName>& ExposedPins, int32 PosX = 0, int32 PosY = 0, FName RelevanceCallback = NAME_None);

	/** Wire a native AnimInstance property to a rig input using a typed getter, without reconstruction. */
	UFUNCTION(BlueprintCallable, Category="AZ|Animation|Authoring")
	static FString ConnectControlRigInput(const FString& BlueprintPath, const FString& NodeGuid,
		FName InputPin, FName NativeProperty, int32 PosX = 0, int32 PosY = 0);
};
