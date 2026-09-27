#include "WorldInteraction/AZ_ObjectInspection.h"
#include "Player/AZ_PlayerController.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "UI/AZ_ActionPrompt.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Components/BackgroundBlur.h"
#include "CommonInputSubsystem.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Input/CommonUIInputTypes.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Styling/CoreStyle.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StoryPhoto, "Item.Type.Consumable.Story.Photo");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StoryNote, "Item.Type.Consumable.Story.Note");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StoryBook, "Item.Type.Consumable.Story.Book");

#define LOCTEXT_NAMESPACE "AZInspection"

UAZ_ObjectActionsComponent::UAZ_ObjectActionsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Actions = {EAZ_ObjectAction::Inspect};
	ObjectName = LOCTEXT("Object", "Object");
}

bool UAZ_ObjectActionsComponent::Supports(EAZ_ObjectAction Action) const
{
	if (!Actions.Contains(Action)) return false;
	if (Action == EAZ_ObjectAction::Read) return !FrontText.IsEmpty() || !BackText.IsEmpty();
	if (Action == EAZ_ObjectAction::Take)
	{
		const auto* Item = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
		return Item && Item->GetItemManifest().GetItemTypeTag().IsValid();
	}
	return true;
}

UStaticMeshComponent* UAZ_ObjectActionsComponent::FindVisual() const
{
	TInlineComponentArray<UStaticMeshComponent*> Meshes(GetOwner());
	for (auto* Mesh : Meshes) if (Mesh->GetStaticMesh()) return Mesh;
	return nullptr;
}

bool UAZ_ObjectActionsComponent::CanInspect(const AAZ_PlayerController* PC) const
{
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const auto* Visual = FindVisual();
	if (!Pawn || !Visual || !Supports(EAZ_ObjectAction::Inspect) || GetOwner()->IsActorBeingDestroyed()
		|| GetWorld() != PC->GetWorld() || !FMath::IsFinite(UseRadius) || UseRadius <= 0.f) return false;
	if (const auto* ASC = PC->GetAbilitySystemComponent())
		if (ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Character_Dead)
			|| ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed)) return false;
	if (const auto* Item = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>(); Item && !Item->IsAccessibleForPickup()) return false;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Visual->Bounds.Origin) > FMath::Square(UseRadius)) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(InspectionReach), true, Pawn);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Visual->Bounds.Origin, ECC_Visibility, Query)
		|| Hit.GetActor() == GetOwner();
}

AAZ_InspectableObject::AAZ_InspectableObject()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetCanEverAffectNavigation(false);
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Mesh);
	InteractionVolume->SetSphereRadius(180.f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetCanEverAffectNavigation(false);
	ObjectActions = CreateDefaultSubobject<UAZ_ObjectActionsComponent>(TEXT("ObjectActions"));
}

void AAZ_InspectableObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Compensate mesh scale so a small photograph still has a usable interaction radius.
	const float Scale = GetActorScale3D().GetAbs().GetMax();
	InteractionVolume->SetSphereRadius(ObjectActions->UseRadius / FMath::Max(Scale, 0.001f));
}

AAZ_InspectablePickup::AAZ_InspectablePickup()
{
	Item = CreateDefaultSubobject<UAZ_Inv_CommonUI_ItemComponent>(TEXT("Item"));
	ObjectActions->Actions.Add(EAZ_ObjectAction::Take);
}

AAZ_InspectionPreview::AAZ_InspectionPreview()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot")); Pivot->SetupAttachment(RootComponent);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Pivot);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCanEverAffectNavigation(false);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture")); Capture->SetupAttachment(RootComponent);
	Capture->FOVAngle = 32.f;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = true;
	Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Light = CreateDefaultSubobject<UPointLightComponent>(Index == 0 ? TEXT("KeyLight") : TEXT("FillLight"));
		Light->SetupAttachment(RootComponent);
		Light->SetRelativeLocation(Index == 0 ? FVector(65,-65,85) : FVector(85,75,-15));
		Light->SetIntensity(Index == 0 ? 5000.f : 2500.f);
		Light->SetAttenuationRadius(450.f); Light->SetCastShadows(false);
	}
}

bool AAZ_InspectionPreview::Initialize(UStaticMeshComponent* Source, FRotator Rotation)
{
	if (!Source || !Source->GetStaticMesh()) return false;
	Visual->SetStaticMesh(Source->GetStaticMesh());
	for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index) Visual->SetMaterial(Index, Source->GetMaterial(Index));
	const FBox Bounds = Source->GetStaticMesh()->GetBoundingBox();
	const float Scale = 34.f / FMath::Max(float(Bounds.GetExtent().GetMax()), 0.01f);
	Visual->SetRelativeScale3D(FVector(Scale)); Visual->SetRelativeLocation(-Bounds.GetCenter() * Scale);
	// The model rotates around its centered bounds. Fit the enclosing sphere,
	// so the closest zoom remains safe for every orientation without zoom pumping.
	// The square capture leaves a six-percent margin on each screen axis.
	const float Radius = float(Bounds.GetExtent().Size()) * Scale;
	const float SafeHalfAngle = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * .5f)) * .94f);
	MinimumDistance = FMath::Max(80.f, Radius / FMath::Max(FMath::Sin(SafeHalfAngle), .01f));
	DefaultDistance = FMath::Max(160.f, MinimumDistance * 1.25f);
	MaximumDistance = FMath::Max(250.f, MinimumDistance * 2.f);
	InitialRotation = Rotation; ResetView();
	Target = NewObject<UTextureRenderTarget2D>(this);
	Target->RenderTargetFormat = RTF_RGBA16f;
	Target->ClearColor = FLinearColor(0.f,0.f,0.f,1.f); // Scene capture stores inverse opacity in alpha.
	Target->InitAutoFormat(1024,1024);
	Capture->TextureTarget = Target; Capture->ShowOnlyActors.Add(this);
	Capture->CaptureScene();
	return true;
}
void AAZ_InspectionPreview::Rotate(FVector2D Degrees)
{
	const FQuat Yaw(FVector::UpVector,FMath::DegreesToRadians(Degrees.X));
	const FQuat Pitch(FVector::RightVector,FMath::DegreesToRadians(Degrees.Y));
	Pivot->SetRelativeRotation((Yaw * Pitch * Pivot->GetRelativeRotation().Quaternion()).GetNormalized());
}
void AAZ_InspectionPreview::Zoom(float Amount) { Distance = FMath::Clamp(Distance - Amount * 12.f, MinimumDistance, MaximumDistance); Pivot->SetRelativeLocation(FVector(Distance,0,0)); }
void AAZ_InspectionPreview::Flip() { Rotate(FVector2D(180,0)); }
void AAZ_InspectionPreview::ResetView() { Distance=DefaultDistance; Pivot->SetRelativeLocation(FVector(Distance,0,0)); Pivot->SetRelativeRotation(InitialRotation); }

UAZ_InspectionPanel::UAZ_InspectionPanel(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true); bIsBackHandler = false;
}
void UAZ_InspectionPanel::SetInspectionOwner(UAZ_InspectionComponent* Owner) { OwnerComponent = Owner; }

TSharedRef<SWidget> UAZ_InspectionPanel::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Background = WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor::Transparent);
		WidgetTree->RootWidget = Background;
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); Background->AddChild(Canvas);
		auto Place = [Canvas](UWidget* Widget, FAnchors Anchors, FMargin Offsets)
		{ auto* Slot = Canvas->AddChildToCanvas(Widget); Slot->SetAnchors(Anchors); Slot->SetOffsets(Offsets); return Slot; };
		EnvironmentBlur=WidgetTree->ConstructWidget<UBackgroundBlur>();
		EnvironmentBlur->SetApplyAlphaToBlur(false);
		EnvironmentBlur->SetBlurStrength(0.f);
		EnvironmentBlur->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(EnvironmentBlur,FAnchors(0,0,1,1),FMargin(0))->SetZOrder(-100);
		auto Text = [this](int32 Size)
		{ auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size)); T->SetColorAndOpacity(FSlateColor(FLinearColor(.855f,.823f,.745f,1))); return T; };
		Title=Text(22);
		TitleSlot=Place(Title,FAnchors(.27f,.77f,.85f,.77f),FMargin(0,0,0,32));
		Description=Text(18); Description->SetAutoWrapText(true);
		DescriptionSlot=Place(Description,FAnchors(.27f,.81f,.85f,.81f),FMargin(0,0,0,52));
		auto* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
		PictureSlot=Place(Fit,FAnchors(.17f,.04f,.91f,.74f),FMargin(0));
		Picture=WidgetTree->ConstructWidget<UImage>(); Fit->AddChild(Picture);
		ReadingCard=WidgetTree->ConstructWidget<UBorder>();
		ReadingCard->SetBrushColor(FLinearColor::FromSRGBColor(FColor(221,214,196)));
		ReadingCard->SetPadding(FMargin(32));
		ReadingCard->SetVisibility(ESlateVisibility::Collapsed);
		Place(ReadingCard,FAnchors(.57f,.15f,.95f,.82f),FMargin(0));
		auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); ReadingCard->AddChild(Scroll);
		Reading=Text(24); Reading->SetAutoWrapText(true);
		Reading->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(40,46,41))));
		Scroll->AddChild(Reading);
		Controls=WidgetTree->ConstructWidget<UBorder>();
		Controls->SetBrushColor(FLinearColor(.012f,.018f,.014f,.73f));
		Controls->SetPadding(FMargin(24,18));
		ControlsSlot=Place(Controls,FAnchors(.5f,1.f),FMargin(0,-38,0,0));
		ControlsSlot->SetAutoSize(true); ControlsSlot->SetAlignment(FVector2D(.5f,1.f));
		ControlsWidth=WidgetTree->ConstructWidget<USizeBox>(); ControlsWidth->SetWidthOverride(1790); ControlsWidth->SetMinDesiredHeight(34);
		Controls->AddChild(ControlsWidth);
		auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(); ControlsWidth->AddChild(Buttons);
		auto AddCell=[this,Buttons](UWidget* Child)
		{
			auto* Cell=WidgetTree->ConstructWidget<UScaleBox>(); Cell->SetStretch(EStretch::ScaleToFit); Cell->SetStretchDirection(EStretchDirection::DownOnly);
			Cell->AddChild(Child);
			auto* CellSlot=Buttons->AddChildToHorizontalBox(Cell);
			CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CellSlot->SetHorizontalAlignment(HAlign_Fill); CellSlot->SetVerticalAlignment(VAlign_Center);
			CellSlot->SetPadding(FMargin(0,0,18,0));
		};
		auto Hint=[this,&Text,&AddCell](const FText& Label,TObjectPtr<UTextBlock>& Key)
		{
			auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
			auto* Cap=WidgetTree->ConstructWidget<UBorder>();
			FSlateBrush Brush; Brush.DrawAs=ESlateBrushDrawType::RoundedBox;
			Brush.TintColor=FSlateColor(FLinearColor(.021f,.027f,.021f,1));
			Brush.OutlineSettings.CornerRadii=FVector4(0,0,0,0);
			Brush.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.Width=1.f; Brush.OutlineSettings.Color=FSlateColor(FLinearColor(.855f,.823f,.745f,1));
			Cap->SetBrush(Brush); Cap->SetPadding(FMargin(7,4));
			Key=Text(16); Cap->AddChild(Key);
			Row->AddChildToHorizontalBox(Cap)->SetVerticalAlignment(VAlign_Center);
			auto* Caption=Text(18); Caption->SetText(Label);
			auto* CaptionSlot=Row->AddChildToHorizontalBox(Caption); CaptionSlot->SetPadding(FMargin(12,0,0,0)); CaptionSlot->SetVerticalAlignment(VAlign_Center);
			AddCell(Row);
		};
		Hint(LOCTEXT("RotateLabel","Rotate"),RotateHintKey);
		Hint(LOCTEXT("ZoomLabel","Zoom"),ZoomHintKey);
		Feedback=Text(16); Feedback->SetAutoWrapText(true);
		Place(Feedback,FAnchors(.27f,.86f,.85f,.86f),FMargin(0,0,0,28));
		const FText Labels[] = {LOCTEXT("Back","Return"),LOCTEXT("Flip","Flip"),LOCTEXT("Reset","Reset view"),LOCTEXT("Read","Read"),LOCTEXT("Take","Take")};
		for (int32 Index : {1,3,4,0})
		{
			auto* Button=WidgetTree->ConstructWidget<UAZ_InspectionButton>();
			FButtonStyle ButtonStyle=Button->GetStyle();
			ButtonStyle.SetNormal(FSlateNoResource()); ButtonStyle.SetHovered(FSlateColorBrush(FLinearColor(1,1,1,.04f)));
			ButtonStyle.SetPressed(FSlateColorBrush(FLinearColor(1,1,1,.08f))); ButtonStyle.SetDisabled(FSlateNoResource());
			ButtonStyle.SetNormalPadding(FMargin(0)); ButtonStyle.SetPressedPadding(FMargin(0)); Button->SetStyle(ButtonStyle);
			UWidget* Content=nullptr;
			if (OwnerComponent && OwnerComponent->PromptClass && OwnerComponent->UIActions.IsValidIndex(Index))
			{
				auto* Prompt=CreateWidget<UAZ_ActionPrompt>(GetOwningPlayer(),OwnerComponent->PromptClass);
				Prompt->ConfigureAction(OwnerComponent->UIActions[Index],Labels[Index],TEXT("Inspection")); Content=Prompt;
			}
			if (!Content) { auto* Label=Text(18); Label->SetText(Labels[Index]); Content=Label; }
			Button->AddChild(Content);
			AddCell(Button);
			switch(Index)
			{
			case 0: BackButton=Button; Button->OnClicked.AddDynamic(this,&ThisClass::Close); break;
			case 1: Button->OnClicked.AddDynamic(this,&ThisClass::Flip); break;
			case 2: Button->OnClicked.AddDynamic(this,&ThisClass::ResetView); break;
			case 3: ReadButton=Button; Button->OnClicked.AddDynamic(this,&ThisClass::Read); break;
			case 4: TakeButton=Button; Button->OnClicked.AddDynamic(this,&ThisClass::Take); break;
			}
		}
	}
	return Super::RebuildWidget();
}
void UAZ_InspectionPanel::NativeOnActivated()
{
	Super::NativeOnActivated();
	bExiting=false; BlurBlend=0.f;
	if (OwnerComponent && OwnerComponent->UIActions.Num()==5)
	{
		const auto Add=[this](int32 Index, FSimpleDelegate Delegate) { Bindings.Add(RegisterUIActionBinding(FBindUIActionArgs(OwnerComponent->UIActions[Index],false,Delegate))); };
		Add(0,FSimpleDelegate::CreateUObject(this,&ThisClass::Close)); Add(1,FSimpleDelegate::CreateUObject(this,&ThisClass::Flip));
		Add(2,FSimpleDelegate::CreateUObject(this,&ThisClass::ResetView)); Add(3,FSimpleDelegate::CreateUObject(this,&ThisClass::Read));
		Add(4,FSimpleDelegate::CreateUObject(this,&ThisClass::Take));
	}
	Refresh();
}
void UAZ_InspectionPanel::NativeOnDeactivated() { bDragging=false; for(auto& B:Bindings) B.Unregister(); Bindings.Reset(); Super::NativeOnDeactivated(); if(OwnerComponent && OwnerComponent->IsCurrentPanel(this)) OwnerComponent->Close(true); }
void UAZ_InspectionPanel::NativeDestruct() { bDragging=false; for(auto& B:Bindings) B.Unregister(); Bindings.Reset(); Super::NativeDestruct(); if(OwnerComponent && OwnerComponent->IsCurrentPanel(this)) OwnerComponent->Close(true); }
bool UAZ_InspectionPanel::NativeOnHandleBackAction() { Close(); return true; }
UWidget* UAZ_InspectionPanel::NativeGetDesiredFocusTarget() const { return const_cast<UAZ_InspectionPanel*>(this); }
TOptional<FUIInputConfig> UAZ_InspectionPanel::GetDesiredInputConfig() const { return FUIInputConfig(ECommonInputMode::Menu,EMouseCaptureMode::NoCapture); }
FReply UAZ_InspectionPanel::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
	if (!bExiting && bDragging && E.IsMouseButtonDown(EKeys::LeftMouseButton) && OwnerComponent && OwnerComponent->IsCurrentPanel(this) && OwnerComponent->GetPreview())
	{ OwnerComponent->GetPreview()->Rotate(E.GetCursorDelta() * .35f); return FReply::Handled(); }
	return Super::NativeOnMouseMove(G,E);
}
FReply UAZ_InspectionPanel::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
	if (!bExiting && E.GetEffectingButton()==EKeys::LeftMouseButton && OwnerComponent && OwnerComponent->IsCurrentPanel(this)
		&& Picture && Picture->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()))
	{
		bDragging=true;
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(),EFocusCause::Mouse);
	}
	return Super::NativeOnMouseButtonDown(G,E);
}
FReply UAZ_InspectionPanel::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
	if (E.GetEffectingButton()==EKeys::LeftMouseButton && bDragging)
	{ bDragging=false; return FReply::Handled().ReleaseMouseCapture(); }
	return Super::NativeOnMouseButtonUp(G,E);
}
void UAZ_InspectionPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& E)
{ bDragging=false; Super::NativeOnMouseCaptureLost(E); }
FReply UAZ_InspectionPanel::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& E)
{ if(OwnerComponent && OwnerComponent->GetPreview()) OwnerComponent->GetPreview()->Zoom(E.GetWheelDelta()); return FReply::Handled(); }
FReply UAZ_InspectionPanel::NativeOnAnalogValueChanged(const FGeometry& G,const FAnalogInputEvent& E)
{
	if (!OwnerComponent || !OwnerComponent->GetPreview()) return Super::NativeOnAnalogValueChanged(G,E);
	const float V=FMath::Abs(E.GetAnalogValue())>.15f ? E.GetAnalogValue() : 0.f;
	const float D=GetWorld()->GetDeltaSeconds();
	if(E.GetKey()==EKeys::Gamepad_RightX) OwnerComponent->GetPreview()->Rotate(FVector2D(V*D*120.f,0));
	else if(E.GetKey()==EKeys::Gamepad_RightY) OwnerComponent->GetPreview()->Rotate(FVector2D(0,V*D*120.f));
	else if(E.GetKey()==EKeys::Gamepad_RightTriggerAxis) OwnerComponent->GetPreview()->Zoom(V*D*5.f);
	else if(E.GetKey()==EKeys::Gamepad_LeftTriggerAxis) OwnerComponent->GetPreview()->Zoom(-V*D*5.f);
	else return Super::NativeOnAnalogValueChanged(G,E);
	return FReply::Handled();
}
void UAZ_InspectionPanel::Refresh()
{
	const auto* Actions=OwnerComponent ? OwnerComponent->GetActions() : nullptr;
	if(!Actions || !Title) return;
	Title->SetText(Actions->ObjectName);
	if(Description) Description->SetText(Actions->Description);
	if(OwnerComponent->GetPreview() && Picture)
	{
		if (!PictureMaterial && OwnerComponent->PreviewMaterial)
		{
			Picture->SetBrushFromMaterial(OwnerComponent->PreviewMaterial);
			PictureMaterial=Picture->GetDynamicMaterial();
			FSlateBrush Brush=Picture->GetBrush(); Brush.ImageSize=FVector2D(1024,1024); Picture->SetBrush(Brush);
		}
		if (PictureMaterial) PictureMaterial->SetTextureParameterValue(TEXT("InspectionTexture"),OwnerComponent->GetPreview()->Target);
	}
	TakeButton->SetVisibility(Actions->Supports(EAZ_ObjectAction::Take)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	ReadButton->SetVisibility(Actions->Supports(EAZ_ObjectAction::Read)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	Reading->SetText(bBackSide?Actions->BackText:Actions->FrontText);
	ReadingCard->SetVisibility(bReadText?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	if (PictureSlot)
	{
		PictureSlot->SetAnchors(bReadText?FAnchors(.05f,.08f,.53f,.73f):FAnchors(.17f,.04f,.91f,.74f));
		PictureSlot->SetOffsets(FMargin(0));
		if(TitleSlot) TitleSlot->SetAnchors(bReadText?FAnchors(.10f,.77f,.51f,.77f):FAnchors(.27f,.77f,.85f,.77f));
		if(DescriptionSlot) DescriptionSlot->SetAnchors(bReadText?FAnchors(.10f,.81f,.51f,.81f):FAnchors(.27f,.81f,.85f,.81f));
	}
}
void UAZ_InspectionPanel::ShowError(const FText& Error) { if(Feedback) Feedback->SetText(Error); }
void UAZ_InspectionPanel::Close() { if(OwnerComponent) OwnerComponent->Close(); }
void UAZ_InspectionPanel::Flip() { if(bExiting) return; if(OwnerComponent && OwnerComponent->GetPreview()) { OwnerComponent->GetPreview()->Flip(); bBackSide=!bBackSide; Refresh(); } }
void UAZ_InspectionPanel::ResetView() { if(bExiting) return; if(OwnerComponent && OwnerComponent->GetPreview()) OwnerComponent->GetPreview()->ResetView(); bBackSide=false; Refresh(); }
void UAZ_InspectionPanel::Read() { if(bExiting) return; if(OwnerComponent && OwnerComponent->GetActions() && OwnerComponent->GetActions()->Supports(EAZ_ObjectAction::Read)) { bReadText=!bReadText; Refresh(); } }
void UAZ_InspectionPanel::Take() { if(bExiting) return; if(OwnerComponent) OwnerComponent->RequestTake(); }

UAZ_InspectionComponent::UAZ_InspectionComponent()
{
	SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true;
}
AAZ_PlayerController* UAZ_InspectionComponent::Player() const { return Cast<AAZ_PlayerController>(GetOwner()); }
UAZ_ObjectActionsComponent* UAZ_InspectionComponent::GetActions() const { return InspectedActor.IsValid()?InspectedActor->FindComponentByClass<UAZ_ObjectActionsComponent>():nullptr; }
void UAZ_InspectionComponent::RequestInspect(AActor* Target) { if(Player() && !IsInspecting()) Server_BeginInspection(Target); }
void UAZ_InspectionComponent::Server_BeginInspection_Implementation(AActor* Target)
{
	const auto* Actions=IsValid(Target)?Target->FindComponentByClass<UAZ_ObjectActionsComponent>():nullptr;
	if(!Player() || !Player()->CanUseImmediateWorldInteraction() || !Actions || !Actions->CanInspect(Player())) return;
	ServerTarget=Target; Client_BeginInspection(Target);
}
void UAZ_InspectionComponent::CreateInputMapping()
{
	if(Mapping) return;
	Mapping=NewObject<UAZ_InspectionInputContext>(this);
	const FKey Keys[]={EKeys::Escape,EKeys::SpaceBar,EKeys::R,EKeys::F,EKeys::E};
	const FKey Pads[]={EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_FaceButton_Top,EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Left};
	for(int32 I=0;I<5;++I)
	{
		auto* Action=NewObject<UInputAction>(this); Action->ValueType=EInputActionValueType::Boolean; Action->bConsumeInput=true;
		UIActions.Add(Action); Mapping->MapKey(Action,Keys[I]); Mapping->MapKey(Action,Pads[I]);
	}
}
void UAZ_InspectionComponent::Client_BeginInspection_Implementation(AActor* Target)
{
	if(!Player() || !Player()->IsLocalController() || IsInspecting() || !IsValid(Target)) return;
	auto* Actions=Target->FindComponentByClass<UAZ_ObjectActionsComponent>();
	if(!Actions || !Actions->CanInspect(Player())) { Server_EndInspection(); return; }
	Preview=GetWorld()->SpawnActor<AAZ_InspectionPreview>(FVector(0,0,-100000),FRotator::ZeroRotator);
	if(!Preview || !Preview->Initialize(Actions->FindVisual(),Actions->InitialRotation))
	{ if(Preview) Preview->Destroy(); Preview=nullptr; Server_EndInspection(); return; }
	InspectedActor=Target; bOriginalHidden=Target->IsHidden(); Target->SetActorHiddenInGame(true);
	if (Player()->PlayerCameraManager)
	{
		if (!CameraModifier) CameraModifier=Cast<UAZ_InspectionCameraModifier>(Player()->PlayerCameraManager->AddNewCameraModifier(UAZ_InspectionCameraModifier::StaticClass()));
		if (CameraModifier) CameraModifier->EnableModifier();
	}
	CreateInputMapping();
	Player()->SetInspectionInputCaptured(true);
	if(auto* Input=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player()->GetLocalPlayer()))
	{
		FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true; Options.bForceImmediately=true;
		Input->AddMappingContext(Mapping,200,Options);
	}
	Panel=CreateWidget<UAZ_InspectionPanel>(Player(),UAZ_InspectionPanel::StaticClass());
	Panel->SetInspectionOwner(this); Panel->AddToPlayerScreen(150); Panel->ActivateWidget();
	FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(Panel->TakeWidget()); Mode.SetHideCursorDuringCapture(false);
	Player()->SetInputMode(Mode); Player()->SetShowMouseCursor(true);
}
void UAZ_InspectionComponent::Close(bool bImmediate)
{
	if(bClosing) return;
	if(!bImmediate && Panel && Panel->IsActivated())
	{
		Panel->BeginCloseTransition();
		return;
	}
	TGuardValue<bool> Guard(bClosing,true);
	if(InspectedActor.IsValid()) InspectedActor->SetActorHiddenInGame(bOriginalHidden);
	InspectedActor.Reset();
	if(Panel)
	{
		// Detach callbacks before Slate destroys the old widget. A delayed callback
		// must never close a newer inspection session owned by this component.
		UAZ_InspectionPanel* ClosingPanel=Panel;
		Panel=nullptr;
		ClosingPanel->SetInspectionOwner(nullptr);
		ClosingPanel->DeactivateWidget(); ClosingPanel->RemoveFromParent();
	}
	if(Preview) { Preview->Destroy(); Preview=nullptr; }
	if(CameraModifier) CameraModifier->DisableModifier(false);
	if(Player() && Player()->IsLocalController())
	{
		if(auto* Input=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player()->GetLocalPlayer()))
		{ if(Mapping) Input->RemoveMappingContext(Mapping); }
		Player()->SetInspectionInputCaptured(false);
		if(!Player()->IsGameplayInputCaptured()) { Player()->SetInputMode(FInputModeGameOnly()); Player()->SetShowMouseCursor(false); }
		Server_EndInspection();
	}
}
void UAZ_InspectionComponent::Server_EndInspection_Implementation() { ServerTarget.Reset(); }
void UAZ_InspectionComponent::RequestTake() { if(GetActions() && GetActions()->Supports(EAZ_ObjectAction::Take)) Server_TakeInspectedItem(); }
void UAZ_InspectionComponent::Server_TakeInspectedItem_Implementation()
{
	AActor* Target=ServerTarget.Get();
	const auto* Actions=Target?Target->FindComponentByClass<UAZ_ObjectActionsComponent>():nullptr;
	auto* Inventory=Player()?Player()->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>():nullptr;
	auto* Item=Target?Target->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>():nullptr;
	const bool bTaken=Actions && Actions->Supports(EAZ_ObjectAction::Take) && Actions->CanInspect(Player()) && Inventory && Item && Inventory->TryPickupItem(Item);
	if(bTaken) ServerTarget.Reset();
	Client_InspectionTakeResult(bTaken);
}
void UAZ_InspectionComponent::Client_InspectionTakeResult_Implementation(bool bTaken)
{
	if(bTaken) Close();
	else if(Panel) Panel->ShowError(LOCTEXT("PickupFailed","Unable to take this item. Check inventory space."));
}
void UAZ_InspectionComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
	Super::TickComponent(Delta,Type,Function);
	if(IsInspecting() && !Panel->IsClosingTransition() && (!GetActions() || !GetActions()->CanInspect(Player()))) Close(true);
}
void UAZ_InspectionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Close(true);
	if(CameraModifier && Player() && Player()->PlayerCameraManager) Player()->PlayerCameraManager->RemoveCameraModifier(CameraModifier);
	CameraModifier=nullptr;
	Super::EndPlay(Reason);
}

UAZ_InspectionCameraModifier::UAZ_InspectionCameraModifier()
{
	AlphaInTime=.22f; AlphaOutTime=.18f; Priority=200;
}
bool UAZ_InspectionCameraModifier::ModifyCamera(float DeltaTime,FMinimalViewInfo& View)
{
	Super::ModifyCamera(DeltaTime,View);
	View.FOV *= FMath::Lerp(1.f,.82f,Alpha);
	return false;
}


FBox2D AAZ_InspectionPreview::GetViewBounds() const
{
	FBox2D Result(ForceInit);
	const FBox Bounds=Visual->Bounds.GetBox();
	const FTransform Camera=Capture->GetComponentTransform();
	const float HalfTan=FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
	for(int32 I=0; I<8; ++I)
	{
		const FVector Corner((I&1)?Bounds.Max.X:Bounds.Min.X,(I&2)?Bounds.Max.Y:Bounds.Min.Y,(I&4)?Bounds.Max.Z:Bounds.Min.Z);
		const FVector P=Camera.InverseTransformPosition(Corner);
		if(P.X<=1.f) continue;
		Result+=FVector2D(FMath::Clamp(.5+P.Y/(2*P.X*HalfTan),0.,1.),FMath::Clamp(.5-P.Z/(2*P.X*HalfTan),0.,1.));
	}
	return Result;
}

void UAZ_InspectionPanel::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
	Super::NativeTick(Geometry,DeltaSeconds);
	if(EnvironmentBlur && OwnerComponent)
	{
		const float Duration=FMath::Max(0.f,OwnerComponent->BlurTransitionSeconds);
		BlurBlend=Duration<=0.f ? (bExiting?0.f:1.f) : FMath::FInterpConstantTo(BlurBlend,bExiting?0.f:1.f,DeltaSeconds,1.f/Duration);
		const float Smooth=BlurBlend*BlurBlend*(3.f-2.f*BlurBlend);
		EnvironmentBlur->SetBlurStrength(FMath::Clamp(OwnerComponent->EnvironmentBlurStrength,0.f,30.f)*Smooth);
		if(bExiting)
		{
			if(Picture) Picture->SetRenderOpacity(Smooth);
			if(Controls) Controls->SetRenderOpacity(Smooth);
			if(ReadingCard) ReadingCard->SetRenderOpacity(Smooth);
			if(Title) Title->SetRenderOpacity(Smooth);
			if(Description) Description->SetRenderOpacity(Smooth);
			if(BlurBlend<=0.f) { OwnerComponent->Close(true); return; }
		}
	}
	if(!Controls || !ControlsSlot) return;
	const FGeometry& ParentGeometry=Controls->GetParent()->GetCachedGeometry();
	const FVector2D ParentSize=ParentGeometry.GetLocalSize();
	if(ParentSize.X<120 || ParentSize.Y<120) return;
	// Keep the controls at the bottom, independent of object rotation, zoom and reading mode.
	const double Width=FMath::Max(100.,ParentSize.X-132.);
	ControlsWidth->SetWidthOverride(Width);
	ControlsSlot->SetPosition(FVector2D(0,-38));
	const auto* Input=UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad=Input && Input->GetCurrentInputType()==ECommonInputType::Gamepad;
	if(RotateHintKey) RotateHintKey->SetText(bGamepad?LOCTEXT("RightStickKey","RS"):LOCTEXT("MouseDragKey","LMB"));
	if(ZoomHintKey)
	{
		const FString PadName=Input?Input->GetCurrentGamepadName().ToString():FString();
		const bool bSony=PadName.Contains(TEXT("Dual")) || PadName.Contains(TEXT("PlayStation")) || PadName.Contains(TEXT("PS4")) || PadName.Contains(TEXT("PS5"));
		ZoomHintKey->SetText(bGamepad?(bSony?LOCTEXT("SonyTriggersKey","L2 / R2"):LOCTEXT("TriggersKey","LT / RT")):LOCTEXT("MouseWheelKey","Wheel"));
	}
}

void UAZ_InspectionPanel::BeginCloseTransition()
{
	bExiting=true;
	bDragging=false;
}

#undef LOCTEXT_NAMESPACE
