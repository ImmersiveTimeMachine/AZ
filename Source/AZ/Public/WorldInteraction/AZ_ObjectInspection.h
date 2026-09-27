#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "CommonActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "InputMappingContext.h"
#include "Components/Button.h"
#include "Camera/CameraModifier.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystem/AZ_Interactable.h"
#include "AZ_ObjectInspection.generated.h"

class AAZ_PlayerController;
class UStaticMeshComponent;
class USphereComponent;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class UAZ_InspectionComponent;
class UAZ_ActionPrompt;
class UInputAction;
class UInputMappingContext;
class UTextBlock;
class UImage;
class UButton;
class UBorder;
class UCanvasPanelSlot;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USizeBox;
class UWrapBox;
class UBackgroundBlur;

/** A reversible lens blend; does not overwrite the gameplay camera's FOV. */
UCLASS(Transient)
class AZ_API UAZ_InspectionCameraModifier : public UCameraModifier
{
	GENERATED_BODY()
public:
	UAZ_InspectionCameraModifier();
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& View) override;
};

UCLASS(Transient, NotBlueprintable)
class AZ_API UAZ_InspectionInputContext : public UInputMappingContext
{
	GENERATED_BODY()
public:
	UAZ_InspectionInputContext()
	{
		InputModeFilterOptions = EMappingContextInputModeFilterOptions::DoNotFilter;
		RegistrationTrackingMode = EMappingContextRegistrationTrackingMode::Untracked;
	}
};

UCLASS()
class AZ_API UAZ_InspectionButton : public UButton
{
	GENERATED_BODY()
public:
	UAZ_InspectionButton() { InitIsFocusable(false); }
};

UENUM(BlueprintType)
enum class EAZ_ObjectAction : uint8 { Inspect, Read, Take };

/** Optional capabilities for an object. No inventory payload is implied by its visual type. */
UCLASS(ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_ObjectActionsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_ObjectActionsComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") TArray<EAZ_ObjectAction> Actions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") FText ObjectName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(MultiLine=true)) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(MultiLine=true)) FText FrontText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(MultiLine=true)) FText BackText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inspection") FRotator InitialRotation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="50",ClampMax="300")) float UseRadius = 180.f;
	UFUNCTION(BlueprintPure, Category="Interaction") bool Supports(EAZ_ObjectAction Action) const;
	bool CanInspect(const AAZ_PlayerController* Player) const;
	UStaticMeshComponent* FindVisual() const;
};

/** Lightweight placeable prop for photos, letters, books and other inspectable objects. */
UCLASS(Blueprintable)
class AZ_API AAZ_InspectableObject : public AActor, public IAZ_Interactable
{
	GENERATED_BODY()
public:
	AAZ_InspectableObject();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> InteractionVolume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAZ_ObjectActionsComponent> ObjectActions;
	virtual bool IsAvailableForInteraction_Implementation(UPrimitiveComponent*) const override { return true; }
	virtual float GetInteractionDuration_Implementation(UPrimitiveComponent*) const override { return 0.f; }
};

UCLASS(Blueprintable)
class AZ_API AAZ_InspectablePickup : public AAZ_InspectableObject
{
	GENERATED_BODY()
public:
	AAZ_InspectablePickup();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UAZ_Inv_CommonUI_ItemComponent> Item;
};

/** Local cosmetic viewer. Never becomes a world pickup or campaign participant. */
UCLASS(NotBlueprintable, Transient)
class AZ_API AAZ_InspectionPreview : public AActor
{
	GENERATED_BODY()
public:
	AAZ_InspectionPreview();
	bool Initialize(UStaticMeshComponent* Source, FRotator Rotation);
	void Rotate(FVector2D Degrees);
	void Zoom(float Amount);
	void Flip();
	void ResetView();
	FBox2D GetViewBounds() const;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;
private:
	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	FRotator InitialRotation;
	float Distance = 160.f;
	float MinimumDistance = 110.f;
	float DefaultDistance = 160.f;
	float MaximumDistance = 250.f;
};

UCLASS()
class AZ_API UAZ_InspectionPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UAZ_InspectionPanel(const FObjectInitializer& Initializer);
	void SetInspectionOwner(UAZ_InspectionComponent* Owner);
	void Refresh();
	void ShowError(const FText& Error);
	void BeginCloseTransition();
	bool IsClosingTransition() const { return bExiting; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry&, float DeltaSeconds) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
	virtual FReply NativeOnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry&, const FAnalogInputEvent&) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_InspectionComponent> OwnerComponent;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Description;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> TitleSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> DescriptionSlot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RotateHintKey;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ZoomHintKey;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Reading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
	UPROPERTY(Transient) TObjectPtr<UImage> Picture;
	UPROPERTY(Transient) TObjectPtr<UBorder> ReadingCard;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> PictureSlot;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PictureMaterial;
	UPROPERTY(Transient) TObjectPtr<UBorder> Controls;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> ControlsSlot;
	UPROPERTY(Transient) TObjectPtr<USizeBox> ControlsWidth;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> ControlButtons;
	UPROPERTY(Transient) TObjectPtr<UBackgroundBlur> EnvironmentBlur;
	UPROPERTY(Transient) TObjectPtr<UButton> TakeButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ReadButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
	TArray<FUIActionBindingHandle> Bindings;
	bool bReadText = false;
	bool bBackSide = false;
	bool bDragging = false;
	bool bExiting = false;
	float BlurBlend = 0.f;
	UFUNCTION() void Close();
	UFUNCTION() void Flip();
	UFUNCTION() void ResetView();
	UFUNCTION() void Read();
	UFUNCTION() void Take();
};

/** Owns the inspection lifetime, modal input and authoritative optional pickup. */
UCLASS(ClassGroup=(AZ))
class AZ_API UAZ_InspectionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_InspectionComponent();
	void RequestInspect(AActor* Target);
	void Close(bool bImmediate = false);
	void RequestTake();
	bool IsInspecting() const { return IsValid(Panel); }
	bool IsCurrentPanel(const UAZ_InspectionPanel* Candidate) const { return Panel == Candidate; }
	AActor* GetInspectedActor() const { return InspectedActor.Get(); }
	UAZ_ObjectActionsComponent* GetActions() const;
	AAZ_InspectionPreview* GetPreview() const { return Preview; }
	UPROPERTY(EditDefaultsOnly, Category="Inspection") TSubclassOf<UAZ_ActionPrompt> PromptClass;
	UPROPERTY(EditDefaultsOnly, Category="Inspection") TObjectPtr<UMaterialInterface> PreviewMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Inspection|Presentation", meta=(ClampMin="0",ClampMax="30")) float EnvironmentBlurStrength = 6.9f;
	UPROPERTY(EditDefaultsOnly, Category="Inspection|Presentation", meta=(ClampMin="0",ClampMax="1")) float BlurTransitionSeconds = .2f;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> UIActions;
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_InspectionPanel> Panel;
	UPROPERTY(Transient) TObjectPtr<AAZ_InspectionPreview> Preview;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UAZ_InspectionCameraModifier> CameraModifier;
	TWeakObjectPtr<AActor> InspectedActor;
	TWeakObjectPtr<AActor> ServerTarget;
	bool bClosing = false;
	bool bOriginalHidden = false;
	void CreateInputMapping();
	AAZ_PlayerController* Player() const;
	UFUNCTION(Server, Reliable) void Server_BeginInspection(AActor* Target);
	UFUNCTION(Client, Reliable) void Client_BeginInspection(AActor* Target);
	UFUNCTION(Server, Reliable) void Server_EndInspection();
	UFUNCTION(Server, Reliable) void Server_TakeInspectedItem();
	UFUNCTION(Client, Reliable) void Client_InspectionTakeResult(bool bTaken);
};
