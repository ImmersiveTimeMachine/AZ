// Copyright Artur. AZ project.

#include "InventoryUI/AZ_Inv_AuthoringUtils.h"

#include "AZ/AZ.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "UObject/Package.h"

namespace
{
	/** The ItemComponent TEMPLATE, i.e. the one edits persist on — not a spawned instance. */
	UAZ_Inv_CommonUI_ItemComponent* FindItemComponentTemplate(const UBlueprint* Blueprint)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}
		for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node)
			{
				if (auto* Item = Cast<UAZ_Inv_CommonUI_ItemComponent>(Node->ComponentTemplate))
				{
					return Item;
				}
			}
		}
		return nullptr;
	}

	FProperty* FindManifestProperty(const UAZ_Inv_CommonUI_ItemComponent* Component)
	{
		return Component ? Component->GetClass()->FindPropertyByName(TEXT("PickupItemManifest")) : nullptr;
	}
}

FString UAZ_Inv_AuthoringUtils::GetPickupManifestText(UBlueprint* Blueprint)
{
	UAZ_Inv_CommonUI_ItemComponent* Component = FindItemComponentTemplate(Blueprint);
	const FProperty* Property = FindManifestProperty(Component);
	if (!Property)
	{
		UE_LOG(Log_AZ, Warning, TEXT("[ItemAuthoring] No ItemComponent template on %s"), *GetNameSafe(Blueprint));
		return FString();
	}
	FString Out;
	Property->ExportTextItem_Direct(Out, Property->ContainerPtrToValuePtr<void>(Component), nullptr, Component, PPF_None);
	return Out;
}

bool UAZ_Inv_AuthoringUtils::SetPickupManifestText(UBlueprint* Blueprint, const FString& ManifestText)
{
	UAZ_Inv_CommonUI_ItemComponent* Component = FindItemComponentTemplate(Blueprint);
	const FProperty* Property = FindManifestProperty(Component);
	if (!Property)
	{
		UE_LOG(Log_AZ, Warning, TEXT("[ItemAuthoring] No ItemComponent template on %s"), *GetNameSafe(Blueprint));
		return false;
	}
	Component->Modify();
	Blueprint->Modify();
	// ImportText returns where it stopped; null means it rejected the text. Checked, because a manifest that
	// parsed HALFWAY would leave an item that looks authored and behaves as if half its fragments vanished.
	const TCHAR* Result = Property->ImportText_Direct(
		*ManifestText, Property->ContainerPtrToValuePtr<void>(Component), Component, PPF_None, GLog);
	if (!Result)
	{
		UE_LOG(Log_AZ, Warning, TEXT("[ItemAuthoring] Manifest text rejected for %s"), *GetNameSafe(Blueprint));
		return false;
	}
	// ★ Deliberately NOT MarkBlueprintAsStructurallyModified. That refreshes every node and can trigger a
	// recompile, and a recompile's garbage collection with the Python VM on the stack is the documented way
	// to kill this editor (PyUtil::CollectGarbage vs FPythonScriptPlugin::OnPreGarbageCollect). Modify() plus
	// a dirty package records the edit; COMPILING IS THE CALLER'S JOB, from the editor UI.
	Blueprint->MarkPackageDirty();
	UE_LOG(Log_AZ, Log, TEXT("[ItemAuthoring] Manifest written to %s (%d chars)"),
		*GetNameSafe(Blueprint), ManifestText.Len());
	return true;
}
