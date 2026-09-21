#include "Navigation/AZ_NavigationTargetComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"

UAZ_NavigationTargetComponent::UAZ_NavigationTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAZ_NavigationTargetComponent::GetNavigationLocation(FVector& OutWorldLocation) const
{
	OutWorldLocation = FVector::ZeroVector;
	if (LocalOffset.ContainsNaN() || GetComponentTransform().ContainsNaN())
	{
		return false;
	}
	const FVector Result = GetComponentTransform().TransformPosition(LocalOffset);
	if (Result.ContainsNaN())
	{
		return false;
	}
	OutWorldLocation = Result;
	return true;
}

FAZ_NavigationTargetDescriptor UAZ_NavigationTargetComponent::GetTargetDescriptor() const
{
	FAZ_NavigationTargetDescriptor Result;
	Result.TargetId = TargetId;
	Result.MapId = MapId;
	Result.LayerId = LayerId;
	Result.bHasWorldLocation = GetNavigationLocation(Result.WorldLocation);
	return Result;
}

void UAZ_NavigationTargetComponent::SetTargetEnabled(bool bEnabled)
{
	if (bTargetEnabled != bEnabled)
	{
		bTargetEnabled = bEnabled;
		RefreshRegistration();
	}
}

void UAZ_NavigationTargetComponent::RefreshRegistration()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || !HasBegunPlay() || !bTargetEnabled)
	{
		UnregisterTarget();
		return;
	}
	UAZ_NavigationTargetSubsystem* Subsystem = World->GetSubsystem<UAZ_NavigationTargetSubsystem>();
	if (RegisteredSubsystem.Get() != Subsystem)
	{
		UnregisterTarget();
		RegisteredSubsystem = Subsystem;
	}
	if (Subsystem)
	{
		Subsystem->RegisterTarget(this);
	}
}

void UAZ_NavigationTargetComponent::UnregisterTarget()
{
	if (UAZ_NavigationTargetSubsystem* Subsystem = RegisteredSubsystem.Get())
	{
		Subsystem->UnregisterTarget(this);
	}
	RegisteredSubsystem.Reset();
}

void UAZ_NavigationTargetComponent::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	EnsureEditorTargetId(false);
#endif
	if (HasBegunPlay())
	{
		RefreshRegistration();
	}
}

void UAZ_NavigationTargetComponent::OnUnregister()
{
	UnregisterTarget();
	Super::OnUnregister();
}

void UAZ_NavigationTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshRegistration();
}

void UAZ_NavigationTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterTarget();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UAZ_NavigationTargetComponent::EnsureEditorTargetId(bool bRegenerate)
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || World->WorldType != EWorldType::Editor || IsTemplate() || HasAnyFlags(RF_Transient)
		|| !Owner || Owner->IsTemplate() || Owner->HasAnyFlags(RF_Transient)
		|| (!bRegenerate && !TargetId.IsNone()))
	{
		return;
	}
	// Serialized on the placed instance. PIE duplication retains this ID; runtime never invents an identity.
	TargetId = FName(*FString::Printf(TEXT("Nav_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	MarkPackageDirty();
}

void UAZ_NavigationTargetComponent::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		EnsureEditorTargetId(true);
	}
}

void UAZ_NavigationTargetComponent::PostEditImport()
{
	Super::PostEditImport();
	EnsureEditorTargetId(true);
}
#endif
