#include "UI/AZ_QuestMapComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Navigation/AZ_MapDefinition.h"
#include "Navigation/AZ_NavigationLibrary.h"
#include "Navigation/AZ_NavigationTargetComponent.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"
#include "Quests/AZ_QuestDefinition.h"
#include "Quests/AZ_QuestProgressComponent.h"

UAZ_QuestMapComponent::UAZ_QuestMapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

bool UAZ_QuestMapComponent::IsLocalOwner() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return IsValid(Controller) && Controller->IsLocalController();
}

UAZ_QuestMapComponent* UAZ_QuestMapComponent::FindForController(APlayerController* Controller)
{
	return IsValid(Controller) ? Controller->FindComponentByClass<UAZ_QuestMapComponent>() : nullptr;
}

UAZ_QuestMapComponent* UAZ_QuestMapComponent::GetOrCreateForController(APlayerController* Controller)
{
	if (!IsValid(Controller) || !Controller->IsLocalController()) return nullptr;
	if (UAZ_QuestMapComponent* Existing = FindForController(Controller)) return Existing;
	UAZ_QuestMapComponent* Created = NewObject<UAZ_QuestMapComponent>(Controller, NAME_None, RF_Transient);
	Controller->AddInstanceComponent(Created);
	Created->RegisterComponent();
	return Created;
}

void UAZ_QuestMapComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(IsLocalOwner());
	RefreshBindings();
}

void UAZ_QuestMapComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(QuestProgress)) QuestProgress->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::HandleQuestChanged);
	if (IsValid(TargetRegistry)) TargetRegistry->OnTargetsChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
	RetireAnchor(QuestAnchor, bQuestAnchorRegistered);
	RetireAnchor(WaypointAnchor, bWaypointAnchorRegistered);
	if (IsValid(QuestAnchor)) QuestAnchor->DestroyComponent();
	if (IsValid(WaypointAnchor)) WaypointAnchor->DestroyComponent();
	QuestAnchor = nullptr;
	WaypointAnchor = nullptr;
	QuestProgress = nullptr;
	TargetRegistry = nullptr;
	Markers.Reset();
	Super::EndPlay(Reason);
}

void UAZ_QuestMapComponent::RefreshBindings()
{
	if (!IsLocalOwner()) return;
	APlayerController* Controller = CastChecked<APlayerController>(GetOwner());
	APlayerState* Player = Controller->PlayerState;
	UAZ_QuestProgressComponent* Current = IsValid(Player) ? Player->FindComponentByClass<UAZ_QuestProgressComponent>() : nullptr;
	UAZ_NavigationTargetSubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>() : nullptr;
	const bool bChanged = Current != QuestProgress || Registry != TargetRegistry;
	if (Current != QuestProgress)
	{
		if (IsValid(QuestProgress)) QuestProgress->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::HandleQuestChanged);
		QuestProgress = Current;
		if (IsValid(QuestProgress)) QuestProgress->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestChanged);
	}
	if (Registry != TargetRegistry)
	{
		if (IsValid(TargetRegistry)) TargetRegistry->OnTargetsChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
		TargetRegistry = Registry;
		if (IsValid(TargetRegistry)) TargetRegistry->OnTargetsChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
	}
	if (bChanged) { RefreshNavigation(); OnViewChanged.Broadcast(); }
}

void UAZ_QuestMapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsLocalOwner()) return;
	// Only this controller's pointers and already-known quest targets. No world scan.
	RefreshBindings();
	RefreshNavigation();
}

void UAZ_QuestMapComponent::HandleQuestChanged()
{
	RefreshNavigation();
	OnViewChanged.Broadcast();
}

void UAZ_QuestMapComponent::HandleTargetChanged(FName TargetId)
{
	RefreshNavigation();
	OnViewChanged.Broadcast();
}

USceneComponent* UAZ_QuestMapComponent::EnsureAnchor(TObjectPtr<USceneComponent>& Anchor, FName Name)
{
	if (!IsValid(Anchor) && IsLocalOwner())
	{
		Anchor = NewObject<USceneComponent>(GetOwner(), Name, RF_Transient);
		Anchor->SetMobility(EComponentMobility::Movable);
		GetOwner()->AddInstanceComponent(Anchor);
		Anchor->RegisterComponent();
	}
	return Anchor;
}

void UAZ_QuestMapComponent::RetireAnchor(USceneComponent* Anchor, bool& bWasPublished)
{
	if (!bWasPublished) return;
	bWasPublished = false;
	// Removal keeps exact identity, including during teardown. The owned bridge
	// already supports removing keys whose world target is no longer valid.
	OnMarkerRemove.Broadcast(Anchor);
}

void UAZ_QuestMapComponent::RefreshNavigation()
{
	if (!IsLocalOwner()) return;
	Markers.Reset();
	const FName TrackedQuest = IsValid(QuestProgress) ? QuestProgress->GetTrackedQuestId() : NAME_None;
	const FName TrackedObjective = IsValid(QuestProgress) ? QuestProgress->GetTrackedObjectiveId() : NAME_None;
	const FAZ_QuestMapMarkerView* TrackedView = nullptr;
	if (IsValid(QuestProgress))
	{
		for (const FAZ_QuestProgressRecord& Record : QuestProgress->GetQuestRecords())
		{
			if (Record.Status != EAZ_QuestStatus::Active) continue;
			UAZ_QuestDefinition* Definition = QuestProgress->FindQuestDefinition(Record.QuestId);
			if (!IsValid(Definition)) continue;
			for (const FAZ_QuestObjectiveProgress& Progress : Record.Objectives)
			{
				if (Progress.Status != EAZ_QuestObjectiveStatus::Active) continue;
				const FAZ_QuestObjectiveDefinition* Objective = Definition->FindObjective(Progress.ObjectiveId);
				if (!Objective || !Objective->Target.IsWellFormed()) continue;
				FAZ_QuestMapMarkerView View;
				View.QuestId = Record.QuestId;
				View.ObjectiveId = Progress.ObjectiveId;
				View.QuestCategory = Definition->Category;
				View.Label = Objective->Description;
				View.Target = Objective->Target;
				View.bOptional = Objective->bOptional;
				View.bTracked = Record.QuestId == TrackedQuest && Progress.ObjectiveId == TrackedObjective;
				UAZ_NavigationTargetComponent* Provider = nullptr;
				if (IsValid(TargetRegistry))
				{
					const EAZ_NavigationTargetResolveResult Result = TargetRegistry->ResolveTarget(View.Target, View.WorldLocation, Provider);
					View.bResolved = Result == EAZ_NavigationTargetResolveResult::ResolvedProvider || Result == EAZ_NavigationTargetResolveResult::ResolvedLocation;
					if (Result == EAZ_NavigationTargetResolveResult::ResolvedProvider && Provider)
					{
						// An ID-only descriptor may leave context open; the unique provider supplies its actual map/layer.
						View.Target.MapId = Provider->MapId;
						View.Target.LayerId = Provider->LayerId;
					}
				}
				Markers.Add(MoveTemp(View));
			}
		}
	}
	if (Waypoint.bActive && Waypoint.IsWellFormed())
	{
		FAZ_QuestMapMarkerView Personal;
		Personal.Label = NSLOCTEXT("CHALK", "PersonalWaypoint", "Personal marker");
		Personal.Target.MapId = Waypoint.MapId;
		Personal.Target.LayerId = Waypoint.LayerId;
		Personal.Target.bHasWorldLocation = true;
		Personal.Target.WorldLocation = Waypoint.WorldLocation;
		Personal.WorldLocation = Waypoint.WorldLocation;
		Personal.bPersonal = true;
		Personal.bResolved = true;
		Markers.Add(MoveTemp(Personal));
	}
	// Take the address only after the complete array is built (Add can reallocate).
	TrackedView = Markers.FindByPredicate([](const FAZ_QuestMapMarkerView& View) { return View.bTracked && View.bResolved; });
	const auto InActiveMap = [this](FName MapId, FName LayerId)
	{
		return !IsValid(MapDefinition) || (MapDefinition->MapId == MapId && MapDefinition->LayerId == LayerId);
	};
	if (TrackedView && InActiveMap(TrackedView->Target.MapId, TrackedView->Target.LayerId))
	{
		if (USceneComponent* Anchor = EnsureAnchor(QuestAnchor, TEXT("QuestNavigationAnchor")))
		{
			Anchor->SetWorldLocation(TrackedView->WorldLocation);
			if (!bQuestAnchorRegistered || PublishedQuestId != TrackedQuest || PublishedObjectiveId != TrackedObjective || !QuestAnchorLabel.EqualTo(TrackedView->Label))
			{
				bQuestAnchorRegistered = true;
				PublishedQuestId = TrackedQuest;
				PublishedObjectiveId = TrackedObjective;
				QuestAnchorLabel = TrackedView->Label;
				OnMarkerUpsert.Broadcast(Anchor, QuestAnchorLabel, false);
			}
		}
	}
	else RetireAnchor(QuestAnchor, bQuestAnchorRegistered);
	if (Waypoint.bActive && Waypoint.IsWellFormed() && InActiveMap(Waypoint.MapId, Waypoint.LayerId))
	{
		if (USceneComponent* Anchor = EnsureAnchor(WaypointAnchor, TEXT("PersonalNavigationAnchor")))
		{
			Anchor->SetWorldLocation(Waypoint.WorldLocation);
			if (!bWaypointAnchorRegistered)
			{
				bWaypointAnchorRegistered = true;
				OnMarkerUpsert.Broadcast(Anchor, NSLOCTEXT("CHALK", "PersonalWaypoint", "Personal marker"), true);
			}
		}
	}
	else RetireAnchor(WaypointAnchor, bWaypointAnchorRegistered);
}

void UAZ_QuestMapComponent::ReplayNavigationMarkers()
{
	RefreshBindings();
	RefreshNavigation();
	if (bQuestAnchorRegistered && IsValid(QuestAnchor)) OnMarkerUpsert.Broadcast(QuestAnchor, QuestAnchorLabel, false);
	if (bWaypointAnchorRegistered && IsValid(WaypointAnchor)) OnMarkerUpsert.Broadcast(WaypointAnchor, NSLOCTEXT("CHALK", "PersonalWaypoint", "Personal marker"), true);
}

bool UAZ_QuestMapComponent::SetMapDefinition(UAZ_MapDefinition* Definition)
{
	FText Error;
	if (!IsLocalOwner() || !IsValid(Definition) || !Definition->ValidateDefinition(Error)) return false;
	if (MapDefinition == Definition) return true;
	MapDefinition = Definition;
	RefreshNavigation();
	OnViewChanged.Broadcast();
	return true;
}

bool UAZ_QuestMapComponent::SetWaypoint(const FAZ_MapWaypoint& Value)
{
	if (!IsLocalOwner() || (Value.bActive && !Value.IsWellFormed())) return false;
	if (Value.bActive)
	{
		FVector2D UV;
		if (!IsValid(MapDefinition) || Value.MapId != MapDefinition->MapId || Value.LayerId != MapDefinition->LayerId ||
			!MapDefinition->WorldToMapNormalized(Value.WorldLocation, UV) || !MapDefinition->ContainsMapNormalized(UV)) return false;
	}
	Waypoint = Value.bActive ? Value : FAZ_MapWaypoint();
	RefreshNavigation();
	OnViewChanged.Broadcast();
	return true;
}

void UAZ_QuestMapComponent::ClearWaypoint()
{
	if (!IsLocalOwner()) return;
	Waypoint = FAZ_MapWaypoint();
	RefreshNavigation();
	OnViewChanged.Broadcast();
}

void UAZ_QuestMapComponent::RestoreWaypoint(const FAZ_MapWaypoint& Value, bool bPublish)
{
	if (!IsLocalOwner() || (Value.bActive && !Value.IsWellFormed())) return;
	Waypoint = Value.bActive ? Value : FAZ_MapWaypoint();
	if (bPublish) { RefreshNavigation(); OnViewChanged.Broadcast(); }
}
