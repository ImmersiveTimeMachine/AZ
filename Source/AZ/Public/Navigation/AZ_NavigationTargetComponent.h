#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Navigation/AZ_NavigationTypes.h"
#include "AZ_NavigationTargetComponent.generated.h"

class UAZ_NavigationTargetSubsystem;

/** Authored stable target provider. Registration never implies player knowledge/discovery. */
UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_NavigationTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAZ_NavigationTargetComponent();

	/** Unique within a world. Empty placed-instance IDs are generated in the editor, never at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName TargetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName LayerId = TEXT("Outdoor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	bool bTargetEnabled = true;

	/** Applied with this scene component's full world transform, including attachment rotation/scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(Units="cm"))
	FVector LocalOffset = FVector::ZeroVector;

	UFUNCTION(BlueprintPure, Category="Navigation")
	bool GetNavigationLocation(FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category="Navigation")
	FAZ_NavigationTargetDescriptor GetTargetDescriptor() const;

	UFUNCTION(BlueprintCallable, Category="Navigation")
	void SetTargetEnabled(bool bEnabled);

	/** Reconcile registration after an intentional native runtime identity/context change. */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void RefreshRegistration();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditImport() override;
#endif

private:
	void UnregisterTarget();
	TWeakObjectPtr<UAZ_NavigationTargetSubsystem> RegisteredSubsystem;

#if WITH_EDITOR
	void EnsureEditorTargetId(bool bRegenerate);
#endif
};
