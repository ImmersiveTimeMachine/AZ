#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_AnimBlueprintUtils.generated.h"

class UAnimBlueprint;

/**
 * Utility functions for AnimBlueprint manipulation.
 * Exposes state machine transition creation and AnimGraph node operations to Python/Blueprint.
 */
UCLASS()
class AZ_API UAZ_AnimBlueprintUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Create a transition between two states in an AnimBP state machine.
	 *  @param AnimBlueprintPath Asset path to the AnimBlueprint (e.g., "/Game/AZ/Blueprints/Animation/AZ_ABP_HeroPawn")
	 *  @param StateMachineName Name to identify the SM (matches a state name inside it, e.g., "IdleLoop")
	 *  @param FromStateName Source state name
	 *  @param ToStateName Target state name
	 *  @return true if transition was created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool CreateTransition(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const FString& FromStateName, const FString& ToStateName);

	/** Create multiple transitions at once. Each entry is "FromState->ToState".
	 *  @param AnimBlueprintPath Asset path to the AnimBlueprint
	 *  @param IdentifyingStateName A state name that exists in the target SM (to find it)
	 *  @param Transitions Array of strings in format "FromState->ToState"
	 *  @return Number of transitions created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static int32 CreateTransitions(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const TArray<FString>& Transitions);

	/** Set a state as the entry state of its state machine.
	 *  @return true if entry state was set
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool SetEntryState(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const FString& EntryStateName);

	/** List all states in a state machine (identified by one of its state names).
	 *  @return Array of state names
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static TArray<FString> GetStatesInStateMachine(const FString& AnimBlueprintPath, const FString& IdentifyingStateName);

	/** List all transitions in a state machine.
	 *  @return Array of "FromState -> ToState [RuleNodes=N, HasCustomRule=true/false]"
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static TArray<FString> ListTransitions(const FString& AnimBlueprintPath, const FString& IdentifyingStateName);

	/** Inspect a specific transition's rule graph nodes.
	 *  @return Array of node info strings (GUID, class, title, pins)
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static TArray<FString> InspectTransitionRule(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName);

	/** Inspect ALL properties on a transition node (Priority, Duration, BlendMode, etc.).
	 *  @param TransitionIndex If multiple transitions exist between same states, pick by index (0-based)
	 *  @return Array of "PropertyName = Value" strings
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static TArray<FString> InspectTransitionProperties(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex = 0);

	/** Set a property on a transition node.
	 *  @param TransitionIndex If multiple transitions exist between same states, pick by index (0-based)
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool SetTransitionProperty(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, const FString& PropertyName, const FString& Value,
		int32 TransitionIndex = 0);

	// ========================================
	// TRANSITION RULE GRAPH MODIFICATION
	// ========================================

	/** List all nodes in a transition's rule graph.
	 *  @param TransitionIndex 0-based index when multiple transitions between same states
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static TArray<FString> ListTransitionRuleNodes(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex = 0);

	/** Delete a node from a transition rule graph by GUID. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool DeleteTransitionRuleNode(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, const FString& NodeGUID, int32 TransitionIndex = 0);

	/** Add a function call node to a transition rule graph. Returns GUID. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleFunctionCall(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, UClass* TargetClass, const FString& FunctionName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Add a variable get node to a transition rule graph. Returns GUID. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleVariableGet(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, const FString& VariableName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Connect two pins in a transition rule graph. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool ConnectTransitionRulePins(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName,
		const FString& SourceNodeGUID, const FString& SourcePinName,
		const FString& TargetNodeGUID, const FString& TargetPinName,
		int32 TransitionIndex = 0);

	/** Set a pin default value on a node in a transition rule graph. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool SetTransitionRulePinDefault(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName,
		const FString& NodeGUID, const FString& PinName, const FString& Value,
		int32 TransitionIndex = 0);

	/** Add a K2Node_AnimGetter (e.g., "Current State Time") to a transition rule graph.
	 *  @param GetterName The getter function name (e.g., "GetInstanceCurrentStateElapsedTime")
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleAnimGetter(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, const FString& GetterName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Add a Break Struct node to a transition rule graph. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleBreakStruct(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, UScriptStruct* StructType,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Add an enum equality node to a transition rule graph. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleEnumEquality(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Recreate a transition's BoundGraph if it's missing (e.g., after shared rule was broken).
	 *  Calls UnshareRules() which creates a fresh BoundGraph with default Result node.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool RecreateTransitionRuleGraph(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex = 0);

	/** Clear all nodes from a transition rule graph except the Result node. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static bool ClearTransitionRule(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex = 0);

	/** Add a Contains Item node (K2Node_CallArrayFunction) for array operations like Tags.Contains.
	 *  This handles the CustomThunk type propagation that regular CallFunction can't do.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleArrayContains(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);

	/** Add any K2Node by class name to a transition rule graph.
	 *  Supports: K2Node_Select, K2Node_MakeArray, K2Node_CommutativeAssociativeBinaryOperator, etc.
	 *  @param NodeClassName Short name (e.g., "K2Node_Select")
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|AnimBlueprint")
	static FString AddTransitionRuleGenericNode(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
		const FString& FromStateName, const FString& ToStateName, const FString& NodeClassName,
		int32 PosX = 0, int32 PosY = 0, int32 TransitionIndex = 0);
};
