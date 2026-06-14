// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AZ/AZ.h"
#include "InventoryOld/Widgets/HUD/AZ_InventoryHudWidget.h"
#include "AbilitySystemInterface.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Input/AZ_EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "InventoryOld/Components/AZ_Inv_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Items/AZ_Inv_ItemComponent.h"


void AAZ_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	InventoryComponent = FindComponentByClass<UAZ_Inv_InventoryComponent>();
	CommonUI_InventoryComponent = FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	//ensure(InventoryComponent.IsValid());

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

void AAZ_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	const auto AZ_InputComponent = CastChecked<UAZ_EnhancedInputComponent>(InputComponent);

	// Per-pawn movement (Move/Look/Jump/Sprint/etc.) lives on the pawn's
	// SetupPlayerInputComponent. PC owns only the pawn-agnostic surface:
	// GAS ability input, menu/HUD shortcuts.
	checkf(InputConfig, TEXT("InputConfig is null in AAZ_PlayerController::SetupInputComponent"));
	AZ_InputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void AAZ_PlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
}

void AAZ_PlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

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
	if (IsValid(HUDWidget) && bVisible)
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
	if (!ActivePickupActor.IsValid()) return;
	
	/*UAZ_Inv_ItemComponent* ItemComponent = ActivePickupActor->FindComponentByClass<UAZ_Inv_ItemComponent>();
	InventoryComponent->TryAddItem(ItemComponent);*/
	
	auto ItemComponent = ActivePickupActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
	CommonUI_InventoryComponent->TryAddItem(ItemComponent);
 }

void AAZ_PlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		Asc->AbilityInputTagPressed(InputTag);
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagPressed: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		Asc->AbilityInputTagReleased(InputTag);
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagReleased: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (auto* Asc = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		Asc->AbilityInputTagHeld(InputTag);
	}
	UE_LOG(Log_AZ, Verbose, TEXT("AbilityInputTagHeld: %s"), *InputTag.ToString());
}

void AAZ_PlayerController::CreateHUDWidget()
{
	if (!IsLocalController())
		return;

	HUDWidget = CreateWidget<UAZ_InventoryHudWidget>(this,InventoryHudWidgetClass);

	if (IsValid(HUDWidget))
	{
		HUDWidget->AddToViewport();
	}
	else
	{
		UE_LOG(Log_AZ, Warning, TEXT("CreateHUDWidget: Failed to create HUD Widget"));
	}
}
