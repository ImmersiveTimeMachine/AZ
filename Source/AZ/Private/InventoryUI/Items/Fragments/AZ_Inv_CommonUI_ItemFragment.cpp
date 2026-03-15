// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeBaseWidget.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Image.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Text.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_LabeledValue.h"
#include "Equipment/EquipActor/AZ_Inv_EquipActor.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AbilitySystemGlobals.h"

void FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	if (!MatchesWidgetTag(Composite)) return;
	Composite->Expand();
}

bool FAZ_Inv_CommonUI_InventoryItem_Fragment::MatchesWidgetTag(const UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	return Composite->GetFragmentTag().MatchesTagExact(GetFragmentTag());
}

void FAZ_Inv_CommonUI_ImageFragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UAZ_Inv_CommonUI_LeafWidget_Image* Image = Cast<UAZ_Inv_CommonUI_LeafWidget_Image>(Composite);
	if (!IsValid(Image)) return;

	Image->SetImage(Icon);
	Image->SetBoxSize(IconDimensions);
	Image->SetImageSize(IconDimensions);
}

void FAZ_Inv_CommonUI_Text_Fragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UAZ_Inv_CommonUI_LeafWidget_Text* LeafText = Cast<UAZ_Inv_CommonUI_LeafWidget_Text>(Composite);
	if (!IsValid(LeafText)) return;

	LeafText->SetText(FragmentText);
}

void FAZ_Inv_CommonUI_LabeledNumberFragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UAZ_Inv_CommonUI_LeafWidget_LabeledValue* LabeledValue = Cast<UAZ_Inv_CommonUI_LeafWidget_LabeledValue>(Composite);
	if (!IsValid(LabeledValue)) return;

	LabeledValue->SetText_Label(Text_Label, bCollapseLabel);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = MinFractionalDigits;
	Options.MaximumFractionalDigits = MaxFractionalDigits;

	LabeledValue->SetText_Value(FText::AsNumber(Value, &Options), bCollapseValue);
}

void FAZ_Inv_CommonUI_RandomizedNumberFragment::Manifest()
{
	FAZ_Inv_CommonUI_LabeledNumberFragment::Manifest();

	if (bRandomizeOnManifest)
	{
		SetValue(FMath::FRandRange(Min, Max));
	}
	bRandomizeOnManifest = false;
}

void FAZ_Inv_CommonUI_ConsumableFragment::OnConsume(APlayerController* PC)
{
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnConsume(PC);
	}
}

void FAZ_Inv_CommonUI_ConsumableFragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
	for (const auto& Modifier : ConsumeModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FAZ_Inv_CommonUI_ConsumableFragment::Manifest()
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Manifest();
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

void FAZ_Inv_CommonUI_HealthPotionFragment::OnConsume(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Health Potion consumed! Healing by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_ManaPotionFragment::OnConsume(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Blue,
		FString::Printf(TEXT("Mana Potion consumed! Mana replenished by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_StrengthModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Strength increased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_StrengthModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item unequipped. Strength decreased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_ArmorModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Item equipped. Armor increased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_ArmorModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item unequipped. Armor decreased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_DamageModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Green,
		FString::Printf(TEXT("Item equipped. Damage increased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_DamageModifier::OnUnequip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(
		-1,
		5.f,
		FColor::Red,
		FString::Printf(TEXT("Item unequipped. Damage decreased by: %f"),
			GetValue()));
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnPickup(APlayerController* PC)
{
	if (State != EEquipmentState::None) return;
	State = EEquipmentState::Carried;
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnEquip(APlayerController* PC)
{
	if (State == EEquipmentState::Equipped) return;
	State = EEquipmentState::Equipped;

	// Move actor from carry socket to equip socket
	ReattachActor(EquipSocket);

	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnEquip(PC);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnUnequip(APlayerController* PC)
{
	if (State != EEquipmentState::Equipped) return;
	State = EEquipmentState::Carried;

	// Move actor back to carry socket
	ReattachActor(CarrySocket);

	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnUnequip(PC);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnDrop()
{
	State = EEquipmentState::None;
	DestroyAttachedActor();
}

void FAZ_Inv_CommonUI_EquipmentFragment::Assimilate(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Assimilate(Composite);
	for (const auto& Modifier : EquipModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::Manifest()
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Manifest();
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

AAZ_Inv_EquipActor* FAZ_Inv_CommonUI_EquipmentFragment::SpawnAttachedActor(USkeletalMeshComponent* AttachMesh, FName Socket) const
{
	if (!IsValid(EquipActorClass) || !IsValid(AttachMesh)) return nullptr;

	AAZ_Inv_EquipActor* SpawnedActor = AttachMesh->GetWorld()->SpawnActor<AAZ_Inv_EquipActor>(EquipActorClass);
	SpawnedActor->AttachToComponent(AttachMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);

	return SpawnedActor;
}

void FAZ_Inv_CommonUI_EquipmentFragment::ReattachActor(FName NewSocket) const
{
	if (!EquippedActor.IsValid()) return;

	if (USceneComponent* Parent = EquippedActor->GetRootComponent()->GetAttachParent())
	{
		EquippedActor->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewSocket);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::DestroyAttachedActor()
{
	if (EquippedActor.IsValid())
	{
		EquippedActor->Destroy();
		EquippedActor = nullptr;
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::SetEquippedActor(AAZ_Inv_EquipActor* EquipActor)
{
	EquippedActor = EquipActor;
}

void FAZ_Inv_CommonUI_AbilityGrantFragment::OnEquip(APlayerController* PC, AActor* SourceObject)
{
	if (!IsValid(PC)) return;
	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn)) return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC) return;

	UObject* AbilitySource = SourceObject ? static_cast<UObject*>(SourceObject) : static_cast<UObject*>(Pawn);

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (AbilityClass)
		{
			const UAZ_GameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
			const int32 InputID = AbilityCDO ? static_cast<int32>(AbilityCDO->EchoAbilityInputID) : INDEX_NONE;

			FGameplayAbilitySpec Spec(AbilityClass, 1, InputID, AbilitySource);
			if (AbilityCDO && AbilityCDO->InputTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(AbilityCDO->InputTag);
			}
			FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			GrantedAbilityHandles.Add(Handle);
		}
	}
}

void FAZ_Inv_CommonUI_AbilityGrantFragment::OnUnequip(APlayerController* PC)
{
	if (!IsValid(PC)) return;
	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn)) return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC) return;

	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		ASC->ClearAbility(Handle);
	}
	GrantedAbilityHandles.Empty();
}
