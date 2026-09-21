#include "Quests/AZ_QuestDefinition.h"

const FAZ_QuestObjectiveDefinition* UAZ_QuestDefinition::FindObjective(FName Id) const
{
	return Objectives.FindByPredicate([Id](const auto& Item) { return Item.ObjectiveId == Id; });
}

FPrimaryAssetId UAZ_QuestDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Quest"), QuestId.IsNone() ? GetFName() : QuestId);
}

bool UAZ_QuestDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	if (QuestId.IsNone() || Objectives.IsEmpty()) { OutError = TEXT("Quest needs an ID and at least one objective."); return false; }
	TSet<FName> Ids;
	for (const auto& Objective : Objectives)
	{
		if (Objective.ObjectiveId.IsNone() || Ids.Contains(Objective.ObjectiveId) || Objective.RequiredCount < 1
			|| static_cast<uint8>(Objective.Kind) > static_cast<uint8>(EAZ_QuestObjectiveKind::DeliverItem)
			|| !FMath::IsFinite(Objective.InteractionRadius) || Objective.InteractionRadius <= 0.f)
		{ OutError = TEXT("Invalid/duplicate objective ID, count or interaction radius."); return false; }
		Ids.Add(Objective.ObjectiveId);
		if (Objective.Kind == EAZ_QuestObjectiveKind::ReachArea && Objective.RequiredCount != 1)
		{ OutError = TEXT("ReachArea is a condition and requires a count of one."); return false; }
		if ((Objective.Kind == EAZ_QuestObjectiveKind::PossessItem || Objective.Kind == EAZ_QuestObjectiveKind::DeliverItem) && !Objective.ItemType.IsValid())
		{ OutError = TEXT("Inventory objectives need an item type tag."); return false; }
		if (Objective.Kind != EAZ_QuestObjectiveKind::PossessItem && Objective.Target.TargetId.IsNone())
		{ OutError = TEXT("Spatial/action objectives need an authored target ID."); return false; }
		if ((!Objective.Target.TargetId.IsNone() || Objective.Target.bHasWorldLocation) && !Objective.Target.IsWellFormed())
		{ OutError = TEXT("Objective navigation descriptor is invalid."); return false; }
	}
	TSet<FName> QuestPrerequisites;
	for (FName Id : PrerequisiteQuests)
	{
		if (Id.IsNone() || Id == QuestId || QuestPrerequisites.Contains(Id))
		{ OutError = TEXT("Invalid/duplicate prerequisite quest."); return false; }
		QuestPrerequisites.Add(Id);
	}
	TSet<FName> Resolved;
	for (const auto& Objective : Objectives)
	{
		TSet<FName> Seen;
		for (FName Id : Objective.PrerequisiteObjectives)
		{
			if (!Ids.Contains(Id) || Id == Objective.ObjectiveId || Seen.Contains(Id))
			{ OutError = TEXT("Missing/self/duplicate objective prerequisite."); return false; }
			Seen.Add(Id);
		}
	}
	for (int32 Pass = 0; Pass < Objectives.Num(); ++Pass)
	{
		for (const auto& Objective : Objectives)
		{
			if (!Resolved.Contains(Objective.ObjectiveId) && !Objective.PrerequisiteObjectives.ContainsByPredicate([&](FName Id) { return !Resolved.Contains(Id); }))
				Resolved.Add(Objective.ObjectiveId);
		}
	}
	if (Resolved.Num() != Objectives.Num()) { OutError = TEXT("Objective prerequisites contain a cycle."); return false; }
	return true;
}

bool UAZ_QuestDefinition::ValidateQuestCatalog(const TArray<UAZ_QuestDefinition*>& Definitions, FString& Error)
{
	Error.Reset();
	TSet<FName> Ids;
	for (const auto* Definition : Definitions)
	{
		if (!IsValid(Definition) || !Definition->ValidateDefinition(Error))
		{ if (Error.IsEmpty()) Error = TEXT("Missing quest definition."); return false; }
		if (Ids.Contains(Definition->QuestId)) { Error = TEXT("Duplicate quest ID in catalog."); return false; }
		Ids.Add(Definition->QuestId);
	}
	for (const auto* Definition : Definitions)
		for (FName Prerequisite : Definition->PrerequisiteQuests)
			if (!Ids.Contains(Prerequisite)) { Error = TEXT("Quest prerequisite is missing from catalog."); return false; }
	TSet<FName> Resolved;
	for (int32 Pass = 0; Pass < Definitions.Num(); ++Pass)
		for (const auto* Definition : Definitions)
			if (!Resolved.Contains(Definition->QuestId) && !Definition->PrerequisiteQuests.ContainsByPredicate([&](FName Id) { return !Resolved.Contains(Id); }))
				Resolved.Add(Definition->QuestId);
	if (Resolved.Num() != Definitions.Num()) { Error = TEXT("Quest prerequisites contain a cycle."); return false; }
	return true;
}
