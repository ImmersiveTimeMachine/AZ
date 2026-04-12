#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_BlueprintNodeUtils.generated.h"

/**
 * Utility for programmatic Blueprint node creation in function graphs.
 * Based on UE5.7 K2Node API patterns from engine source + community research.
 *
 * Key rules:
 * - Set type-specific config BEFORE AllocateDefaultPins (TargetType, StructType, FunctionReference)
 * - Use Schema->TryCreateConnection for editor-time connections
 * - FunctionEntry parameter pins are EGPD_Output
 * - FunctionResult return pins are EGPD_Input
 * - Pin names use UEdGraphSchema_K2 constants (PN_Execute, PN_Then, PN_ReturnValue, PN_Self)
 */
UCLASS()
class AZ_API UAZ_BlueprintNodeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ========================================
	// NODE CREATION
	// ========================================

	/** Add a Sequence node with N exec outputs (then_0, then_1, ...). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddSequenceNode(const FString& BlueprintPath, const FString& FunctionName, int32 NumOutputs, int32 PosX = 0, int32 PosY = 0);

	/** Add a Variable Get node (class member). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddVariableGetNode(const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName, int32 PosX = 0, int32 PosY = 0);

	/** Add a Variable Set node (class member). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddVariableSetNode(const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName, int32 PosX = 0, int32 PosY = 0);

	/** Add a Branch (if/then/else) node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddBranchNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX = 0, int32 PosY = 0);

	/** Add a function call node. TargetClass=nullptr means self. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddFunctionCallNode(const FString& BlueprintPath, const FString& FunctionName,
		UClass* TargetClass, const FString& UFunctionName, int32 PosX = 0, int32 PosY = 0);

	/** Add a Return node that syncs its pins from the function signature. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddReturnNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX = 0, int32 PosY = 0);

	/** Add a Dynamic Cast node (Cast To X). bPureCast=false has exec pins. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddCastNode(const FString& BlueprintPath, const FString& FunctionName,
		UClass* TargetClass, bool bPureCast = false, int32 PosX = 0, int32 PosY = 0);

	/** Add a "Set members in struct" node for the given struct type. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddSetFieldsInStructNode(const FString& BlueprintPath, const FString& FunctionName,
		UScriptStruct* StructType, int32 PosX = 0, int32 PosY = 0);

	/** Add a "Break struct" node for the given struct type. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddBreakStructNode(const FString& BlueprintPath, const FString& FunctionName,
		UScriptStruct* StructType, int32 PosX = 0, int32 PosY = 0);

	/** Add a Reroute (knot) node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddRerouteNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX = 0, int32 PosY = 0);

	/** Add an AnimNodeReference node that references a tagged anim graph node.
	 *  The target anim node must have its Tag set to the same name.
	 *  Output pin "Value" is FAnimNodeReference.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddAnimNodeReferenceNode(const FString& BlueprintPath, const FString& FunctionName,
		const FString& Tag, int32 PosX = 0, int32 PosY = 0);

	// ========================================
	// LOCAL VARIABLES
	// ========================================

	/** Add a local variable to a function. VarType: "float", "bool", "int", "AnimSequence", etc. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool AddLocalVariable(const FString& BlueprintPath, const FString& FunctionName,
		const FString& VarName, const FString& VarType);

	/** Add a Get node for a local variable. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddLocalVariableGetNode(const FString& BlueprintPath, const FString& FunctionName,
		const FString& VarName, int32 PosX = 0, int32 PosY = 0);

	/** Add a Set node for a local variable. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static FString AddLocalVariableSetNode(const FString& BlueprintPath, const FString& FunctionName,
		const FString& VarName, int32 PosX = 0, int32 PosY = 0);

	// ========================================
	// CONNECTIONS & CONFIGURATION
	// ========================================

	/** Connect two pins. Uses Schema->TryCreateConnection for proper type handling. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool ConnectNodes(const FString& BlueprintPath, const FString& FunctionName,
		const FString& SourceNodeGUID, const FString& SourcePinName,
		const FString& TargetNodeGUID, const FString& TargetPinName);

	/** Set a pin's default value. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool SetPinDefaultValue(const FString& BlueprintPath, const FString& FunctionName,
		const FString& NodeGUID, const FString& PinName, const FString& DefaultValue);

	// ========================================
	// FUNCTION PIN MANAGEMENT
	// ========================================

	/**
	 * Ensure that a function's Entry and Result nodes have the correct parameter/return pins.
	 * Call this AFTER creating a function via MCP add_function or AddFunctionGraph.
	 *
	 * The FunctionEntry node gets output pins for each input parameter.
	 * The FunctionResult node gets input pins for each output parameter (created if needed).
	 *
	 * @param BlueprintPath   Asset path to the blueprint
	 * @param FunctionName    Name of the function graph
	 * @param InputPins       Array of "PinName:TypeString" for function inputs (e.g., "BlendStackInput:AnimNodeReference")
	 * @param OutputPins      Array of "PinName:TypeString" for function outputs (e.g., "ReturnValue:float")
	 * @return true if all pins were created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool EnsureFunctionPins(const FString& BlueprintPath, const FString& FunctionName,
		const TArray<FString>& InputPins, const TArray<FString>& OutputPins);

	// ========================================
	// UTILITY
	// ========================================

	/** List all nodes in a function with GUIDs and pin info. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static TArray<FString> ListFunctionNodes(const FString& BlueprintPath, const FString& FunctionName);

	/** Fix a function to be compatible with AnimBP SM state bindings.
	 *  Sets bThreadSafe, bIsEditable, and proper function flags.
	 *  Call this after creating functions that need to appear in SM state
	 *  On State Entry/Update/Exit dropdowns.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool FixFunctionForAnimBinding(const FString& BlueprintPath, const FString& FunctionName);

	/** Compile the blueprint. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Blueprint")
	static bool CompileBlueprint(const FString& BlueprintPath);
};
