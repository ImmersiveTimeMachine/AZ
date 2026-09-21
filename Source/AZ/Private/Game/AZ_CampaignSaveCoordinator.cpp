#include "Game/AZ_CampaignSaveCoordinator.h"
#include "Game/AZ_CampaignCheckpoint.h"
#include "Game/AZ_CampaignSaveGame.h"
#include "Game/AZ_CampaignWorldSubsystem.h"
#include "Game/AZ_CampaignTeleportEffect.h"
#include "Game/AZ_GameInstance.h"
#include "Player/AZ_PlayerState.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "UI/AZ_QuestMapComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Misc/Paths.h"

namespace
{
FString CampaignWorldName(const UWorld* World)
{
	return World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
}
bool ValidHeader(const UAZ_CampaignSaveGame* Save)
{
	return Save && Save->SchemaVersion == UAZ_CampaignSaveGame::CurrentVersion && Save->Sequence > 0 &&
		Save->Sequence < MAX_int64 && Save->SnapshotId.IsValid() && !Save->WorldPackage.IsEmpty();
}
bool PersistentAttribute(const FStructProperty* Property)
{
	return Property && Property->Struct == FGameplayAttributeData::StaticStruct() &&
		!Property->HasAnyPropertyFlags(CPF_Transient) && !Property->GetName().StartsWith(TEXT("Incoming"));
}
FProperty* ResolveAttribute(UAbilitySystemComponent* ASC, const FAZ_CampaignAttributeSnapshot& Record)
{
	UClass* Class = Record.SetClass.LoadSynchronous();
	if (!Class || !FMath::IsFinite(Record.BaseValue)) return nullptr;
	bool bFoundSet = false;
	for (const UAttributeSet* Set : ASC->GetSpawnedAttributes()) if (Set && Set->GetClass() == Class) { bFoundSet = true; break; }
	auto* Property = bFoundSet ? FindFProperty<FStructProperty>(Class, Record.AttributeName) : nullptr;
	return PersistentAttribute(Property) ? Property : nullptr;
}
}

UAZ_CampaignSaveCoordinator::UAZ_CampaignSaveCoordinator()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UAZ_CampaignSaveCoordinator* UAZ_CampaignSaveCoordinator::GetOrCreateForController(APlayerController* Controller)
{
	if (!IsValid(Controller) || !Controller->HasAuthority() || !Controller->IsLocalController() ||
		!Controller->GetWorld() || !Controller->GetWorld()->IsGameWorld()) return nullptr;
	if (auto* Existing = Controller->FindComponentByClass<UAZ_CampaignSaveCoordinator>()) return Existing;
	auto* Result = NewObject<UAZ_CampaignSaveCoordinator>(Controller, TEXT("CampaignSaveCoordinator"));
	Controller->AddInstanceComponent(Result);
	Result->RegisterComponent();
	return Result;
}

void UAZ_CampaignSaveCoordinator::BeginPlay()
{
	Super::BeginPlay();
	if (auto* PC = Cast<APlayerController>(GetOwner()); PC && PC->HasAuthority() && PC->IsLocalController())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		RefreshBindings();
	}
}

void UAZ_CampaignSaveCoordinator::RefreshBindings()
{
	if (bLoading || bEndingPlay) return;
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->HasAuthority() || !PC->IsLocalController()) return;
	auto* PS = PC->GetPlayerState<AAZ_PlayerState>();
	auto* Progress = PS ? PS->FindComponentByClass<UAZ_QuestProgressComponent>() : nullptr;
	if (Progress != QuestProgress)
	{
		if (QuestProgress) QuestProgress->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::HandleQuestChanged);
		QuestProgress = Progress;
		if (Progress) Progress->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestChanged);
		LastProgressFingerprint = ProgressFingerprint();
	}
	if (Progress) GetWorld()->GetTimerManager().ClearTimer(BindingTimer);
	else if (!GetWorld()->GetTimerManager().IsTimerActive(BindingTimer))
		GetWorld()->GetTimerManager().SetTimer(BindingTimer, this, &ThisClass::RefreshBindings, .25f, true);
}

void UAZ_CampaignSaveCoordinator::HandlePawnChanged(APawn*, APawn*)
{
	if (bLoading)
	{
		if (bRestoreCallInProgress) { bContextChangedDuringRestore = true; return; }
		CancelRestoreForContextChange(TEXT("Possession changed; checkpoint load was abandoned."));
	}
	RefreshBindings();
}

void UAZ_CampaignSaveCoordinator::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	if (bLoading)
	{
		if (bRestoreCallInProgress) bContextChangedDuringRestore = true;
		else CancelRestoreForContextChange(TEXT("World ended during checkpoint load."));
	}
	if (QuestProgress) QuestProgress->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::HandleQuestChanged);
	if (auto* PC = Cast<APlayerController>(GetOwner())) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

void UAZ_CampaignSaveCoordinator::CancelRestoreForContextChange(const FString& Message)
{
	// Teardown/possession is not a teleport rollback. The original pawn may no
	// longer exist, so never wait for it or reconcile areas for the new pawn.
	if (TeleportToken) TeleportToken->CancelBeforeApply();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LoadTimeout);
		if (auto* WorldState = GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>()) WorldState->CancelRestore();
	}
	if (RestoreMover)
	{
		RestoreMover->OnTeleportSucceeded.RemoveDynamic(this, &ThisClass::HandleTeleportSuccess);
		RestoreMover->OnTeleportFailed.RemoveDynamic(this, &ThisClass::HandleTeleportFailure);
	}
	if (bDataCommitted)
	{
		if (RestoreInventory) RestoreInventory->PublishCampaignInventoryRestore(false);
		if (RestoreEquipment) RestoreEquipment->PublishCampaignRestore(false);
		if (RestoreQuickBar) RestoreQuickBar->PublishCampaignRestore(false);
	}
	else
	{
		if (RestoreEquipment) RestoreEquipment->CancelCampaignRestore();
		if (RestoreInventory) RestoreInventory->CancelCampaignInventoryRestore();
		if (RestoreQuickBar) RestoreQuickBar->CancelCampaignRestore();
	}
	if (QuestProgress) QuestProgress->EndRestoreTransaction(false);
	if (auto* PC = Cast<APlayerController>(GetOwner()); PC && bOwnsInputLock)
	{ PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); }
	bOwnsInputLock = bDataCommitted = bLoading = bReturningToStart = bRollbackBlocked = bContextChangedDuringRestore = false;
	TeleportToken.Reset(); PendingLoad = nullptr;
	RestoreInventory = nullptr; RestoreEquipment = nullptr; RestoreQuickBar = nullptr; RestoreMover = nullptr;
	LastMessage = Message;
	if (!bEndingPlay) OnLoadCompleted.Broadcast(false, Message);
}

bool UAZ_CampaignSaveCoordinator::ResolveSlot(FString& Slot, int32& Index) const
{
	Slot = DefaultSlotName;
	Index = 0;
	if (const auto* GI = GetWorld() ? Cast<UAZ_GameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (!GI->LoadSlotName.IsEmpty()) Slot = GI->LoadSlotName;
		Index = GI->LoadSlotIndex;
	}
	return !Slot.IsEmpty() && Slot.Len() <= 100 && FPaths::GetCleanFilename(Slot) == Slot &&
		!Slot.Contains(TEXT("..")) && !Slot.Contains(TEXT(":")) && Index >= 0;
}

bool UAZ_CampaignSaveCoordinator::HasCampaignSave() const
{
	FString Slot; int32 Index;
	if (!ResolveSlot(Slot, Index)) return false;
	for (const TCHAR* Suffix : {TEXT("_A"), TEXT("_B")})
		if (ValidHeader(Cast<UAZ_CampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot + Suffix, Index)))) return true;
	return false;
}

bool UAZ_CampaignSaveCoordinator::ValidateContext(FString& Error) const
{
	const auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->HasAuthority() || !PC->IsLocalController() || !GetWorld() || !GetWorld()->IsGameWorld() || GetWorld()->IsPaused())
	{ Error = TEXT("Checkpointing requires the local authoritative running campaign."); return false; }
	int32 Players = 0;
	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It) if (!It->IsActorBeingDestroyed()) ++Players;
	if (Players != 1) { Error = TEXT("This checkpoint format supports one protagonist; multiplayer saving is not enabled."); return false; }
	const APawn* Pawn = PC->GetPawn();
	const auto* PS = PC->GetPlayerState<AAZ_PlayerState>();
	const auto* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!IsValid(Pawn) || !PS || !PS->GetAbilitySystemComponent() || !QuestProgress || QuestProgress->IsRestoreTransactionActive() ||
		QuestProgress->GetOwner() != PS || !Mover || Mover->GetMovementModeName() != FName(TEXT("Walking")))
	{ Error = TEXT("Wait for the player and grounded movement to become ready."); return false; }
	const auto* Inventory = PC->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	const auto* Equipment = PC->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	const auto* QuickBar = PC->FindComponentByClass<UAZ_QuickBarComponent>();
	if (!Inventory || !Equipment || !QuickBar) { Error = TEXT("Campaign inventory/equipment is unavailable."); return false; }
	return Inventory->CanCaptureCampaignInventory(Error) && Equipment->CanCaptureCampaignState(Error) && QuickBar->CanCaptureCampaignState(Error);
}

bool UAZ_CampaignSaveCoordinator::Capture(UAZ_CampaignSaveGame& Save, FString& Error) const
{
	if (!ValidateContext(Error)) return false;
	auto* PC = CastChecked<APlayerController>(GetOwner());
	auto* PS = PC->GetPlayerState<AAZ_PlayerState>();
	APawn* Pawn = PC->GetPawn();
	Save.SchemaVersion = UAZ_CampaignSaveGame::CurrentVersion;
	Save.SnapshotId = FGuid::NewGuid(); Save.SavedAtUtc = FDateTime::UtcNow();
	Save.WorldPackage = CampaignWorldName(GetWorld());
	Save.PlayerTransform = Pawn->GetActorTransform(); Save.ControlRotation = PC->GetControlRotation();
	if (const auto* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>()) Save.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Save.Level = PS->GetPlayerLevel(); Save.XP = PS->GetXP();
	Save.AttributePoints = PS->GetAttributePoints(); Save.SpellPoints = PS->GetSpellPoints();
	Save.Attributes.Reset();
	auto* ASC = PS->GetAbilitySystemComponent();
	for (const UAttributeSet* Set : ASC->GetSpawnedAttributes())
	{
		if (!Set) continue;
		for (TFieldIterator<FStructProperty> It(Set->GetClass()); It; ++It)
			if (PersistentAttribute(*It))
			{
				auto& Record = Save.Attributes.AddDefaulted_GetRef();
				Record.SetClass = Set->GetClass(); Record.AttributeName = It->GetFName();
				Record.BaseValue = ASC->GetNumericAttributeBase(FGameplayAttribute(*It));
			}
	}
	if (!PC->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>()->CaptureCampaignInventory(Save.Inventory, Error) ||
		!PC->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()->CaptureCampaignState(Save.Equipment, Error) ||
		!PC->FindComponentByClass<UAZ_QuickBarComponent>()->CaptureCampaignState(Save.QuickBar, Error) ||
		!GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>()->Capture(Save.World, Error)) return false;
	Save.Quests = QuestProgress->ExportProgress();
	Save.TrackedQuestId = QuestProgress->GetTrackedQuestId(); Save.TrackedObjectiveId = QuestProgress->GetTrackedObjectiveId();
	if (const auto* Map = UAZ_QuestMapComponent::FindForController(PC)) Save.Waypoint = Map->GetWaypoint();
	if (const auto* State = GetWorld()->GetGameState<AAZ_GameStateBase>()) Save.Difficulty = State->GetDifficultyLevel();
	return true;
}

bool UAZ_CampaignSaveCoordinator::ValidateSave(UAZ_CampaignSaveGame& Save, FString& Error) const
{
	if (!ValidHeader(&Save) || Save.WorldPackage != CampaignWorldName(GetWorld()) || Save.PlayerTransform.ContainsNaN() ||
		!Save.PlayerTransform.IsValid() || Save.ControlRotation.ContainsNaN() || !FMath::IsFinite(Save.CapsuleHalfHeight) ||
		Save.CapsuleHalfHeight < 0.f || Save.Level < 1 || Save.XP < 0 || Save.AttributePoints < 0 || Save.SpellPoints < 0 ||
		Save.Attributes.Num() > 1000 || static_cast<uint8>(Save.Difficulty) > static_cast<uint8>(EDifficultyLevel::Expert))
	{ Error = TEXT("Checkpoint version, map or player data is incompatible. Load the saved map first."); return false; }
	if (Save.Waypoint.bActive && !Save.Waypoint.IsWellFormed()) { Error = TEXT("Saved waypoint is invalid."); return false; }
	auto* PC = CastChecked<APlayerController>(GetOwner());
	auto* ASC = PC->GetPlayerState<AAZ_PlayerState>()->GetAbilitySystemComponent();
	TSet<FString> Keys;
	for (const auto& A : Save.Attributes)
	{
		const FString Key = A.SetClass.ToSoftObjectPath().ToString() + TEXT(":") + A.AttributeName.ToString();
		if (Keys.Contains(Key) || !ResolveAttribute(ASC, A)) { Error = TEXT("Saved attribute schema is incompatible."); return false; }
		Keys.Add(Key);
		if (A.SetClass.Get() == UAZ_VitalsAttributeSet::StaticClass() &&
			(A.AttributeName == TEXT("Health") || A.AttributeName == TEXT("MaxHealth")) && A.BaseValue <= 0.f)
		{ Error = TEXT("Checkpoint does not contain a living protagonist."); return false; }
	}
	if (!Save.TrackedQuestId.IsNone())
	{
		const auto* Quest = Save.Quests.FindByPredicate([&](const auto& Q) { return Q.QuestId == Save.TrackedQuestId; });
		const auto* Objective = Quest ? Quest->Objectives.FindByPredicate([&](const auto& O) { return O.ObjectiveId == Save.TrackedObjectiveId; }) : nullptr;
		if (!Quest || Quest->Status != EAZ_QuestStatus::Active || !Objective || Objective->Status != EAZ_QuestObjectiveStatus::Active)
		{ Error = TEXT("Saved tracked objective is invalid."); return false; }
	}
	else if (!Save.TrackedObjectiveId.IsNone()) { Error = TEXT("Saved tracking IDs are incomplete."); return false; }
	return PC->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>()->ValidateCampaignInventory(Save.Inventory, Error) &&
		PC->FindComponentByClass<UAZ_QuickBarComponent>()->ValidateCampaignState(Save.QuickBar, Save.Inventory, Error) &&
		QuestProgress->ValidateRestoreProgress(Save.Quests, Error) &&
		GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>()->Validate(Save.World, Save.Inventory, Error);
}

bool UAZ_CampaignSaveCoordinator::WriteCheckpoint(FName Checkpoint, FString& Error)
{
	if (bLoading) { Error = TEXT("Checkpoint load is still running."); return false; }
	RefreshBindings();
	FString Slot; int32 Index;
	if (!ResolveSlot(Slot, Index)) { Error = TEXT("Campaign save slot name is invalid."); return false; }
	TStrongObjectPtr<UAZ_CampaignSaveGame> Save(NewObject<UAZ_CampaignSaveGame>());
	if (!Capture(*Save, Error)) return false;
	int64 Sequences[2] = {0, 0};
	for (int32 I = 0; I < 2; ++I)
		if (auto* Previous = Cast<UAZ_CampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot + (I == 0 ? TEXT("_A") : TEXT("_B")), Index)); ValidHeader(Previous))
			Sequences[I] = Previous->Sequence;
	Save->Sequence = FMath::Max(Sequences[0], Sequences[1]) + 1;
	Save->CheckpointId = Checkpoint;
	if (!ValidateSave(*Save, Error)) return false;
	// Write the older/inactive slot. A failed write never replaces the newest
	// complete checkpoint; load validates records and can fall back to the other.
	const FString Destination = Slot + (Sequences[0] <= Sequences[1] ? TEXT("_A") : TEXT("_B"));
	if (!UGameplayStatics::SaveGameToSlot(Save.Get(), Destination, Index)) { Error = TEXT("Writing the checkpoint failed; the previous slot is retained."); return false; }
	TStrongObjectPtr<UAZ_CampaignSaveGame> Readback(Cast<UAZ_CampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Destination, Index)));
	if (!Readback.IsValid() || Readback->SnapshotId != Save->SnapshotId || !ValidateSave(*Readback, Error))
	{ if (Error.IsEmpty()) Error = TEXT("Checkpoint readback failed; the previous slot is retained."); return false; }
	LastCheckpointId = Checkpoint;
	LastMessage = TEXT("Checkpoint saved.");
	OnSaveCompleted.Broadcast(true, LastMessage);
	return true;
}

bool UAZ_CampaignSaveCoordinator::RequestCheckpointSave(AAZ_CampaignCheckpoint* Checkpoint, FString& Error)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(Checkpoint) || !Checkpoint->CanSaveForPlayer(PC, Error) || !WriteCheckpoint(Checkpoint->CheckpointId, Error))
	{
		if (Error.IsEmpty()) Error = TEXT("Save point is unavailable.");
		LastMessage = Error; OnSaveCompleted.Broadcast(false, Error); return false;
	}
	bAutoSavePending = false;
	GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer);
	return true;
}

FString UAZ_CampaignSaveCoordinator::ProgressFingerprint() const
{
	FString Result;
	if (QuestProgress)
		for (const auto& Q : QuestProgress->GetQuestRecords())
		{
			Result += FString::Printf(TEXT("%s:%d;"), *Q.QuestId.ToString(), static_cast<int32>(Q.Status));
			for (const auto& O : Q.Objectives) Result += FString::Printf(TEXT("%s:%d:%d;"), *O.ObjectiveId.ToString(), static_cast<int32>(O.Status), O.CurrentCount);
		}
	return Result;
}

void UAZ_CampaignSaveCoordinator::HandleQuestChanged()
{
	if (bLoading || !bAutoSaveImportantQuestChanges) return;
	const FString Current = ProgressFingerprint();
	if (Current == LastProgressFingerprint) return; // tracking/UI refresh is not a new quest moment
	LastProgressFingerprint = Current;
	bAutoSavePending = true;
	if (!GetWorld()->GetTimerManager().IsTimerActive(AutoSaveTimer))
		GetWorld()->GetTimerManager().SetTimer(AutoSaveTimer, this, &ThisClass::TryAutoSave, 1.f, true);
}

void UAZ_CampaignSaveCoordinator::TryAutoSave()
{
	if (!bAutoSavePending || bLoading) return;
	FString Error;
	if (WriteCheckpoint(LastCheckpointId, Error))
	{ bAutoSavePending = false; GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer); }
	else if (LastMessage != Error)
	{
		LastMessage = Error;
		UE_LOG(LogTemp, Log, TEXT("[Campaign] Autosave deferred: %s"), *Error);
		OnSaveCompleted.Broadcast(false, Error);
	}
}

bool UAZ_CampaignSaveCoordinator::LoadCampaign(FString& Error)
{
	if (bLoading) { Error = TEXT("A checkpoint load is already running."); return false; }
	RefreshBindings();
	if (!ValidateContext(Error)) return false;
	FString Slot; int32 Index;
	if (!ResolveSlot(Slot, Index)) { Error = TEXT("Campaign slot is invalid."); return false; }
	TArray<TStrongObjectPtr<UAZ_CampaignSaveGame>> Candidates;
	for (const TCHAR* Suffix : {TEXT("_A"), TEXT("_B")})
		if (auto* Save = Cast<UAZ_CampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot + Suffix, Index)); ValidHeader(Save)) Candidates.Emplace(Save);
	Candidates.Sort([](const auto& A, const auto& B) { return A->Sequence > B->Sequence; });
	for (const auto& Candidate : Candidates) if (ValidateSave(*Candidate, Error)) { PendingLoad = Candidate.Get(); break; }
	if (!PendingLoad) { if (Error.IsEmpty()) Error = TEXT("No compatible checkpoint is available."); return false; }
	auto* PC = CastChecked<APlayerController>(GetOwner());
	APawn* Pawn = PC->GetPawn();
	auto* WorldState = GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>();
	FAZ_CampaignWorldSnapshot CurrentWorld;
	if (!WorldState->Capture(CurrentWorld, Error)) { PendingLoad = nullptr; return false; }
	FVector Destination = PendingLoad->PlayerTransform.GetLocation();
	if (const auto* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
		Destination.Z += Capsule->GetScaledCapsuleHalfHeight() - PendingLoad->CapsuleHalfHeight;
	const FRotator Rotation = PendingLoad->PlayerTransform.Rotator();
	if (!GetWorld()->FindTeleportSpot(Pawn, Destination, Rotation)) { PendingLoad = nullptr; Error = TEXT("Saved player destination is obstructed."); return false; }
	RestoreInventory = PC->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	RestoreEquipment = PC->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	RestoreQuickBar = PC->FindComponentByClass<UAZ_QuickBarComponent>();
	RestoreMover = Pawn->FindComponentByClass<UMoverComponent>();
	RestoreStartTransform = Pawn->GetActorTransform();
	RestoreDestination = Destination;
	RestorePreviousTrackedQuest = QuestProgress->GetTrackedQuestId();
	RestorePreviousTrackedObjective = QuestProgress->GetTrackedObjectiveId();
	bReturningToStart = bRollbackBlocked = false;
	bLoading = true;
	TGuardValue<bool> RestoreCall(bRestoreCallInProgress, true);
	bContextChangedDuringRestore = false;
	if (!QuestProgress->BeginRestoreTransaction())
	{
		bLoading = false; PendingLoad = nullptr;
		RestoreInventory = nullptr; RestoreEquipment = nullptr; RestoreQuickBar = nullptr; RestoreMover = nullptr;
		Error = TEXT("Another quest transaction is active."); return false;
	}
	RestoreQuickBar->BeginCampaignRestore();
	const auto Abort = [&]()
	{
		WorldState->CancelRestore();
		RestoreEquipment->CancelCampaignRestore();
		RestoreInventory->CancelCampaignInventoryRestore();
		RestoreQuickBar->CancelCampaignRestore();
		QuestProgress->EndRestoreTransaction(false);
		bLoading = false; PendingLoad = nullptr;
		RestoreInventory = nullptr; RestoreEquipment = nullptr; RestoreQuickBar = nullptr; RestoreMover = nullptr;
	};
	if (!RestoreInventory->PrepareCampaignInventoryRestore(PendingLoad->Inventory, Error) ||
		!RestoreEquipment->PrepareCampaignRestore(PendingLoad->Equipment, Error) || !WorldState->PrepareRestore(PendingLoad->World, Error))
	{ Abort(); return false; }
	if (bContextChangedDuringRestore || bEndingPlay)
	{ Error = TEXT("Player context changed while staging checkpoint load."); CancelRestoreForContextChange(Error); return false; }
	bAutoSavePending = false; GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer);
	PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); bOwnsInputLock = true;
	RestoreMover->OnTeleportSucceeded.AddUniqueDynamic(this, &ThisClass::HandleTeleportSuccess);
	RestoreMover->OnTeleportFailed.AddUniqueDynamic(this, &ThisClass::HandleTeleportFailure);
	QueueCheckpointTeleport(FTransform(Rotation, Destination));
	return true;
}

void UAZ_CampaignSaveCoordinator::QueueCheckpointTeleport(const FTransform& Transform)
{
	RestoreDestination = Transform.GetLocation();
	TeleportToken = MakeShared<FAZ_CampaignTeleportToken, ESPMode::ThreadSafe>();
	auto Effect = MakeShared<FAZ_CampaignTeleportEffect>();
	Effect->Token = TeleportToken;
	Effect->TargetLocation = RestoreDestination; Effect->TargetRotation = Transform.Rotator(); Effect->bUseActorRotation = false;
	RestoreMover->QueueInstantMovementEffect(Effect);
	GetWorld()->GetTimerManager().SetTimer(LoadTimeout, this, &ThisClass::HandleLoadTimeout, 3.f, false);
}

void UAZ_CampaignSaveCoordinator::HandleLoadTimeout()
{
	if (!bLoading || !TeleportToken || bRollbackBlocked) return;
	if (TeleportToken->CancelBeforeApply())
	{ FinishLoad(false, TEXT("Checkpoint teleport was cancelled before applying.")); return; }
	if (TeleportToken->State.load() == 1)
	{
		// It already owns the movement step. Never unlock and acknowledge a
		// cancellation while that step can still change the pawn later.
		GetWorld()->GetTimerManager().SetTimer(LoadTimeout, this, &ThisClass::HandleLoadTimeout, .1f, false);
		return;
	}
	if (TeleportToken->State.load() == 2 && !TeleportToken->bSucceeded.load())
	{ FinishLoad(false, TEXT("Checkpoint teleport did not apply on this movement backend.")); return; }
	// Synchronous TeleportEffect supplies a completed outcome. If its event was
	// lost, verify the resulting actual location before committing anything.
	const auto* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->GetPawn() && PC->GetPawn()->GetActorLocation().Equals(RestoreDestination, 2.f))
	{ HandleTeleportSuccess(FVector::ZeroVector, FQuat::Identity, RestoreDestination, FQuat::Identity); return; }
	FinishLoad(false, TEXT("Checkpoint movement outcome could not be confirmed."));
}

bool UAZ_CampaignSaveCoordinator::CommitPendingLoad(FString& Error)
{
	TGuardValue<bool> RestoreCall(bRestoreCallInProgress, true);
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PendingLoad || !QuestProgress || !RestoreInventory || !RestoreEquipment || !RestoreQuickBar ||
		!PC->GetPawn() || PC->GetPawn()->FindComponentByClass<UMoverComponent>() != RestoreMover ||
		PC->GetPlayerState<AAZ_PlayerState>() != QuestProgress->GetOwner())
	{ Error = TEXT("Player context changed during checkpoint teleport."); return false; }
	auto* PS = PC->GetPlayerState<AAZ_PlayerState>();
	auto* ASC = PS->GetAbilitySystemComponent();
	auto* WorldState = GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>();
	FAZ_CampaignEquipmentState PreviousSelection;
	PreviousSelection.ActiveItemId = RestoreEquipment->GetActiveItem() ? RestoreEquipment->GetActiveItem()->GetInstanceId() : FGuid();
	PreviousSelection.IntrinsicSlot = RestoreEquipment->GetActiveIntrinsicSlotIndex();
	const auto PreviousQuests = QuestProgress->ExportProgress();
	if (!QuestProgress->RestoreProgress(PendingLoad->Quests, Error, false)) return false;
	RestoreInventory->CommitCampaignInventoryRestore();
	if (!RestoreEquipment->CommitCampaignRestore(Error))
	{
		RestoreInventory->CancelCampaignInventoryRestore();
		FString Ignored; QuestProgress->RestoreProgress(PreviousQuests, Ignored, false);
		RestoreEquipment->RecoverCampaignSelection(PreviousSelection);
		return false;
	}
	WorldState->CommitRestore();
	RestoreQuickBar->RestoreCampaignState(PendingLoad->QuickBar);
	PS->SetXP(PendingLoad->XP); PS->SetLevel(PendingLoad->Level);
	PS->SetAttributePoints(PendingLoad->AttributePoints); PS->SetSpellPoints(PendingLoad->SpellPoints);
	// Maxima before their clamped current values.
	for (bool Maxima : {true, false})
		for (const auto& A : PendingLoad->Attributes)
			if (A.AttributeName.ToString().StartsWith(TEXT("Max")) == Maxima)
				ASC->SetNumericAttributeBase(FGameplayAttribute(ResolveAttribute(ASC, A)), A.BaseValue);
	if (auto* State = GetWorld()->GetGameState<AAZ_GameStateBase>()) State->SetDifficultyLevel(PendingLoad->Difficulty);
	if (auto* Map = UAZ_QuestMapComponent::GetOrCreateForController(PC)) Map->RestoreWaypoint(PendingLoad->Waypoint, false);
	bDataCommitted = true;
	auto Stop = MakeShared<FApplyVelocityEffect>(); Stop->VelocityToApply = FVector::ZeroVector; Stop->bAdditiveVelocity = false;
	RestoreMover->QueueInstantMovementEffect(Stop);
	return true;
}

void UAZ_CampaignSaveCoordinator::HandleTeleportSuccess(const FVector&, const FQuat&, const FVector& To, const FQuat&)
{
	if (!To.Equals(RestoreDestination, 1.f) || !TeleportToken || TeleportToken->State.load() != 2) return;
	FinishLoad(!bReturningToStart, bReturningToStart ? RollbackMessage : TEXT("Checkpoint loaded."));
}
void UAZ_CampaignSaveCoordinator::HandleTeleportFailure(const FVector&, const FQuat&, const FVector& To, const FQuat&, ETeleportFailureReason)
{ if (To.Equals(RestoreDestination, 1.f)) FinishLoad(false, TEXT("Checkpoint teleport failed; staged load was cancelled.")); }

void UAZ_CampaignSaveCoordinator::FinishLoad(bool bSuccess, const FString& Message)
{
	if (!bLoading) return;
	FString ResultMessage = Message;
	if (!bSuccess && !bReturningToStart && TeleportToken && TeleportToken->State.load() < 2 && !TeleportToken->CancelBeforeApply())
	{
		GetWorld()->GetTimerManager().SetTimer(LoadTimeout, this, &ThisClass::HandleLoadTimeout, .1f, false);
		return;
	}
	if (bSuccess && !CommitPendingLoad(ResultMessage)) bSuccess = false;
	if (bContextChangedDuringRestore || bEndingPlay)
	{ CancelRestoreForContextChange(TEXT("Player context changed during checkpoint commit.")); return; }
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!bSuccess && !bDataCommitted)
	{
		GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>()->CancelRestore();
		if (RestoreEquipment) RestoreEquipment->CancelCampaignRestore();
		if (RestoreInventory) RestoreInventory->CancelCampaignInventoryRestore();
		if (RestoreQuickBar) RestoreQuickBar->CancelCampaignRestore();
		const bool bAtStart = PC && PC->GetPawn() && PC->GetPawn()->FindComponentByClass<UMoverComponent>() == RestoreMover &&
			PC->GetPawn()->GetActorLocation().Equals(RestoreStartTransform.GetLocation(), 2.f);
		if (!bAtStart)
		{
			if (!bReturningToStart && RestoreMover && PC && PC->GetPawn() && PC->GetPawn()->FindComponentByClass<UMoverComponent>() == RestoreMover)
			{
				bReturningToStart = true; RollbackMessage = ResultMessage;
				QueueCheckpointTeleport(RestoreStartTransform);
				return; // quest/input guard remains owned until return confirmation
			}
			bRollbackBlocked = true;
			LastMessage = TEXT("Checkpoint rollback could not return the player. Progress/input remain locked; reload the level to recover.");
			GetWorld()->GetTimerManager().ClearTimer(LoadTimeout);
			OnLoadCompleted.Broadcast(false, LastMessage);
			return;
		}
	}
	GetWorld()->GetTimerManager().ClearTimer(LoadTimeout);
	if (RestoreMover)
	{
		RestoreMover->OnTeleportSucceeded.RemoveDynamic(this, &ThisClass::HandleTeleportSuccess);
		RestoreMover->OnTeleportFailed.RemoveDynamic(this, &ThisClass::HandleTeleportFailure);
	}
	if (bDataCommitted)
	{
		// All domains now agree. Release inventory/equipment/UI notifications only
		// after Mover updated the pawn so area objectives do not see its old location.
		if (QuestProgress) QuestProgress->EndRestoreTransaction(false);
		if (RestoreInventory) RestoreInventory->PublishCampaignInventoryRestore();
		if (RestoreEquipment) RestoreEquipment->PublishCampaignRestore();
		if (RestoreQuickBar) RestoreQuickBar->PublishCampaignRestore();
		if (QuestProgress && PendingLoad)
		{
			QuestProgress->SetTrackedObjective(PendingLoad->TrackedQuestId, PendingLoad->TrackedObjectiveId);
			QuestProgress->PublishProgressChanged();
		}
		if (PC && PendingLoad) PC->SetControlRotation(PendingLoad->ControlRotation);
		if (auto* Map = UAZ_QuestMapComponent::FindForController(PC)) { Map->RefreshNavigation(); Map->OnViewChanged.Broadcast(); }
		LastCheckpointId = PendingLoad ? PendingLoad->CheckpointId : NAME_None;
	}
	else
	{
		GetWorld()->GetSubsystem<UAZ_CampaignWorldSubsystem>()->CancelRestore();
		if (RestoreEquipment) RestoreEquipment->CancelCampaignRestore();
		if (RestoreInventory) RestoreInventory->CancelCampaignInventoryRestore();
		if (RestoreQuickBar) RestoreQuickBar->CancelCampaignRestore();
		if (QuestProgress) QuestProgress->EndRestoreTransaction(false);
		if (QuestProgress) QuestProgress->SetTrackedObjective(RestorePreviousTrackedQuest, RestorePreviousTrackedObjective);
	}
	if (bOwnsInputLock && PC) { PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); }
	bOwnsInputLock = bDataCommitted = bLoading = false;
	bReturningToStart = bRollbackBlocked = false; TeleportToken.Reset();
	LastProgressFingerprint = ProgressFingerprint();
	PendingLoad = nullptr; RestoreInventory = nullptr; RestoreEquipment = nullptr; RestoreQuickBar = nullptr; RestoreMover = nullptr;
	LastMessage = ResultMessage;
	OnLoadCompleted.Broadcast(bSuccess, ResultMessage);
}
