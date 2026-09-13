// Copyright Artur. AZ project.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_PairedHandContactComponent.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;
class UAnimInstance;
class AController;

/** Replicate the identity of the GAS-owned pair, never cosmetic bone transforms. */
USTRUCT()
struct FAZ_PairedHandContactAction
{
	GENERATED_BODY()
	UPROPERTY() FGuid Id;
	UPROPERTY() TObjectPtr<AActor> Partner = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> OwnMontage = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> PartnerMontage = nullptr;
};

/** Copied on the game thread; safe for the anim instance to consume during evaluation. */
struct FAZ_PairedHandContactSnapshot
{
	FTransform LeftTargetCS = FTransform::Identity;
	FTransform RightTargetCS = FTransform::Identity;
	float LeftWeight = 0.f;
	float RightWeight = 0.f;
	FGuid ActionId;
	bool bActive = false;
};

/** One explicitly registered grab/Wrestle contact. Does not own alignment, damage or montage timing. */
UCLASS(ClassGroup = (Animation), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_PairedHandContactComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_PairedHandContactComponent();
	FGuid BeginPairedGrab(AActor* Partner, UAnimMontage* OwnMontage, UAnimMontage* PartnerMontage);
	void EndPairedGrab(const FGuid& ActionId);
	FAZ_PairedHandContactSnapshot EvaluateHandContacts(USkeletalMeshComponent* OwnMesh, float DeltaSeconds);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") bool bEnabled = true;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") FName ContactSection = TEXT("Wrestle");
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") FName LeftPartnerSocket = TEXT("GrabIK_HandL");
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") FName RightPartnerSocket = TEXT("GrabIK_HandR");
	/** Offset expressed in the partner socket frame, including hand-frame rotation calibration. */
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") FTransform LeftSocketOffset = FTransform::Identity;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") FTransform RightSocketOffset = FTransform::Identity;
	/** Seconds relative to the Wrestle section. Negative end means the full section. */
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0")) float ContactStartSeconds = 0.f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands") float ContactEndSeconds = -1.f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0.5", ClampMax = "1")) float ReachScale = 0.97f;
	/** Per-update correction limit; the independent arm-length clamp prevents overextension. */
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0")) float MaxHandCorrectionCm = 25.f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0")) float BlendInSeconds = 0.1f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0")) float BlendOutSeconds = 0.2f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "0")) float MaxPartnerDistanceCm = 250.f;
	UPROPERTY(EditAnywhere, Category = "AZ|Procedural|Paired Hands", meta = (ClampMin = "1")) float MaxActionSeconds = 30.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Action) FAZ_PairedHandContactAction Action;
	UFUNCTION() void OnRep_Action();
	void ClearAuthorityAction();
	bool IsContactWindow(const UAnimInstance* Anim, const UAnimMontage* Montage) const;
	USkeletalMeshComponent* FindPartnerMesh() const;
	bool BuildHandTarget(USkeletalMeshComponent* OwnMesh, USkeletalMeshComponent* PartnerMesh,
		FName UpperArm, FName LowerArm, FName Hand, FName Socket, const FTransform& Offset,
		FTransform& OutTargetCS) const;
	FAZ_PairedHandContactSnapshot Cached;
	TWeakObjectPtr<USkeletalMeshComponent> LastOwnMesh;
	TWeakObjectPtr<UAnimInstance> LastOwnAnim;
	TWeakObjectPtr<AController> StartController;
	TWeakObjectPtr<UAnimInstance> AuthorityOwnAnim;
	TWeakObjectPtr<UAnimInstance> AuthorityPartnerAnim;
	int32 OwnMontageInstanceId = INDEX_NONE;
	int32 PartnerMontageInstanceId = INDEX_NONE;
	double AuthorityStartTime = 0.0;
};
