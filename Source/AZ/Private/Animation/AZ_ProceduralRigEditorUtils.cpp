#include "Animation/AZ_ProceduralRigEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_ControlRig.h"
#include "ControlRigAssetReference.h"
#include "K2Node_VariableGet.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#endif

FString UAZ_ProceduralRigEditorUtils::AddControlRigNode(const FString& BlueprintPath,
	const FString& RigAssetPath, const TArray<FName>& ExposedPins, int32 PosX, int32 PosY, FName RelevanceCallback)
{
#if WITH_EDITOR
	// Restrict this authoring operation to project-owned animation and rig assets.
	if (!BlueprintPath.StartsWith(TEXT("/Game/AZ/")) || !RigAssetPath.StartsWith(TEXT("/Game/AZ/")))
	{
		return FString();
	}
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
	UObject* RigAsset = LoadObject<UObject>(nullptr, *RigAssetPath);
	FControlRigAssetStrongReference RigReference(RigAsset);
	if (!Blueprint || !RigReference.IsValid()) return FString();

	UAnimationGraph* Graph = nullptr;
	for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
	{
		if (Candidate && Candidate->GetFName() == TEXT("AnimGraph"))
		{
			Graph = Cast<UAnimationGraph>(Candidate);
			break;
		}
	}
	if (!Graph) return FString();
	if (!RelevanceCallback.IsNone() && (!Blueprint->ParentClass || !Blueprint->ParentClass->FindFunctionByName(RelevanceCallback))) return FString();

	// These properties are private engine state. Reflect their exact checked types;
	// do not invoke PostEditChangeProperty (it reconstructs nodes and can trigger GC).
	FStructProperty* RigProperty = FindFProperty<FStructProperty>(FAnimNode_ControlRig::StaticStruct(), TEXT("ControlRigAssetReference"));
	FStructProperty* DefaultRigProperty = FindFProperty<FStructProperty>(FAnimNode_ControlRig::StaticStruct(), TEXT("DefaultControlRigAssetReference"));
	FArrayProperty* PinsProperty = FindFProperty<FArrayProperty>(UAnimGraphNode_ControlRig::StaticClass(), TEXT("CustomPinProperties"));
	FStructProperty* PinInner = PinsProperty ? CastField<FStructProperty>(PinsProperty->Inner) : nullptr;
	if (!RigProperty || !DefaultRigProperty || !PinInner
		|| RigProperty->Struct != FControlRigAssetStrongReference::StaticStruct()
		|| DefaultRigProperty->Struct != FControlRigAssetStrongReference::StaticStruct()
		|| PinInner->Struct != FOptionalPinFromProperty::StaticStruct()) return FString();

	Blueprint->Modify();
	Graph->Modify();
	UAnimGraphNode_ControlRig* Node = NewObject<UAnimGraphNode_ControlRig>(Graph, NAME_None, RF_Transactional);
	if (!RelevanceCallback.IsNone()) Node->BecomeRelevantFunction.SetSelfMember(RelevanceCallback);
	RigProperty->CopyCompleteValue(RigProperty->ContainerPtrToValuePtr<void>(&Node->Node), &RigReference);
	DefaultRigProperty->CopyCompleteValue(DefaultRigProperty->ContainerPtrToValuePtr<void>(&Node->Node), &RigReference);
	FScriptArrayHelper Pins(PinsProperty, PinsProperty->ContainerPtrToValuePtr<void>(Node));
	TSet<FName> UniquePins;
	for (FName PinName : ExposedPins)
	{
		if (PinName.IsNone() || UniquePins.Contains(PinName)) continue;
		UniquePins.Add(PinName);
		FOptionalPinFromProperty* Entry = reinterpret_cast<FOptionalPinFromProperty*>(Pins.GetRawPtr(Pins.AddValue()));
		Entry->PropertyName = PinName;
		Entry->bShowPin = true;
		Entry->bCanToggleVisibility = true;
	}
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	for (FName PinName : UniquePins)
	{
		if (!Node->FindPin(PinName, EGPD_Input))
		{
			UE_LOG(LogTemp, Error, TEXT("[ProceduralRigAuthor] Missing input %s on %s; removing incomplete node"), *PinName.ToString(), *RigAssetPath);
			Graph->RemoveNode(Node);
			return FString();
		}
	}
	Blueprint->MarkPackageDirty();
	return Node->NodeGuid.ToString();
#else
	return FString();
#endif
}

FString UAZ_ProceduralRigEditorUtils::ConnectControlRigInput(const FString& BlueprintPath,
	const FString& NodeGuid, FName InputPin, FName NativeProperty, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	if (!BlueprintPath.StartsWith(TEXT("/Game/AZ/"))) return FString();
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
	FGuid Guid;
	if (!Blueprint || !Blueprint->ParentClass || !FGuid::Parse(NodeGuid, Guid)) return FString();
	FProperty* Property = FindFProperty<FProperty>(Blueprint->ParentClass, NativeProperty);
	if (!Property || !Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) return FString();
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph || Graph->GetFName() != TEXT("AnimGraph")) continue;
		UAnimGraphNode_ControlRig* Target = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Guid) Target = Cast<UAnimGraphNode_ControlRig>(Node);
		}
		UEdGraphPin* TargetPin = Target ? Target->FindPin(InputPin, EGPD_Input) : nullptr;
		if (!TargetPin || !TargetPin->LinkedTo.IsEmpty()) return FString();
		Blueprint->Modify();
		Graph->Modify();
		UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
		Getter->VariableReference.SetSelfMember(NativeProperty);
		Getter->CreateNewGuid();
		Getter->NodePosX = PosX;
		Getter->NodePosY = PosY;
		Graph->AddNode(Getter, false, false);
		Getter->PostPlacedNewNode();
		Getter->AllocateDefaultPins();
		UEdGraphPin* ValuePin = Getter->FindPin(NativeProperty, EGPD_Output);
		if (!ValuePin || !Graph->GetSchema()->TryCreateConnection(ValuePin, TargetPin))
		{
			Graph->RemoveNode(Getter);
			return FString();
		}
		Blueprint->MarkPackageDirty();
		return Getter->NodeGuid.ToString();
	}
#endif
	return FString();
}
