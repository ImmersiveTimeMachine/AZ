// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeBaseWidget.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Image.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Text.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_LabeledValue.h"
#include "Equipment/EquipActor/AZ_Inv_EquipActor.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AbilitySystem/AttributeSets/AZ_WeaponAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Weapon/AZ_Weapon.h"

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

// --- WeaponStateFragment ---

void FAZ_Inv_CommonUI_WeaponStateFragment::ApplyToASC(UAbilitySystemComponent* ASC) const
{
	if (!ASC) return;

	const UAZ_WeaponAttributeSet* WeaponAS = ASC->GetSet<UAZ_WeaponAttributeSet>();
	if (!WeaponAS) return;

	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetBaseDamageAttribute(), BaseDamage);
	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetFireRateAttribute(), FireRate);
	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetReloadTimeAttribute(), ReloadSpeed);
	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetSpreadModifierAttribute(), SpreadBase);

	// Clip ammo — resolve by weapon tag
	const FGameplayAttribute ClipAttr = WeaponAS->GetReserveAmmoAttributeFromTag(WeaponTag);
	const FGameplayAttribute MaxClipAttr = WeaponAS->GetMaxReserveAmmoAttributeFromTag(WeaponTag);

	// Use Rifle attributes as default for clip ammo
	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetRifleClipAmmoAttribute(), static_cast<float>(CurrentClipAmmo));
	ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetMaxRifleClipAmmoAttribute(), static_cast<float>(MaxClipAmmo));

	// Reserve ammo via weapon tag
	if (ClipAttr.IsValid())
	{
		ASC->SetNumericAttributeBase(ClipAttr, static_cast<float>(CurrentReserveAmmo));
	}
	if (MaxClipAttr.IsValid())
	{
		ASC->SetNumericAttributeBase(MaxClipAttr, static_cast<float>(MaxReserveAmmo));
	}
}

void FAZ_Inv_CommonUI_WeaponStateFragment::SaveFromASC(UAbilitySystemComponent* ASC)
{
	if (!ASC) return;

	const UAZ_WeaponAttributeSet* WeaponAS = ASC->GetSet<UAZ_WeaponAttributeSet>();
	if (!WeaponAS) return;

	BaseDamage = ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetBaseDamageAttribute());
	FireRate = ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetFireRateAttribute());
	ReloadSpeed = ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetReloadTimeAttribute());
	SpreadBase = ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetSpreadModifierAttribute());

	// Read clip ammo (using Rifle as default)
	CurrentClipAmmo = static_cast<int32>(ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetRifleClipAmmoAttribute()));
	MaxClipAmmo = static_cast<int32>(ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetMaxRifleClipAmmoAttribute()));

	// Read reserve ammo via weapon tag
	const FGameplayAttribute ReserveAttr = WeaponAS->GetReserveAmmoAttributeFromTag(WeaponTag);
	const FGameplayAttribute MaxReserveAttr = WeaponAS->GetMaxReserveAmmoAttributeFromTag(WeaponTag);
	if (ReserveAttr.IsValid())
	{
		CurrentReserveAmmo = static_cast<int32>(ASC->GetNumericAttribute(ReserveAttr));
	}
	if (MaxReserveAttr.IsValid())
	{
		MaxReserveAmmo = static_cast<int32>(ASC->GetNumericAttribute(MaxReserveAttr));
	}
}

// --- EquipmentFragment (pure state + modifiers) ---

void FAZ_Inv_CommonUI_EquipmentFragment::OnPickup(APlayerController* PC)
{
	if (State != EEquipmentState::None) return;
	State = EEquipmentState::Carried;
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnEquip(APlayerController* PC)
{
	if (State == EEquipmentState::Equipped) return;
	State = EEquipmentState::Equipped;

	for (auto& Modifier : EquipModifiers)
	{
		Modifier.GetMutable().OnEquip(PC);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::OnUnequip(APlayerController* PC)
{
	if (State != EEquipmentState::Equipped) return;
	State = EEquipmentState::Carried;

	for (auto& Modifier : EquipModifiers)
	{
		Modifier.GetMutable().OnUnequip(PC);
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
		Modifier.Get().Assimilate(Composite);
	}
}

void FAZ_Inv_CommonUI_EquipmentFragment::Manifest()
{
	FAZ_Inv_CommonUI_InventoryItem_Fragment::Manifest();
	for (auto& Modifier : EquipModifiers)
	{
		Modifier.GetMutable().Manifest();
	}
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

void FAZ_Inv_CommonUI_EquipmentFragment::SetEquippedActor(AActor* InActor)
{
	EquippedActor = InActor;
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
