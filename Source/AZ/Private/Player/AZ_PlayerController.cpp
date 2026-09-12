// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"
#include "AZ/AZ.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"
#include "UI/AZ_PlayerUIComponent.h"
#include "UI/AZ_QuickSelectComponent.h"
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
}

void AAZ_PlayerController::BeginPlay()
{
	Super::BeginPlay();

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
	if (bQuickSelectMouseReleasePending && FSlateApplication::IsInitialized())
	{
		const TSet<FKey>& Pressed = FSlateApplication::Get().GetPressedMouseButtons();
		if (!Pressed.Contains(EKeys::RightMouseButton) && !Pressed.Contains(EKeys::LeftMouseButton)
			&& !Pressed.Contains(EKeys::MiddleMouseButton))
		{
			bQuickSelectMouseReleasePending = false;
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
	ClearFirearmRecoil();
	RecentRecoilShotIds.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAZ_PlayerController::ToggleQuickSelect()
{
	if (QuickSelect) QuickSelect->Toggle();
}

void AAZ_PlayerController::ToggleInventoryMenu()
{
	if (!InventoryComponent.IsValid())
		return;
	
	InventoryComponent->ToggleInventoryMenu();
}

void AAZ_PlayerController::ToggleCommonUI_InventoryMenu()
{
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
		HUDWidget->ShowPickupMessage(PickupMessage);
	}
	else
	{
		HUDWidget->HidePickupMessage();
	}
}

void AAZ_PlayerController::PrimaryInteract()
{
	if (IsInventoryInputCaptured() || !CanUseInventoryInteraction() || !CommonUI_InventoryComponent.IsValid()) return;
	RefreshPickupTarget();
	if (!ActivePickupActor.IsValid()) return;
	if (UAZ_Inv_CommonUI_ItemComponent* ItemComponent = ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>())
	{
		CommonUI_InventoryComponent->TryAddItem(ItemComponent);
	}
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
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_PrimaryAttack
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_Reload
		|| InputTag == FAZ_GameplayTags::Get().Input_Action_Crouch) return;
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		if (IsFreshPressFireInput(Asc, InputTag)) return;
		Asc->AbilityInputTagHeld(InputTag);
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagHeld: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::CreateHUDWidget()
{
	if (!IsLocalController())
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
	if (const UAZ_Inv_CommonUI_ItemComponent* Item = NewActor ? NewActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>() : nullptr)
	{
		NextMessage = Item->GetPickupMessage();
		if (NextMessage.IsEmpty()) NextMessage = TEXT("Press E to pick up");
	}
	const bool bActorChanged = ActivePickupActor.Get() != NewActor;
	// Replication can finish while the player keeps looking at the same pickup.
	// Refresh changed ammunition text without toggling an unchanged prompt each tick.
	if (!bActorChanged && PickupMessage == NextMessage) return;
	if (bActorChanged)
	{
		LastActivePickupActor = ActivePickupActor;
		ActivePickupActor = NewActor;
	}
	PickupMessage = MoveTemp(NextMessage);
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
	SetActivePickUpActor(Closest);
}
