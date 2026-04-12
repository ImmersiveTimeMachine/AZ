#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_AnimGraphNodeUtils.generated.h"

/**
 * Utility for creating AnimGraph nodes programmatically.
 * Creates BlendStack, TwoWayBlend, Inertialization, OffsetRootBone, PoseHistory
 * and connects them via PoseLink pins.
 */
UCLASS()
class AZ_API UAZ_AnimGraphNodeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ========================================
	// NODE CREATION
	// ========================================

	/** Add a BlendStack node to the AnimGraph. Returns node GUID. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddBlendStackNode(const FString& BlueprintPath, int32 PosX = 0, int32 PosY = 0);

	/** Add a TwoWayBlend node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddTwoWayBlendNode(const FString& BlueprintPath, float Alpha = 1.0f, int32 PosX = 0, int32 PosY = 0);

	/** Add an Inertialization node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddInertializationNode(const FString& BlueprintPath, int32 PosX = 0, int32 PosY = 0);

	/** Add an OffsetRootBone node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddOffsetRootBoneNode(const FString& BlueprintPath, int32 PosX = 0, int32 PosY = 0);

	/** Add a PoseHistory (PoseSearchHistoryCollector) node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddPoseHistoryNode(const FString& BlueprintPath, int32 PosX = 0, int32 PosY = 0);

	// ========================================
	// NODE CONFIGURATION
	// ========================================

	/** Set the Tag on an AnimGraph node (by GUID). Used for AnimNodeReference. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool SetAnimNodeTag(const FString& BlueprintPath, const FString& NodeGUID, const FString& Tag);

	/** Set a property binding on an AnimGraph node pin.
	 *  @param PinName The pin/property name (e.g., "AnimationAsset", "BlendTime", "Trajectory")
	 *  @param PropertyPath Dot-separated path (e.g., "BlendStackInputs.Anim" or "Get_OffsetRootTranslationMode")
	 *  @param bIsFunction True if binding to a function, false for property/variable path
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool SetPinBinding(const FString& BlueprintPath, const FString& NodeGUID,
		const FString& PinName, const FString& PropertyPath, bool bIsFunction = false);

	// ========================================
	// CONNECTIONS
	// ========================================

	/** Connect two AnimGraph nodes via PoseLink (output "Pose" → input pin). */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool ConnectPoseLink(const FString& BlueprintPath,
		const FString& SourceNodeGUID, const FString& TargetNodeGUID, const FString& TargetPinName = TEXT("Source"));

	/** List all nodes in the AnimGraph with GUIDs. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static TArray<FString> ListAnimGraphNodes(const FString& BlueprintPath);

	// ========================================
	// BLEND STACK INTERNAL GRAPH
	// ========================================

	/** List all nodes inside a BlendStack node's internal graph (BoundGraph). */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static TArray<FString> ListBlendStackGraphNodes(const FString& BlueprintPath, const FString& BlendStackNodeGUID);

	/** Add an anim graph node to the BlendStack's internal graph.
	 *  @param NodeClassName Short class name (e.g., "AnimGraphNode_Steering", "AnimGraphNode_StrideWarping", "AnimGraphNode_OrientationWarping")
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddBlendStackGraphNode(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		const FString& NodeClassName, int32 PosX = 0, int32 PosY = 0);

	/** Connect two nodes inside the BlendStack's internal graph via PoseLink. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool ConnectBlendStackGraphNodes(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		const FString& SourceNodeGUID, const FString& TargetNodeGUID, const FString& TargetPinName = TEXT("Source"));

	/** Set a pin binding on a node inside the BlendStack's internal graph. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool SetBlendStackGraphPinBinding(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		const FString& InternalNodeGUID, const FString& PinName, const FString& PropertyPath, bool bIsFunction = false);

	/** Add a K2Node_CallFunction inside the BlendStack's internal graph.
	 *  @param TargetClass The class owning the function (nullptr = search ABP generated class + common libraries)
	 *  @param FunctionName The UFunction name
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddBlendStackGraphFunctionCall(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		UClass* TargetClass, const FString& FunctionName, int32 PosX = 0, int32 PosY = 0);

	/** Add a K2Node_AnimNodeReference inside the BlendStack's internal graph.
	 *  Uses the tag from the existing BlendStack Input node.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static FString AddBlendStackGraphAnimNodeRef(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		int32 PosX = 0, int32 PosY = 0);

	/** Connect any two pins by name inside the BlendStack's internal graph (data or pose). */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool ConnectBlendStackGraphPins(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
		const FString& SourceNodeGUID, const FString& SourcePinName,
		const FString& TargetNodeGUID, const FString& TargetPinName);

	// ========================================
	// INSPECTION / DETAILS
	// ========================================

	/** Dump all UPROPERTY values on an AnimGraph node's internal FAnimNode struct.
	 *  Works on both main AnimGraph nodes and BlendStack internal graph nodes.
	 *  @param BlendStackNodeGUID If empty, searches main AnimGraph. If set, searches inside BlendStack BoundGraph.
	 *  @return Array of "PropertyName = Value" strings
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static TArray<FString> InspectAnimNodeProperties(const FString& BlueprintPath, const FString& NodeGUID,
		const FString& BlendStackNodeGUID = TEXT(""));

	/** Set a property value on an AnimGraph node's internal FAnimNode struct via reflection.
	 *  @param BlendStackNodeGUID If empty, searches main AnimGraph. If set, searches inside BlendStack BoundGraph.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimGraph")
	static bool SetAnimNodeProperty(const FString& BlueprintPath, const FString& NodeGUID,
		const FString& PropertyName, const FString& Value, const FString& BlendStackNodeGUID = TEXT(""));
};