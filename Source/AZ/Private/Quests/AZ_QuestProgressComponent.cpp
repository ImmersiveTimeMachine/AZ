#include "Quests/AZ_QuestProgressComponent.h"
#include "Quests/AZ_QuestDefinition.h"
#include "Quests/AZ_QuestReachArea.h"
#include "Quests/AZ_QuestInventoryAdapter.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"
#include "Navigation/AZ_NavigationTargetComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
const FAZ_QuestObjectiveProgress* ObjectiveIn(const FAZ_QuestProgressRecord& Record, FName Id)
{
	return Record.Objectives.FindByPredicate([Id](const auto& Item) { return Item.ObjectiveId == Id; });
}
bool PrerequisitesMet(const FAZ_QuestProgressRecord& Record, const FAZ_QuestObjectiveDefinition& Definition)
{
	for (FName Id : Definition.PrerequisiteObjectives)
	{
		const auto* Progress = ObjectiveIn(Record, Id);
		if (!Progress || Progress->Status != EAZ_QuestObjectiveStatus::Completed) return false;
	}
	return true;
}
}

UAZ_QuestProgressComponent::UAZ_QuestProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_QuestProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UAZ_QuestProgressComponent, Records, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAZ_QuestProgressComponent, TrackedQuestId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAZ_QuestProgressComponent, TrackedObjectiveId, COND_OwnerOnly);
}

void UAZ_QuestProgressComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* PS = Cast<APlayerState>(GetOwner())) PS->OnPawnSet.AddUniqueDynamic(this, &ThisClass::OnPawnChanged);
	if (HasQuestAuthority())
	{
		if (auto* Registry = GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>())
			Registry->OnTargetsChanged.AddUniqueDynamic(this, &ThisClass::OnTargetsChanged);
		BindInventory();
		if (!BoundInventory.IsValid()) GetWorld()->GetTimerManager().SetTimer(InventoryBindingRetry, this, &ThisClass::BindInventory, .5f, true);
	}
}

void UAZ_QuestProgressComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* PS = Cast<APlayerState>(GetOwner())) PS->OnPawnSet.RemoveDynamic(this, &ThisClass::OnPawnChanged);
	if (BoundInventory.IsValid()) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshInventoryObjectives);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(InventoryBindingRetry);
	if (GetWorld())
		if (auto* Registry = GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>())
			Registry->OnTargetsChanged.RemoveDynamic(this, &ThisClass::OnTargetsChanged);
	Super::EndPlay(Reason);
}

bool UAZ_QuestProgressComponent::HasQuestAuthority() const { return GetOwner() && GetOwner()->HasAuthority(); }
APlayerController* UAZ_QuestProgressComponent::GetQuestController() const
{
	const auto* PS = Cast<APlayerState>(GetOwner());
	return PS ? PS->GetPlayerController() : nullptr;
}
UAZ_QuestProgressComponent* UAZ_QuestProgressComponent::FromPawn(const APawn* Pawn)
{
	APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PS ? PS->FindComponentByClass<UAZ_QuestProgressComponent>() : nullptr;
}
void UAZ_QuestProgressComponent::BindInventory()
{
	if (!HasQuestAuthority()) return;
	APlayerController* PC = GetQuestController();
	auto* Inventory = PC ? PC->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	if (BoundInventory.Get() != Inventory)
	{
		if (BoundInventory.IsValid()) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshInventoryObjectives);
		BoundInventory = Inventory;
		if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventoryObjectives);
	}
	if (Inventory)
	{
		GetWorld()->GetTimerManager().ClearTimer(InventoryBindingRetry);
		RefreshInventoryObjectives();
	}
}
void UAZ_QuestProgressComponent::OnPawnChanged(APlayerState*, APawn*, APawn*)
{
	if (HasQuestAuthority()) { BindInventory(); PublishProgressChanged(); }
}
void UAZ_QuestProgressComponent::OnTargetsChanged(FName)
{
	if (HasQuestAuthority()) PublishProgressChanged();
}
void UAZ_QuestProgressComponent::OnRep_Progress() { OnQuestProgressChanged.Broadcast(); }

FAZ_QuestProgressRecord* UAZ_QuestProgressComponent::FindRecord(FName Id)
{
	return Records.FindByPredicate([Id](const auto& Item) { return Item.QuestId == Id; });
}
const FAZ_QuestProgressRecord* UAZ_QuestProgressComponent::FindRecord(FName Id) const
{
	return Records.FindByPredicate([Id](const auto& Item) { return Item.QuestId == Id; });
}
UAZ_QuestDefinition* UAZ_QuestProgressComponent::FindQuestDefinition(FName Id) const
{
	const auto* Record = FindRecord(Id);
	return Record ? Record->Definition.LoadSynchronous() : nullptr;
}

bool UAZ_QuestProgressComponent::AcceptQuest(UAZ_QuestDefinition* Definition)
{
	FString Error;
	if (!HasQuestAuthority() || bRestoreTransaction || !IsValid(Definition) || !Definition->ValidateDefinition(Error)) return false;
	if (const auto* Existing = FindRecord(Definition->QuestId)) return Existing->Definition.Get() == Definition;
	if (!CanAcceptQuest(Definition, Error)) return false;
	FAZ_QuestProgressRecord Record;
	Record.QuestId = Definition->QuestId;
	Record.Definition = Definition;
	for (const auto& Objective : Definition->Objectives)
	{
		FAZ_QuestObjectiveProgress Progress;
		Progress.ObjectiveId = Objective.ObjectiveId;
		Record.Objectives.Add(Progress);
	}
	Reevaluate(Record, *Definition);
	Records.Add(MoveTemp(Record));
	LoadedDefinitions.AddUnique(Definition);
	PublishProgressChanged();
	return true;
}
bool UAZ_QuestProgressComponent::CanAcceptQuest(UAZ_QuestDefinition* Definition, FString& Error) const
{
	Error.Reset();
	if (bRestoreTransaction) { Error = TEXT("Quest progress is being restored."); return false; }
	if (!IsValid(Definition)) { Error = TEXT("Quest definition is missing."); return false; }
	if (!Definition->ValidateDefinition(Error)) return false;
	if (FindRecord(Definition->QuestId)) { Error = TEXT("Quest has already been accepted."); return false; }
	for (FName Id : Definition->PrerequisiteQuests)
	{
		const auto* Prerequisite = FindRecord(Id);
		if (!Prerequisite || Prerequisite->Status != EAZ_QuestStatus::Completed)
		{ Error = TEXT("Prerequisite quest has not completed."); return false; }
	}
	return true;
}

bool UAZ_QuestProgressComponent::Reevaluate(FAZ_QuestProgressRecord& Record, const UAZ_QuestDefinition& Definition)
{
	if (Record.Status != EAZ_QuestStatus::Active) return false;
	bool Changed = false;
	const bool HasRequired = Definition.Objectives.ContainsByPredicate([](const auto& Item) { return !Item.bOptional; });
	bool AllComplete = true;
	for (auto& Progress : Record.Objectives)
	{
		const auto* Objective = Definition.FindObjective(Progress.ObjectiveId);
		if (!Objective) return false;
		if (Progress.Status == EAZ_QuestObjectiveStatus::Locked && PrerequisitesMet(Record, *Objective))
		{ Progress.Status = EAZ_QuestObjectiveStatus::Active; Changed = true; }
		if ((!Objective->bOptional || Definition.bRequireOptionalObjectives || !HasRequired) && Progress.Status != EAZ_QuestObjectiveStatus::Completed)
			AllComplete = false;
	}
	if (AllComplete)
	{
		Record.Status = EAZ_QuestStatus::Completed;
		for (auto& Progress : Record.Objectives)
			if (Progress.Status == EAZ_QuestObjectiveStatus::Locked || Progress.Status == EAZ_QuestObjectiveStatus::Active)
				Progress.Status = EAZ_QuestObjectiveStatus::Cancelled;
		Changed = true;
	}
	return Changed;
}

bool UAZ_QuestProgressComponent::GetActiveObjective(FName QuestId, FName ObjectiveId, FAZ_QuestObjectiveDefinition& Out, int32& Remaining) const
{
	Remaining = 0;
	if (bRestoreTransaction) return false;
	const auto* Record = FindRecord(QuestId);
	const auto* Definition = FindQuestDefinition(QuestId);
	const auto* Progress = Record ? ObjectiveIn(*Record, ObjectiveId) : nullptr;
	const auto* Objective = Definition ? Definition->FindObjective(ObjectiveId) : nullptr;
	if (!Record || Record->Status != EAZ_QuestStatus::Active || !Progress || Progress->Status != EAZ_QuestObjectiveStatus::Active || !Objective) return false;
	Out = *Objective;
	Remaining = Objective->RequiredCount - Progress->CurrentCount;
	return Remaining > 0;
}

bool UAZ_QuestProgressComponent::ApplyCount(FName QuestId, FName ObjectiveId, int32 Count, FGuid ReceiptId, bool bAbsolute)
{
	FAZ_QuestObjectiveDefinition Objective;
	int32 Remaining;
	if (!HasQuestAuthority() || bRestoreTransaction || Count < 0 || !GetActiveObjective(QuestId, ObjectiveId, Objective, Remaining)) return false;
	auto* Record = FindRecord(QuestId);
	auto* Progress = Record->Objectives.FindByPredicate([ObjectiveId](const auto& Item) { return Item.ObjectiveId == ObjectiveId; });
	if (ReceiptId.IsValid() && Progress->ReceiptIds.Contains(ReceiptId)) return false;
	const int32 Next = bAbsolute ? FMath::Min(Count, Objective.RequiredCount) : Progress->CurrentCount + FMath::Min(Count, Remaining);
	if (Next == Progress->CurrentCount) return false;
	Progress->CurrentCount = Next;
	if (ReceiptId.IsValid()) Progress->ReceiptIds.Add(ReceiptId);
	if (Next >= Objective.RequiredCount) Progress->Status = EAZ_QuestObjectiveStatus::Completed;
	Reevaluate(*Record, *FindQuestDefinition(QuestId));
	return true;
}

bool UAZ_QuestProgressComponent::ReportObjectiveFact(FName QuestId, FName ObjectiveId, EAZ_QuestObjectiveKind Kind,
	APawn* InstigatorPawn, FName SourceTargetId, FGuid ReceiptId)
{
	if (bRestoreTransaction) return false;
	FAZ_QuestObjectiveDefinition Definition;
	int32 Remaining;
	if (!ReceiptId.IsValid() || !IsValid(InstigatorPawn) || InstigatorPawn->GetPlayerState() != GetOwner()
		|| !Cast<APlayerState>(GetOwner()) || Cast<APlayerState>(GetOwner())->GetPawn() != InstigatorPawn
		|| InstigatorPawn->GetWorld() != GetWorld() || (Kind != EAZ_QuestObjectiveKind::ReachArea && Kind != EAZ_QuestObjectiveKind::Interact)
		|| !GetActiveObjective(QuestId, ObjectiveId, Definition, Remaining) || Definition.Kind != Kind
		|| Definition.Target.TargetId != SourceTargetId) return false;
	if (!ApplyCount(QuestId, ObjectiveId, 1, ReceiptId, false)) return false;
	PublishProgressChanged();
	return true;
}

void UAZ_QuestProgressComponent::RefreshInventoryObjectives()
{
	if (!HasQuestAuthority() || bRestoreTransaction || !GetQuestController()) return;
	bool Changed = false;
	const auto Snapshot = Records;
	for (const auto& Record : Snapshot)
		for (const auto& Progress : Record.Objectives)
		{
			FAZ_QuestObjectiveDefinition Definition;
			int32 Remaining;
			if (GetActiveObjective(Record.QuestId, Progress.ObjectiveId, Definition, Remaining) && Definition.Kind == EAZ_QuestObjectiveKind::PossessItem)
				Changed |= ApplyCount(Record.QuestId, Progress.ObjectiveId, UAZ_QuestInventoryAdapter::CountOwnedItems(GetQuestController(), Definition.ItemType), FGuid(), true);
		}
	if (Changed) PublishProgressChanged();
}

bool UAZ_QuestProgressComponent::CancelQuest(FName Id)
{
	auto* Record = FindRecord(Id);
	if (!HasQuestAuthority() || bRestoreTransaction || !Record || Record->Status != EAZ_QuestStatus::Active) return false;
	Record->Status = EAZ_QuestStatus::Cancelled;
	for (auto& Progress : Record->Objectives)
		if (Progress.Status == EAZ_QuestObjectiveStatus::Locked || Progress.Status == EAZ_QuestObjectiveStatus::Active) Progress.Status = EAZ_QuestObjectiveStatus::Cancelled;
	PublishProgressChanged();
	return true;
}
bool UAZ_QuestProgressComponent::FailObjective(FName QuestId, FName Id)
{
	FAZ_QuestObjectiveDefinition Definition;
	int32 Remaining;
	if (!HasQuestAuthority() || bRestoreTransaction || !GetActiveObjective(QuestId, Id, Definition, Remaining)) return false;
	auto* Record = FindRecord(QuestId);
	for (auto& Progress : Record->Objectives) if (Progress.ObjectiveId == Id) Progress.Status = EAZ_QuestObjectiveStatus::Failed;
	if (Definition.bFailureFailsQuest)
	{
		Record->Status = EAZ_QuestStatus::Failed;
		for (auto& Progress : Record->Objectives)
			if (Progress.Status == EAZ_QuestObjectiveStatus::Active || Progress.Status == EAZ_QuestObjectiveStatus::Locked) Progress.Status = EAZ_QuestObjectiveStatus::Cancelled;
	}
	PublishProgressChanged();
	return true;
}

bool UAZ_QuestProgressComponent::SetTrackedObjective(FName QuestId, FName ObjectiveId)
{
	if (bRestoreTransaction) return false;
	if (!HasQuestAuthority())
	{
		if (APlayerController* PC = GetQuestController(); PC && PC->IsLocalController())
		{ ServerSetTrackedObjective(QuestId, ObjectiveId); return true; }
		return false;
	}
	if (!QuestId.IsNone())
	{
		const auto* Record = FindRecord(QuestId);
		if (!Record || Record->Status != EAZ_QuestStatus::Active) return false;
		const auto* Objective = ObjectiveIn(*Record, ObjectiveId);
		if (!Objective || Objective->Status != EAZ_QuestObjectiveStatus::Active) return false;
	}
	else if (!ObjectiveId.IsNone()) return false;
	if (TrackedQuestId == QuestId && TrackedObjectiveId == ObjectiveId) return true;
	TrackedQuestId = QuestId;
	TrackedObjectiveId = ObjectiveId;
	PublishProgressChanged();
	return true;
}
void UAZ_QuestProgressComponent::ServerSetTrackedObjective_Implementation(FName QuestId, FName ObjectiveId)
{
	SetTrackedObjective(QuestId, ObjectiveId);
}
bool UAZ_QuestProgressComponent::GetTrackedTarget(FAZ_NavigationTargetDescriptor& OutTarget) const
{
	OutTarget = FAZ_NavigationTargetDescriptor();
	FAZ_QuestObjectiveDefinition Definition;
	int32 Remaining;
	if (!GetActiveObjective(TrackedQuestId, TrackedObjectiveId, Definition, Remaining)) return false;
	OutTarget = Definition.Target;
	return !OutTarget.TargetId.IsNone() || OutTarget.bHasWorldLocation;
}
void UAZ_QuestProgressComponent::NormalizeTracking()
{
	const auto* Record = FindRecord(TrackedQuestId);
	if (!Record || Record->Status != EAZ_QuestStatus::Active) { TrackedQuestId = NAME_None; TrackedObjectiveId = NAME_None; return; }
	const auto* Current = ObjectiveIn(*Record, TrackedObjectiveId);
	if (Current && Current->Status == EAZ_QuestObjectiveStatus::Active) return;
	const auto* Next = Record->Objectives.FindByPredicate([](const auto& Item) { return Item.Status == EAZ_QuestObjectiveStatus::Active; });
	TrackedObjectiveId = Next ? Next->ObjectiveId : NAME_None;
}

bool UAZ_QuestProgressComponent::IsDeliveryReceiptCommitted(FName QuestId, FName Id, FGuid Receipt) const
{
	const auto* Record = FindRecord(QuestId);
	const auto* Progress = Record ? ObjectiveIn(*Record, Id) : nullptr;
	return Receipt.IsValid() && Progress && Progress->ReceiptIds.Contains(Receipt);
}
bool UAZ_QuestProgressComponent::ValidateDelivery(FName QuestId, FName Id, AActor* Recipient, FGuid Receipt,
	FGameplayTag& ItemType, int32& Remaining, FString& Error) const
{
	Error.Reset(); ItemType = FGameplayTag(); Remaining = 0;
	FAZ_QuestObjectiveDefinition Definition;
	if (!HasQuestAuthority() || bRestoreTransaction || !Receipt.IsValid() || !GetActiveObjective(QuestId, Id, Definition, Remaining) || Definition.Kind != EAZ_QuestObjectiveKind::DeliverItem)
	{ Error = TEXT("Delivery objective is not eligible."); return false; }
	APlayerController* PC = GetQuestController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	auto* Registry = GetWorld() ? GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>() : nullptr;
	UAZ_NavigationTargetComponent* Provider = nullptr;
	FVector Location;
	if (!IsValid(Recipient) || !IsValid(Pawn) || !Registry || Recipient->GetWorld() != GetWorld()
		|| Registry->ResolveTarget(Definition.Target, Location, Provider) != EAZ_NavigationTargetResolveResult::ResolvedProvider
		|| !Provider || Provider->GetOwner() != Recipient || FVector::DistSquared(Pawn->GetActorLocation(), Location) > FMath::Square(Definition.InteractionRadius))
	{ Error = TEXT("Delivery recipient is unresolved, ambiguous or out of range."); return false; }
	ItemType = Definition.ItemType;
	return true;
}
bool UAZ_QuestProgressComponent::CommitDeliverySilent(FName QuestId, FName Id, int32 Amount, FGuid Receipt)
{
	FAZ_QuestObjectiveDefinition Definition;
	int32 Remaining;
	if (bRestoreTransaction || !Receipt.IsValid() || Amount <= 0 || !GetActiveObjective(QuestId, Id, Definition, Remaining)
		|| Definition.Kind != EAZ_QuestObjectiveKind::DeliverItem || Amount > Remaining) return false;
	return ApplyCount(QuestId, Id, Amount, Receipt, false);
}
bool UAZ_QuestProgressComponent::TryDeliverObjective(FName QuestId, FName Id, AActor* Recipient, FGuid Receipt, FString& Error)
{
	if (bRestoreTransaction) { Error = TEXT("Cannot deliver while quest progress is being restored."); return false; }
	return UAZ_QuestInventoryAdapter::TryDeliverObjective(this, QuestId, Id, Recipient, Receipt, Error);
}

bool UAZ_QuestProgressComponent::ValidateRestoreProgress(const TArray<FAZ_QuestProgressRecord>& Progress, FString& Error) const
{
	Error.Reset();
	TSet<FName> QuestIds;
	TArray<UAZ_QuestDefinition*> Catalog;
	for (const auto& Record : Progress)
	{
		auto* Definition = Record.Definition.LoadSynchronous();
		if (!Definition || !Definition->ValidateDefinition(Error) || Definition->QuestId != Record.QuestId || QuestIds.Contains(Record.QuestId)
			|| Record.Objectives.Num() != Definition->Objectives.Num() || Record.Status == EAZ_QuestStatus::Available
			|| static_cast<uint8>(Record.Status) > static_cast<uint8>(EAZ_QuestStatus::Cancelled))
		{ Error = TEXT("Invalid, duplicate or incompatible saved quest definition."); return false; }
		QuestIds.Add(Record.QuestId);
		Catalog.Add(Definition);
		TSet<FName> ObjectiveIds;
		const bool HasRequired = Definition->Objectives.ContainsByPredicate([](const auto& Item) { return !Item.bOptional; });
		for (const auto& Item : Record.Objectives)
		{
			const auto* Objective = Definition->FindObjective(Item.ObjectiveId);
			if (!Objective || ObjectiveIds.Contains(Item.ObjectiveId) || Item.CurrentCount < 0 || Item.CurrentCount > Objective->RequiredCount
				|| static_cast<uint8>(Item.Status) > static_cast<uint8>(EAZ_QuestObjectiveStatus::Cancelled)
				|| (Item.Status == EAZ_QuestObjectiveStatus::Completed && Item.CurrentCount != Objective->RequiredCount)
				|| ((Item.Status == EAZ_QuestObjectiveStatus::Active || Item.Status == EAZ_QuestObjectiveStatus::Completed) && !PrerequisitesMet(Record, *Objective))
				|| (Item.Status == EAZ_QuestObjectiveStatus::Locked && Item.CurrentCount != 0)
				|| (Item.Status != EAZ_QuestObjectiveStatus::Completed && Item.CurrentCount == Objective->RequiredCount)
				|| (Item.Status == EAZ_QuestObjectiveStatus::Failed && Objective->bFailureFailsQuest && Record.Status != EAZ_QuestStatus::Failed)
				|| Item.ReceiptIds.Num() > Item.CurrentCount
				|| (Record.Status != EAZ_QuestStatus::Active && (Item.Status == EAZ_QuestObjectiveStatus::Active || Item.Status == EAZ_QuestObjectiveStatus::Locked))
				|| (Record.Status == EAZ_QuestStatus::Completed && (!Objective->bOptional || !HasRequired || Definition->bRequireOptionalObjectives) && Item.Status != EAZ_QuestObjectiveStatus::Completed))
			{ Error = TEXT("Invalid saved objective state or count."); return false; }
			ObjectiveIds.Add(Item.ObjectiveId);
			TSet<FGuid> Receipts;
			for (const FGuid& Receipt : Item.ReceiptIds)
			{
				if (!Receipt.IsValid() || Receipts.Contains(Receipt)) { Error = TEXT("Invalid/duplicate objective receipt."); return false; }
				Receipts.Add(Receipt);
			}
		}
	}
	if (!UAZ_QuestDefinition::ValidateQuestCatalog(Catalog, Error)) return false;
	for (const auto& Record : Progress)
	{
		const auto* Definition = Record.Definition.LoadSynchronous();
		for (FName PrerequisiteId : Definition->PrerequisiteQuests)
		{
			const auto* Prerequisite = Progress.FindByPredicate([PrerequisiteId](const auto& Item) { return Item.QuestId == PrerequisiteId; });
			if (!Prerequisite || Prerequisite->Status != EAZ_QuestStatus::Completed)
			{ Error = TEXT("Saved quest prerequisites are missing or incomplete."); return false; }
		}
	}
	return true;
}
bool UAZ_QuestProgressComponent::RestoreProgress(const TArray<FAZ_QuestProgressRecord>& Progress, FString& Error, bool bPublish)
{
	if (!HasQuestAuthority()) { Error = TEXT("Only authority can restore quest progress."); return false; }
	if (bRestoreTransaction && bPublish) { Error = TEXT("Use silent progress restore inside the campaign transaction."); return false; }
	if (!ValidateRestoreProgress(Progress, Error)) return false;
	Records = Progress;
	LoadedDefinitions.Reset();
	for (auto& Record : Records)
	{
		auto* Definition = Record.Definition.LoadSynchronous();
		LoadedDefinitions.AddUnique(Definition);
		Reevaluate(Record, *Definition);
	}
	NormalizeTracking();
	if (bPublish) PublishProgressChanged();
	return true;
}
void UAZ_QuestProgressComponent::ReconcileWorldAreas()
{
	if (bRestoreTransaction || !GetWorld()) return;
	for (TActorIterator<AAZ_QuestReachArea> It(GetWorld()); It; ++It) It->ReconcilePlayer(Cast<APlayerState>(GetOwner()));
}
void UAZ_QuestProgressComponent::PublishProgressChanged()
{
	if (!HasQuestAuthority() || bRestoreTransaction) return;
	if (bPublishing) { bPublishAgain = true; return; }
	TGuardValue<bool> Guard(bPublishing, true);
	do
	{
		bPublishAgain = false;
		RefreshInventoryObjectives();
		ReconcileWorldAreas();
		NormalizeTracking();
		// Finish cascaded already-satisfied prerequisites before exposing a snapshot.
		if (bPublishAgain) continue;
		GetOwner()->ForceNetUpdate();
		OnQuestProgressChanged.Broadcast();
	} while (bPublishAgain);
}

bool UAZ_QuestProgressComponent::BeginRestoreTransaction()
{
	if (!HasQuestAuthority() || bRestoreTransaction || bPublishing) return false;
	bRestoreTransaction = true;
	bPublishAgain = false;
	return true;
}

void UAZ_QuestProgressComponent::EndRestoreTransaction(bool bPublish)
{
	if (!HasQuestAuthority() || !bRestoreTransaction) return;
	bRestoreTransaction = false;
	// Facts arriving during restore are deliberately not replayed: completion
	// conditions are freshly reconciled against the committed pawn/world state.
	if (bPublish) PublishProgressChanged();
}
