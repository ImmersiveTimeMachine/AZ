#include "Animation/AZ_BlueprintNodeUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_Knot.h"
#include "K2Node_AnimNodeReference.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#if WITH_EDITOR

// ========================================
// HELPERS
// ========================================

static UBlueprint* LoadBP(const FString& Path)
{
	return LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)));
}

static UEdGraph* FindFunctionGraph(UBlueprint* BP, const FString& FuncName)
{
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FuncName) return Graph;
	}
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (Graph && Graph->GetName() == FuncName) return Graph;
	}
	TArray<UEdGraph*> AllGraphs;
	BP->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == FuncName) return Graph;
	}
	return nullptr;
}

static UEdGraphNode* FindBPNodeByGUID(UEdGraph* Graph, const FString& GUIDString)
{
	FGuid ParsedGuid;
	FGuid::Parse(GUIDString, ParsedGuid);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == ParsedGuid) return Node;
	}
	return nullptr;
}

static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			return Entry;
	}
	return nullptr;
}

// Robust pin finder: exact → case-insensitive → prefix → log all
static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX)
{
	auto MatchesDir = [Direction](UEdGraphPin* Pin) { return Direction == EGPD_MAX || Pin->Direction == Direction; };

	// 1. Exact match
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->PinName.ToString() == PinName && MatchesDir(Pin))
			return Pin;

	// 2. Case-insensitive
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && MatchesDir(Pin))
			return Pin;

	// 3. Prefix match (e.g., "As" matches "AsAnim Sequence", "then" matches "then_0")
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->PinName.ToString().StartsWith(PinName, ESearchCase::IgnoreCase) && MatchesDir(Pin))
			return Pin;

	// 4. Log all available pins for debugging
	UE_LOG(LogTemp, Warning, TEXT("AZ_BPNodeUtils: Pin '%s' (dir=%d) not found on '%s'. Available:"),
		*PinName, (int32)Direction, *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	for (UEdGraphPin* Pin : Node->Pins)
	{
		UE_LOG(LogTemp, Warning, TEXT("  '%s' (%s, %s)"), *Pin->PinName.ToString(),
			Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"), *Pin->PinType.PinCategory.ToString());
	}
	return nullptr;
}

// Create node with proper editor-time setup
template<typename TNode>
static TNode* CreateNode(UEdGraph* Graph, int32 PosX, int32 PosY)
{
	TNode* Node = NewObject<TNode>(Graph);
	Node->SetFlags(RF_Transactional);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	return Node;
}

// Finalize node after type-specific config
static void FinalizeNode(UEdGraphNode* Node, UEdGraph* Graph)
{
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Graph->AddNode(Node, false, false);
}

// Forward declaration — defined in FUNCTION PIN MANAGEMENT section below
static bool ParsePinType(const FString& TypeString, FEdGraphPinType& OutPinType);

// For local variable Get/Set nodes, AllocateDefaultPins/ReconstructNode may fail to create
// the data pin because GetPropertyForVariable() can't resolve the FProperty from the generated
// class (only the skeleton was regenerated after AddLocalVariable, but GetPropertyForVariable
// checks the generated class first when IsGeneratedClassLayoutReady() is true). This helper
// finds the FBPVariableDescription on the FunctionEntry node and manually creates the missing pin.
static bool EnsureLocalVarPinExists(UK2Node_Variable* Node, UEdGraph* Graph, const FName& VarName, EEdGraphPinDirection Direction)
{
	// Check if the data pin already exists
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinName == VarName && Pin->Direction == Direction)
			return true; // Pin already created by AllocateDefaultPins
	}

	// Find the FBPVariableDescription from the FunctionEntry node's LocalVariables
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FBPVariableDescription* VarDesc = nullptr;
	for (const FBPVariableDescription& Desc : Entry->LocalVariables)
	{
		if (Desc.VarName == VarName)
		{
			VarDesc = &Desc;
			break;
		}
	}
	if (!VarDesc)
	{
		UE_LOG(LogTemp, Warning, TEXT("AZ_BPNodeUtils: Local var '%s' not found on FunctionEntry"), *VarName.ToString());
		return false;
	}

	// Manually create the pin using the stored pin type from the variable description
	UEdGraphPin* NewPin = Node->CreatePin(Direction, NAME_None, VarName);
	if (NewPin)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		NewPin->PinType = VarDesc->VarType;
		K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);
		UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Manually created pin '%s' (category=%s) for local var"),
			*VarName.ToString(), *VarDesc->VarType.PinCategory.ToString());
		return true;
	}
	return false;
}

#endif // WITH_EDITOR

// ========================================
// NODE CREATION
// ========================================

FString UAZ_BlueprintNodeUtils::AddSequenceNode(const FString& BlueprintPath, const FString& FunctionName, int32 NumOutputs, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Function '%s' not found"), *FunctionName); return FString(); }

	UK2Node_ExecutionSequence* Node = CreateNode<UK2Node_ExecutionSequence>(Graph, PosX, PosY);
	FinalizeNode(Node, Graph);

	// Default has 2 outputs. Add more if needed.
	while (Node->Pins.Num() < NumOutputs + 1) // +1 for input exec
		Node->AddInputPin(); // Despite name, adds output exec pins

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Sequence (%d outputs) GUID=%s"), NumOutputs, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddVariableGetNode(const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_VariableGet* Node = CreateNode<UK2Node_VariableGet>(Graph, PosX, PosY);
	Node->VariableReference.SetSelfMember(FName(*VariableName));
	FinalizeNode(Node, Graph);
	Node->ReconstructNode(); // Rebuild pins with correct variable type

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Get %s GUID=%s"), *VariableName, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddVariableSetNode(const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_VariableSet* Node = CreateNode<UK2Node_VariableSet>(Graph, PosX, PosY);
	Node->VariableReference.SetSelfMember(FName(*VariableName));
	FinalizeNode(Node, Graph);
	Node->ReconstructNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Set %s GUID=%s"), *VariableName, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddBranchNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_IfThenElse* Node = CreateNode<UK2Node_IfThenElse>(Graph, PosX, PosY);
	FinalizeNode(Node, Graph);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Branch GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddFunctionCallNode(const FString& BlueprintPath, const FString& FunctionName,
	UClass* TargetClass, const FString& UFunctionName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UFunction* Func = nullptr;
	if (TargetClass)
	{
		Func = TargetClass->FindFunctionByName(FName(*UFunctionName));
	}
	else if (BP->GeneratedClass)
	{
		Func = BP->GeneratedClass->FindFunctionByName(FName(*UFunctionName));
	}

	// Fallback: search common engine function libraries
	if (!Func)
	{
		static const TCHAR* SearchClasses[] = {
			TEXT("/Script/Engine.KismetMathLibrary"),
			TEXT("/Script/Engine.KismetSystemLibrary"),
			TEXT("/Script/Engine.KismetStringLibrary"),
			TEXT("/Script/Engine.KismetArrayLibrary"),
			TEXT("/Script/Engine.GameplayStatics"),
			TEXT("/Script/BlendStack.BlendStackAnimNodeLibrary"),
			TEXT("/Script/AnimationWarpingRuntime.AnimationWarpingLibrary"),
			TEXT("/Script/PoseSearch.PoseSearchTrajectoryLibrary"),
		};
		for (const TCHAR* ClassPath : SearchClasses)
		{
			UClass* SearchClass = FindObject<UClass>(nullptr, ClassPath);
			if (SearchClass)
			{
				Func = SearchClass->FindFunctionByName(FName(*UFunctionName));
				if (Func) break;
			}
		}
	}

	if (!Func)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Function '%s' not found"), *UFunctionName);
		return FString();
	}

	// CRITICAL: Set FunctionReference BEFORE AllocateDefaultPins
	UK2Node_CallFunction* Node = CreateNode<UK2Node_CallFunction>(Graph, PosX, PosY);
	Node->SetFromFunction(Func);
	FinalizeNode(Node, Graph);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added CallFunction %s GUID=%s"), *UFunctionName, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddReturnNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_FunctionResult* Node = CreateNode<UK2Node_FunctionResult>(Graph, PosX, PosY);
	Graph->AddNode(Node, false, false);

	// CRITICAL: PostPlacedNewNode syncs FunctionReference from FunctionEntry,
	// then ReconstructNode creates the ReturnValue pin from the function signature
	Node->PostPlacedNewNode();
	Node->ReconstructNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Return GUID=%s (pins=%d)"), *Node->NodeGuid.ToString(), Node->Pins.Num());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddCastNode(const FString& BlueprintPath, const FString& FunctionName,
	UClass* TargetClass, bool bPureCast, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	if (!TargetClass) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: TargetClass null")); return FString(); }
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	// CRITICAL: Set TargetType and purity BEFORE AllocateDefaultPins
	UK2Node_DynamicCast* Node = CreateNode<UK2Node_DynamicCast>(Graph, PosX, PosY);
	Node->TargetType = TargetClass;
	if (bPureCast) Node->SetPurity(true);
	FinalizeNode(Node, Graph);

	// Log cast result pin name for callers
	if (UEdGraphPin* ResultPin = Node->GetCastResultPin())
	{
		UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added Cast to %s GUID=%s (result pin='%s')"),
			*TargetClass->GetName(), *Node->NodeGuid.ToString(), *ResultPin->PinName.ToString());
	}
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddSetFieldsInStructNode(const FString& BlueprintPath, const FString& FunctionName,
	UScriptStruct* StructType, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	if (!StructType) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: StructType null")); return FString(); }
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_SetFieldsInStruct* Node = CreateNode<UK2Node_SetFieldsInStruct>(Graph, PosX, PosY);
	Node->StructType = StructType; // BEFORE AllocateDefaultPins
	FinalizeNode(Node, Graph);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added SetFieldsInStruct %s GUID=%s"), *StructType->GetName(), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddBreakStructNode(const FString& BlueprintPath, const FString& FunctionName,
	UScriptStruct* StructType, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	if (!StructType) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: StructType null")); return FString(); }
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_BreakStruct* Node = CreateNode<UK2Node_BreakStruct>(Graph, PosX, PosY);
	Node->StructType = StructType; // BEFORE AllocateDefaultPins
	FinalizeNode(Node, Graph);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added BreakStruct %s GUID=%s"), *StructType->GetName(), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddRerouteNode(const FString& BlueprintPath, const FString& FunctionName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_Knot* Node = CreateNode<UK2Node_Knot>(Graph, PosX, PosY);
	FinalizeNode(Node, Graph);

	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddAnimNodeReferenceNode(const FString& BlueprintPath, const FString& FunctionName,
	const FString& Tag, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_AnimNodeReference* Node = CreateNode<UK2Node_AnimNodeReference>(Graph, PosX, PosY);
	Node->SetTag(FName(*Tag));
	FinalizeNode(Node, Graph);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added AnimNodeReference tag='%s' GUID=%s"), *Tag, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

// ========================================
// LOCAL VARIABLES
// ========================================

bool UAZ_BlueprintNodeUtils::AddLocalVariable(const FString& BlueprintPath, const FString& FunctionName,
	const FString& VarName, const FString& VarType)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;

	// Build pin type from string using shared parser
	FEdGraphPinType PinType;
	if (!ParsePinType(VarType, PinType))
	{
		return false;
	}

	bool bResult = FBlueprintEditorUtils::AddLocalVariable(BP, Graph, FName(*VarName), PinType, FString());
	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: AddLocalVariable '%s' type '%s': %s"), *VarName, *VarType, bResult ? TEXT("OK") : TEXT("FAILED"));
	return bResult;
#else
	return false;
#endif
}

FString UAZ_BlueprintNodeUtils::AddLocalVariableGetNode(const FString& BlueprintPath, const FString& FunctionName,
	const FString& VarName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_VariableGet* Node = CreateNode<UK2Node_VariableGet>(Graph, PosX, PosY);

	// Try local variable first
	FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(BP, Graph, FName(*VarName));
	if (LocalVarGuid.IsValid())
	{
		Node->VariableReference.SetLocalMember(FName(*VarName), Graph->GetName(), LocalVarGuid);
	}
	else
	{
		// Fallback to self member
		Node->VariableReference.SetSelfMember(FName(*VarName));
	}

	FinalizeNode(Node, Graph);
	Node->ReconstructNode();

	// If GetPropertyForVariable() couldn't resolve the FProperty (generated class not yet
	// recompiled after AddLocalVariable), the data pin won't exist. Create it manually
	// from the FBPVariableDescription stored on the FunctionEntry node.
	if (LocalVarGuid.IsValid())
	{
		EnsureLocalVarPinExists(Node, Graph, FName(*VarName), EGPD_Output);
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added LocalGet %s GUID=%s (local=%s)"),
		*VarName, *Node->NodeGuid.ToString(), LocalVarGuid.IsValid() ? TEXT("yes") : TEXT("no/self"));
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_BlueprintNodeUtils::AddLocalVariableSetNode(const FString& BlueprintPath, const FString& FunctionName,
	const FString& VarName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();

	UK2Node_VariableSet* Node = CreateNode<UK2Node_VariableSet>(Graph, PosX, PosY);

	FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(BP, Graph, FName(*VarName));
	if (LocalVarGuid.IsValid())
	{
		Node->VariableReference.SetLocalMember(FName(*VarName), Graph->GetName(), LocalVarGuid);
	}
	else
	{
		Node->VariableReference.SetSelfMember(FName(*VarName));
	}

	FinalizeNode(Node, Graph);
	Node->ReconstructNode();

	// If GetPropertyForVariable() couldn't resolve the FProperty (generated class not yet
	// recompiled after AddLocalVariable), the data pin won't exist. Create it manually
	// from the FBPVariableDescription stored on the FunctionEntry node.
	if (LocalVarGuid.IsValid())
	{
		EnsureLocalVarPinExists(Node, Graph, FName(*VarName), EGPD_Input);
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Added LocalSet %s GUID=%s (local=%s)"),
		*VarName, *Node->NodeGuid.ToString(), LocalVarGuid.IsValid() ? TEXT("yes") : TEXT("no/self"));
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

// ========================================
// CONNECTIONS & CONFIGURATION
// ========================================

bool UAZ_BlueprintNodeUtils::ConnectNodes(const FString& BlueprintPath, const FString& FunctionName,
	const FString& SourceNodeGUID, const FString& SourcePinName,
	const FString& TargetNodeGUID, const FString& TargetPinName)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;

	UEdGraphNode* SourceNode = FindBPNodeByGUID(Graph, SourceNodeGUID);
	UEdGraphNode* TargetNode = FindBPNodeByGUID(Graph, TargetNodeGUID);
	if (!SourceNode || !TargetNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Node not found (source=%s target=%s)"), *SourceNodeGUID, *TargetNodeGUID);
		return false;
	}

	// Try with expected directions first, then fallback without direction constraint
	UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin) SourcePin = FindPin(SourceNode, SourcePinName); // any direction

	UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin) TargetPin = FindPin(TargetNode, TargetPinName); // any direction

	if (!SourcePin || !TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Pin not found (source='%s' target='%s')"), *SourcePinName, *TargetPinName);
		return false;
	}

	// Use Schema->TryCreateConnection for proper type handling + callbacks
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		bool bResult = Schema->TryCreateConnection(SourcePin, TargetPin);
		if (!bResult)
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_BPNodeUtils: TryCreateConnection failed %s.%s → %s.%s"),
				*SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), *SourcePinName,
				*TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), *TargetPinName);
		}
		return bResult;
	}
	return false;
#else
	return false;
#endif
}

bool UAZ_BlueprintNodeUtils::SetPinDefaultValue(const FString& BlueprintPath, const FString& FunctionName,
	const FString& NodeGUID, const FString& PinName, const FString& DefaultValue)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;

	UEdGraphNode* Node = FindBPNodeByGUID(Graph, NodeGUID);
	if (!Node) return false;

	UEdGraphPin* Pin = FindPin(Node, PinName);
	if (!Pin) return false;

	// Use schema for proper validation
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		Schema->TrySetDefaultValue(*Pin, DefaultValue);
	}
	else
	{
		Pin->DefaultValue = DefaultValue;
	}
	return true;
#else
	return false;
#endif
}

// ========================================
// FUNCTION PIN MANAGEMENT
// ========================================

// Parse a type string into FEdGraphPinType. Supports:
// - Primitives: float, double, int, int32, bool, name, FName, string, FString, byte
// - Structs: by short name (e.g., AnimNodeReference, Vector, Rotator) or full path
// - Objects: by short name (e.g., AnimSequence) or full path
// - Enums: by short name or full path
static bool ParsePinType(const FString& TypeString, FEdGraphPinType& OutPinType)
{
	// Check for reference suffix "&" (e.g., "AnimNodeReference&")
	FString CleanType = TypeString;
	bool bIsReference = false;
	if (CleanType.EndsWith(TEXT("&")))
	{
		bIsReference = true;
		CleanType = CleanType.LeftChop(1);
		CleanType.TrimEndInline();
	}

	// Auto-reference for known anim struct types (SM binding requires pass-by-ref)
	static const TSet<FString> AutoRefTypes = {
		TEXT("AnimUpdateContext"), TEXT("AnimNodeReference"),
		TEXT("FAnimUpdateContext"), TEXT("FAnimNodeReference"),
	};
	if (AutoRefTypes.Contains(CleanType))
	{
		bIsReference = true;
	}

	// Store reference flag — will be applied at the end
	auto ApplyFlags = [&]()
	{
		OutPinType.bIsReference = bIsReference;
		OutPinType.bIsConst = bIsReference; // const ref for SM binding compatibility
	};

	// Primitives
	if (CleanType == TEXT("float") || CleanType == TEXT("double") || CleanType == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("int") || CleanType == TEXT("int32"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("bool"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("name") || CleanType == TEXT("FName"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("string") || CleanType == TEXT("FString"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		ApplyFlags(); return true;
	}
	if (CleanType == TEXT("text") || CleanType == TEXT("FText"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		ApplyFlags(); return true;
	}

	// Try as UScriptStruct (by short name, then by full path)
	UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *CleanType);
	if (!Struct)
	{
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->GetName() == CleanType)
			{
				Struct = *It;
				break;
			}
		}
	}
	if (Struct)
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = Struct;
		ApplyFlags(); return true;
	}

	// Try as UClass (object reference)
	UClass* ObjClass = FindObject<UClass>(nullptr, *CleanType);
	if (!ObjClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == CleanType)
			{
				ObjClass = *It;
				break;
			}
		}
	}
	if (ObjClass)
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPinType.PinSubCategoryObject = ObjClass;
		ApplyFlags(); return true;
	}

	// Try as UEnum
	UEnum* Enum = FindObject<UEnum>(nullptr, *TypeString);
	if (!Enum)
	{
		for (TObjectIterator<UEnum> It; It; ++It)
		{
			if (It->GetName() == TypeString)
			{
				Enum = *It;
				break;
			}
		}
	}
	if (Enum)
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		OutPinType.PinSubCategoryObject = Enum;
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: ParsePinType failed for '%s'"), *TypeString);
	return false;
}

// Parse "PinName:TypeString" into name + FEdGraphPinType
static bool ParsePinDescriptor(const FString& Descriptor, FName& OutName, FEdGraphPinType& OutType)
{
	FString NameStr, TypeStr;
	if (!Descriptor.Split(TEXT(":"), &NameStr, &TypeStr))
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Invalid pin descriptor '%s' (expected 'Name:Type')"), *Descriptor);
		return false;
	}

	NameStr.TrimStartAndEndInline();
	TypeStr.TrimStartAndEndInline();

	OutName = FName(*NameStr);
	return ParsePinType(TypeStr, OutType);
}

bool UAZ_BlueprintNodeUtils::EnsureFunctionPins(const FString& BlueprintPath, const FString& FunctionName,
	const TArray<FString>& InputPins, const TArray<FString>& OutputPins)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Blueprint not found: %s"), *BlueprintPath); return false; }
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Function '%s' not found"), *FunctionName); return false; }

	// Find the FunctionEntry node
	UK2Node_FunctionEntry* EntryNode = FindFunctionEntry(Graph);
	if (!EntryNode) { UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: No FunctionEntry in '%s'"), *FunctionName); return false; }

	bool bAllGood = true;

	// Add input parameter pins to the FunctionEntry node
	// FunctionEntry inputs appear as EGPD_Output pins (they output data into the graph)
	for (const FString& PinDesc : InputPins)
	{
		FName PinName;
		FEdGraphPinType PinType;
		if (!ParsePinDescriptor(PinDesc, PinName, PinType))
		{
			bAllGood = false;
			continue;
		}

		// Skip if pin already exists
		if (EntryNode->FindPin(PinName, EGPD_Output))
		{
			UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Input pin '%s' already exists on FunctionEntry"), *PinName.ToString());
			continue;
		}

		UEdGraphPin* NewPin = EntryNode->CreateUserDefinedPin(PinName, PinType, EGPD_Output);
		if (NewPin)
		{
			UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Created input pin '%s' on FunctionEntry"), *PinName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Failed to create input pin '%s'"), *PinName.ToString());
			bAllGood = false;
		}
	}

	// Add output parameter pins to the FunctionResult node
	if (OutputPins.Num() > 0)
	{
		// Use the engine's FindOrCreateFunctionResultNode which handles creation + exec wiring
		UK2Node_FunctionResult* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EntryNode);
		if (!ResultNode)
		{
			UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Failed to find/create FunctionResult node"));
			return false;
		}

		// FunctionResult outputs appear as EGPD_Input pins (they receive data from the graph)
		for (const FString& PinDesc : OutputPins)
		{
			FName PinName;
			FEdGraphPinType PinType;
			if (!ParsePinDescriptor(PinDesc, PinName, PinType))
			{
				bAllGood = false;
				continue;
			}

			// Skip if pin already exists
			if (ResultNode->FindPin(PinName, EGPD_Input))
			{
				UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Output pin '%s' already exists on FunctionResult"), *PinName.ToString());
				continue;
			}

			UEdGraphPin* NewPin = ResultNode->CreateUserDefinedPin(PinName, PinType, EGPD_Input);
			if (NewPin)
			{
				UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Created output pin '%s' on FunctionResult"), *PinName.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("AZ_BPNodeUtils: Failed to create output pin '%s'"), *PinName.ToString());
				bAllGood = false;
			}
		}
	}

	// Mark blueprint as structurally modified so compilation picks up the new signature
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: EnsureFunctionPins '%s' done (inputs=%d, outputs=%d, success=%s)"),
		*FunctionName, InputPins.Num(), OutputPins.Num(), bAllGood ? TEXT("true") : TEXT("false"));
	return bAllGood;
#else
	return false;
#endif
}

// ========================================
// UTILITY
// ========================================

TArray<FString> UAZ_BlueprintNodeUtils::ListFunctionNodes(const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return Result;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		FString Info = FString::Printf(TEXT("GUID=%s Class=%s Title=%s Pins=["),
			*Node->NodeGuid.ToString(),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

		for (UEdGraphPin* Pin : Node->Pins)
		{
			Info += FString::Printf(TEXT("%s(%s,%s) "),
				*Pin->PinName.ToString(),
				Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
				*Pin->PinType.PinCategory.ToString());
		}
		Info += TEXT("]");
		Result.Add(Info);
	}
#endif
	return Result;
}

bool UAZ_BlueprintNodeUtils::FixFunctionForAnimBinding(const FString& BlueprintPath, const FString& FunctionName)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;

	UK2Node_FunctionEntry* EntryNode = FindFunctionEntry(Graph);
	if (!EntryNode) return false;

	// 1. Set thread-safe metadata — required for SM state binding dropdown
	EntryNode->MetaData.bThreadSafe = true;

	// 2. Mark the function entry as editable — allows UserDefinedPins to compile into UFunction
	EntryNode->bIsEditable = true;

	// 3. Add proper function flags via schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (K2Schema)
	{
		K2Schema->MarkFunctionEntryAsEditable(Graph, true);
		K2Schema->AddExtraFunctionFlags(Graph, FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public);
	}

	// 4. Rebuild to apply changes
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Fixed '%s' for AnimBinding (ThreadSafe + Editable + Flags)"), *FunctionName);
	return true;
#else
	return false;
#endif
}

bool UAZ_BlueprintNodeUtils::CompileBlueprint(const FString& BlueprintPath)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);

	UE_LOG(LogTemp, Log, TEXT("AZ_BPNodeUtils: Compiled %s"), *FPackageName::GetShortName(BlueprintPath));
	return true;
#else
	return false;
#endif
}
