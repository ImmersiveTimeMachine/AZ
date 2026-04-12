// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AZ_PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AZ/AZ.h"
#include "InventoryOld/Widgets/HUD/AZ_InventoryHudWidget.h"
#include "AbilitySystemInterface.h"
#include "Character/AZ_HeroPawn.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Input/AZ_EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
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

	// get the enhanced input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// add the mapping context so we get controls
		Subsystem->AddMappingContext(AlwaysInputMappingContext, 0);
		Subsystem->AddMappingContext(InputMappingContext, 1);
	}
}

TObjectPtr<UAbilitySystemComponent> AAZ_PlayerController::GetAbilitySystemComponent() const
{
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetPawn()))
	{
		return ASI->GetAbilitySystemComponent();
	}
	return nullptr;
}

TObjectPtr<UAZ_AbilitySystemComponent> AAZ_PlayerController::GetAzAbilitySystemComponent() const
{
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetPawn()))
	{
		return Cast<UAZ_AbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	}
	return nullptr;
}

void AAZ_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	const auto AZ_InputComponent = CastChecked<UAZ_EnhancedInputComponent>(InputComponent);

	// Move/Look bound here for old ACharacter pawns — HeroPawn skips these via guard check
	AZ_InputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAZ_PlayerController::Move);
	AZ_InputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAZ_PlayerController::Look);

	// GAS ability input — works for both pawn types
	checkf(InputConfig, TEXT("InputConfig is null in AAZ_PlayerController::SetupInputComponent"));
	AZ_InputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void AAZ_PlayerController::Move(const FInputActionInstance& Value)
{
	// HeroPawn handles Move via its own Mover input pipeline
	if (GetPawn() && GetPawn()->IsA<AAZ_HeroPawn>())
	{
		return;
	}

	CharacterState = ECharacterState::CS_Walking;

	const FVector2D MovementVector{ CharacterState == ECharacterState::CS_Running ? Value.GetValue().Get<FVector2D>() * RunSpeedMultiplayer : Value.GetValue().Get<FVector2D>()};

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(RightDirection, MovementVector.X);
		ControlledPawn->AddMovementInput(ForwardDirection, MovementVector.Y);
	}
}

void AAZ_PlayerController::Look(const FInputActionValue& Value)
{
	// HeroPawn handles Look via its own Mover input pipeline
	if (GetPawn() && GetPawn()->IsA<AAZ_HeroPawn>())
	{
		return;
	}

	check(Value.GetValueType() == EInputActionValueType::Axis2D);

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddControllerYawInput(LookAxisVector.X);
		ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AAZ_PlayerController::Run(const FInputActionValue& Value)
{
	//CharacterState = ECharacterState::CS_Running;
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

void AAZ_PlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	if (auto* EquipMgr = aPawn ? aPawn->FindComponentByClass<UAZ_EquipmentManagerComponent>() : nullptr)
	{
		EquipMgr->OnShowPickupPrompt.AddDynamic(this, &AAZ_PlayerController::HandlePickupPromptToggled);
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
	if (const auto Asc = GetAzAbilitySystemComponent())
	{
		Asc->AbilityInputTagPressed(InputTag);
	}
	
	UE_LOG(Log_AZ, Warning, TEXT( "AbilityInputTagPressed: %s" ), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (const auto Asc = GetAzAbilitySystemComponent())
	{
		Asc->AbilityInputTagReleased(InputTag);
	}
	UE_LOG(Log_AZ, Warning, TEXT( "AbilityInputTagReleased: %s" ), *InputTag.ToString());
}

void AAZ_PlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (const auto Asc = GetAzAbilitySystemComponent())
	{
		Asc->AbilityInputTagHeld(InputTag);
	}
	UE_LOG(Log_AZ, Warning, TEXT( "AbilityInputTagHeld: %s" ), *InputTag.ToString());
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
