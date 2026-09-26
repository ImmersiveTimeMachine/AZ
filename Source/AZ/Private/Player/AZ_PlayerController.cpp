// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerController.h"

#include "GameFramework/InputDeviceSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "UObject/StrongObjectPtr.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"
#include "AbilitySystem/Abilities/AZ_GA_Throw.h"
#include "Throwables/AZ_ThrowableHandComponent.h"
#include "AZ/AZ.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"
#include "UI/AZ_PlayerUIComponent.h"
#include "UI/AZ_QuickSelectComponent.h"
#include "UI/AZ_QuestMapComponent.h"
#include "UI/AZ_MenuRoutesComponent.h"
#include "Game/AZ_CampaignSaveCoordinator.h"
#include "Game/AZ_CampaignCheckpoint.h"
#include "Quests/AZ_QuestWorldActor.h"
#include "Components/SphereComponent.h"
#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "AbilitySystemInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "Input/AZ_EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "InventoryOld/Components/AZ_Inv_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Items/AZ_Inv_ItemComponent.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InputAction.h"
#include "AZ_GameplayTags.h"
#include "Input/AZ_InputConfig.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	bool IsImmediateQuestWorldTarget(const AActor* Target)
	{
		return IsValid(Target) && (Target->IsA<AAZ_QuestWorldActor>() || Target->IsA<AAZ_CampaignCheckpoint>());
	}

	bool IsFreshPressFireInput(const UAZ_AbilitySystemComponent* ASC, const FGameplayTag InputTag)
	{
		if (!ASC) return false;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UAZ_GA_FirearmFire>()
				&& Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) return true;
		}
		return false;
	}

	double ConsumeRecoilCounterSteering(double Offset, double Input)
	{
		if (Offset * Input >= 0.0) return Offset;
		return FMath::Sign(Offset) * FMath::Max(0.0, FMath::Abs(Offset) - FMath::Abs(Input));
	}

	double AppliedRecoilPortion(double ActualDelta, double RequestedDelta)
	{
		// Other camera processing must not manufacture return debt or reverse its sign.
		return RequestedDelta >= 0.0 ? FMath::Clamp(ActualDelta, 0.0, RequestedDelta)
			: FMath::Clamp(ActualDelta, RequestedDelta, 0.0);
	}
}


AAZ_PlayerController::AAZ_PlayerController()
{
	// Code-owned cross-pawn quick-bar. Configure its Slots on BP_AZ_PlayerController.
	QuickBar = CreateDefaultSubobject<UAZ_QuickBarComponent>(TEXT("QuickBar"));
	PlayerUI = CreateDefaultSubobject<UAZ_PlayerUIComponent>(TEXT("PlayerUI"));
	QuickSelect = CreateDefaultSubobject<UAZ_QuickSelectComponent>(TEXT("QuickSelect"));
	MenuRoutes = CreateDefaultSubobject<UAZ_MenuRoutesComponent>(TEXT("MenuRoutes"));
	ThrowableHand = CreateDefaultSubobject<UAZ_ThrowableHandComponent>(TEXT("ThrowableHand"));
	QuestMap = CreateDefaultSubobject<UAZ_QuestMapComponent>(TEXT("QuestMap"));
	CampaignSave = CreateDefaultSubobject<UAZ_CampaignSaveCoordinator>(TEXT("CampaignSave"));
}

void AAZ_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (bFrontEndController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (PauseMenuMappingContext) Subsystem->AddMappingContext(PauseMenuMappingContext, 3);
		}
		if (MenuRoutes)
		{
			MenuRoutes->MenuWidgetClass = MenuRoutesWidgetClass;
			MenuRoutes->bShowTitleOnStartup = true;
			MenuRoutes->InitializeForLocalPlayer();
		}
		return;
	}

	InventoryComponent = FindComponentByClass<UAZ_Inv_InventoryComponent>();
	CommonUI_InventoryComponent = FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (CommonUI_InventoryComponent.IsValid())
	{
		CommonUI_InventoryComponent->OnInventoryMenuToggled.AddDynamic(this, &ThisClass::HandleInventoryMenuToggled);
	}

	if (PlayerUI) PlayerUI->RefreshBindings();
	CreateHUDWidget();
	if (QuickSelect) QuickSelect->RefreshBindings();

	// Cross-pawn base layer of the IMC stack. Per-pawn IAs land on top via the
	// pawn's DefaultMappingContext at priority 2 in OnPossess.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		ensureMsgf(SharedInputMappingContext,
			TEXT("%s: SharedInputMappingContext is null. Set it in the PC BP defaults — without it, "
			     "cross-pawn input (pause, inventory, menu nav, photo mode) won't work."),
			*GetName());
		Subsystem->AddMappingContext(SharedInputMappingContext, 0);
		if (PauseMenuMappingContext) Subsystem->AddMappingContext(PauseMenuMappingContext, 3);
	}
	if (MenuRoutes)
	{
		MenuRoutes->MenuWidgetClass = MenuRoutesWidgetClass;
		MenuRoutes->bShowTitleOnStartup = bShowTitleMenuOnStartup;
		MenuRoutes->InitializeForLocalPlayer();
	}
}

UAbilitySystemComponent* AAZ_PlayerController::GetAbilitySystemComponent() const
{
	// PlayerState FIRST: the player ASC lives there and exists PAWN-LESS. Resolving through GetPawn()
	// silently dropped any press/release landing in a possession gap — a release lost mid-swap left
	// WaitInputRelease waiting forever and the next pawn spawned with a stuck Movement.Crouching tag
	// (audit P1-13). Pawn-interface fallback kept for pawn-owned-ASC setups (e.g. future vehicle ASC).
	if (const APlayerState* PS = PlayerState)
	{
		if (const IAbilitySystemInterface* PSI = Cast<IAbilitySystemInterface>(PS))
		{
			if (UAbilitySystemComponent* ASC = PSI->GetAbilitySystemComponent())
			{
				return ASC;
			}
		}
	}
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetPawn()))
	{
		return ASI->GetAbilitySystemComponent();
	}
	return nullptr;
}

void AAZ_PlayerController::OnRep_PlayerState()
{
	ClearFirearmRecoil();
	Super::OnRep_PlayerState();
	if (PlayerUI) PlayerUI->RefreshBindings();
	if (QuickSelect) QuickSelect->RefreshBindings();
}

void AAZ_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	const auto AZ_InputComponent = CastChecked<UAZ_EnhancedInputComponent>(InputComponent);
	if (bFrontEndController)
	{
		if (PauseMenuAction) AZ_InputComponent->BindAction(PauseMenuAction, ETriggerEvent::Started, this, &ThisClass::HandlePauseMenuAction);
		return;
	}

	// Per-pawn movement (Move/Look/Jump/Sprint/etc.) lives on the pawn's
	// SetupPlayerInputComponent. PC owns only the pawn-agnostic surface:
	// GAS ability input, menu/HUD shortcuts.
	checkf(InputConfig, TEXT("InputConfig is null in AAZ_PlayerController::SetupInputComponent"));
	AZ_InputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
	if (OpenInventoryAction)
	{
		AZ_InputComponent->BindAction(OpenInventoryAction, ETriggerEvent::Started, this, &ThisClass::ToggleCommonUI_InventoryMenu);
	}
	if (ChangeFireModeAction)
	{
		AZ_InputComponent->BindAction(ChangeFireModeAction, ETriggerEvent::Started, this, &ThisClass::OnChangeFireModeInput);
	}
	if (QuickSelectToggleAction)
	{
		AZ_InputComponent->BindAction(QuickSelectToggleAction, ETriggerEvent::Started, this, &ThisClass::ToggleQuickSelect);
	}
	if (PauseMenuAction) AZ_InputComponent->BindAction(PauseMenuAction, ETriggerEvent::Started, this, &ThisClass::HandlePauseMenuAction);

	// Native (non-ability) quick-slot/equip inputs -> QuickBar->Select. Same component,
	// different lane than BindAbilityActions (which only ACTIVATES GAS abilities).
	for (int32 SlotIdx = 0; SlotIdx < WeaponSlotActions.Num(); ++SlotIdx)
	{
		if (WeaponSlotActions[SlotIdx])
		{
			AZ_InputComponent->BindAction(WeaponSlotActions[SlotIdx], ETriggerEvent::Started, this, &ThisClass::OnQuickSlotInput);
		}
	}
}

void AAZ_PlayerController::OnQuickSlotInput(const FInputActionInstance& Instance)
{
	if (!QuickBar || IsInventoryInputCaptured())
	{
		return;
	}
	// Map the firing action back to its slot index (array index = slot), then select it.
	const int32 SlotIndex = WeaponSlotActions.IndexOfByKey(Instance.GetSourceAction());
	if (SlotIndex != INDEX_NONE)
	{
		// Mouse and number keys (including Fists) have identical activation semantics.
		if (QuickSelect) QuickSelect->ActivateSlot(SlotIndex);
	}
}

void AAZ_PlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RecoilItemId.IsValid() && (RecoilPawn.Get() != GetPawn()
		|| !CanApplyFirearmRecoil(RecoilWeapon.Get(), RecoilItemId, RecoilEquipmentGeneration)))
	{
		ClearFirearmRecoil();
	}
	if ((bQuickSelectMouseReleasePending || bMenuMouseReleasePending) && FSlateApplication::IsInitialized())
	{
		const TSet<FKey>& Pressed = FSlateApplication::Get().GetPressedMouseButtons();
		if (!Pressed.Contains(EKeys::RightMouseButton) && !Pressed.Contains(EKeys::LeftMouseButton)
			&& !Pressed.Contains(EKeys::MiddleMouseButton))
		{
			bQuickSelectMouseReleasePending = false;
			bMenuMouseReleasePending = false;
			if (PlayerUI) PlayerUI->NotifyInputCaptureChanged();
		}
	}
	if (QuickSelect) QuickSelect->CheckPause();
	if (IsLocalController()) RefreshPickupTarget();
}

bool AAZ_PlayerController::CanApplyFirearmRecoil(const AAZ_Weapon* Weapon, const FGuid& WeaponItemId,
	uint32 EquipmentGeneration) const
{
	if (!IsLocalPlayerController() || !IsValid(Weapon) || !IsValid(GetPawn())
		|| GetViewTarget() != GetPawn() || IsLookInputIgnored() || IsInventoryInputCaptured()
		|| Weapon->GetOwner() != GetPawn() || !WeaponItemId.IsValid()) return false;
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	if (!Equipment || Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| !Equipment->IsActiveWeaponSource(Weapon)) return false;
	const UAZ_Inv_CommonUI_InventoryItem* Item = Equipment->GetActiveItem();
	if (!IsValid(Item) || Item->GetInstanceId() != WeaponItemId) return false;
	const auto* Definition = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	if (!Definition || !Definition->Recoil.bEnabled || !Definition->Recoil.bCameraKickEnabled) return false;
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return false;
	const FAZ_GameplayTags& RecoilTags = FAZ_GameplayTags::Get();
	// IsActiveWeaponSource also rejects death/dying, grab, stagger and struck-pair states.
	return !ASC->HasMatchingGameplayTag(RecoilTags.Ability_State_Reloading)
		&& Equipment->IsFirearmRaised();
}

void AAZ_PlayerController::ClearFirearmRecoil()
{
	RecoilWeapon.Reset();
	RecoilPawn.Reset();
	RecoilItemId.Invalidate();
	RecoilEquipmentGeneration = 0;
	PendingRecoil = FVector2D::ZeroVector;
	RecoilReturnOffset = FVector2D::ZeroVector;
	RecoilRecoveryStartTime = 0.0;
}

void AAZ_PlayerController::Client_ApplyFirearmRecoil_Implementation(AAZ_Weapon* Weapon, FGuid WeaponItemId,
	uint32 EquipmentGeneration, FGuid ShotId, double AcceptedShotServerTime, int32 AcceptedPreparationKey,
	const FAZ_FirearmRecoilSettings& Settings)
{
	// This reliable accepted-shot receipt also refreshes the owner's Ready lifetime,
	// including weapons with camera kick disabled. The equipment API validates source
	// identity/generation and deduplicates ShotId before changing presentation state.
	auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	if (!Equipment || !Equipment->RefreshFirearmReadyAfterShot(
		Weapon, WeaponItemId, EquipmentGeneration, ShotId, AcceptedShotServerTime, AcceptedPreparationKey)) return;
	if (!ShotId.IsValid() || RecentRecoilShotIds.Contains(ShotId)) return;
	// Remember even rejected receipts: a delayed duplicate cannot restart recoil after aim/UI cleanup.
	if (RecentRecoilShotIds.Num() >= 64) RecentRecoilShotIds.RemoveAt(0);
	RecentRecoilShotIds.Add(ShotId);
	if (!Settings.bEnabled || !Settings.bCameraKickEnabled
		|| !CanApplyFirearmRecoil(Weapon, WeaponItemId, EquipmentGeneration)) return;
	const float Values[] = {Settings.CameraPitchKickDegrees, Settings.CameraYawKickRadiusDegrees,
		Settings.MaxCameraPitchDegrees, Settings.MaxCameraYawDegrees, Settings.CameraRecoveryDelaySeconds,
		Settings.CameraRecoverySpeedDegreesPerSecond};
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.f) return;
	}
	UWorld* World = GetWorld();
	if (!World) return;
	if (RecoilWeapon.Get() != Weapon || RecoilPawn.Get() != GetPawn() || RecoilItemId != WeaponItemId
		|| RecoilEquipmentGeneration != EquipmentGeneration)
	{
		ClearFirearmRecoil();
	}
	RecoilWeapon = Weapon;
	RecoilPawn = GetPawn();
	RecoilItemId = WeaponItemId;
	RecoilEquipmentGeneration = EquipmentGeneration;
	RecoilSettings = Settings;
	// Bound local camera state even when an authored value is far outside a useful aiming range.
	RecoilSettings.MaxCameraPitchDegrees = FMath::Min(Settings.MaxCameraPitchDegrees, 89.f);
	RecoilSettings.MaxCameraYawDegrees = FMath::Min(Settings.MaxCameraYawDegrees, 89.f);
	RecoilSettings.CameraRecoverySpeedDegreesPerSecond = FMath::Clamp(Settings.CameraRecoverySpeedDegreesPerSecond, 0.01f, 360.f);
	FRandomStream Random(GetTypeHash(ShotId));
	const double PitchKick = FMath::Min(Settings.CameraPitchKickDegrees, RecoilSettings.MaxCameraPitchDegrees);
	const float YawRadius = FMath::Min(Settings.CameraYawKickRadiusDegrees, RecoilSettings.MaxCameraYawDegrees);
	PendingRecoil.X = FMath::Clamp(RecoilReturnOffset.X + PendingRecoil.X + PitchKick,
		0.0, static_cast<double>(RecoilSettings.MaxCameraPitchDegrees)) - RecoilReturnOffset.X;
	PendingRecoil.Y = FMath::Clamp(RecoilReturnOffset.Y + PendingRecoil.Y + Random.FRandRange(-YawRadius, YawRadius),
		-static_cast<double>(RecoilSettings.MaxCameraYawDegrees), static_cast<double>(RecoilSettings.MaxCameraYawDegrees))
		- RecoilReturnOffset.Y;
	RecoilRecoveryStartTime = World->GetTimeSeconds() + FMath::Min(Settings.CameraRecoveryDelaySeconds, 60.f);
}

void AAZ_PlayerController::UpdateRotation(float DeltaTime)
{
	if (!RecoilItemId.IsValid())
	{
		Super::UpdateRotation(DeltaTime);
		return;
	}
	if (RecoilPawn.Get() != GetPawn() || !CanApplyFirearmRecoil(RecoilWeapon.Get(), RecoilItemId, RecoilEquipmentGeneration)
		|| !PlayerCameraManager || !GetWorld() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f
		|| RotationInput.ContainsNaN() || GetControlRotation().ContainsNaN())
	{
		ClearFirearmRecoil();
		Super::UpdateRotation(DeltaTime);
		return;
	}
	const FRotator StartRotation = GetControlRotation();
	auto LimitView = [this](FRotator& Rotation)
	{
		PlayerCameraManager->LimitViewPitch(Rotation, PlayerCameraManager->ViewPitchMin, PlayerCameraManager->ViewPitchMax);
		PlayerCameraManager->LimitViewYaw(Rotation, PlayerCameraManager->ViewYawMin, PlayerCameraManager->ViewYawMax);
		PlayerCameraManager->LimitViewRoll(Rotation, PlayerCameraManager->ViewRollMin, PlayerCameraManager->ViewRollMax);
	};
	FRotator PlayerOnlyRotation = StartRotation + RotationInput;
	LimitView(PlayerOnlyRotation);
	const FVector2D EffectiveInput(FMath::FindDeltaAngleDegrees(StartRotation.Pitch, PlayerOnlyRotation.Pitch),
		FMath::FindDeltaAngleDegrees(StartRotation.Yaw, PlayerOnlyRotation.Yaw));
	// Mouse movement is still applied exactly once below. Its opposing portion consumes return debt.
	// Example: +1 degree kick followed by -1 degree mouse input leaves no later -1 degree recovery.
	FVector2D RemainingOffset(ConsumeRecoilCounterSteering(RecoilReturnOffset.X, EffectiveInput.X),
		ConsumeRecoilCounterSteering(RecoilReturnOffset.Y, EffectiveInput.Y));
	const FVector2D UnusedCounterInput = EffectiveInput - (RemainingOffset - RecoilReturnOffset);
	// Input clipped at a view limit can replace existing kick with the player's own aim intent.
	// Consume that debt before computing a recoil delta; this bookkeeping must not move the camera.
	auto ConsumeClippedInput = [](double Offset, double Input, double Effective)
	{
		const double NormalInput = FRotator::NormalizeAxis(Input);
		const double Overflow = FRotator::NormalizeAxis(NormalInput - Effective);
		if (NormalInput * Overflow <= 0.0) return Offset;
		return ConsumeRecoilCounterSteering(Offset, -Overflow);
	};
	RemainingOffset.X = ConsumeClippedInput(RemainingOffset.X, RotationInput.Pitch, EffectiveInput.X);
	const double YawRange = FMath::Abs(static_cast<double>(PlayerCameraManager->ViewYawMax) - PlayerCameraManager->ViewYawMin);
	if (FMath::IsFinite(YawRange) && YawRange < 359.99)
	{
		// The default 0..359.999 yaw is unrestricted; crossing its wrap is not clipped input.
		RemainingOffset.Y = ConsumeClippedInput(RemainingOffset.Y, RotationInput.Yaw, EffectiveInput.Y);
	}
	FVector2D DesiredOffset = RemainingOffset;
	const double RecoverySeconds = FMath::Clamp(GetWorld()->GetTimeSeconds() - RecoilRecoveryStartTime,
		0.0, static_cast<double>(DeltaTime));
	const double RecoveryStep = RecoverySeconds * RecoilSettings.CameraRecoverySpeedDegreesPerSecond;
	DesiredOffset.X = FMath::Sign(DesiredOffset.X) * FMath::Max(0.0, FMath::Abs(DesiredOffset.X) - RecoveryStep);
	DesiredOffset.Y = FMath::Sign(DesiredOffset.Y) * FMath::Max(0.0, FMath::Abs(DesiredOffset.Y) - RecoveryStep);
	DesiredOffset += PendingRecoil;
	PendingRecoil = FVector2D::ZeroVector;
	DesiredOffset.X = FMath::Clamp(DesiredOffset.X, 0.0, static_cast<double>(RecoilSettings.MaxCameraPitchDegrees));
	DesiredOffset.Y = FMath::Clamp(DesiredOffset.Y, -static_cast<double>(RecoilSettings.MaxCameraYawDegrees),
		static_cast<double>(RecoilSettings.MaxCameraYawDegrees));
	const FVector2D RecoilDelta = DesiredOffset - RemainingOffset;
	FRotator DesiredRotation = PlayerOnlyRotation + FRotator(RecoilDelta.X, RecoilDelta.Y, 0.0);
	LimitView(DesiredRotation);
	const FVector2D ClippedRecoilDelta(FMath::FindDeltaAngleDegrees(PlayerOnlyRotation.Pitch, DesiredRotation.Pitch),
		FMath::FindDeltaAngleDegrees(PlayerOnlyRotation.Yaw, DesiredRotation.Yaw));
	FRotator CombinedInput = RotationInput;
	CombinedInput.Pitch += RecoilDelta.X;
	CombinedInput.Yaw += RecoilDelta.Y;
	const FGuid ExpectedItemId = RecoilItemId;
	const uint32 ExpectedGeneration = RecoilEquipmentGeneration;
	{
		// Direct degrees avoid reapplying look sensitivity/inversion. Super remains the only rotation writer.
		TGuardValue<FRotator> InputGuard(RotationInput, CombinedInput);
		Super::UpdateRotation(DeltaTime);
	}
	const FRotator ActualRotation = GetControlRotation();
	if (RecoilItemId != ExpectedItemId || RecoilEquipmentGeneration != ExpectedGeneration) return;
	if (ActualRotation.ContainsNaN())
	{
		ClearFirearmRecoil();
		return;
	}
	RecoilReturnOffset.X = RemainingOffset.X + AppliedRecoilPortion(
		FMath::FindDeltaAngleDegrees(PlayerOnlyRotation.Pitch, ActualRotation.Pitch), ClippedRecoilDelta.X);
	RecoilReturnOffset.Y = RemainingOffset.Y + AppliedRecoilPortion(
		FMath::FindDeltaAngleDegrees(PlayerOnlyRotation.Yaw, ActualRotation.Yaw), ClippedRecoilDelta.Y);
	// Counter-steering left over after cancelling older kick can also cancel a kick arriving this frame.
	RecoilReturnOffset.X = ConsumeRecoilCounterSteering(RecoilReturnOffset.X, UnusedCounterInput.X);
	RecoilReturnOffset.Y = ConsumeRecoilCounterSteering(RecoilReturnOffset.Y, UnusedCounterInput.Y);
	// At the pitch limit a rejected kick adds zero debt, so moving away cannot reveal a hidden return.
	if (RecoilReturnOffset.IsNearlyZero(UE_KINDA_SMALL_NUMBER)) ClearFirearmRecoil();
}

void AAZ_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (PauseMenuMappingContext) Subsystem->RemoveMappingContext(PauseMenuMappingContext);
		if (SharedInputMappingContext) Subsystem->RemoveMappingContext(SharedInputMappingContext);
	}
	ClearFirearmRecoil();
	RecentRecoilShotIds.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAZ_PlayerController::ToggleQuickSelect()
{
	if (bMenuRouteInputCaptured) return;
	if (QuickSelect) QuickSelect->Toggle();
}

void AAZ_PlayerController::ToggleInventoryMenu()
{
	if (bMenuRouteInputCaptured) return;
	if (!InventoryComponent.IsValid())
		return;
	
	InventoryComponent->ToggleInventoryMenu();
}

void AAZ_PlayerController::ToggleCommonUI_InventoryMenu()
{
	if (bMenuRouteInputCaptured) return;
	if (!CommonUI_InventoryComponent.IsValid())
		return;
	if (!bInventoryInputCaptured && !CanUseInventoryInteraction()) return;
	
	CommonUI_InventoryComponent->ToggleInventoryMenu();
}

// ============================================================
// Possession + IMC push/pop
// ============================================================
//
// Two-sided dispatch — UE5 possession callbacks are asymmetric:
//   server: OnPossess(P) on possess, OnUnPossess() on unpossess
//   client: AcknowledgePossession(P) on possess (and on unpossess with P=nullptr)
// Without overriding AcknowledgePossession the client never runs the IMC push,
// so remote clients have no per-pawn IAs (Move/Look/etc) active — input dies
// silently on the client side. Listen-host doesn't show this because the host is
// both server AND client in the same instance, so OnPossess covers both roles.

void AAZ_PlayerController::OnPossess(APawn* aPawn)
{
	ClearFirearmRecoil();
	if (QuickSelect) QuickSelect->Close();
	Super::OnPossess(aPawn);

	// Server-side init (equipment wiring is authoritative).
	if (auto* EquipMgr = aPawn ? aPawn->FindComponentByClass<UAZ_EquipmentManagerComponent>() : nullptr)
	{
		EquipMgr->OnShowPickupPrompt.AddDynamic(this, &AAZ_PlayerController::HandlePickupPromptToggled);
	}

	// Push on listen-host (where this PC is also local-controller). Skipped on
	// dedicated server (no LocalPlayer → AddMappingContext guarded inside helper).
	PushPawnInputMappingContext(aPawn);
	if (PlayerUI) PlayerUI->RefreshBindings();
	if (QuickSelect) QuickSelect->RefreshBindings();
}

void AAZ_PlayerController::AcknowledgePossession(APawn* P)
{
	ClearFirearmRecoil();
	if (QuickSelect) QuickSelect->Close();
	Super::AcknowledgePossession(P);
	if (PlayerUI) PlayerUI->RefreshBindings();
	if (QuickSelect) QuickSelect->RefreshBindings();

	// Client-side push. P can be nullptr when the server unpossesses — treat that
	// as a pop (RemoveMappingContext with nullptr IMC is a no-op via the guard).
	if (P)
	{
		PushPawnInputMappingContext(P);
	}
	else
	{
		RemovePawnInputMappingContext(GetPawn());
	}
}

void AAZ_PlayerController::OnUnPossess()
{
	ClearFirearmRecoil();
	if (QuickSelect) QuickSelect->Close();
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		// Toggle stance must not follow the persistent PlayerState ASC into a new pawn.
		const FGameplayTag CrouchInput = FAZ_GameplayTags::Get().Input_Action_Crouch;
		TArray<FGameplayAbilitySpecHandle> CrouchHandles;
		for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(CrouchInput))
				CrouchHandles.Add(Spec.Handle);
		}
		Asc->ClearWeaponInput(FGameplayTagContainer(CrouchInput));
		for (const FGameplayAbilitySpecHandle& Handle : CrouchHandles) Asc->CancelAbilityHandle(Handle);
	}
	RemovePawnInputMappingContext(GetPawn());
	Super::OnUnPossess();
	if (PlayerUI) PlayerUI->RefreshBindings();
}

void AAZ_PlayerController::PushPawnInputMappingContext(APawn* InPawn)
{
	// Pawn-owned IMC (multi-pawn design — hero today, vehicles later). The cast
	// becomes an IAZ_InputContextProvider interface query when the second pawn
	// class lands; for one class a direct cast is fine.
	const AAZ_PawnMoverHeroCharacter* MoverPawn = Cast<AAZ_PawnMoverHeroCharacter>(InPawn);
	if (!MoverPawn) return;

	UInputMappingContext* PawnIMC = MoverPawn->GetDefaultMappingContext();
	if (!PawnIMC) return;

	// GetLocalPlayer() returns null on server-side proxies for remote clients —
	// that's the desired skip path on dedicated-server-only routes.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// Priority 2: stacks above SharedInputMappingContext (priority 0).
		// Gap at 1 reserved for future between-layer pushes (menu, cinematic).
		Subsystem->AddMappingContext(PawnIMC, 2);
	}
}

void AAZ_PlayerController::RemovePawnInputMappingContext(APawn* InPawn)
{
	const AAZ_PawnMoverHeroCharacter* MoverPawn = Cast<AAZ_PawnMoverHeroCharacter>(InPawn);
	if (!MoverPawn) return;

	UInputMappingContext* PawnIMC = MoverPawn->GetDefaultMappingContext();
	if (!PawnIMC) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(PawnIMC);
	}
}

void AAZ_PlayerController::HandlePickupPromptToggled(bool bVisible)
{
	if (!IsValid(HUDWidget)) return;
	if (bVisible && !IsInventoryInputCaptured())
	{
		HUDWidget->ShowInteractionPrompt(PickupCaption, PickupMessage);
	}
	else
	{
		HUDWidget->HidePickupMessage();
	}
}

UAZ_GA_Throw* AAZ_PlayerController::FindActiveThrow() const
{
	const UAbilitySystemComponent* Asc = GetAbilitySystemComponent();
	if (!Asc)
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || !Spec.Ability->IsA<UAZ_GA_Throw>())
		{
			continue;
		}
		UAZ_GA_Throw* Throw = Cast<UAZ_GA_Throw>(Spec.GetPrimaryInstance());
		// Active but not yet in a throw phase (the frame between activation and ActivateAbility on a
		// replicated instance) is NOT "owning the mouse": the ability must have entered a phase first.
		if (Throw && Throw->IsThrowContextActive())
		{
			return Throw;
		}
	}
	return nullptr;
}

bool AAZ_PlayerController::RouteThrowInput(const FGameplayTag& InputTag, const bool bPressed)
{
	const FAZ_GameplayTags& ThrowInputTags = FAZ_GameplayTags::Get();
	// Once the grenade is readied the player is already in the ready pose, so there is nothing left to
	// "start" — both buttons are outcomes of a state they are already in. One throws, the other puts it away.
	const bool bPrimaryRoute = InputTag == ThrowInputTags.Input_Action_PrimaryAttack;
	// RMB (and the left trigger) reach the controller through TWO actions bound to one physical button, so
	// both spellings count. Whichever side this ends up being, the duplicate edge is harmless: RequestCancel
	// is idempotent, and RequestThrow's Aiming case calls EnterWindup, which moves Phase to Windup
	// synchronously so the second edge lands in its "already under way" default.
	const bool bSecondaryRoute = InputTag == ThrowInputTags.Input_Action_SecondaryAttack
		|| InputTag == ThrowInputTags.Input_Action_Aim;
	if (!bPrimaryRoute && !bSecondaryRoute)
	{
		return false;
	}
	// ★ THE THROW SITS ON THE RIGHT-HAND BUTTON OF WHATEVER THE PLAYER IS HOLDING (user call 2026-09-22),
	// and the two devices spell "right-hand button" with opposite tags: on a pad the RIGHT TRIGGER is
	// PrimaryAttack, on a mouse the RIGHT BUTTON is SecondaryAttack/Aim. A tag-only route has to pick one
	// device and gets the other backwards — which is exactly what it did, throwing on LMB while the pad
	// threw on RT. So the active device is part of the question, and the other button cancels.
	//
	// ★ Asked of UInputDeviceSubsystem, NOT of CommonInput (corrected 2026-09-22). CommonInput's "current
	// input type" only moves when the CommonUI layer observes something, so in plain gameplay — no menu, no
	// focused widget — it sits on MouseAndKeyboard while the player is on a pad, and this read false every
	// time. That is exactly how the same mistake broke the melee hand swap. The device subsystem is fed by
	// the input stack itself and updates on real input.
	const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get();
	const FHardwareDeviceIdentifier Hardware = Devices
		? Devices->GetMostRecentlyUsedHardwareDevice(GetPlatformUserId()) : FHardwareDeviceIdentifier();
	const bool bGamepad = Hardware.IsValid()
		&& Hardware.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad;
	const bool bThrowRoute = bGamepad ? bPrimaryRoute : bSecondaryRoute;
	UAZ_GA_Throw* Throw = FindActiveThrow();
	if (!Throw)
	{
		// No grenade in hand: LMB attacks and RMB aims exactly as they always did. Entry into the throw is
		// readying the item, never a click, so there is no activation to do here.
		return false;
	}
	if (bPressed)
	{
		if (bThrowRoute)
		{
			Throw->RequestThrow();
		}
		else
		{
			Throw->RequestCancel();
		}
	}
	// The matching release is swallowed too, so the click that threw or cancelled cannot also reach a weapon.
	return true;
}

bool AAZ_PlayerController::IsGameplayInputCaptured() const
{
	return bInventoryInputCaptured || bQuickSelectInputCaptured || bMenuRouteInputCaptured || (CampaignSave && CampaignSave->IsBusy());
}

void AAZ_PlayerController::HandlePauseMenuAction()
{
	if (MenuRoutes) MenuRoutes->HandlePauseAction();
}

bool AAZ_PlayerController::CanOpenInventoryFromMenuRoute() const
{
	return bMenuRouteInputCaptured && CommonUI_InventoryComponent.IsValid() && CommonUI_InventoryComponent->GetInventoryMenu() && CanUseInventoryInteraction();
}

void AAZ_PlayerController::SuppressHeldButtonsAfterMenu()
{
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem || !Subsystem->GetPlayerInput()) return;
	FModifyContextOptions Immediate;
	Immediate.bForceImmediately = true;
	Immediate.bIgnoreAllPressedKeysUntilRelease = true;
	Immediate.bNotifyUserSettings = false;
	// Deactivation queues context removal. Drain that and any accumulated false
	// ignore-held option now, while our capture remains owned, before sampling keys.
	Subsystem->RequestRebuildControlMappings(Immediate, EInputMappingRebuildType::Rebuild);
	UEnhancedPlayerInput* Input = Subsystem->GetPlayerInput();
	if (!Input) return;
	TSet<FKey> HeldKeys;
	for (const FEnhancedActionKeyMapping& Mapping : Input->GetEnhancedActionMappingsView())
	{
		if (!Mapping.Action || Mapping.Action->ValueType != EInputActionValueType::Boolean) continue;
		const FKeyState* State = Input->GetKeyState(Mapping.Key);
		if (State && State->bDown && State->EventCounts[IE_Released].IsEmpty() && State->EventAccumulator[IE_Released].IsEmpty()) HeldKeys.Add(Mapping.Key);
	}
	if (HeldKeys.IsEmpty()) return;
	TStrongObjectPtr<UInputAction> BarrierAction(NewObject<UInputAction>(this, NAME_None, RF_Transient));
	BarrierAction->ValueType = EInputActionValueType::Boolean;
	BarrierAction->bConsumeInput = true;
	BarrierAction->bConsumesActionAndAxisMappings = false;
	TStrongObjectPtr<UAZ_MenuResumeInputContext> Barrier(NewObject<UAZ_MenuResumeInputContext>(this, NAME_None, RF_Transient));
	for (const FKey& Key : HeldKeys) Barrier->MapKey(BarrierAction.Get(), Key);
	FModifyContextOptions Deferred = Immediate;
	Deferred.bForceImmediately = false;
	// RebuildWithFlush does NOT set ignore-held flags in this engine. The temporary
	// consuming barrier first removes only these keys' underlying runtime mappings.
	// Authored contexts/priorities/registration counts stay installed throughout.
	Subsystem->AddMappingContext(Barrier.Get(), MAX_int32, Deferred);
	Subsystem->RequestRebuildControlMappings(Immediate, EInputMappingRebuildType::RebuildWithFlush);
	// The flush drained pending option-AND state. Removing only our barrier now
	// restores genuinely new Boolean mappings with ignore-held=true. Analog action
	// mappings are never marked ignored. No frame/input evaluation or key injection
	// occurs between these synchronous passes, and no transient context is retained.
	Subsystem->RemoveMappingContext(Barrier.Get(), Immediate);
}

void AAZ_PlayerController::SetMenuRouteInputCaptured(bool bOpen, bool bProtectHeldInput)
{
	if (bMenuRouteInputCaptured == bOpen) return;
	const bool bWasCaptured = IsGameplayInputCaptured();
	if (!bOpen && bProtectHeldInput) SuppressHeldButtonsAfterMenu();
	bMenuRouteInputCaptured = bOpen;
	if (!bOpen && FSlateApplication::IsInitialized())
	{
		const TSet<FKey>& Pressed = FSlateApplication::Get().GetPressedMouseButtons();
		bMenuMouseReleasePending = Pressed.Contains(EKeys::RightMouseButton) || Pressed.Contains(EKeys::LeftMouseButton) || Pressed.Contains(EKeys::MiddleMouseButton);
	}
	ApplyGameplayInputCapture(bWasCaptured);
}

void AAZ_PlayerController::PrimaryInteract()
{
	if (IsInventoryInputCaptured() || !CanUseInventoryInteraction()) return;
	RefreshPickupTarget();
	if (!ActivePickupActor.IsValid()) return;
	if (UAZ_Inv_CommonUI_ItemComponent* ItemComponent = ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>())
	{
		if (CommonUI_InventoryComponent.IsValid()) CommonUI_InventoryComponent->TryAddItem(ItemComponent);
	}
	else if (IsImmediateQuestWorldTarget(ActivePickupActor.Get()))
	{
		RequestImmediateWorldInteraction(ActivePickupActor.Get());
	}
}

bool AAZ_PlayerController::CanUseImmediateWorldInteraction() const
{
	if (IsInventoryInputCaptured() || !CanUseInventoryInteraction() || !IsValid(GetPawn())
		|| GetPawn()->GetController() != this || !IsValid(PlayerState) || PlayerState->GetPawn() != GetPawn()) return false;
	if (FindActiveThrow()) return false;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FAZ_GameplayTags& WorldInteractionTags = FAZ_GameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(WorldInteractionTags.State_Traversing) || ASC->HasMatchingGameplayTag(WorldInteractionTags.Ability_State_Throwing)
			|| ASC->HasMatchingGameplayTag(WorldInteractionTags.Ability_State_ThrowPreparing) || ASC->HasMatchingGameplayTag(WorldInteractionTags.Ability_State_Reloading)) return false;
	}
	return true;
}

bool AAZ_PlayerController::ValidateImmediateWorldTarget(AActor* Target) const
{
	if (!CanUseImmediateWorldInteraction() || !IsImmediateQuestWorldTarget(Target) || Target->IsActorBeingDestroyed()
		|| Target->GetWorld() != GetWorld()) return false;
	// Only zero-duration uses on these two owned actor classes take this route. Other interfaces retain their hold/ability path.
	UPrimitiveComponent* UseComponent = nullptr;
	if (const AAZ_QuestWorldActor* QuestActor = Cast<AAZ_QuestWorldActor>(Target)) UseComponent = QuestActor->InteractionVolume;
	else UseComponent = Target->FindComponentByClass<USphereComponent>();
	const float Duration = IAZ_Interactable::Execute_GetInteractionDuration(Target, UseComponent);
	if (!FMath::IsFinite(Duration) || Duration != 0.0f) return false;
	if (const AAZ_QuestWorldActor* QuestActor = Cast<AAZ_QuestWorldActor>(Target))
	{
		FString Error;
		return QuestActor->CanInteractForPlayer(const_cast<AAZ_PlayerController*>(this), Error);
	}
	const AAZ_CampaignCheckpoint* Checkpoint = Cast<AAZ_CampaignCheckpoint>(Target);
	const APawn* ControlledPawn = GetPawn();
	if (!Checkpoint || !Checkpoint->bEnabled || Checkpoint->CheckpointId.IsNone()
		|| !FMath::IsFinite(Checkpoint->SaveRadius) || Checkpoint->SaveRadius <= 0.0f
		|| ControlledPawn->GetActorLocation().ContainsNaN() || Target->GetActorLocation().ContainsNaN()
		|| FVector::DistSquared(ControlledPawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Checkpoint->SaveRadius)) return false;
	const FVector EyeLocation = ControlledPawn->GetPawnViewLocation();
	if (EyeLocation.ContainsNaN()) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ImmediateCheckpointUse), false, ControlledPawn);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, Target->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Target;
}

void AAZ_PlayerController::RequestImmediateWorldInteraction(AActor* Target)
{
	if ((!IsLocalController() && !HasAuthority()) || !ValidateImmediateWorldTarget(Target)
		|| LastImmediateWorldRequestFrame == GFrameCounter) return;
	LastImmediateWorldRequestFrame = GFrameCounter;
	Server_ImmediateWorldInteract(Target, FGuid::NewGuid());
}

void AAZ_PlayerController::Server_ImmediateWorldInteract_Implementation(AActor* Target, FGuid RequestId)
{
	if (!HasAuthority() || !RequestId.IsValid() || RecentWorldInteractionRequests.Contains(RequestId)) return;
	RecentWorldInteractionRequests.Add(RequestId);
	if (RecentWorldInteractionRequests.Num() > 128) RecentWorldInteractionRequests.RemoveAt(0);
	if (!ValidateImmediateWorldTarget(Target)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	// Also coalesce a legacy interface callback paired with this native input request across prediction frames.
	if (Now - LastImmediateWorldServerTime < 0.15) return;
	LastImmediateWorldServerTime = Now;
	FString Error;
	bool bCommitted = false;
	if (AAZ_QuestWorldActor* QuestActor = Cast<AAZ_QuestWorldActor>(Target))
	{
		bCommitted = QuestActor->TryInteractForPlayer(this, RequestId, Error);
	}
	else if (AAZ_CampaignCheckpoint* Checkpoint = Cast<AAZ_CampaignCheckpoint>(Target))
	{
		// Do not invoke PostInteract here: its compatibility implementation routes back through this controller.
		bCommitted = Checkpoint->SaveForPlayer(this, Error);
	}
	if (!bCommitted && !Error.IsEmpty()) UE_LOG(Log_AZ, Warning, TEXT("World interaction refused for %s: %s"), *GetNameSafe(Target), *Error);
}

void AAZ_PlayerController::OnChangeFireModeInput()
{
	if (IsInventoryInputCaptured()) return;
	if (auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()) Equipment->RequestCycleFireMode();
}

void AAZ_PlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (IsInventoryInputCaptured())
	{
		MenuSuppressedInputTags.Add(InputTag);
		return;
	}
	MenuSuppressedInputTags.Remove(InputTag); // a fresh press also clears a release consumed by CommonUI
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Interact) bImmediateInteractPressConsumed = false;
	// The throw owns the mouse while it is aiming, so this runs before every other interpretation of a
	// click: an LMB cancel must not also swing a fist, and an RMB aim must not also raise iron sights.
	if (RouteThrowInput(InputTag, /*bPressed*/ true))
	{
		return;
	}
	// EXCLUSIVE THROW AIM (user, 2026-09-17: "while aiming you should not be able to perform any other
	// actions", and Run "cancels the aim"). Both rules are enforced HERE, ahead of ordinary GAS dispatch,
	// because neither can be expressed by ability tags alone:
	//
	//   * Run/Sprint must cancel the throw BEFORE the sprint ability's blocked tags are evaluated. The throw
	//     blocks Movement.Sprinting, so waiting for Sprint to activate and cancel the throw from the other
	//     side deadlocks — sprint can never activate while the thing blocking it is what it must cancel.
	//     Same ordering as the existing firearm pre-gate a few lines below.
	//   * BlockAbilitiesWithTag only stops NEW activations. It does nothing about an ALREADY-ACTIVE
	//     ability's input, and crouch is exactly that case: its live WaitInputPress would toggle the player
	//     out of the captured stance mid-aim. The toggle has to be consumed before it reaches the task.
	if (UAZ_GA_Throw* AimingThrow = FindActiveThrow())
	{
		const FAZ_GameplayTags& ThrowTags = FAZ_GameplayTags::Get();
		if (InputTag == ThrowTags.Input_Action_Sprint || InputTag == ThrowTags.Input_Action_Run)
		{
			// Gameplay ownership is released immediately; the authored Cancel clip is cosmetic and must not
			// gate sprint eligibility (review Q4.3). Spends nothing before the physical release.
			AimingThrow->RequestCancel();
			// Fall through: the same press still becomes a normal sprint/run this frame.
		}
		else if (InputTag != ThrowTags.Input_Action_Move)
		{
			// Every other voluntary action — jump, melee, fire, reload, interact, crouch/stance toggles,
			// weapon switching — is swallowed while the aim owns the body. Consumed, never buffered, so
			// nothing fires late when the throw ends.
			//
			// This is the VOLUNTARY path only. Death, hit reactions, grabs and other forced system actions
			// do not come through player input, so they still preempt the throw exactly as before.
			ThrowSuppressedInputTags.Add(InputTag);
			return;
		}
	}
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Interact)
	{
		RefreshPickupTarget();
		if (IsImmediateQuestWorldTarget(ActivePickupActor.Get())
			&& !ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>())
		{
			bImmediateInteractPressConsumed = true;
			PrimaryInteract();
			return;
		}
	}
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		if (InputTag == FAZ_GameplayTags::Get().Input_Action_PrimaryAttack)
		{
			const auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
			if (Equipment && Equipment->IsSwitchingWeapon())
			{
				// A press during draw/holster must not enter the melee input buffer
				// and later fire the incoming weapon under the same primary tag.
				Asc->ClearWeaponInput(FGameplayTagContainer(InputTag));
				return;
			}
		}
		if (InputTag == FAZ_GameplayTags::Get().Input_Action_Sprint
			&& IsFreshPressFireInput(Asc, FAZ_GameplayTags::Get().Input_Action_PrimaryAttack)
			&& !Asc->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading))
		{
			// Sprint must cancel a queued/continuous shot before GAS checks the sprint
			// ability's blocked tags. Waiting for sprint activation would deadlock here.
			if (auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>())
			{
				Equipment->CancelFirearmReady();
				Equipment->CancelActiveAim();
			}
			Asc->ClearWeaponInput(FGameplayTagContainer(FAZ_GameplayTags::Get().Input_Action_PrimaryAttack));
			ClearFirearmRecoil();
		}
		// The next crouch press ends its waiting ability synchronously. Snapshot
		// before dispatch so that same press cannot immediately activate it again.
		const bool bCrouchInput = InputTag == FAZ_GameplayTags::Get().Input_Action_Crouch;
		bool bCrouchWasActive = false;
		if (bCrouchInput)
		{
			for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
			{
				if (Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
				{
					bCrouchWasActive = true;
					break;
				}
			}
		}
		Asc->AbilityInputTagPressed(InputTag);
		// Firearm actions own their cadence. A held frame must never become another
		// semi-auto shot, or a shot after a failed press while aim was unavailable.
		if (bCrouchInput)
		{
			if (!bCrouchWasActive) Asc->AbilityInputTagHeld(InputTag, false);
		}
		else if (InputTag == FAZ_GameplayTags::Get().Input_Action_Jump)
		{
			// ASC Pressed only forwards input to an already-active ability. Jump is excluded from
			// repeated Held callbacks, so this fresh Started edge must make its one activation attempt.
			// No buffer: a refused press must not become a delayed jump or traversal.
			Asc->AbilityInputTagHeld(InputTag, false);
		}
		else if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim
			|| InputTag == FAZ_GameplayTags::Get().Input_Action_Reload || IsFreshPressFireInput(Asc, InputTag))
		{
			const bool bActivated = Asc->AbilityInputTagHeld(InputTag, false);
			if (!bActivated && InputTag == FAZ_GameplayTags::Get().Input_Action_PrimaryAttack)
			{
				if (auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()) Equipment->RequestReloadIfEmpty();
			}
		}
		else if (InputTag == FAZ_GameplayTags::Get().Input_Action_PrimaryAttack)
			// Primary's old Pressed trigger provided one melee activation attempt.
			// Keep that pulse and its combo buffer when the action becomes hold-capable.
			Asc->AbilityInputTagHeld(InputTag);
	}
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Interact) PrimaryInteract();
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagPressed: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	MenuSuppressedInputTags.Remove(InputTag);
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Interact && bImmediateInteractPressConsumed)
	{
		bImmediateInteractPressConsumed = false;
		return;
	}
	// The release IS the throw. Routed first and consumed, so lifting RMB cannot also drop a firearm out
	// of aim on the same edge.
	if (RouteThrowInput(InputTag, /*bPressed*/ false))
	{
		return;
	}
	// The press was swallowed by an exclusive aim, so its release is swallowed too — an unpaired release
	// would otherwise reach the ASC and stop/resume an ability this press never started. Consumed once:
	// the NEXT press of the same key, after the aim ends, behaves completely normally.
	if (ThrowSuppressedInputTags.Remove(InputTag) > 0)
	{
		return;
	}
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		Asc->AbilityInputTagReleased(InputTag);
	}
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim)
	{
		const auto* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
		if (!Equipment || !Equipment->IsFirearmRaised()) ClearFirearmRecoil();
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagReleased: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (IsInventoryInputCaptured())
	{
		MenuSuppressedInputTags.Add(InputTag);
		return;
	}
	if (MenuSuppressedInputTags.Contains(InputTag)) return;
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Interact
		&& (bImmediateInteractPressConsumed || (IsImmediateQuestWorldTarget(ActivePickupActor.Get())
			&& !ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>()))) return;
	// Jump is a FRESH-PRESS action, and until 2026-09-15 it was not. Held frames reached the ASC, whose
	// loop re-activates any INACTIVE matching spec — and Jump ends immediately after Started or BodyBusy,
	// so its spec was inactive again the very next frame. Holding Space therefore re-asked the traversal
	// query every tick until something took, which is indistinguishable from an input buffer and is the
	// opposite of the one-press-one-decision policy. It also drowned the logs: 109 rejections against 20
	// traversals in a single session, most of them the same press re-counted.
	// Excluded HERE rather than by disabling held input globally, which other abilities rely on (a held
	// LMB re-punches when the recovery window opens). Hold-to-jump-higher is unaffected: that rides
	// AbilityInputTagReleased -> UAbilityTask_WaitInputRelease, not this path.
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_PrimaryAttack
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_Reload
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_Jump
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_Crouch) return;
	// A key already DOWN when the aim began keeps generating Held frames. Without this, the ASC's held loop
	// would re-activate the very abilities the press gate is swallowing (the known held-input retry trap),
	// so exclusivity would hold for fresh presses only. Run/Sprint is exempt: it cancels the aim, and its
	// cancellation is driven from the press edge above.
	if (FindActiveThrow())
	{
		const FAZ_GameplayTags& HeldTags = FAZ_GameplayTags::Get();
		if (InputTag != HeldTags.Input_Action_Sprint && InputTag != HeldTags.Input_Action_Run
			&& InputTag != HeldTags.Input_Action_Move)
		{
			return;
		}
	}
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		if (IsFreshPressFireInput(Asc, InputTag)) return;
		Asc->AbilityInputTagHeld(InputTag);
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagHeld: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::CreateHUDWidget()
{
	if (bFrontEndController || !IsLocalController())
		return;

	if (IsValid(HUDWidget)) return;
	HUDWidget = CreateWidget<UAZ_Inv_CommonUI_InventoryHudWidget>(this,InventoryHudWidgetClass);

	if (IsValid(HUDWidget))
	{
		HUDWidget->AddToPlayerScreen(0);
	}
	else
	{
		UE_LOG(Log_AZ, Warning, TEXT("CreateHUDWidget: Failed to create HUD Widget"));
	}
}

bool AAZ_PlayerController::CanUseInventoryInteraction() const
{
	if (!GetPawn()) return false;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FAZ_GameplayTags& GameplayTags = FAZ_GameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(GameplayTags.State_Grabbed) || ASC->HasMatchingGameplayTag(GameplayTags.Character_Dead)) return false;
		if (ASC->HasMatchingGameplayTag(GameplayTags.Ability_State_MeleeAttacking)
			&& !ASC->HasMatchingGameplayTag(GameplayTags.State_Combat_CancelWindow)) return false;
	}
	return true;
}

void AAZ_PlayerController::HandleInventoryMenuToggled(bool bOpen)
{
	if (bInventoryInputCaptured == bOpen) return;
	const bool bWasCaptured = IsGameplayInputCaptured();
	bInventoryInputCaptured = bOpen;
	if (!bOpen && FSlateApplication::IsInitialized())
	{
		// Equip may ready a throw while the menu click is still held. Consume its tail,
		// just like quick-select, so it cannot become a gameplay throw/cancel press.
		const TSet<FKey>& Pressed = FSlateApplication::Get().GetPressedMouseButtons();
		bMenuMouseReleasePending |= Pressed.Contains(EKeys::RightMouseButton)
			|| Pressed.Contains(EKeys::LeftMouseButton) || Pressed.Contains(EKeys::MiddleMouseButton);
	}
	ApplyGameplayInputCapture(bWasCaptured);
	// Inventory has acquired its capture before the selector releases its own.
	if (bOpen && QuickSelect) QuickSelect->Close();
}

void AAZ_PlayerController::SetQuickSelectInputCaptured(bool bOpen)
{
	if (bQuickSelectInputCaptured == bOpen) return;
	const bool bWasCaptured = IsGameplayInputCaptured();
	bQuickSelectInputCaptured = bOpen;
	if (!bOpen && FSlateApplication::IsInitialized())
	{
		const TSet<FKey>& Pressed = FSlateApplication::Get().GetPressedMouseButtons();
		bQuickSelectMouseReleasePending = Pressed.Contains(EKeys::RightMouseButton)
			|| Pressed.Contains(EKeys::LeftMouseButton) || Pressed.Contains(EKeys::MiddleMouseButton);
	}
	ApplyGameplayInputCapture(bWasCaptured);
}

void AAZ_PlayerController::ApplyGameplayInputCapture(bool bWasCaptured)
{
	const bool bOpen = IsGameplayInputCaptured();
	if (bOpen) ClearFirearmRecoil();
	if (!HasAuthority()) Server_SetInventoryInputCaptured(bInventoryInputCaptured, bQuickSelectInputCaptured);
	if (bOpen != bWasCaptured)
	{
		SetIgnoreMoveInput(bOpen);
		SetIgnoreLookInput(bOpen);
	}
	if (AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetPawn()))
	{
		Hero->ResetGameplayMovementIntent();
	}
	if (bOpen && !bWasCaptured)
	{
		if (UAZ_Inv_CommonUI_EquipmentComponent* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()) Equipment->CancelActiveAim();
		if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
		{
			const FAZ_GameplayTags& GameplayTags = FAZ_GameplayTags::Get();
			FGameplayTagContainer WeaponInputs;
			WeaponInputs.AddTag(GameplayTags.Input_Action_PrimaryAttack);
			WeaponInputs.AddTag(GameplayTags.Input_Action_SecondaryAttack);
			WeaponInputs.AddTag(GameplayTags.Input_Action_MeleeAttack);
			WeaponInputs.AddTag(GameplayTags.Input_Action_Aim);
			WeaponInputs.AddTag(GameplayTags.Input_Action_Reload);
			ASC->ClearWeaponInput(WeaponInputs);
			if (ASC->HasMatchingGameplayTag(GameplayTags.State_Combat_CancelWindow))
			{
				const FGameplayTagContainer MeleeTags(GameplayTags.Ability_Combat_Melee);
				ASC->CancelAbilities(&MeleeTags);
			}
		}
	}
	HandlePickupPromptToggled(!bOpen && ActivePickupActor.IsValid());
	if (PlayerUI) PlayerUI->NotifyInputCaptureChanged();
}

void AAZ_PlayerController::Server_SetInventoryInputCaptured_Implementation(bool bInventoryOpen, bool bQuickSelectOpen)
{
	const bool bWasCaptured = IsGameplayInputCaptured();
	bInventoryInputCaptured = bInventoryOpen;
	bQuickSelectInputCaptured = bQuickSelectOpen;
	if (IsGameplayInputCaptured() && !bWasCaptured)
	{
		if (UAZ_Inv_CommonUI_EquipmentComponent* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()) Equipment->CancelActiveAim();
	}
}

void AAZ_PlayerController::SetActivePickUpActor(AActor* NewActor)
{
	FString NextMessage;
	FText NextCaption;
	if (const UAZ_Inv_CommonUI_ItemComponent* Item = NewActor ? NewActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>() : nullptr)
	{
		NextMessage = Item->GetPickupMessage();
		NextCaption = Item->GetPickupCaption();
		if (NextMessage.IsEmpty()) NextMessage = TEXT("Press E to pick up");
	}
	else if (const AAZ_QuestWorldActor* QuestActor = Cast<AAZ_QuestWorldActor>(NewActor))
	{
		NextMessage = QuestActor->GetInteractionPrompt().ToString();
		NextCaption = QuestActor->GetInteractionCaption();
	}
	else if (Cast<AAZ_CampaignCheckpoint>(NewActor))
	{
		NextMessage = NSLOCTEXT("CHALK", "CheckpointUsePrompt", "Press E to save at checkpoint").ToString();
		NextCaption = NSLOCTEXT("CHALK", "CheckpointUseCaption", "Save at checkpoint");
	}
	const bool bActorChanged = ActivePickupActor.Get() != NewActor;
	// Replication can finish while the player keeps looking at the same pickup.
	// Refresh changed ammunition text without toggling an unchanged prompt each tick.
	if (!bActorChanged && PickupMessage == NextMessage && PickupCaption.EqualTo(NextCaption)) return;
	if (bActorChanged)
	{
		LastActivePickupActor = ActivePickupActor;
		ActivePickupActor = NewActor;
	}
	PickupMessage = MoveTemp(NextMessage);
	PickupCaption = MoveTemp(NextCaption);
	HandlePickupPromptToggled(IsValid(NewActor));
}

void AAZ_PlayerController::RefreshPickupTarget()
{
	if (!IsLocalController() || IsInventoryInputCaptured()) return;
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) { SetActivePickUpActor(nullptr); return; }
	TArray<AActor*> Overlapping;
	ControlledPawn->GetOverlappingActors(Overlapping);
	AActor* Closest = nullptr;
	double ClosestDistance = TNumericLimits<double>::Max();
	for (AActor* Candidate : Overlapping)
	{
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed()
			|| !Candidate->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>()) continue;
		const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			Closest = Candidate;
			ClosestDistance = Distance;
		}
	}
	// Item pickups retain their previous nearest-overlap priority. Only when none exist consider our immediate world uses.
	if (!Closest)
	{
		for (AActor* Candidate : Overlapping)
		{
			if (!ValidateImmediateWorldTarget(Candidate)) continue;
			const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
			if (Distance < ClosestDistance) { Closest = Candidate; ClosestDistance = Distance; }
		}
	}
	SetActivePickUpActor(Closest);
}
