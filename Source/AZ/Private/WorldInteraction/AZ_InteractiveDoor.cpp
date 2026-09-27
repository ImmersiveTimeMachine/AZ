#include "WorldInteraction/AZ_InteractiveDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Player/AZ_PlayerController.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "NativeGameplayTags.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ShowcaseDoorKey, "Item.Type.Craftable.Key.ShowcaseDoor");

AAZ_InteractiveDoor::AAZ_InteractiveDoor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(RootComponent);
	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(RootComponent);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetCanEverAffectNavigation(false);
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(RootComponent);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);
	InteractionVolume->SetCanEverAffectNavigation(false);
	NavigationBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationBlocker"));
	NavigationBlocker->SetupAttachment(RootComponent);
	NavigationBlocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NavigationBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavigationBlocker->SetGenerateOverlapEvents(false);
	NavigationBlocker->bDynamicObstacle = true;
	NavigationBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());
	DisplayName = NSLOCTEXT("CHALK", "DoorName", "Door");
}

void AAZ_InteractiveDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshDoorPreview();
}

void AAZ_InteractiveDoor::RefreshDoorPreview()
{
	if (HasActorBegunPlay()) return;
	InteractionVolume->SetSphereRadius(FMath::Max(50.f, InteractionRadius));
	DoorPivot->SetRelativeTransform(PivotAt(0.f));
	if (const UStaticMesh* Mesh = DoorMesh->GetStaticMesh())
	{
		FBox Bounds = Mesh->GetBoundingBox().TransformBy(DoorMesh->GetRelativeTransform() * DoorPivot->GetRelativeTransform());
		if (!bPassageDoor && FrameMesh->GetStaticMesh()) Bounds = FrameMesh->GetStaticMesh()->GetBoundingBox().TransformBy(FrameMesh->GetRelativeTransform());
		NavigationBlocker->SetRelativeLocation(Bounds.GetCenter());
		NavigationBlocker->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(5.f)));
		InteractionVolume->SetRelativeLocation(Bounds.GetCenter());
	}
	Motion.bLocked = bInitiallyLocked;
	Motion.bMoving = false;
	Motion.From = Motion.To = bInitiallyOpen && !bInitiallyLocked ? 1.f : 0.f;
	ApplyFraction(Motion.To);
	UpdateNavigation();
}

void AAZ_InteractiveDoor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) RestoreDoorState(bInitiallyOpen && !bInitiallyLocked ? 1.f : 0.f, bInitiallyLocked);
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshContentAccess);
}

double AAZ_InteractiveDoor::ServerTime() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AAZ_InteractiveDoor::ApplyFraction(float Fraction)
{
	AppliedFraction = FMath::Clamp(Fraction, 0.f, 1.f);
	DoorPivot->SetRelativeTransform(PivotAt(AppliedFraction));
}

void AAZ_InteractiveDoor::UpdateNavigation()
{
	// Keep the closed footprint blocked until the leaf is fully open. No per-frame nav rebuilds.
	NavigationBlocker->SetCanEverAffectNavigation(bPassageDoor ? (Motion.bMoving || AppliedFraction < 0.99f) : FrameMesh->GetStaticMesh() != nullptr);
}

bool AAZ_InteractiveDoor::HasKey(const AAZ_PlayerController* Player) const
{
	if (!RequiredKeyType.IsValid() || !Player) return false;
	const auto* Inventory = Player->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (!Inventory) return false;
	for (const auto* Item : Inventory->GetItems())
		if (IsValid(Item) && Item->GetLocation() == EAZ_InventoryItemLocation::Backpack && !Item->GetParentItemId().IsValid()
			&& Item->GetTotalStackCount() > 0 && Item->GetItemManifest().GetItemTypeTag().MatchesTagExact(RequiredKeyType)) return true;
	return false;
}

bool AAZ_InteractiveDoor::CanFocus(const AAZ_PlayerController* Player) const
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Pawn || !DoorMesh->GetStaticMesh() || !FMath::IsFinite(InteractionRadius)) return false;
	const FVector Point = GetInteractionPoint();
	if (FVector::DistSquared(Pawn->GetActorLocation(), Point) > FMath::Square(InteractionRadius)) return false;
	const FVector Eye = Pawn->GetPawnViewLocation();
	if (FVector::DotProduct(Player->GetControlRotation().Vector(), (Point - Eye).GetSafeNormal()) < 0.15f) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorInteraction), true, Pawn);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Query) || Hit.GetActor() == this || AssemblyActors.Contains(Hit.GetActor());
}

FText AAZ_InteractiveDoor::GetInteractionCaption(const AAZ_PlayerController* Player) const
{
	FText Action;
	if (Motion.bMoving) Action = NSLOCTEXT("CHALK", "DoorMoving", "Moving...");
	else if (Motion.bLocked) Action = HasKey(Player) ? NSLOCTEXT("CHALK", "DoorUnlock", "Unlock and open") : NSLOCTEXT("CHALK", "DoorLocked", "Locked - key required");
	else if (Motion.bBlocked) Action = NSLOCTEXT("CHALK", "DoorBlocked", "Blocked - clear the space and retry");
	else Action = AppliedFraction > 0.01f ? NSLOCTEXT("CHALK", "DoorClose", "Close") : NSLOCTEXT("CHALK", "DoorOpen", "Open");
	return FText::Format(NSLOCTEXT("CHALK", "DoorCaption", "{0}: {1}"), DisplayName, Action);
}

bool AAZ_InteractiveDoor::IsSweepClear(float From, float To, float& SafeFraction) const
{
	SafeFraction = From;
	const UStaticMesh* Mesh = DoorMesh->GetStaticMesh();
	if (!Mesh) return false;
	const FBox Bounds = Mesh->GetBoundingBox();
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(To - From) * FMath::Max(FMath::Abs(OpenAngle) / 2.f, OpenTranslation.Size() / 2.f)));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorClearance), false, this);
	for (const auto& Assembly : AssemblyActors) if (IsValid(Assembly)) Query.AddIgnoredActor(Assembly);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (const auto* Item = It->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>(); Item && ContentPickupIds.Contains(Item->CampaignPickupId)) Query.AddIgnoredActor(*It);
	for (int32 Index = 1; Index <= Steps; ++Index)
	{
		const float Fraction = FMath::Lerp(From, To, float(Index) / Steps);
		const FTransform Pivot = PivotAt(Fraction);
		const FTransform Leaf = DoorMesh->GetRelativeTransform() * Pivot * GetActorTransform();
		const FVector Extent = (Bounds.GetExtent() * Leaf.GetScale3D().GetAbs() - FVector(2.f)).ComponentMax(FVector(1.f));
		if (GetWorld()->OverlapBlockingTestByChannel(Leaf.TransformPosition(Bounds.GetCenter()), Leaf.GetRotation(),
			ECC_Pawn, FCollisionShape::MakeBox(Extent), Query)) return false;
		SafeFraction = Fraction;
	}
	return true;
}

bool AAZ_InteractiveDoor::TryInteractForPlayer(AAZ_PlayerController* Player, FString& Error)
{
	if (!HasAuthority() || !CanFocus(Player) || Motion.bMoving) return false;
	if (!FMath::IsFinite(OpenAngle) || (FMath::Abs(OpenAngle) < 1.f && OpenTranslation.IsNearlyZero()) || FMath::Abs(OpenAngle) > 170.f
		|| RotationAxis.ContainsNaN() || RotationAxis.IsNearlyZero() || OpenTranslation.ContainsNaN() || ClosedPivotLocation.ContainsNaN()
		|| !FMath::IsFinite(OpenDuration) || OpenDuration < 0.1f) { Error = TEXT("Invalid door motion settings."); return false; }
	if (Motion.bLocked && !HasKey(Player)) return false;
	const float Target = AppliedFraction > 0.01f ? 0.f : 1.f;
	float Safe;
	if (!IsSweepClear(AppliedFraction, Target, Safe))
	{
		Motion.bBlocked = true;
		ForceNetUpdate();
		return false;
	}
	const auto CommitMotion = [this, Target]()
	{
		Motion.bLocked = false;
		Motion.bBlocked = false;
		Motion.From = AppliedFraction;
		Motion.To = Target;
		Motion.StartedAt = ServerTime();
		Motion.Duration = FMath::Max(0.1f, OpenDuration * FMath::Abs(Target - AppliedFraction));
		Motion.bMoving = true;
		return true;
	};
	if (Motion.bLocked)
	{
		auto* Inventory = Player->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
		// The existing inventory transaction commits lock state before publishing removal.
		// Clearance was checked above; a later obstruction never re-locks the door.
		if (!Inventory || !Inventory->TryConsumeForQuest(RequiredKeyType, 1, CommitMotion, Error)) return false;
	}
	else CommitMotion();
	RefreshContentAccess();
	UpdateNavigation();
	SetActorTickEnabled(true);
	ForceNetUpdate();
	PlayDoorSound(Target > Motion.From);
	if (NoiseLoudness > 0.f && NoiseRange > 0.f)
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), DoorMesh->Bounds.Origin, NoiseLoudness, Player->GetPawn(), NoiseRange, FName("Door"));
	UE_LOG(LogTemp, Log, TEXT("Door %s: moving %.2f -> %.2f"), *GetName(), Motion.From, Motion.To);
	return true;
}

void AAZ_InteractiveDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Motion.bMoving) { SetActorTickEnabled(false); return; }
	const float Alpha = FMath::Clamp(float((ServerTime() - Motion.StartedAt) / FMath::Max(0.1f, Motion.Duration)), 0.f, 1.f);
	const float Next = FMath::Lerp(Motion.From, Motion.To, Alpha * Alpha * (3.f - 2.f * Alpha));
	float Safe = Next;
	const bool bBlocked = HasAuthority() && !IsSweepClear(AppliedFraction, Next, Safe);
	ApplyFraction(bBlocked ? Safe : Next);
	if (HasAuthority() && (bBlocked || Alpha >= 1.f))
	{
		Motion.From = Motion.To = AppliedFraction;
		Motion.bMoving = false;
		Motion.bBlocked = bBlocked;
		RefreshContentAccess();
		UpdateNavigation();
		ForceNetUpdate();
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("Door %s: stopped at %.2f blocked=%d"), *GetName(), AppliedFraction, bBlocked);
	}
}

void AAZ_InteractiveDoor::OnRep_Motion()
{
	if (!Motion.bMoving) ApplyFraction(Motion.To);
	SetActorTickEnabled(Motion.bMoving);
	RefreshContentAccess();
	UpdateNavigation();
}

void AAZ_InteractiveDoor::RestoreDoorState(float Fraction, bool bLocked)
{
	if (!HasAuthority()) return;
	Motion = FAZ_DoorMotion();
	Motion.bLocked = bLocked;
	Motion.From = Motion.To = bLocked ? 0.f : FMath::Clamp(Fraction, 0.f, 1.f);
	OnRep_Motion();
	ForceNetUpdate();
}

void AAZ_InteractiveDoor::PlayDoorSound_Implementation(bool bOpening)
{
	if (USoundBase* Sound = bOpening ? OpenSound.Get() : CloseSound.Get())
		UGameplayStatics::PlaySoundAtLocation(this, Sound, DoorMesh->Bounds.Origin);
}

bool AAZ_InteractiveDoor::IsAvailableForInteraction_Implementation(UPrimitiveComponent*) const { return !Motion.bMoving; }

void AAZ_InteractiveDoor::PostInteract_Implementation(AActor* Actor, UPrimitiveComponent*)
{
	if (const APawn* Pawn = Cast<APawn>(Actor))
		if (auto* PC = Cast<AAZ_PlayerController>(Pawn->GetController())) PC->RequestImmediateWorldInteraction(this);
}

void AAZ_InteractiveDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAZ_InteractiveDoor, Motion);
}

FTransform AAZ_InteractiveDoor::PivotAt(float Fraction) const
{
	return FTransform(FQuat(RotationAxis.GetSafeNormal(), FMath::DegreesToRadians(OpenAngle * Fraction)),
		ClosedPivotLocation + OpenTranslation * Fraction);
}


void AAZ_InteractiveDoor::RefreshContentAccess()
{
	if (!GetWorld() || ContentPickupIds.IsEmpty()) return;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const auto* Item = It->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
		if (!Item || !ContentPickupIds.Contains(Item->CampaignPickupId)) continue;
		// Rendering is independent from permission to pick up. The closed mesh
		// occludes real contents; IsAccessibleForPickup still enforces the lock.
		It->SetActorHiddenInGame(false);
		It->SetActorEnableCollision(true);
		const auto* Slot = Contents.FindByPredicate([Item](const auto& Entry) { return Entry.PickupId == Item->CampaignPickupId; });
		if (Slot)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
			for (UPrimitiveComponent* Primitive : Primitives)
			{
				Primitive->SetSimulatePhysics(false);
				Primitive->SetMobility(EComponentMobility::Movable);
				Primitive->SetCanEverAffectNavigation(false);
			}
			USceneComponent* Parent = Slot->bMovesWithPanel ? DoorPivot.Get() : RootComponent.Get();
			It->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
			It->SetActorRelativeTransform(Slot->LocalTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void AAZ_InteractiveDoor::ApplyContentSetup()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("Content setup is editor-only; runtime opening never refills loot."));
		return;
	}
	for (TActorIterator<AAZ_InteractiveDoor> It(GetWorld()); It; ++It)
	{
		if (*It == this) continue;
		if (DoorId.IsValid() && It->DoorId == DoorId)
		{ UE_LOG(LogTemp, Error, TEXT("Container setup refused: duplicated container identity.")); return; }
		for (const FGuid& Id : ContentPickupIds)
			if (It->ContentPickupIds.Contains(Id))
			{ UE_LOG(LogTemp, Error, TEXT("Container setup refused: contents are shared with another container.")); return; }
	}
	// Stage every row before replacing anything. Invalid class/quantity leaves
	// the previously authored pickups intact.
	TArray<AActor*> Staged;
	TArray<FAZ_ContainerContentSlot> Proposed = Contents;
	TSet<FGuid> UniqueIds;
	const auto Abort = [&Staged](const TCHAR* Reason)
	{
		for (AActor* Actor : Staged) if (IsValid(Actor)) Actor->Destroy();
		UE_LOG(LogTemp, Error, TEXT("Container setup refused: %s"), Reason);
	};
	for (auto& Slot : Proposed)
	{
		if (!Slot.PickupClass || Slot.PickupClass->HasAnyClassFlags(CLASS_Abstract) || Slot.Quantity < 1 ||
			Slot.LocalTransform.ContainsNaN() || !Slot.LocalTransform.IsValid() || Slot.LocalTransform.GetScale3D().GetMin() <= 0.f)
		{ Abort(TEXT("Invalid pickup class, quantity or transform.")); return; }
		if (!Slot.PickupId.IsValid()) Slot.PickupId = FGuid::NewGuid();
		if (UniqueIds.Contains(Slot.PickupId)) { Abort(TEXT("Repeated pickup identity.")); return; }
		UniqueIds.Add(Slot.PickupId);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (const auto* Other = It->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>(); Other &&
				Other->CampaignPickupId == Slot.PickupId && !ContentPickupIds.Contains(Slot.PickupId))
			{ Abort(TEXT("Pickup identity belongs to another object.")); return; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transactional;
		AActor* Actor = GetWorld()->SpawnActor<AActor>(Slot.PickupClass, GetActorTransform(), Params);
		if (!Actor) { Abort(TEXT("Could not create pickup.")); return; }
		Staged.Add(Actor);
		auto* Item = Actor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
		if (!Item) { Abort(TEXT("Selected class has no inventory pickup component.")); return; }
		const auto& Manifest = Item->GetItemManifest();
		const auto* Stack = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
		if (!Manifest.GetItemTypeTag().IsValid() || (Stack ? Slot.Quantity > Stack->GetMaxStackSize() : Slot.Quantity != 1))
		{ Abort(TEXT("Quantity exceeds this pickup's stack capacity, or its item type is missing.")); return; }
		Item->CampaignPickupId = Slot.PickupId;
		Item->SetRemainingStackCount(Slot.Quantity);
		Actor->SetActorLabel(FString::Printf(TEXT("%s - Content %d"), *GetActorLabel(), Staged.Num()));
		Actor->SetFolderPath(FName(*(GetFolderPath().ToString() + TEXT("/Contents"))));
		Actor->Tags.AddUnique(TEXT("AZ_AuthoredContainerContent"));
	}
	TArray<AActor*> Previous;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (!Staged.Contains(*It))
			if (const auto* Item = It->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>(); Item && ContentPickupIds.Contains(Item->CampaignPickupId)) Previous.Add(*It);
	Modify();
	Contents = MoveTemp(Proposed);
	ContentPickupIds.Reset();
	for (const auto& Slot : Contents) ContentPickupIds.Add(Slot.PickupId);
	for (AActor* Actor : Previous) { Actor->Modify(); Actor->Destroy(); }
	RefreshContentAccess();
	MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("Container %s: authored %d content slots."), *GetName(), Contents.Num());
#endif
}

FVector AAZ_InteractiveDoor::GetInteractionPoint() const
{
	return bUseCustomInteractionPoint ? DoorMesh->GetComponentTransform().TransformPosition(InteractionPoint) : DoorMesh->Bounds.Origin;
}
