#include "Animation/AZ_AnimGraphNodeUtils.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_Base.h"
// UAnimGraphNodeBinding is in Internal/ — include path may not be available
// Use UObject* cast instead to avoid the include issue
#include "AnimGraphNode_BlendStack.h"
#include "AnimationBlendStackGraph.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraph/AnimGraphNode_OffsetRootBone.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AnimNodeReference.h"
#endif

#if WITH_EDITOR

static UAnimBlueprint* LoadAnimBP(const FString& Path)
{
	return LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)));
}

static UAnimationGraph* FindAnimGraph(UAnimBlueprint* ABP)
{
	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph))
			return AnimGraph;
	}
	return nullptr;
}

static UEdGraphNode* FindNodeByGUID(UEdGraph* Graph, const FString& GUIDString)
{
	FGuid GUID;
	FGuid::Parse(GUIDString, GUID);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == GUID) return Node;
	}
	return nullptr;
}

#endif

// ========================================
// NODE CREATION
// ========================================

FString UAZ_AnimGraphNodeUtils::AddBlendStackNode(const FString& BlueprintPath, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return FString();

	auto* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_BlendStack>(
		AnimGraph, FVector2D(PosX, PosY), EK2NewNodeFlags::None,
		[](UAnimGraphNode_BlendStack* N)
		{
			// Node member is private — use defaults. Configure via pin bindings after creation.
		});

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added BlendStack GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimGraphNodeUtils::AddTwoWayBlendNode(const FString& BlueprintPath, float Alpha, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return FString();

	auto* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_TwoWayBlend>(
		AnimGraph, FVector2D(PosX, PosY), EK2NewNodeFlags::None,
		[Alpha](UAnimGraphNode_TwoWayBlend* N)
		{
			N->BlendNode.Alpha = Alpha;
			N->BlendNode.AlphaInputType = EAnimAlphaInputType::Float;
		});

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added TwoWayBlend GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimGraphNodeUtils::AddInertializationNode(const FString& BlueprintPath, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return FString();

	auto* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_Inertialization>(
		AnimGraph, FVector2D(PosX, PosY), EK2NewNodeFlags::None,
		[](UAnimGraphNode_Inertialization* N) {});

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added Inertialization GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimGraphNodeUtils::AddOffsetRootBoneNode(const FString& BlueprintPath, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return FString();

	auto* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_OffsetRootBone>(
		AnimGraph, FVector2D(PosX, PosY), EK2NewNodeFlags::None,
		[](UAnimGraphNode_OffsetRootBone* N)
		{
			N->Node.EvaluationMode = EWarpingEvaluationMode::Graph;
			N->Node.TranslationHalflife = 0.2f;
			N->Node.RotationHalfLife = 0.2f;
			N->Node.MaxTranslationError = 30.f;
		});

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added OffsetRootBone GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimGraphNodeUtils::AddPoseHistoryNode(const FString& BlueprintPath, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return FString();

	auto* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_PoseSearchHistoryCollector>(
		AnimGraph, FVector2D(PosX, PosY), EK2NewNodeFlags::None,
		[](UAnimGraphNode_PoseSearchHistoryCollector* N)
		{
			// Node member is private — use defaults. Configure via pin bindings after creation.
		});

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added PoseHistory GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

// ========================================
// NODE CONFIGURATION
// ========================================

bool UAZ_AnimGraphNodeUtils::SetAnimNodeTag(const FString& BlueprintPath, const FString& NodeGUID, const FString& Tag)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return false;

	UEdGraphNode* Node = FindNodeByGUID(AnimGraph, NodeGUID);
	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
	if (!AnimNode) return false;

	AnimNode->SetTag(FName(*Tag));
	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Set tag '%s' on node %s"), *Tag, *NodeGUID);
	return true;
#else
	return false;
#endif
}

bool UAZ_AnimGraphNodeUtils::SetPinBinding(const FString& BlueprintPath, const FString& NodeGUID,
	const FString& PinName, const FString& PropertyPath, bool bIsFunction)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return false;

	UEdGraphNode* Node = FindNodeByGUID(AnimGraph, NodeGUID);
	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
	if (!AnimNode) return false;

	// Access the Binding UPROPERTY via reflection (avoids incomplete type issue)
	FObjectProperty* BindingProp = CastField<FObjectProperty>(
		AnimNode->GetClass()->FindPropertyByName(TEXT("Binding")));
	if (!BindingProp) return false;

	UObject* BindingObj = BindingProp->GetObjectPropertyValue(BindingProp->ContainerPtrToValuePtr<void>(AnimNode));
	if (!BindingObj) return false;

	// Access PropertyBindings TMap via reflection on the binding subobject
	FMapProperty* MapProp = CastField<FMapProperty>(
		BindingObj->GetClass()->FindPropertyByName(TEXT("PropertyBindings")));
	if (!MapProp)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: PropertyBindings not found via reflection"));
		return false;
	}

	void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(BindingObj);
	TMap<FName, FAnimGraphNodePropertyBinding>* PropertyBindings =
		reinterpret_cast<TMap<FName, FAnimGraphNodePropertyBinding>*>(MapPtr);

	BindingObj->Modify();

	// Parse property path from dot notation
	TArray<FString> PathArray;
	PropertyPath.ParseIntoArray(PathArray, TEXT("."));

	// Build binding
	FAnimGraphNodePropertyBinding Binding;
	Binding.PropertyName = FName(*PinName);
	Binding.PropertyPath = PathArray;
	Binding.bIsBound = true;
	Binding.Type = bIsFunction
		? EAnimGraphNodePropertyBindingType::Function
		: EAnimGraphNodePropertyBindingType::Property;
	Binding.PathAsText = FText::FromString(PropertyPath);

	PropertyBindings->Add(FName(*PinName), Binding);

	// Break any existing wire on that pin
	if (UEdGraphPin* Pin = AnimNode->FindPin(FName(*PinName)))
	{
		Pin->BreakAllPinLinks();
	}

	// NOTE: Skip ReconstructNode/MarkStructurallyModified — triggers GC crash from Python
	AnimNode->Modify();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Bound %s.%s → %s (%s)"),
		*NodeGUID.Left(8), *PinName, *PropertyPath, bIsFunction ? TEXT("func") : TEXT("prop"));
	return true;
#else
	return false;
#endif
}

// ========================================
// CONNECTIONS
// ========================================

bool UAZ_AnimGraphNodeUtils::ConnectPoseLink(const FString& BlueprintPath,
	const FString& SourceNodeGUID, const FString& TargetNodeGUID, const FString& TargetPinName)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return false;

	UEdGraphNode* SourceNode = FindNodeByGUID(AnimGraph, SourceNodeGUID);
	UEdGraphNode* TargetNode = FindNodeByGUID(AnimGraph, TargetNodeGUID);
	if (!SourceNode || !TargetNode) return false;

	UEdGraphPin* OutputPin = SourceNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* InputPin = TargetNode->FindPin(FName(*TargetPinName), EGPD_Input);

	if (!OutputPin || !InputPin)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: Pose pin not found (src=%s tgt=%s.%s)"),
			*SourceNodeGUID.Left(8), *TargetNodeGUID.Left(8), *TargetPinName);
		return false;
	}

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	bool bResult = Schema->TryCreateConnection(OutputPin, InputPin);

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Connected %s.Pose → %s.%s: %s"),
		*SourceNodeGUID.Left(8), *TargetNodeGUID.Left(8), *TargetPinName, bResult ? TEXT("OK") : TEXT("FAILED"));
	return bResult;
#else
	return false;
#endif
}

TArray<FString> UAZ_AnimGraphNodeUtils::ListAnimGraphNodes(const FString& BlueprintPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return Result;
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return Result;

	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		FString Info = FString::Printf(TEXT("GUID=%s Class=%s Title=%s"),
			*Node->NodeGuid.ToString(),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		Result.Add(Info);
	}
#endif
	return Result;
}

// ========================================
// BLEND STACK INTERNAL GRAPH
// ========================================

#if WITH_EDITOR
static UEdGraph* FindBlendStackBoundGraph(UAnimBlueprint* ABP, const FString& BlendStackNodeGUID)
{
	UAnimationGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return nullptr;

	UEdGraphNode* Node = FindNodeByGUID(AnimGraph, BlendStackNodeGUID);
	UAnimGraphNode_BlendStack* BSNode = Cast<UAnimGraphNode_BlendStack>(Node);
	if (!BSNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: BlendStack node not found: %s"), *BlendStackNodeGUID);
		return nullptr;
	}

	// Access BoundGraph via the UPROPERTY
	FObjectProperty* BoundGraphProp = CastField<FObjectProperty>(
		BSNode->GetClass()->FindPropertyByName(TEXT("BoundGraph")));
	if (BoundGraphProp)
	{
		UObject* GraphObj = BoundGraphProp->GetObjectPropertyValue(BoundGraphProp->ContainerPtrToValuePtr<void>(BSNode));
		return Cast<UEdGraph>(GraphObj);
	}

	// Fallback: check SubGraphs
	UEdGraph* ParentGraph = BSNode->GetGraph();
	if (ParentGraph)
	{
		for (UEdGraph* Sub : ParentGraph->SubGraphs)
		{
			if (Sub && Sub->IsA(UAnimationBlendStackGraph::StaticClass()))
			{
				return Sub;
			}
		}
	}

	return nullptr;
}
#endif

TArray<FString> UAZ_AnimGraphNodeUtils::ListBlendStackGraphNodes(const FString& BlueprintPath, const FString& BlendStackNodeGUID)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return Result;

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: BlendStack BoundGraph not found"));
		return Result;
	}

	for (UEdGraphNode* Node : BSGraph->Nodes)
	{
		// Build pin info
		FString PinInfo;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			PinInfo += FString::Printf(TEXT("%s(%s,%s,conn=%d) "),
				*Pin->PinName.ToString(),
				Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
				*Pin->PinType.PinCategory.ToString(),
				Pin->LinkedTo.Num());
		}

		FString Info = FString::Printf(TEXT("GUID=%s Class=%s Title=%s Pins=[%s]"),
			*Node->NodeGuid.ToString(),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(),
			*PinInfo);
		Result.Add(Info);
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BlendStack internal graph has %d nodes"), BSGraph->Nodes.Num());
#endif
	return Result;
}

FString UAZ_AnimGraphNodeUtils::AddBlendStackGraphNode(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	const FString& NodeClassName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return FString();

	// Find the node class
	FString FullClassName = NodeClassName;
	if (!FullClassName.StartsWith(TEXT("U")))
	{
		FullClassName = TEXT("U") + FullClassName;
	}

	UClass* NodeClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::ExactClass);
	if (!NodeClass)
	{
		// Try without U prefix
		NodeClass = FindFirstObject<UClass>(*NodeClassName, EFindFirstObjectOptions::ExactClass);
	}
	if (!NodeClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: Node class '%s' not found"), *NodeClassName);
		return FString();
	}

	// Create the typed node directly via NewObject (SpawnNode<Base> creates ghost nodes)
	BSGraph->Modify();
	UAnimGraphNode_Base* NewNode = NewObject<UAnimGraphNode_Base>(BSGraph, NodeClass);
	NewNode->CreateNewGuid();
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->AllocateDefaultPins();
	BSGraph->AddNode(NewNode, false, false);
	NewNode->PostPlacedNewNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Added BS internal node %s GUID=%s"),
		*NodeClassName, *NewNode->NodeGuid.ToString());
	return NewNode->NodeGuid.ToString();
#else
	return FString();
#endif
}

bool UAZ_AnimGraphNodeUtils::ConnectBlendStackGraphNodes(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	const FString& SourceNodeGUID, const FString& TargetNodeGUID, const FString& TargetPinName)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return false;

	UEdGraphNode* SourceNode = FindNodeByGUID(BSGraph, SourceNodeGUID);
	UEdGraphNode* TargetNode = FindNodeByGUID(BSGraph, TargetNodeGUID);
	if (!SourceNode || !TargetNode) return false;

	// Try common output pin names: Pose, ComponentPose
	UEdGraphPin* OutputPin = SourceNode->FindPin(TEXT("Pose"), EGPD_Output);
	if (!OutputPin) OutputPin = SourceNode->FindPin(TEXT("ComponentPose"), EGPD_Output);
	UEdGraphPin* InputPin = TargetNode->FindPin(FName(*TargetPinName), EGPD_Input);

	if (!OutputPin || !InputPin)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: BS internal pin not found (src=%s tgt=%s.%s)"),
			*SourceNodeGUID.Left(8), *TargetNodeGUID.Left(8), *TargetPinName);
		return false;
	}

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	bool bResult = Schema->TryCreateConnection(OutputPin, InputPin);

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BS internal connect %s.Pose → %s.%s: %s"),
		*SourceNodeGUID.Left(8), *TargetNodeGUID.Left(8), *TargetPinName, bResult ? TEXT("OK") : TEXT("FAIL"));
	return bResult;
#else
	return false;
#endif
}

bool UAZ_AnimGraphNodeUtils::SetBlendStackGraphPinBinding(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	const FString& InternalNodeGUID, const FString& PinName, const FString& PropertyPath, bool bIsFunction)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return false;

	UEdGraphNode* Node = FindNodeByGUID(BSGraph, InternalNodeGUID);
	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
	if (!AnimNode) return false;

	// Reuse the same reflection-based binding approach from SetPinBinding
	FObjectProperty* BindingProp = CastField<FObjectProperty>(
		AnimNode->GetClass()->FindPropertyByName(TEXT("Binding")));
	if (!BindingProp) return false;

	UObject* BindingObj = BindingProp->GetObjectPropertyValue(BindingProp->ContainerPtrToValuePtr<void>(AnimNode));
	if (!BindingObj) return false;

	FMapProperty* MapProp = CastField<FMapProperty>(
		BindingObj->GetClass()->FindPropertyByName(TEXT("PropertyBindings")));
	if (!MapProp) return false;

	void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(BindingObj);
	TMap<FName, FAnimGraphNodePropertyBinding>* PropertyBindings =
		reinterpret_cast<TMap<FName, FAnimGraphNodePropertyBinding>*>(MapPtr);

	BindingObj->Modify();

	TArray<FString> PathArray;
	PropertyPath.ParseIntoArray(PathArray, TEXT("."));

	FAnimGraphNodePropertyBinding Binding;
	Binding.PropertyName = FName(*PinName);
	Binding.PropertyPath = PathArray;
	Binding.bIsBound = true;
	Binding.Type = bIsFunction
		? EAnimGraphNodePropertyBindingType::Function
		: EAnimGraphNodePropertyBindingType::Property;
	Binding.PathAsText = FText::FromString(PropertyPath);

	PropertyBindings->Add(FName(*PinName), Binding);

	if (UEdGraphPin* Pin = AnimNode->FindPin(FName(*PinName)))
	{
		Pin->BreakAllPinLinks();
	}

	// NOTE: Skip ReconstructNode/MarkStructurallyModified — triggers GC crash from Python
	AnimNode->Modify();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BS internal bound %s.%s → %s"),
		*InternalNodeGUID.Left(8), *PinName, *PropertyPath);
	return true;
#else
	return false;
#endif
}

FString UAZ_AnimGraphNodeUtils::AddBlendStackGraphFunctionCall(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	UClass* TargetClass, const FString& FunctionName, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return FString();

	// Find the function
	UFunction* Func = nullptr;
	if (TargetClass)
	{
		Func = TargetClass->FindFunctionByName(FName(*FunctionName));
	}
	if (!Func && ABP->GeneratedClass)
	{
		Func = ABP->GeneratedClass->FindFunctionByName(FName(*FunctionName));
	}
	// Fallback: search common libraries
	if (!Func)
	{
		static const TCHAR* SearchClasses[] = {
			TEXT("/Script/Engine.KismetMathLibrary"),
			TEXT("/Script/BlendStack.BlendStackAnimNodeLibrary"),
			TEXT("/Script/AnimationWarpingRuntime.AnimationWarpingLibrary"),
			TEXT("/Script/PoseSearch.PoseSearchTrajectoryLibrary"),
		};
		for (const TCHAR* ClassPath : SearchClasses)
		{
			UClass* C = FindObject<UClass>(nullptr, ClassPath);
			if (C) { Func = C->FindFunctionByName(FName(*FunctionName)); if (Func) break; }
		}
	}
	if (!Func)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: BS function '%s' not found"), *FunctionName);
		return FString();
	}

	BSGraph->Modify();
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(BSGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->SetFromFunction(Func);
	Node->AllocateDefaultPins();
	BSGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BS added FuncCall %s GUID=%s"), *FunctionName, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimGraphNodeUtils::AddBlendStackGraphAnimNodeRef(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return FString();

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return FString();

	BSGraph->Modify();
	UK2Node_AnimNodeReference* Node = NewObject<UK2Node_AnimNodeReference>(BSGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	BSGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->ReconstructNode();  // Triggers AllocateDefaultPins (which is private)

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BS added AnimNodeRef GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

bool UAZ_AnimGraphNodeUtils::ConnectBlendStackGraphPins(const FString& BlueprintPath, const FString& BlendStackNodeGUID,
	const FString& SourceNodeGUID, const FString& SourcePinName,
	const FString& TargetNodeGUID, const FString& TargetPinName)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;

	UEdGraph* BSGraph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	if (!BSGraph) return false;

	UEdGraphNode* SourceNode = FindNodeByGUID(BSGraph, SourceNodeGUID);
	UEdGraphNode* TargetNode = FindNodeByGUID(BSGraph, TargetNodeGUID);
	if (!SourceNode || !TargetNode) return false;

	UEdGraphPin* OutputPin = SourceNode->FindPin(FName(*SourcePinName), EGPD_Output);
	UEdGraphPin* InputPin = TargetNode->FindPin(FName(*TargetPinName), EGPD_Input);

	if (!OutputPin || !InputPin)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: BS pin not found (%s.%s → %s.%s)"),
			*SourceNodeGUID.Left(8), *SourcePinName, *TargetNodeGUID.Left(8), *TargetPinName);
		return false;
	}

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	bool bResult = Schema->TryCreateConnection(OutputPin, InputPin);

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: BS pin connect %s.%s → %s.%s: %s"),
		*SourceNodeGUID.Left(8), *SourcePinName, *TargetNodeGUID.Left(8), *TargetPinName,
		bResult ? TEXT("OK") : TEXT("FAIL"));
	return bResult;
#else
	return false;
#endif
}

// ========================================
// INSPECTION / DETAILS
// ========================================

#if WITH_EDITOR
static UEdGraphNode* FindNodeInGraphOrBS(UAnimBlueprint* ABP, const FString& NodeGUID, const FString& BlendStackNodeGUID)
{
	UEdGraph* Graph = nullptr;
	if (BlendStackNodeGUID.IsEmpty())
	{
		Graph = FindAnimGraph(ABP);
	}
	else
	{
		Graph = FindBlendStackBoundGraph(ABP, BlendStackNodeGUID);
	}
	if (!Graph) return nullptr;
	return FindNodeByGUID(Graph, NodeGUID);
}
#endif

TArray<FString> UAZ_AnimGraphNodeUtils::InspectAnimNodeProperties(const FString& BlueprintPath, const FString& NodeGUID,
	const FString& BlendStackNodeGUID)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return Result;

	UEdGraphNode* RawNode = FindNodeInGraphOrBS(ABP, NodeGUID, BlendStackNodeGUID);
	if (!RawNode)
	{
		Result.Add(TEXT("ERROR: Node not found"));
		return Result;
	}

	// Dump properties from the node class itself
	for (TFieldIterator<FProperty> It(RawNode->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Prop = *It;
		FString PropName = Prop->GetName();

		// Skip noisy inherited stuff
		if (PropName == TEXT("Binding") || PropName == TEXT("BoundGraph") ||
			PropName.Contains(TEXT("ShowPinForProperties")) || PropName == TEXT("NodeGuid") ||
			PropName == TEXT("NodePosX") || PropName == TEXT("NodePosY") ||
			PropName == TEXT("NodeComment") || PropName == TEXT("ErrorMsg") ||
			PropName == TEXT("Pins") || PropName == TEXT("DeprecatedPins") ||
			PropName == TEXT("EnabledState") || PropName == TEXT("bCommentBubblePinned") ||
			PropName == TEXT("bCommentBubbleVisible") || PropName == TEXT("bCanResizeNode") ||
			PropName == TEXT("bIsNodeEnabled") || PropName == TEXT("AdvancedPinDisplay") ||
			PropName == TEXT("bDisplayAsDisabled") || PropName == TEXT("bUserSetEnabledState") ||
			PropName == TEXT("bHasCompilerMessage") || PropName == TEXT("NodeUpgradeMessage") ||
			PropName == TEXT("CustomProperties"))
			continue;

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RawNode);
		FString Value;
		Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);

		// Skip empty/default-looking values
		if (Value.IsEmpty() || Value == TEXT("()") || Value == TEXT("None"))
			continue;

		// Truncate very long values
		if (Value.Len() > 5000)
			Value = Value.Left(5000) + TEXT("...");

		Result.Add(FString::Printf(TEXT("%s = %s"), *PropName, *Value));
	}

	// Also dump pin bindings from the Binding object
	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(RawNode);
	if (AnimNode)
	{
		FObjectProperty* BindingProp = CastField<FObjectProperty>(
			AnimNode->GetClass()->FindPropertyByName(TEXT("Binding")));
		if (BindingProp)
		{
			UObject* BindingObj = BindingProp->GetObjectPropertyValue(BindingProp->ContainerPtrToValuePtr<void>(AnimNode));
			if (BindingObj)
			{
				FMapProperty* MapProp = CastField<FMapProperty>(
					BindingObj->GetClass()->FindPropertyByName(TEXT("PropertyBindings")));
				if (MapProp)
				{
					void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(BindingObj);
					TMap<FName, FAnimGraphNodePropertyBinding>* Bindings =
						reinterpret_cast<TMap<FName, FAnimGraphNodePropertyBinding>*>(MapPtr);

					for (auto& Pair : *Bindings)
					{
						FString PathStr;
						for (const FString& P : Pair.Value.PropertyPath)
						{
							if (!PathStr.IsEmpty()) PathStr += TEXT(".");
							PathStr += P;
						}
						Result.Add(FString::Printf(TEXT("Binding:%s = %s (%s)"),
							*Pair.Key.ToString(), *PathStr,
							Pair.Value.Type == EAnimGraphNodePropertyBindingType::Function ? TEXT("func") : TEXT("prop")));
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Inspected %s — %d properties"), *NodeGUID.Left(8), Result.Num());
#endif
	return Result;
}

bool UAZ_AnimGraphNodeUtils::SetAnimNodeProperty(const FString& BlueprintPath, const FString& NodeGUID,
	const FString& PropertyName, const FString& Value, const FString& BlendStackNodeGUID)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadAnimBP(BlueprintPath);
	if (!ABP) return false;

	UEdGraphNode* RawNode = FindNodeInGraphOrBS(ABP, NodeGUID, BlendStackNodeGUID);
	if (!RawNode) return false;

	FProperty* Prop = RawNode->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: Property '%s' not found on node %s"), *PropertyName, *NodeGUID.Left(8));
		return false;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RawNode);
	bool bSuccess = Prop->ImportText_Direct(*Value, ValuePtr, RawNode, PPF_None) != nullptr;

	if (bSuccess)
	{
		// NOTE: Do NOT call ReconstructNode() or MarkBlueprintAsStructurallyModified() here.
		// These trigger GC which crashes the Python plugin. User should compile manually.
		RawNode->Modify();
		UE_LOG(LogTemp, Log, TEXT("AZ_AnimGraphUtils: Set %s.%s = %s"), *NodeGUID.Left(8), *PropertyName, *Value);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimGraphUtils: Failed to set %s.%s = %s"), *NodeGUID.Left(8), *PropertyName, *Value);
	}

	return bSuccess;
#else
	return false;
#endif
}