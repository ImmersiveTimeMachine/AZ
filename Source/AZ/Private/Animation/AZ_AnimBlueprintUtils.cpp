#include "Animation/AZ_AnimBlueprintUtils.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_AnimGetter.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_CallArrayFunction.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "AnimationGraph.h"
#include "Kismet/KismetArrayLibrary.h"
#endif

#if WITH_EDITOR
// Helper: find SM graph that contains a state with the given name
static UAnimationStateMachineGraph* FindSMGraphByStateName(UAnimBlueprint* ABP, const FString& StateName)
{
	// GetAllGraphs returns top-level graphs. SM graphs are subgraphs of AnimGraphNode_StateMachine nodes.
	// We need to search deeper.
	TArray<UEdGraph*> AllGraphs;
	ABP->GetAllGraphs(AllGraphs);

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Searching %d graphs for state '%s'"), AllGraphs.Num(), *StateName);

	for (UEdGraph* Graph : AllGraphs)
	{
		// Check if this graph itself is an SM graph
		if (UAnimationStateMachineGraph* SM = Cast<UAnimationStateMachineGraph>(Graph))
		{
			for (UEdGraphNode* Node : SM->Nodes)
			{
				if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
				{
					if (State->GetStateName() == StateName)
					{
						return SM;
					}
				}
			}
		}

		// Also check nodes in this graph for SM nodes that contain subgraphs
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				if (UAnimationStateMachineGraph* SMSubGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph))
				{
					for (UEdGraphNode* SubNode : SMSubGraph->Nodes)
					{
						if (UAnimStateNode* State = Cast<UAnimStateNode>(SubNode))
						{
							if (State->GetStateName() == StateName)
							{
								UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Found SM via SMNode '%s'"), *SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
								return SMSubGraph;
							}
						}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("AZ_AnimBPUtils: No SM found with state '%s' in %d graphs"), *StateName, AllGraphs.Num());
	return nullptr;
}

// Helper: find a state node by name in an SM graph
static UAnimStateNode* FindStateInGraph(UAnimationStateMachineGraph* SM, const FString& StateName)
{
	for (UEdGraphNode* Node : SM->Nodes)
	{
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			if (State->GetStateName() == StateName)
			{
				return State;
			}
		}
	}
	return nullptr;
}
#endif

bool UAZ_AnimBlueprintUtils::CreateTransition(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const FString& FromStateName, const FString& ToStateName)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: Could not load ABP: %s"), *AnimBlueprintPath);
		return false;
	}

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: No SM found containing state '%s'"), *IdentifyingStateName);
		return false;
	}

	UAnimStateNode* FromState = FindStateInGraph(SMGraph, FromStateName);
	UAnimStateNode* ToState = FindStateInGraph(SMGraph, ToStateName);
	if (!FromState || !ToState)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: State not found: '%s' or '%s'"), *FromStateName, *ToStateName);
		return false;
	}

	// Use the engine's proper method
	FVector2f Location = (FVector2f(FromState->NodePosX, FromState->NodePosY) + FVector2f(ToState->NodePosX, ToState->NodePosY)) * 0.5f;
	UAnimStateTransitionNode* TransNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
		SMGraph, NewObject<UAnimStateTransitionNode>(), Location, false);
	TransNode->CreateConnections(FromState, ToState);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP);
	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Created transition %s -> %s"), *FromStateName, *ToStateName);
	return true;
#else
	return false;
#endif
}

int32 UAZ_AnimBlueprintUtils::CreateTransitions(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const TArray<FString>& Transitions)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: Could not load ABP: %s"), *AnimBlueprintPath);
		return 0;
	}

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: No SM found containing state '%s'"), *IdentifyingStateName);
		return 0;
	}

	// Collect states
	TMap<FString, UAnimStateNode*> States;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			States.Add(State->GetStateName(), State);
		}
	}

	int32 Created = 0;
	for (const FString& TransStr : Transitions)
	{
		FString FromName, ToName;
		if (!TransStr.Split(TEXT("->"), &FromName, &ToName))
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_AnimBPUtils: Invalid transition format '%s', expected 'From->To'"), *TransStr);
			continue;
		}
		FromName.TrimStartAndEndInline();
		ToName.TrimStartAndEndInline();

		UAnimStateNode* FromState = States.FindRef(FromName);
		UAnimStateNode* ToState = States.FindRef(ToName);
		if (!FromState || !ToState)
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_AnimBPUtils: State not found: '%s' or '%s'"), *FromName, *ToName);
			continue;
		}

		// Use the engine's proper method: SpawnNodeFromTemplate + CreateConnections
		FVector2f Location = (FVector2f(FromState->NodePosX, FromState->NodePosY) + FVector2f(ToState->NodePosX, ToState->NodePosY)) * 0.5f;
		UAnimStateTransitionNode* TransNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
			SMGraph, NewObject<UAnimStateTransitionNode>(), Location, false);
		TransNode->CreateConnections(FromState, ToState);

		UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Created transition %s -> %s"), *FromName, *ToName);
		Created++;
	}

	// Mark blueprint as structurally modified — this is critical for the editor to pick up changes
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP);
	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Created %d/%d transitions"), Created, Transitions.Num());
	return Created;
#else
	return 0;
#endif
}

bool UAZ_AnimBlueprintUtils::SetEntryState(const FString& AnimBlueprintPath, const FString& IdentifyingStateName, const FString& EntryStateName)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return false;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return false;

	UAnimStateNode* EntryTarget = FindStateInGraph(SMGraph, EntryStateName);
	if (!EntryTarget) return false;

	if (SMGraph->EntryNode)
	{
		UEdGraphPin* EntryOutput = SMGraph->EntryNode->GetOutputPin();
		if (EntryOutput)
		{
			EntryOutput->BreakAllPinLinks();
			EntryOutput->MakeLinkTo(EntryTarget->GetInputPin());
			ABP->MarkPackageDirty();
			UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Set entry state to '%s'"), *EntryStateName);
			return true;
		}
	}
	return false;
#else
	return false;
#endif
}

TArray<FString> UAZ_AnimBlueprintUtils::GetStatesInStateMachine(const FString& AnimBlueprintPath, const FString& IdentifyingStateName)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return Result;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return Result;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			Result.Add(State->GetStateName());
		}
	}
#endif
	return Result;
}

TArray<FString> UAZ_AnimBlueprintUtils::ListTransitions(const FString& AnimBlueprintPath, const FString& IdentifyingStateName)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return Result;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return Result;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node);
		if (!Trans) continue;

		FString FromName = TEXT("?");
		FString ToName = TEXT("?");
		if (Trans->GetPreviousState()) FromName = Trans->GetPreviousState()->GetStateName();
		if (Trans->GetNextState()) ToName = Trans->GetNextState()->GetStateName();

		// Check rule graph
		int32 RuleNodes = 0;
		bool bHasCustomRule = false;
		if (UEdGraph* RuleGraph = Trans->BoundGraph)
		{
			RuleNodes = RuleGraph->Nodes.Num();
			// More than just the result node means custom logic
			bHasCustomRule = RuleNodes > 1;
		}

		Result.Add(FString::Printf(TEXT("%s -> %s [RuleNodes=%d, Custom=%s]"),
			*FromName, *ToName, RuleNodes,
			bHasCustomRule ? TEXT("yes") : TEXT("no")));
	}
#endif
	return Result;
}

TArray<FString> UAZ_AnimBlueprintUtils::InspectTransitionRule(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return Result;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return Result;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node);
		if (!Trans) continue;

		FString FromN = Trans->GetPreviousState() ? Trans->GetPreviousState()->GetStateName() : TEXT("?");
		FString ToN = Trans->GetNextState() ? Trans->GetNextState()->GetStateName() : TEXT("?");

		if (FromN != FromStateName || ToN != ToStateName) continue;

		// Found the transition — dump rule graph nodes
		if (UEdGraph* RuleGraph = Trans->BoundGraph)
		{
			for (UEdGraphNode* RuleNode : RuleGraph->Nodes)
			{
				FString PinInfo;
				for (UEdGraphPin* Pin : RuleNode->Pins)
				{
					FString DefVal = Pin->DefaultValue.IsEmpty() ? TEXT("") : FString::Printf(TEXT(",def=%s"), *Pin->DefaultValue);
					PinInfo += FString::Printf(TEXT("%s(%s,%s,conn=%d%s) "),
						*Pin->PinName.ToString(),
						Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
						*Pin->PinType.PinCategory.ToString(),
						Pin->LinkedTo.Num(),
						*DefVal);
				}

				Result.Add(FString::Printf(TEXT("GUID=%s Class=%s Title=%s Pins=[%s]"),
					*RuleNode->NodeGuid.ToString(),
					*RuleNode->GetClass()->GetName(),
					*RuleNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(),
					*PinInfo));
			}
		}
		break;
	}
#endif
	return Result;
}

#if WITH_EDITOR
static UAnimStateTransitionNode* FindTransitionNode(UAnimationStateMachineGraph* SMGraph,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex)
{
	int32 MatchIndex = 0;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node);
		if (!Trans) continue;

		FString FromN = Trans->GetPreviousState() ? Trans->GetPreviousState()->GetStateName() : TEXT("?");
		FString ToN = Trans->GetNextState() ? Trans->GetNextState()->GetStateName() : TEXT("?");

		if (FromN == FromStateName && ToN == ToStateName)
		{
			if (MatchIndex == TransitionIndex) return Trans;
			MatchIndex++;
		}
	}
	return nullptr;
}
#endif

TArray<FString> UAZ_AnimBlueprintUtils::InspectTransitionProperties(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return Result;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return Result;

	UAnimStateTransitionNode* Trans = FindTransitionNode(SMGraph, FromStateName, ToStateName, TransitionIndex);
	if (!Trans)
	{
		Result.Add(TEXT("ERROR: Transition not found"));
		return Result;
	}

	for (TFieldIterator<FProperty> It(Trans->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Prop = *It;
		FString PropName = Prop->GetName();

		// Skip noisy inherited
		if (PropName == TEXT("Pins") || PropName == TEXT("DeprecatedPins") || PropName == TEXT("NodeGuid") ||
			PropName == TEXT("NodePosX") || PropName == TEXT("NodePosY") || PropName == TEXT("NodeComment") ||
			PropName == TEXT("BoundGraph") || PropName == TEXT("CustomProperties") ||
			PropName == TEXT("EnabledState") || PropName == TEXT("bCommentBubblePinned") ||
			PropName == TEXT("bCommentBubbleVisible") || PropName == TEXT("bCanResizeNode") ||
			PropName == TEXT("bIsNodeEnabled") || PropName == TEXT("AdvancedPinDisplay") ||
			PropName == TEXT("bDisplayAsDisabled") || PropName == TEXT("bUserSetEnabledState") ||
			PropName == TEXT("bHasCompilerMessage") || PropName == TEXT("NodeUpgradeMessage") ||
			PropName == TEXT("ErrorMsg"))
			continue;

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Trans);
		FString Value;
		Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);

		if (Value.IsEmpty() || Value == TEXT("()") || Value == TEXT("None"))
			continue;

		if (Value.Len() > 500)
			Value = Value.Left(500) + TEXT("...");

		Result.Add(FString::Printf(TEXT("%s = %s"), *PropName, *Value));
	}
#endif
	return Result;
}

bool UAZ_AnimBlueprintUtils::SetTransitionProperty(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, const FString& PropertyName, const FString& Value,
	int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *AnimBlueprintPath, *FPackageName::GetShortName(AnimBlueprintPath)));
	if (!ABP) return false;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return false;

	UAnimStateTransitionNode* Trans = FindTransitionNode(SMGraph, FromStateName, ToStateName, TransitionIndex);
	if (!Trans) return false;

	FProperty* Prop = Trans->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: Property '%s' not found on transition"), *PropertyName);
		return false;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Trans);
	bool bSuccess = Prop->ImportText_Direct(*Value, ValuePtr, Trans, PPF_None) != nullptr;

	if (bSuccess)
	{
		Trans->Modify();
		UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Set transition %s->%s [%d] %s = %s"),
			*FromStateName, *ToStateName, TransitionIndex, *PropertyName, *Value);
	}

	return bSuccess;
#else
	return false;
#endif
}

// ========================================
// TRANSITION RULE GRAPH MODIFICATION
// ========================================

#if WITH_EDITOR
static UAnimBlueprint* LoadABP(const FString& Path)
{
	return LoadObject<UAnimBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)));
}

static UEdGraph* GetTransitionRuleGraph(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex, UAnimBlueprint*& OutABP)
{
	OutABP = LoadABP(AnimBlueprintPath);
	if (!OutABP) return nullptr;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(OutABP, IdentifyingStateName);
	if (!SMGraph) return nullptr;

	UAnimStateTransitionNode* Trans = FindTransitionNode(SMGraph, FromStateName, ToStateName, TransitionIndex);
	if (!Trans) return nullptr;

	// If this is a shared rule and BoundGraph is null, find the owner transition
	if (!Trans->BoundGraph && Trans->bSharedRules)
	{
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			UAnimStateTransitionNode* Other = Cast<UAnimStateTransitionNode>(Node);
			if (Other && Other != Trans && Other->BoundGraph && Other->SharedRulesGuid == Trans->SharedRulesGuid)
			{
				UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Using shared rule owner for %s->%s"), *FromStateName, *ToStateName);
				return Other->BoundGraph;
			}
		}
	}

	return Trans->BoundGraph;
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

TArray<FString> UAZ_AnimBlueprintUtils::ListTransitionRuleNodes(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return Result;

	for (UEdGraphNode* Node : RuleGraph->Nodes)
	{
		FString PinInfo;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			FString DefVal = Pin->DefaultValue.IsEmpty() ? TEXT("") : FString::Printf(TEXT(",def=%s"), *Pin->DefaultValue);
			PinInfo += FString::Printf(TEXT("%s(%s,%s,conn=%d%s) "),
				*Pin->PinName.ToString(),
				Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
				*Pin->PinType.PinCategory.ToString(),
				Pin->LinkedTo.Num(), *DefVal);
		}
		Result.Add(FString::Printf(TEXT("GUID=%s Class=%s Title=%s Pins=[%s]"),
			*Node->NodeGuid.ToString(), *Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), *PinInfo));
	}
#endif
	return Result;
}

bool UAZ_AnimBlueprintUtils::DeleteTransitionRuleNode(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, const FString& NodeGUID, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return false;

	UEdGraphNode* Node = FindNodeByGUID(RuleGraph, NodeGUID);
	if (!Node) return false;

	RuleGraph->Modify();
	Node->BreakAllNodeLinks();
	RuleGraph->RemoveNode(Node);
	return true;
#else
	return false;
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleFunctionCall(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, UClass* TargetClass, const FString& FunctionName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	UFunction* Func = nullptr;
	if (TargetClass) Func = TargetClass->FindFunctionByName(FName(*FunctionName));
	if (!Func && ABP->GeneratedClass) Func = ABP->GeneratedClass->FindFunctionByName(FName(*FunctionName));
	// Fallback: search common libraries
	if (!Func)
	{
		static const TCHAR* SearchClasses[] = {
			TEXT("/Script/Engine.KismetMathLibrary"),
			TEXT("/Script/Engine.KismetSystemLibrary"),
			TEXT("/Script/Engine.KismetArrayLibrary"),
			TEXT("/Script/Engine.KismetStringLibrary"),
			TEXT("/Script/Engine.GameplayStatics"),
			TEXT("/Script/BlendStack.BlendStackAnimNodeLibrary"),
		};
		for (const TCHAR* ClassPath : SearchClasses)
		{
			UClass* C = FindObject<UClass>(nullptr, ClassPath);
			if (C) { Func = C->FindFunctionByName(FName(*FunctionName)); if (Func) break; }
		}
	}
	if (!Func) return FString();

	RuleGraph->Modify();
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->SetFromFunction(Func);
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleVariableGet(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, const FString& VariableName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	FProperty* Prop = ABP->GeneratedClass ? ABP->GeneratedClass->FindPropertyByName(FName(*VariableName)) : nullptr;
	if (!Prop) return FString();

	RuleGraph->Modify();
	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->VariableReference.SetSelfMember(FName(*VariableName));
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

bool UAZ_AnimBlueprintUtils::ConnectTransitionRulePins(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName,
	const FString& SourceNodeGUID, const FString& SourcePinName,
	const FString& TargetNodeGUID, const FString& TargetPinName,
	int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return false;

	UEdGraphNode* SrcNode = FindNodeByGUID(RuleGraph, SourceNodeGUID);
	UEdGraphNode* TgtNode = FindNodeByGUID(RuleGraph, TargetNodeGUID);
	if (!SrcNode || !TgtNode) return false;

	UEdGraphPin* OutPin = SrcNode->FindPin(FName(*SourcePinName), EGPD_Output);
	UEdGraphPin* InPin = TgtNode->FindPin(FName(*TargetPinName), EGPD_Input);
	if (!OutPin || !InPin) return false;

	const UEdGraphSchema* Schema = RuleGraph->GetSchema();
	bool bResult = Schema->TryCreateConnection(OutPin, InPin);

	// Notify both nodes of connection change — critical for K2Node_CallArrayFunction type propagation
	if (bResult)
	{
		if (UK2Node* K2Src = Cast<UK2Node>(SrcNode)) K2Src->NotifyPinConnectionListChanged(OutPin);
		if (UK2Node* K2Tgt = Cast<UK2Node>(TgtNode)) K2Tgt->NotifyPinConnectionListChanged(InPin);
	}

	return bResult;
#else
	return false;
#endif
}

bool UAZ_AnimBlueprintUtils::SetTransitionRulePinDefault(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName,
	const FString& NodeGUID, const FString& PinName, const FString& Value,
	int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return false;

	UEdGraphNode* Node = FindNodeByGUID(RuleGraph, NodeGUID);
	if (!Node) return false;

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin)
	{
		// Try with direction
		Pin = Node->FindPin(FName(*PinName), EGPD_Input);
		if (!Pin) Pin = Node->FindPin(FName(*PinName), EGPD_Output);
	}
	if (!Pin) return false;

	Pin->Modify();
	const UEdGraphSchema* Schema = RuleGraph->GetSchema();
	Schema->TrySetDefaultValue(*Pin, Value);
	return true;
#else
	return false;
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleAnimGetter(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, const FString& GetterName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return FString();

	// Find the SM node (AnimGraphNode_StateMachine) that owns this SM graph
	UAnimGraphNode_StateMachine* SMNode = nullptr;
	TArray<UEdGraph*> AllGraphs;
	ABP->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* TestSM = Cast<UAnimGraphNode_StateMachine>(Node);
			if (TestSM)
			{
				// Check if this SM owns our SMGraph
				TArray<UEdGraph*> SubGraphs = TestSM->GetSubGraphs();
				for (UEdGraph* Sub : SubGraphs)
				{
					if (Sub == SMGraph)
					{
						SMNode = TestSM;
						break;
					}
				}
			}
			if (SMNode) break;
		}
		if (SMNode) break;
	}

	// Find the transition node to get the SourceStateNode
	UAnimStateTransitionNode* TransNode = FindTransitionNode(SMGraph, FromStateName, ToStateName, TransitionIndex);

	// Find the getter function
	UClass* AnimInstanceClass = ABP->GetAnimBlueprintGeneratedClass();
	if (!AnimInstanceClass) AnimInstanceClass = ABP->ParentClass;

	UFunction* GetterFunc = nullptr;
	if (AnimInstanceClass)
	{
		GetterFunc = AnimInstanceClass->FindFunctionByName(FName(*GetterName));
	}
	if (!GetterFunc)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: Getter '%s' not found"), *GetterName);
		return FString();
	}

	RuleGraph->Modify();
	UK2Node_AnimGetter* Node = NewObject<UK2Node_AnimGetter>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->SourceNode = SMNode;
	Node->SourceStateNode = TransNode ? TransNode->GetPreviousState() : nullptr;
	Node->GetterClass = AnimInstanceClass;
	Node->SourceAnimBlueprint = ABP;
	Node->SetFromFunction(GetterFunc);
	Node->Contexts.Add(TEXT("Transition"));
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	// Build the cached title manually: "FunctionDisplayName (SMNodeName)"
	FText FuncDisplayName = GetterFunc->GetDisplayNameText();
	FString SMName = SMNode ? SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	Node->CachedTitle = FText::Format(NSLOCTEXT("AnimGetter", "NodeTitle", "{0} ({1})"), FuncDisplayName, FText::FromString(SMName));

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Added AnimGetter '%s' GUID=%s Title='%s'"),
		*GetterName, *Node->NodeGuid.ToString(), *Node->CachedTitle.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleBreakStruct(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, UScriptStruct* StructType,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph || !StructType) return FString();

	RuleGraph->Modify();
	UK2Node_BreakStruct* Node = NewObject<UK2Node_BreakStruct>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->StructType = StructType;
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Added BreakStruct '%s' GUID=%s"),
		*StructType->GetName(), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleEnumEquality(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	RuleGraph->Modify();
	UK2Node_EnumEquality* Node = NewObject<UK2Node_EnumEquality>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleArrayContains(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	UFunction* Func = UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Contains"));
	if (!Func) return FString();

	RuleGraph->Modify();
	UK2Node_CallArrayFunction* Node = NewObject<UK2Node_CallArrayFunction>(RuleGraph);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->SetFromFunction(Func);
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Added ArrayContains GUID=%s"), *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

bool UAZ_AnimBlueprintUtils::ClearTransitionRule(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return false;

	RuleGraph->Modify();
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : RuleGraph->Nodes)
	{
		// Keep the Result node, remove everything else (except comments)
		if (!Node->GetClass()->GetName().Contains(TEXT("TransitionResult")))
		{
			NodesToRemove.Add(Node);
		}
	}

	for (UEdGraphNode* Node : NodesToRemove)
	{
		Node->BreakAllNodeLinks();
		RuleGraph->RemoveNode(Node);
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Cleared %d nodes from %s->%s[%d]"),
		NodesToRemove.Num(), *FromStateName, *ToStateName, TransitionIndex);
	return true;
#else
	return false;
#endif
}

FString UAZ_AnimBlueprintUtils::AddTransitionRuleGenericNode(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, const FString& NodeClassName,
	int32 PosX, int32 PosY, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP;
	UEdGraph* RuleGraph = GetTransitionRuleGraph(AnimBlueprintPath, IdentifyingStateName, FromStateName, ToStateName, TransitionIndex, ABP);
	if (!RuleGraph) return FString();

	FString FullName = NodeClassName;
	if (!FullName.StartsWith(TEXT("U"))) FullName = TEXT("U") + FullName;

	UClass* NodeClass = FindFirstObject<UClass>(*FullName, EFindFirstObjectOptions::ExactClass);
	if (!NodeClass)
	{
		NodeClass = FindFirstObject<UClass>(*NodeClassName, EFindFirstObjectOptions::ExactClass);
	}
	if (!NodeClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_AnimBPUtils: Node class '%s' not found"), *NodeClassName);
		return FString();
	}

	RuleGraph->Modify();
	UEdGraphNode* Node = NewObject<UEdGraphNode>(RuleGraph, NodeClass);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->AllocateDefaultPins();
	RuleGraph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Added generic node '%s' GUID=%s"),
		*NodeClassName, *Node->NodeGuid.ToString());
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

bool UAZ_AnimBlueprintUtils::RecreateTransitionRuleGraph(const FString& AnimBlueprintPath, const FString& IdentifyingStateName,
	const FString& FromStateName, const FString& ToStateName, int32 TransitionIndex)
{
#if WITH_EDITOR
	UAnimBlueprint* ABP = LoadABP(AnimBlueprintPath);
	if (!ABP) return false;

	UAnimationStateMachineGraph* SMGraph = FindSMGraphByStateName(ABP, IdentifyingStateName);
	if (!SMGraph) return false;

	UAnimStateTransitionNode* Trans = FindTransitionNode(SMGraph, FromStateName, ToStateName, TransitionIndex);
	if (!Trans) return false;

	// UnshareRules creates a new BoundGraph if needed
	Trans->UnshareRules();

	UE_LOG(LogTemp, Log, TEXT("AZ_AnimBPUtils: Recreated BoundGraph for %s->%s[%d] (has graph: %s)"),
		*FromStateName, *ToStateName, TransitionIndex, Trans->BoundGraph ? TEXT("yes") : TEXT("no"));
	return Trans->BoundGraph != nullptr;
#else
	return false;
#endif
}
