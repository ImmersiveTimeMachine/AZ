#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Navigation/AZ_NavigationTypes.h"
#include "AZ_NavigationTargetSubsystem.generated.h"

class UAZ_NavigationTargetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_NavigationTargetsChanged, FName, TargetId);

/** Per-world weak provider registry. Duplicate IDs remain ambiguous until only one provider is registered. */
UCLASS()
class AZ_API UAZ_NavigationTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Navigation")
	FAZ_NavigationTargetsChanged OnTargetsChanged;

	/** Missing provider may use a valid descriptor location. Duplicate/provider-context errors never do. */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	EAZ_NavigationTargetResolveResult ResolveTarget(const FAZ_NavigationTargetDescriptor& Descriptor,
		FVector& OutWorldLocation, UAZ_NavigationTargetComponent*& OutProvider) const;

	UFUNCTION(BlueprintPure, Category="Navigation")
	int32 GetProviderCount(FName TargetId) const;

	void RegisterTarget(UAZ_NavigationTargetComponent* Provider);
	void UnregisterTarget(UAZ_NavigationTargetComponent* Provider);
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	struct FProviderSet
	{
		TArray<TWeakObjectPtr<UAZ_NavigationTargetComponent>> Providers;
	};
	TMap<FName, FProviderSet> ProvidersById;
	bool bDeinitializing = false;
	bool IsCurrentProvider(const UAZ_NavigationTargetComponent* Provider, FName TargetId) const;
};
