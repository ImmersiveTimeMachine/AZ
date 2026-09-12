// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/HUD/AZ_InventoryHudWidget.h"

#include "InventoryOld/Components/AZ_Inv_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Weapon/AZ_Weapon.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"


void UAZ_InventoryHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UAZ_Inv_CommonUI_InventoryComponent* CommonInventory = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer()))
	{
		CommonInventory->OnNoRoomInInventory.AddDynamic(this, &ThisClass::OnNoRoom);
	}
	else if (UAZ_Inv_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::GetInventoryComponent(GetOwningPlayer()))
	{
		InventoryComponent->OnNoRoomInInventory.AddDynamic(this, &ThisClass::OnNoRoom);
	}
}

void UAZ_InventoryHudWidget::OnNoRoom()
{
	if (!IsValid(InfoMessage)) return;

	InfoMessage->SetMessage(FText::FromString("No Room In Inventory."));
}

void UAZ_InventoryHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerController* Player = GetOwningPlayer();
	AmmoInventory = Player ? Player->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
	AmmoEquipment = Player ? Player->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
	if (AmmoInventory.IsValid()) AmmoInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshWeaponAmmo);
	if (AmmoEquipment.IsValid()) AmmoEquipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::RefreshWeaponAmmo);
	RefreshWeaponAmmo();
}

void UAZ_InventoryHudWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitFeedbackTimer);
	if (AmmoInventory.IsValid()) AmmoInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshWeaponAmmo);
	if (AmmoEquipment.IsValid()) AmmoEquipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::RefreshWeaponAmmo);
	if (HitFeedbackWeapon.IsValid()) HitFeedbackWeapon->OnFirearmHitConfirmed.RemoveDynamic(this, &ThisClass::OnFirearmHitConfirmed);
	HitFeedbackWeapon.Reset();
	AmmoInventory.Reset();
	AmmoEquipment.Reset();
	bShowWeaponAmmo = false;
	Super::NativeDestruct();
}

void UAZ_InventoryHudWidget::RefreshWeaponAmmo()
{
	const UAZ_Inv_CommonUI_InventoryItem* Item = AmmoEquipment.IsValid() ? AmmoEquipment->GetActiveItem() : nullptr;
	const auto* Definition = IsValid(Item) && Item->IsInitialized()
		? Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() : nullptr;
	bShowWeaponAmmo = AmmoInventory.IsValid() && Definition && Definition->bUsesDetachableMagazines;
	AmmoSnapshot = bShowWeaponAmmo ? AmmoInventory->GetWeaponAmmoSnapshot(Item->GetInstanceId()) : FAZ_WeaponAmmoSnapshot();
	AAZ_Weapon* Weapon = bShowWeaponAmmo ? AmmoEquipment->GetActiveWeapon() : nullptr;
	if (HitFeedbackWeapon != Weapon)
	{
		if (HitFeedbackWeapon.IsValid()) HitFeedbackWeapon->OnFirearmHitConfirmed.RemoveDynamic(this, &ThisClass::OnFirearmHitConfirmed);
		HitFeedbackWeapon = Weapon;
		HitFeedbackUntil = 0.0;
		if (Weapon) Weapon->OnFirearmHitConfirmed.AddUniqueDynamic(this, &ThisClass::OnFirearmHitConfirmed);
	}
	InvalidateLayoutAndVolatility();
}

void UAZ_InventoryHudWidget::OnFirearmHitConfirmed(const FHitResult& Hit)
{
	HitFeedbackUntil = GetWorld() ? GetWorld()->GetRealTimeSeconds() + 0.15 : 0.0;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(HitFeedbackTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			HitFeedbackUntil = 0.0;
			InvalidateLayoutAndVolatility();
		}), 0.15f, false);
	}
	InvalidateLayoutAndVolatility();
}

int32 UAZ_InventoryHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Result = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bShowWeaponAmmo) return Result;
	// A small functional readout for the firing milestone. The approved HUD will
	// consume this same inventory snapshot through its own styled widgets.
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 18);
	const FLinearColor Color = InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(0.94f, 0.91f, 0.86f);
	FString Rounds = TEXT("-- / --");
	if (AmmoSnapshot.MagazineState == EAZ_WeaponMagazineState::NoMagazine) Rounds = TEXT("NO MAG");
	else if (AmmoSnapshot.MagazineState == EAZ_WeaponMagazineState::Empty || AmmoSnapshot.MagazineState == EAZ_WeaponMagazineState::Loaded)
		Rounds = FString::Printf(TEXT("%d / %d"), AmmoSnapshot.Rounds, AmmoSnapshot.Capacity);
	FSlateDrawElement::MakeText(OutDrawElements, ++Result,
		AllottedGeometry.ToPaintGeometry(FVector2D(280.f, 28.f), FSlateLayoutTransform(FVector2D(FMath::Max(12.0, Size.X - 310.0), FMath::Max(12.0, Size.Y - 70.0)))),
		Rounds, Font, ESlateDrawEffect::None, Color);
	if (GetWorld() && GetWorld()->GetRealTimeSeconds() < HitFeedbackUntil)
	{
		FSlateDrawElement::MakeText(OutDrawElements, ++Result,
			AllottedGeometry.ToPaintGeometry(FVector2D(24.f, 24.f), FSlateLayoutTransform(Size * 0.5f - FVector2D(6.f, 12.f))),
			TEXT("x"), Font, ESlateDrawEffect::None, Color);
	}
	return Result;
}
