// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Items/Fragments/AZ_Inv_ItemFragment.h"

#include "Inventory/EquipmentManagement/EquipActor/AZ_Inv_EquipActor.h"
#include "Inventory/Widgets/Composite/AZ_Inv_Leaf_Image.h" 
#include "Inventory/Widgets/Composite/AZ_Inv_Leaf_LabeledValue.h"
#include "Inventory/Widgets/Composite/AZ_Inv_Leaf_Text.h"

void FAZ_Inv_InventoryItemFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	if (!MatchesWidgetTag(Composite)) return;
	Composite->Expand();
}

bool FAZ_Inv_InventoryItemFragment::MatchesWidgetTag(const UAZ_Inv_CompositeBase* Composite) const
{
	return Composite->GetFragmentTag().MatchesTagExact(GetFragmentTag());
}

void FAZ_Inv_ImageFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	FAZ_Inv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UAZ_Inv_Leaf_Image* Image = Cast<UAZ_Inv_Leaf_Image>(Composite);
	if (!IsValid(Image)) return;

	Image->SetImage(Icon);
	Image->SetBoxSize(IconDimensions);
	Image->SetImageSize(IconDimensions);
}

void FAZ_Inv_TextFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	FAZ_Inv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UAZ_Inv_Leaf_Text* LeafText = Cast<UAZ_Inv_Leaf_Text>(Composite);
	if (!IsValid(LeafText)) return;

	LeafText->SetText(FragmentText);
}

void FAZ_Inv_LabeledNumberFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	FAZ_Inv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;
	
	UAZ_Inv_Leaf_LabeledValue* LabeledValue = Cast<UAZ_Inv_Leaf_LabeledValue>(Composite);
	if (!IsValid(LabeledValue)) return;

	LabeledValue->SetText_Label(Text_Label, bCollapseLabel);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = MinFractionalDigits;
	Options.MaximumFractionalDigits = MaxFractionalDigits;
	
	LabeledValue->SetText_Value(FText::AsNumber(Value, &Options), bCollapseValue);
}

void FAZ_Inv_LabeledNumberFragment::Manifest()
{
	FAZ_Inv_InventoryItemFragment::Manifest();

	if (bRandomizeOnManifest)
	{
		Value = FMath::FRandRange(Min, Max);
	}
	bRandomizeOnManifest = false;
}

void FAZ_Inv_ConsumableFragment::OnConsume(APlayerController* PC)
{
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnConsume(PC);
	}
}

void FAZ_Inv_ConsumableFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	FAZ_Inv_InventoryItemFragment::Assimilate(Composite);
	for (const auto& Modifier : ConsumeModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FAZ_Inv_ConsumableFragment::Manifest()
{
	FAZ_Inv_InventoryItemFragment::Manifest();
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

void FAZ_Inv_HealthPotionFragment::OnConsume(APlayerController* PC)
{
	// Get a stats component from the PC or the PC->GetPawn()
	// or get the Ability System Component and apply a Gameplay Effect
	// or call an interface function for Healing()

	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Health Potion consumed! Healing by: %f"),
			GetValue()));
}

void FAZ_Inv_ManaPotionFragment::OnConsume(APlayerController* PC)
{
	// Replenish mana however you wish

	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Blue,
		FString::Printf(TEXT("Mana Potion consumed! Mana replenished by: %f"),
			GetValue()));
}

void FAZ_Inv_StrengthModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Strength increased by: %f"),
			GetValue()));
}

void FAZ_Inv_StrengthModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item unequipped. Strength decreased by: %f"),
			GetValue()));
}

void FAZ_Inv_ArmorModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Item equipped. Armor increased by: %f"),
			GetValue()));
}

void FAZ_Inv_ArmorModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item unequipped. Armor decreased by: %f"),
			GetValue()));
}

void FAZ_Inv_DamageModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Item equipped. Damage increased by: %f"),
			GetValue()));
}

void FAZ_Inv_DamageModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item equipped. Damage increased by: %f"),
			GetValue()));
}

void FAZ_Inv_EquipmentFragment::OnEquip(APlayerController* PC)
{
	if (bEquipped) return;
	bEquipped = true;
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnEquip(PC);
	}
}

void FAZ_Inv_EquipmentFragment::OnUnequip(APlayerController* PC)
{
	if (!bEquipped) return;
	bEquipped = false;
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnUnequip(PC);
	}
}

void FAZ_Inv_EquipmentFragment::Assimilate(UAZ_Inv_CompositeBase* Composite) const
{
	FAZ_Inv_InventoryItemFragment::Assimilate(Composite);
	for (const auto& Modifier : EquipModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FAZ_Inv_EquipmentFragment::Manifest()
{
	FAZ_Inv_InventoryItemFragment::Manifest();
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

AAZ_Inv_EquipActor* FAZ_Inv_EquipmentFragment::SpawnAttachedActor(USkeletalMeshComponent* AttachMesh) const
{
	if (!IsValid(EquipActorClass) || !IsValid(AttachMesh)) return nullptr;

	AAZ_Inv_EquipActor* SpawnedActor = AttachMesh->GetWorld()->SpawnActor<AAZ_Inv_EquipActor>(EquipActorClass);
	SpawnedActor->AttachToComponent(AttachMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketAttachPoint);

	return SpawnedActor;
}

void FAZ_Inv_EquipmentFragment::DestroyAttachedActor() const
{
	if (EquippedActor.IsValid())
	{
		EquippedActor->Destroy();
	}
}

void FAZ_Inv_EquipmentFragment::SetEquippedActor(AAZ_Inv_EquipActor* EquipActor)
{
	EquippedActor = EquipActor;
}

