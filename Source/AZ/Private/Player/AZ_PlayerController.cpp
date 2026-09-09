// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"
#include "AZ/AZ.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"
#include "UI/AZ_PlayerUIComponent.h"
#include "AbilitySystemInterface.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "Input/AZ_EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "InventoryOld/Components/AZ_Inv_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Items/AZ_Inv_ItemComponent.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InputAction.h"
#include "AZ_GameplayTags.h"
#include "Input/AZ_InputConfig.h"

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
}


AAZ_PlayerController::AAZ_PlayerController()
{
	// Code-owned cross-pawn quick-bar. Configure its Slots on BP_AZ_PlayerController.
	QuickBar = CreateDefaultSubobject<UAZ_QuickBarComponent>(TEXT("QuickBar"));
	PlayerUI = CreateDefaultSubobject<UAZ_PlayerUIComponent>(TEXT("PlayerUI"));
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
	Super::OnRep_PlayerState();
	if (PlayerUI) PlayerUI->RefreshBindings();
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

	// Native (non-ability) quick-slot/equip inputs -> QuickBar->Select. Same component,
	// different lane than BindAbilityActions (which only ACTIVATES GAS abilities).
	for (int32 SlotIdx = 0; SlotIdx < WeaponSlotActions.Num(); ++SlotIdx)
	{
		if (WeaponSlotActions[SlotIdx])
		{
			AZ_InputComponent->BindAction(WeaponSlotActions[SlotIdx], ETriggerEvent::Triggered, this, &ThisClass::OnQuickSlotInput);
		}
	}
}

void AAZ_PlayerController::OnQuickSlotInput(const FInputActionInstance& Instance)
{
	if (!QuickBar || bInventoryInputCaptured)
	{
		return;
	}
	// Map the firing action back to its slot index (array index = slot), then select it.
	const int32 SlotIndex = WeaponSlotActions.IndexOfByKey(Instance.GetSourceAction());
	if (SlotIndex != INDEX_NONE)
	{
		QuickBar->Select(SlotIndex);
	}
}

void AAZ_PlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsLocalController()) RefreshPickupTarget();
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
}

void AAZ_PlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	if (PlayerUI) PlayerUI->RefreshBindings();

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
	if (bVisible && !bInventoryInputCaptured)
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
	if (bInventoryInputCaptured || !CanUseInventoryInteraction() || !CommonUI_InventoryComponent.IsValid()) return;
	RefreshPickupTarget();
	if (!ActivePickupActor.IsValid()) return;
	if (UAZ_Inv_CommonUI_ItemComponent* ItemComponent = ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>())
	{
		CommonUI_InventoryComponent->TryAddItem(ItemComponent);
	}
}

void AAZ_PlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (bInventoryInputCaptured)
	{
		MenuSuppressedInputTags.Add(InputTag);
		return;
	}
	MenuSuppressedInputTags.Remove(InputTag); // a fresh press also clears a release consumed by CommonUI
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		Asc->AbilityInputTagPressed(InputTag);
		// Firearm actions own their cadence. A held frame must never become another
		// semi-auto shot, or a shot after a failed press while aim was unavailable.
		if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim || IsFreshPressFireInput(Asc, InputTag))
			Asc->AbilityInputTagHeld(InputTag, false);
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
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagReleased: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (bInventoryInputCaptured)
	{
		MenuSuppressedInputTags.Add(InputTag);
		return;
	}
	if (MenuSuppressedInputTags.Contains(InputTag)) return;
	if (InputTag == FAZ_GameplayTags::Get().Input_Action_Aim) return;
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
	bInventoryInputCaptured = bOpen;
	if (!HasAuthority()) Server_SetInventoryInputCaptured(bOpen);
	SetIgnoreMoveInput(bOpen);
	SetIgnoreLookInput(bOpen);
	if (AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetPawn()))
	{
		Hero->ResetGameplayMovementIntent();
	}
	if (bOpen)
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
			ASC->ClearWeaponInput(WeaponInputs);
			if (ASC->HasMatchingGameplayTag(GameplayTags.State_Combat_CancelWindow))
			{
				const FGameplayTagContainer MeleeTags(GameplayTags.Ability_Combat_Melee);
				ASC->CancelAbilities(&MeleeTags);
			}
		}
	}
	HandlePickupPromptToggled(!bOpen && ActivePickupActor.IsValid());
}

void AAZ_PlayerController::Server_SetInventoryInputCaptured_Implementation(bool bOpen)
{
	bInventoryInputCaptured = bOpen;
	if (bOpen)
	{
		if (UAZ_Inv_CommonUI_EquipmentComponent* Equipment = FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()) Equipment->CancelActiveAim();
	}
}

void AAZ_PlayerController::SetActivePickUpActor(AActor* NewActor)
{
	if (ActivePickupActor.Get() == NewActor) return;
	LastActivePickupActor = ActivePickupActor;
	ActivePickupActor = NewActor;
	if (const UAZ_Inv_CommonUI_ItemComponent* Item = NewActor ? NewActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>() : nullptr)
	{
		PickupMessage = Item->GetPickupMessage().IsEmpty() ? TEXT("Press E to pick up") : Item->GetPickupMessage();
	}
	HandlePickupPromptToggled(IsValid(NewActor));
}

void AAZ_PlayerController::RefreshPickupTarget()
{
	if (!IsLocalController() || bInventoryInputCaptured) return;
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
