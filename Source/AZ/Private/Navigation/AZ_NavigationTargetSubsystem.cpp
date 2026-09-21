#include "Navigation/AZ_NavigationTargetSubsystem.h"

#include "Engine/World.h"
#include "Navigation/AZ_NavigationTargetComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAZNavigation, Log, All);

bool UAZ_NavigationTargetSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UAZ_NavigationTargetSubsystem::IsCurrentProvider(const UAZ_NavigationTargetComponent* Provider, FName TargetId) const
{
	return IsValid(Provider) && Provider->GetWorld() == GetWorld() && Provider->IsRegistered()
		&& Provider->HasBegunPlay() && Provider->bTargetEnabled && Provider->TargetId == TargetId;
}

void UAZ_NavigationTargetSubsystem::RegisterTarget(UAZ_NavigationTargetComponent* Provider)
{
	if (bDeinitializing || !IsValid(Provider) || Provider->GetWorld() != GetWorld())
	{
		return;
	}
	if (!Provider->bTargetEnabled || !Provider->IsRegistered() || !Provider->HasBegunPlay()
		|| Provider->TargetId.IsNone())
	{
		UnregisterTarget(Provider);
		UE_LOG(LogAZNavigation, Warning, TEXT("Target %s was not registered: enabled, begun-play provider and authored TargetId required."),
			*GetPathNameSafe(Provider));
		return;
	}
	if (Provider->MapId.IsNone() || Provider->LayerId.IsNone())
	{
		// Keep the ID reserved: bad context on one duplicate must not make another provider win silently.
		UE_LOG(LogAZNavigation, Error, TEXT("Target %s has no MapId/LayerId; its ID remains reserved but cannot resolve."),
			*GetPathNameSafe(Provider));
	}

	const TWeakObjectPtr<UAZ_NavigationTargetComponent> ProviderKey(Provider);
	TSet<FName> ChangedIds;
	// Identity edits remove only this provider from its old bucket, never another provider sharing the ID.
	for (auto It = ProvidersById.CreateIterator(); It; ++It)
	{
		if (It.Key() != Provider->TargetId && It.Value().Providers.Remove(ProviderKey) > 0)
		{
			ChangedIds.Add(It.Key());
			if (It.Value().Providers.IsEmpty()) { It.RemoveCurrent(); }
		}
	}
	FProviderSet& Bucket = ProvidersById.FindOrAdd(Provider->TargetId);
	Bucket.Providers.RemoveAll([](const TWeakObjectPtr<UAZ_NavigationTargetComponent>& Entry) { return !Entry.IsValid(); });
	const bool bAlreadyRegistered = Bucket.Providers.Contains(ProviderKey);
	Bucket.Providers.AddUnique(ProviderKey);
	ChangedIds.Add(Provider->TargetId);
	if (!bAlreadyRegistered && GetProviderCount(Provider->TargetId) > 1)
	{
		UE_LOG(LogAZNavigation, Error, TEXT("Ambiguous navigation TargetId '%s' after registering %s; resolution disabled until the duplicate is removed."),
			*Provider->TargetId.ToString(), *GetPathNameSafe(Provider));
	}
	for (const FName Id : ChangedIds)
	{
		OnTargetsChanged.Broadcast(Id);
	}
}

void UAZ_NavigationTargetSubsystem::UnregisterTarget(UAZ_NavigationTargetComponent* Provider)
{
	if (!Provider || bDeinitializing)
	{
		return;
	}
	const TWeakObjectPtr<UAZ_NavigationTargetComponent> ProviderKey(Provider);
	TArray<FName> ChangedIds;
	for (auto It = ProvidersById.CreateIterator(); It; ++It)
	{
		if (It.Value().Providers.Remove(ProviderKey) > 0)
		{
			ChangedIds.Add(It.Key());
			if (It.Value().Providers.IsEmpty()) { It.RemoveCurrent(); }
		}
	}
	// Broadcast only after mutation; observers may resolve or register targets in their callback.
	for (const FName Id : ChangedIds)
	{
		OnTargetsChanged.Broadcast(Id);
	}
}

int32 UAZ_NavigationTargetSubsystem::GetProviderCount(FName TargetId) const
{
	int32 Count = 0;
	if (const FProviderSet* Bucket = ProvidersById.Find(TargetId))
	{
		for (const TWeakObjectPtr<UAZ_NavigationTargetComponent>& Entry : Bucket->Providers)
		{
			Count += IsCurrentProvider(Entry.Get(), TargetId) ? 1 : 0;
		}
	}
	return Count;
}

EAZ_NavigationTargetResolveResult UAZ_NavigationTargetSubsystem::ResolveTarget(
	const FAZ_NavigationTargetDescriptor& Descriptor, FVector& OutWorldLocation, UAZ_NavigationTargetComponent*& OutProvider) const
{
	OutWorldLocation = FVector::ZeroVector;
	OutProvider = nullptr;
	if (bDeinitializing || !Descriptor.IsWellFormed())
	{
		return EAZ_NavigationTargetResolveResult::Invalid;
	}
	UAZ_NavigationTargetComponent* Match = nullptr;
	if (const FProviderSet* Bucket = ProvidersById.Find(Descriptor.TargetId))
	{
		for (const TWeakObjectPtr<UAZ_NavigationTargetComponent>& Entry : Bucket->Providers)
		{
			UAZ_NavigationTargetComponent* Provider = Entry.Get();
			if (IsCurrentProvider(Provider, Descriptor.TargetId))
			{
				if (Match) { return EAZ_NavigationTargetResolveResult::Ambiguous; }
				Match = Provider;
			}
		}
	}
	if (Match)
	{
		if (Match->MapId.IsNone() || Match->LayerId.IsNone())
		{
			return EAZ_NavigationTargetResolveResult::Invalid;
		}
		if ((!Descriptor.MapId.IsNone() && Descriptor.MapId != Match->MapId)
			|| (!Descriptor.LayerId.IsNone() && Descriptor.LayerId != Match->LayerId))
		{
			return EAZ_NavigationTargetResolveResult::ContextMismatch;
		}
		if (!Match->GetNavigationLocation(OutWorldLocation))
		{
			return EAZ_NavigationTargetResolveResult::Invalid;
		}
		OutProvider = Match;
		return EAZ_NavigationTargetResolveResult::ResolvedProvider;
	}
	if (Descriptor.bHasWorldLocation)
	{
		OutWorldLocation = Descriptor.WorldLocation;
		return EAZ_NavigationTargetResolveResult::ResolvedLocation;
	}
	return EAZ_NavigationTargetResolveResult::Missing;
}

void UAZ_NavigationTargetSubsystem::Deinitialize()
{
	bDeinitializing = true;
	ProvidersById.Reset();
	OnTargetsChanged.Clear();
	Super::Deinitialize();
}
