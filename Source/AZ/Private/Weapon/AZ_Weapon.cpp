// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AZ_Weapon.h"

#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/TargetActors/AZ_GATA_LineTrace.h"
#include "AbilitySystem/TargetActors/AZ_GATA_SphereTrace.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "Animation/Skeleton.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_HeroCharacter.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Player/AZ_PlayerController.h"
#include "Sound/SoundBase.h"


// Sets default values
AAZ_Weapon::AAZ_Weapon()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	NetUpdateFrequency = 100.0f; // Set this to a value that's appropriate for your game
	bSpawnWithCollision = true;

	/*ObjectLiftingZone = CreateDefaultSubobject<UCapsuleComponent>(FName("ObjectLiftingZone"));
	ObjectLiftingZone->InitCapsuleSize(40.0f, 50.0f);
	ObjectLiftingZone->SetCollisionObjectType(COLLISION_PICKUP);
	ObjectLiftingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Manually enable when in pickup mode
	ObjectLiftingZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	ObjectLiftingZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = ObjectLiftingZone;*/
	
	// Box collision for the weapon's physical presence in the world
	CollisionComp = CreateDefaultSubobject<UBoxComponent>(FName("CollisionComponent"));
	CollisionComp->SetBoxExtent(FVector(32.f, 32.f, 32.f));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CollisionComp->SetupAttachment(GetRootComponent());

	WeaponMesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh1P"));
	WeaponMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh1P->CastShadow = false;
	WeaponMesh1P->SetVisibility(false, true);
	WeaponMesh1P->SetupAttachment(GetRootComponent());
	WeaponMesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	WeaponMesh3PickupRelativeLocation = FVector(0.0f, -25.0f, 0.0f);

	WeaponMesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh3P"));
	WeaponMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh3P->SetupAttachment(GetRootComponent());
	WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
	WeaponMesh3P->CastShadow = true;
	WeaponMesh3P->SetVisibility(true, true);
	WeaponMesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	
	// Create the weapon's own Ability System Component.
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Create the weapon's own Attribute Set, which will be managed by its ASC.
	AttributeSet = CreateDefaultSubobject<UAZ_WeaponAttributeSet>("AttributeSet");

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	const auto& GameplayTags = FAZ_GameplayTags::Get();
	PrimaryAmmoType = GameplayTags.Abilities_Type_None;
	SecondaryAmmoType = GameplayTags.Abilities_Type_None;

	// Pre-populate IK adjustment map with all pose states (zero offset by default)
	const FAZ_LeftHandIKAdjustment ZeroAdj;
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Relaxed,          ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Aiming,           ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Crouching,        ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::CrouchAiming,     ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Shooting,         ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::CrouchShooting,   ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Reloading,        ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::CrouchReloading,  ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Sprinting,        ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::MeleeAttacking,   ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Interacting,      ZeroAdj);
	LeftHandIKAdjustments.Add(EAZ_WeaponPoseState::Throwing,         ZeroAdj);
}

void AAZ_Weapon::SetOwner(AActor* NewOwner)
{
	AActor* PreviousOwner = GetOwner();
	TGuardValue<bool> EquipmentEndingGuard(bEndingEquipmentPresentation,
		bEndingEquipmentPresentation || PreviousOwner != NewOwner);
	if (PreviousOwner != NewOwner)
	{
		StopFirearmAnimation();
		StopReloadAnimation();
		InterruptEquipmentAnimation(EquipmentAnimationActionId);
		ClearEquipmentSocketBlend();
	}
	Super::SetOwner(NewOwner);
	if (GetOwner() != PreviousOwner)
	{
		OnOwnershipChanged.Broadcast();
	}
}

void AAZ_Weapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (const UAnimInstance* AnimInstance = FirearmAnimationInstance.Get();
		AnimInstance && AnimInstance->GetOwningActor() != GetOwner())
	{
		StopFirearmAnimation();
	}
	if (const UAnimInstance* AnimInstance = ReloadAnimationInstance.Get();
		AnimInstance && AnimInstance->GetOwningActor() != GetOwner())
	{
		StopReloadAnimation();
	}
	if (EquipmentAnimationActionId.IsValid() && EquipmentAnimationOwner.Get() != GetOwner())
	{
		TGuardValue<bool> EquipmentEndingGuard(bEndingEquipmentPresentation, true);
		InterruptEquipmentAnimation(EquipmentAnimationActionId);
	}
	if (bEquipmentSocketBlending && (!EquipmentSocketBlendParent.IsValid()
		|| EquipmentSocketBlendParent->GetOwner() != GetOwner())) ClearEquipmentSocketBlend();
	// Replication can resolve Owner after the controller's equipment selection.
	// Some replication paths also call SetOwner; presentation snapshots deduplicate.
	OnOwnershipChanged.Broadcast();
}

void AAZ_Weapon::OnRep_AttachmentReplication()
{
	USceneComponent* WeaponRoot = GetRootComponent();
	USceneComponent* PreviousParent = WeaponRoot ? WeaponRoot->GetAttachParent() : nullptr;
	const FName PreviousSocket = WeaponRoot ? WeaponRoot->GetAttachSocketName() : NAME_None;
	const bool bHavePreviousMesh = IsValid(WeaponMesh3P) && PreviousParent;
	const FTransform PreviousMeshWorld = bHavePreviousMesh ? WeaponMesh3P->GetComponentTransform() : FTransform::Identity;
	const bool bWasBlending = bEquipmentSocketBlending;
	Super::OnRep_AttachmentReplication();
	if (!bHavePreviousMesh || PreviousMeshWorld.ContainsNaN() || !IsValid(WeaponMesh3P) || !WeaponRoot) return;
	USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(WeaponRoot->GetAttachParent());
	if (!IsValid(BodyMesh) || BodyMesh != PreviousParent || BodyMesh->GetOwner() != GetOwner()) return;
	const FName CurrentSocket = WeaponRoot->GetAttachSocketName();
	const bool bCarryHandoff = bIsCosmeticOnly && PreviousSocket != CurrentSocket
		&& ((PreviousSocket == CarrySocketName && (CurrentSocket == RelaxedSocketName || CurrentSocket == AimSocketName))
			|| (CurrentSocket == CarrySocketName && (PreviousSocket == RelaxedSocketName || PreviousSocket == AimSocketName)));
	if (bWasBlending || bCarryHandoff)
	{
		// An attachment can be applied before its cosmetic RPC. Preserve this
		// observer's rendered pose immediately; the RPC will supply the authored
		// duration without replacing it with a dedicated server's unevaluated pose.
		const float RemainingDuration = bWasBlending
			? FMath::Max(0.f, EquipmentSocketBlendDuration - EquipmentSocketBlendElapsed) : 0.1f;
		BeginEquipmentSocketBlendLocal(BodyMesh, CurrentSocket,
			PreviousMeshWorld.GetRelativeTransform(WeaponRoot->GetComponentTransform()), RemainingDuration);
	}
}

USkeletalMeshComponent* AAZ_Weapon::GetWeaponMesh1P() const
{
	return WeaponMesh1P;
}

USkeletalMeshComponent* AAZ_Weapon::GetWeaponMesh3P() const
{
	return WeaponMesh3P;
}

void AAZ_Weapon::ConfigureFirearmPresentation(const FAZ_Inv_CommonUI_WeaponStateFragment& Definition)
{
	if (!HasAuthority()) return;
	FirearmMuzzleSocketName = Definition.MuzzleSocketName;
	FirearmFireSound = Definition.FireSound;
	FirearmMuzzleFlash = Definition.MuzzleFlash;
	FirearmCascadeMuzzleFlash = Definition.CascadeMuzzleFlash;
	const UAZ_WeaponAnimationProfile* Profile = Definition.AnimationProfile.Get();
	FirearmSingleFireAnimation = Profile ? Profile->SingleFireAnimation.Get() : nullptr;
	FirearmAutomaticFireAnimation = Profile ? Profile->AutomaticFireAnimation.Get() : nullptr;
	FirearmCrouchingSingleFireAnimation = Profile ? Profile->CrouchingSingleFireAnimation.Get() : nullptr;
	FirearmCrouchingAutomaticFireAnimation = Profile ? Profile->CrouchingAutomaticFireAnimation.Get() : nullptr;
	FirearmAutomaticAnimationPlayRate = 1.f;
	FirearmAnimationSlot = Profile ? Profile->FireAnimationSlot : FName(TEXT("RifleFire"));
	FirearmAnimationBlendIn = Profile && FMath::IsFinite(Profile->FireAnimationBlendIn)
		? FMath::Clamp(Profile->FireAnimationBlendIn, 0.f, 1.f) : 0.04f;
	FirearmAnimationBlendOut = Profile && FMath::IsFinite(Profile->FireAnimationBlendOut)
		? FMath::Clamp(Profile->FireAnimationBlendOut, 0.f, 1.f) : 0.08f;
	ForceNetUpdate();
}

void AAZ_Weapon::Multicast_BeginFirearmAnimation_Implementation(const FGuid& ActionId, bool bAutomatic)
{
	if (!ActionId.IsValid() || EndedFirearmAnimationActions.Contains(ActionId)
		|| FirearmAnimationActionId == ActionId) return;
	StopFirearmAnimationLocal();
	// Montage-stop callbacks may already have canceled this action or begun a newer one.
	if (FirearmAnimationActionId.IsValid() || EndedFirearmAnimationActions.Contains(ActionId)) return;
	FirearmAnimationActionId = ActionId;
	bFirearmAutomaticAnimation = bAutomatic;
	// Keep action ownership on dedicated servers too, so later equipment cleanup can emit the matching end.
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// Do not resurrect a queued shot pose after the owner lowered the firearm,
		// opened UI, or changed sources. Remote observers receive the accepted action
		// without depending on this player's private inventory/controller state.
		const AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(OwnerPawn->GetController());
		const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = Controller
			? Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
		const UAbilitySystemComponent* HeroASC = Controller ? Controller->GetAbilitySystemComponent() : nullptr;
		if (!Controller || Controller->IsInventoryInputCaptured() || !Equipment || !HeroASC
			|| !Equipment->IsActiveWeaponSource(this) || !Equipment->IsFirearmRaised()
			|| HeroASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading))
		{
			UE_LOG(LogTemp, Display, TEXT("[FireAnim] ignored lowered or blocked owner start action=%s"), *ActionId.ToString());
			StopFirearmAnimation();
			return;
		}
	}

	USkeletalMeshComponent* HeroMesh = nullptr;
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner()))
	{
		HeroMesh = Hero->GetMesh();
	}
	else if (const AAZ_HeroCharacter* HeroCharacter = Cast<AAZ_HeroCharacter>(GetOwner()))
	{
		HeroMesh = HeroCharacter->GetThirdPersonMesh();
	}
	UAnimInstance* AnimInstance = IsValid(HeroMesh) ? HeroMesh->GetAnimInstance() : nullptr;
	// Stance-selected clip, the way reload does it: both fire clips used to be STANDING poses, so a crouched shot
	// popped the torso ~25 deg to the standing pose for the montage's length (crouch aim idle chest pitch 58 deg
	// vs standing 81). Read the Mover's crouch state locally rather than threading it through the RPC: the state
	// is replicated, the clip lasts ~0.4 s, and a one-frame mismatch at a stance edge is invisible. The crouch
	// clips are optional per profile — unset falls back to the standing clip, which is exactly the old behaviour.
	bool bCrouched = false;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const UCharacterMoverComponent* Mover = OwnerPawn->FindComponentByClass<UCharacterMoverComponent>())
		{
			bCrouched = Mover->IsCrouching();
		}
	}
	UAnimSequence* Sequence = bAutomatic
		? ((bCrouched && FirearmCrouchingAutomaticFireAnimation) ? FirearmCrouchingAutomaticFireAnimation.Get() : FirearmAutomaticFireAnimation.Get())
		: ((bCrouched && FirearmCrouchingSingleFireAnimation) ? FirearmCrouchingSingleFireAnimation.Get() : FirearmSingleFireAnimation.Get());
	if (!IsValid(AnimInstance) || !IsValid(Sequence) || FirearmAnimationSlot.IsNone()) return;

	const USkeleton* SequenceSkeleton = Sequence->GetSkeleton();
	const USkeletalMesh* MeshAsset = HeroMesh->GetSkeletalMeshAsset();
	const USkeleton* MeshSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	const FName FireGroup(TEXT("WeaponFire"));
	if (!SequenceSkeleton || !MeshSkeleton
		|| SequenceSkeleton->GetSlotGroupName(FirearmAnimationSlot) != FireGroup
		|| MeshSkeleton->GetSlotGroupName(FirearmAnimationSlot) != FireGroup)
	{
		// A missing slot silently falls back to DefaultGroup, which would interrupt a combat montage.
		UE_LOG(LogTemp, Warning, TEXT("[FireAnim] %s cannot play %s: slot %s must belong to WeaponFire on both skeletons"),
			*GetName(), *GetNameSafe(Sequence), *FirearmAnimationSlot.ToString());
		return;
	}
	AActor* AnimationOwner = GetOwner();
	FirearmAnimationInstance = AnimInstance;
	// Fire cadence belongs to gameplay; it must not retime the hero animation.
	// Use the fixed rate locally too, including weapons created before a code patch.
	const float PlayRate = 1.f;
	UAnimMontage* StartedMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Sequence, FirearmAnimationSlot, FirearmAnimationBlendIn, FirearmAnimationBlendOut, PlayRate, 1);
	// OnMontageStarted runs inside the play call. An end, ownership change, or newer begin there
	// must not be overwritten by the old call's return value, especially before making it loop.
	if (FirearmAnimationActionId != ActionId || FirearmAnimationInstance.Get() != AnimInstance
		|| GetOwner() != AnimationOwner || !IsValid(HeroMesh) || !IsValid(AnimInstance)
		|| HeroMesh->GetAnimInstance() != AnimInstance)
	{
		if (IsValid(AnimInstance) && IsValid(StartedMontage))
		{
			AnimInstance->Montage_Stop(FirearmAnimationBlendOut, StartedMontage);
		}
		return;
	}
	FirearmAnimationMontage = StartedMontage;
	if (bAutomatic && StartedMontage)
	{
		// A one-section loop has bounded time precision and no dependency on the sequence's editor Loop flag.
		AnimInstance->Montage_SetNextSection(TEXT("Default"), TEXT("Default"), StartedMontage);
	}
	UE_LOG(LogTemp, Display, TEXT("[FireAnim] begin action=%s automatic=%d sequence=%s playing=%d rate=1.000"),
		*ActionId.ToString(), bAutomatic, *GetNameSafe(Sequence), FirearmAnimationMontage != nullptr);
}

void AAZ_Weapon::Multicast_EndFirearmAnimation_Implementation(const FGuid& ActionId, bool bInterruptSingle)
{
	if (!ActionId.IsValid()) return;
	// An accepted ammo publication can cancel an ability synchronously BEFORE it publishes its begin.
	// Retain a small ordered receipt history so that late begin cannot restart that canceled action.
	if (!EndedFirearmAnimationActions.Contains(ActionId))
	{
		if (EndedFirearmAnimationActions.Num() == 16) EndedFirearmAnimationActions.RemoveAt(0);
		EndedFirearmAnimationActions.Add(ActionId);
	}
	if (ActionId != FirearmAnimationActionId) return;
	if (bFirearmAutomaticAnimation || bInterruptSingle)
	{
		UE_LOG(LogTemp, Display, TEXT("[FireAnim] end action=%s interrupt=%d"), *ActionId.ToString(), bInterruptSingle);
		StopFirearmAnimationLocal();
	}
	// Retain a released single's ownership until it naturally finishes or an equipment/aim cleanup stops it.
}

void AAZ_Weapon::StopFirearmAnimation()
{
	if (FirearmAnimationActionId.IsValid())
	{
		const FGuid ActionId = FirearmAnimationActionId;
		if (HasAuthority()) Multicast_EndFirearmAnimation(ActionId, true);
		else Multicast_EndFirearmAnimation_Implementation(ActionId, true);
	}
	else
	{
		StopFirearmAnimationLocal();
	}
}

void AAZ_Weapon::StopFirearmAnimationLocal()
{
	UAnimInstance* AnimInstance = FirearmAnimationInstance.Get();
	UAnimMontage* Montage = FirearmAnimationMontage.Get();
	// Clear ownership before Montage_Stop invokes any animation delegates that might start a newer action.
	FirearmAnimationInstance.Reset();
	FirearmAnimationMontage = nullptr;
	FirearmAnimationActionId.Invalidate();
	bFirearmAutomaticAnimation = false;
	if (IsValid(AnimInstance) && IsValid(Montage))
	{
		AnimInstance->Montage_Stop(FirearmAnimationBlendOut, Montage);
	}
}

void AAZ_Weapon::Multicast_BeginReloadAnimation_Implementation(const FGuid& ActionId, UAnimSequence* Sequence,
	float PlayRate, float BlendIn, float BlendOut, bool bCrouched, bool bRaisedAtStart,
	FGuid ExpectedItemId, uint32 ExpectedGeneration, double StartedServerTime)
{
	if (!ActionId.IsValid() || EndedReloadAnimationActions.Contains(ActionId)
		|| ReloadAnimationActionId == ActionId) return;
	StopReloadAnimationLocal();
	// A montage-stop callback may already have canceled this start or installed a newer action.
	if (ReloadAnimationActionId.IsValid() || EndedReloadAnimationActions.Contains(ActionId)) return;
	ReloadAnimationActionId = ActionId;
	ReloadAnimationBlendOut = FMath::IsFinite(BlendOut) ? FMath::Clamp(BlendOut, 0.f, 1.f) : 0.15f;
	const float SafeBlendIn = FMath::IsFinite(BlendIn) ? FMath::Clamp(BlendIn, 0.f, 1.f) : 0.1f;
	if (!IsValid(Sequence) || Sequence->GetAdditiveAnimType() != AAT_None || Sequence->HasRootMotion()
		|| !FMath::IsFinite(Sequence->GetPlayLength()) || Sequence->GetPlayLength() <= 0.0
		|| !FMath::IsFinite(Sequence->RateScale) || Sequence->RateScale <= 0.f
		|| !FMath::IsFinite(PlayRate) || PlayRate <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReloadAnim] %s rejected invalid pose or playback rate action=%s sequence=%s"),
			*GetName(), *ActionId.ToString(), *GetNameSafe(Sequence));
		InterruptReloadAnimation(ActionId);
		return;
	}
	// Authority's timer owns ammunition. An unrendered dedicated server still retains the action token.
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// A server start can arrive after the owning player opened UI, changed equipment,
		// or changed stance. Observers do not have this player's inventory/controller.
		const AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(OwnerPawn->GetController());
		UAZ_Inv_CommonUI_EquipmentComponent* Equipment = Controller
			? Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
		const UAbilitySystemComponent* HeroASC = Controller ? Controller->GetAbilitySystemComponent() : nullptr;
		const UCharacterMoverComponent* Mover = OwnerPawn->FindComponentByClass<UCharacterMoverComponent>();
		if (!Controller || Controller->GetPawn() != OwnerPawn || Controller->IsInventoryInputCaptured()
			|| !Equipment || !Equipment->IsActiveWeaponSource(this) || !HeroASC
			|| !ExpectedItemId.IsValid() || Equipment->GetSelectionGeneration() != ExpectedGeneration
			|| !IsValid(Equipment->GetActiveItem()) || Equipment->GetActiveItem()->GetInstanceId() != ExpectedItemId
			|| !FMath::IsFinite(StartedServerTime) || StartedServerTime < 0.0
			|| HeroASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_MeleeAttacking)
			|| !Mover || Mover->IsCrouching() != bCrouched)
		{
			UE_LOG(LogTemp, Display, TEXT("[ReloadAnim] ignored blocked owner start action=%s"), *ActionId.ToString());
			// The end receipt prevents another arrival of this action from resurrecting its pose.
			InterruptReloadAnimation(ActionId);
			return;
		}
		if (!HasAuthority() && bRaisedAtStart)
		{
			// The authority snapshots raised state. Its reliable presentation receipt
			// can precede replicated Ready tags, so this owner-only mirror carries that
			// validated decision rather than guessing from tag arrival order.
			ReloadReadyEquipment = Equipment;
			if (!Equipment->BeginFirearmReloadHold(this, ExpectedItemId,
				ExpectedGeneration, ActionId, true, StartedServerTime))
			{
				InterruptReloadAnimation(ActionId);
				return;
			}
			if (ReloadAnimationActionId != ActionId || ReloadReadyEquipment.Get() != Equipment) return;
		}
	}

	USkeletalMeshComponent* HeroMesh = nullptr;
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner()))
	{
		HeroMesh = Hero->GetMesh();
	}
	else if (const AAZ_HeroCharacter* HeroCharacter = Cast<AAZ_HeroCharacter>(GetOwner()))
	{
		HeroMesh = HeroCharacter->GetThirdPersonMesh();
	}
	UAnimInstance* AnimInstance = IsValid(HeroMesh) ? HeroMesh->GetAnimInstance() : nullptr;
	const USkeleton* SequenceSkeleton = Sequence->GetSkeleton();
	const USkeletalMesh* MeshAsset = IsValid(HeroMesh) ? HeroMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* MeshSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	const FName ReloadSlot(TEXT("RifleFire"));
	const FName ReloadGroup(TEXT("WeaponFire"));
	if (!IsValid(AnimInstance) || !SequenceSkeleton || !MeshSkeleton
		|| SequenceSkeleton->GetSlotGroupName(ReloadSlot) != ReloadGroup
		|| MeshSkeleton->GetSlotGroupName(ReloadSlot) != ReloadGroup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReloadAnim] %s cannot play %s: live hero anim instance and RifleFire in WeaponFire are required"),
			*GetName(), *GetNameSafe(Sequence));
		InterruptReloadAnimation(ActionId);
		return;
	}

	AActor* AnimationOwner = GetOwner();
	ReloadAnimationInstance = AnimInstance;
	const float ActionBlendOut = ReloadAnimationBlendOut;
	UAnimMontage* StartedMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Sequence, ReloadSlot, SafeBlendIn, ActionBlendOut, PlayRate, 1);
	// OnMontageStarted runs inside Play: do not overwrite an intervening cancel/owner change/new action.
	if (ReloadAnimationActionId != ActionId || ReloadAnimationInstance.Get() != AnimInstance
		|| GetOwner() != AnimationOwner || !IsValid(HeroMesh) || !IsValid(AnimInstance)
		|| HeroMesh->GetAnimInstance() != AnimInstance)
	{
		if (IsValid(AnimInstance) && IsValid(StartedMontage))
		{
			AnimInstance->Montage_Stop(ActionBlendOut, StartedMontage);
		}
		// Mesh replacement may leave this token current without going through weapon ownership cleanup.
		InterruptReloadAnimation(ActionId);
		return;
	}
	ReloadAnimationMontage = StartedMontage;
	if (!StartedMontage || !AnimInstance->Montage_IsPlaying(StartedMontage))
	{
		InterruptReloadAnimation(ActionId);
		return;
	}
	FOnMontageBlendingOutStarted OnBlendingOut;
	OnBlendingOut.BindUObject(this, &ThisClass::OnReloadMontageBlendingOut, ActionId);
	AnimInstance->Montage_SetBlendingOutDelegate(OnBlendingOut, StartedMontage);
	UE_LOG(LogTemp, Display, TEXT("[ReloadAnim] begin action=%s sequence=%s rate=%.3f"),
		*ActionId.ToString(), *GetNameSafe(Sequence), PlayRate);
}

void AAZ_Weapon::Multicast_EndReloadAnimation_Implementation(const FGuid& ActionId, bool bCommitted)
{
	if (!ActionId.IsValid()) return;
	// Remember even an end arriving before its start (e.g. synchronous ability cancellation).
	if (!EndedReloadAnimationActions.Contains(ActionId))
	{
		if (EndedReloadAnimationActions.Num() >= 16) EndedReloadAnimationActions.RemoveAt(0);
		EndedReloadAnimationActions.Add(ActionId);
	}
	if (ActionId != ReloadAnimationActionId) return;
	UE_LOG(LogTemp, Display, TEXT("[ReloadAnim] end action=%s"), *ActionId.ToString());
	const TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> ReadyEquipment = ReloadReadyEquipment;
	ReloadReadyEquipment.Reset();
	StopReloadAnimationLocal();
	if (ReadyEquipment.IsValid()) ReadyEquipment->EndFirearmReloadHold(ActionId, bCommitted);
}

void AAZ_Weapon::StopReloadAnimation()
{
	if (ReloadAnimationActionId.IsValid())
	{
		const FGuid ActionId = ReloadAnimationActionId;
		// Unlike the ability's expected end RPC, an external stop must release its
		// gameplay reservation even when a requested equipment change later fails.
		InterruptReloadAnimation(ActionId);
	}
	else
	{
		StopReloadAnimationLocal();
	}
}

void AAZ_Weapon::StopReloadAnimationLocal()
{
	UAnimInstance* AnimInstance = ReloadAnimationInstance.Get();
	UAnimMontage* Montage = ReloadAnimationMontage.Get();
	const float BlendOut = ReloadAnimationBlendOut;
	const FGuid EndedActionId = ReloadAnimationActionId;
	const TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> ReadyEquipment = ReloadReadyEquipment;
	// Normal ability completion stops the pose too. Clear before the callback so it cannot report interruption.
	ReloadAnimationInstance.Reset();
	ReloadAnimationMontage = nullptr;
	ReloadAnimationActionId.Invalidate();
	ReloadReadyEquipment.Reset();
	if (IsValid(AnimInstance) && IsValid(Montage))
	{
		AnimInstance->Montage_Stop(BlendOut, Montage);
	}
	if (ReadyEquipment.IsValid()) ReadyEquipment->EndFirearmReloadHold(EndedActionId, false);
}

void AAZ_Weapon::InterruptReloadAnimation(const FGuid& ActionId)
{
	if (!ActionId.IsValid() || ReloadAnimationActionId != ActionId) return;
	// End this presentation before notifying gameplay; its callback may cancel or start another action.
	const FGuid InterruptedActionId = ActionId;
	if (HasAuthority()) Multicast_EndReloadAnimation(InterruptedActionId);
	else Multicast_EndReloadAnimation_Implementation(InterruptedActionId, false);
	if (HasAuthority()) OnReloadAnimationInterrupted.Broadcast(InterruptedActionId);
}

void AAZ_Weapon::OnReloadMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, FGuid ExpectedActionId)
{
	if (!bInterrupted || ReloadAnimationActionId != ExpectedActionId || ReloadAnimationMontage != Montage) return;
	InterruptReloadAnimation(ExpectedActionId);
}

void AAZ_Weapon::Multicast_BeginEquipmentAnimation_Implementation(const FGuid& ActionId,
	UAnimSequence* Sequence, FName Slot, float PlayRate, float BlendIn, float BlendOut)
{
	if (bEndingEquipmentPresentation || !ActionId.IsValid() || EndedEquipmentAnimationActions.Contains(ActionId)
		|| EquipmentAnimationActionId == ActionId) return;
	if (EquipmentAnimationActionId.IsValid()) InterruptEquipmentAnimation(EquipmentAnimationActionId);
	else StopEquipmentAnimationLocal();
	if (bEndingEquipmentPresentation || EquipmentAnimationActionId.IsValid()
		|| EndedEquipmentAnimationActions.Contains(ActionId)) return;
	EquipmentAnimationActionId = ActionId;
	EquipmentAnimationOwner = GetOwner();
	EquipmentAnimationBlendOut = FMath::IsFinite(BlendOut) ? FMath::Clamp(BlendOut, 0.f, 1.f) : 0.1f;
	const float SafeBlendIn = FMath::IsFinite(BlendIn) ? FMath::Clamp(BlendIn, 0.f, 1.f) : 0.1f;
	USkeletalMeshComponent* HeroMesh = nullptr;
	if (const AAZ_PawnMoverHeroCharacter* PawnMoverHeroCharacter = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner())) HeroMesh = PawnMoverHeroCharacter->GetMesh();
	else if (const AAZ_HeroCharacter* HeroCharacter = Cast<AAZ_HeroCharacter>(GetOwner())) HeroMesh = HeroCharacter->GetThirdPersonMesh();
	const USkeletalMesh* MeshAsset = IsValid(HeroMesh) ? HeroMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* MeshSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	const USkeleton* SequenceSkeleton = IsValid(Sequence) ? Sequence->GetSkeleton() : nullptr;
	const FName EquipmentGroup(TEXT("WeaponFire"));
	if (!IsValid(Sequence) || Sequence->GetAdditiveAnimType() != AAT_None || Sequence->HasRootMotion()
		|| !FMath::IsFinite(Sequence->GetPlayLength()) || Sequence->GetPlayLength() <= 0.f
		|| !FMath::IsFinite(Sequence->RateScale) || Sequence->RateScale <= 0.f
		|| !FMath::IsFinite(PlayRate) || PlayRate <= 0.f || Slot.IsNone()
		|| !SequenceSkeleton || !MeshSkeleton || !SequenceSkeleton->IsCompatibleMesh(MeshAsset)
		|| SequenceSkeleton->GetSlotGroupName(Slot) != EquipmentGroup
		|| MeshSkeleton->GetSlotGroupName(Slot) != EquipmentGroup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentAnim] rejected action=%s sequence=%s slot=%s"),
			*ActionId.ToString(), *GetNameSafe(Sequence), *Slot.ToString());
		InterruptEquipmentAnimation(ActionId);
		return;
	}
	// An incoming weapon need not be selected yet. Equipment already validated the
	// request, including quick-select UI; observers have no inventory/controller.
	if (GetNetMode() == NM_DedicatedServer) return;
	UAnimInstance* AnimInstance = HeroMesh->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		InterruptEquipmentAnimation(ActionId);
		return;
	}
	AActor* AnimationOwner = GetOwner();
	EquipmentAnimationInstance = AnimInstance;
	const float ActionBlendOut = EquipmentAnimationBlendOut;
	UAnimMontage* StartedMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Sequence, Slot, SafeBlendIn, ActionBlendOut, PlayRate, 1);
	if (StartedMontage)
	{
		// Equipment commits the new selection at the end of the phase. Automatic
		// blend-out would expose the OLD weapon's idle during the last BlendOut
		// seconds, then blend a second time when the new profile is selected.
		// Hold the final pose until the phase advances, commits, or is canceled.
		// Set this before playback: the montage instance snapshots the flag.
		StartedMontage->bEnableAutoBlendOut = false;
		if (AnimInstance->Montage_Play(StartedMontage, PlayRate) <= 0.f) StartedMontage = nullptr;
	}
	if (bEndingEquipmentPresentation || EquipmentAnimationActionId != ActionId
		|| EquipmentAnimationInstance.Get() != AnimInstance || GetOwner() != AnimationOwner
		|| !IsValid(HeroMesh) || !IsValid(AnimInstance) || HeroMesh->GetAnimInstance() != AnimInstance)
	{
		if (IsValid(AnimInstance) && IsValid(StartedMontage)) AnimInstance->Montage_Stop(ActionBlendOut, StartedMontage);
		InterruptEquipmentAnimation(ActionId);
		return;
	}
	EquipmentAnimationMontage = StartedMontage;
	if (!StartedMontage || !AnimInstance->Montage_IsPlaying(StartedMontage))
	{
		InterruptEquipmentAnimation(ActionId);
		return;
	}
	FOnMontageBlendingOutStarted OnBlendingOut;
	OnBlendingOut.BindUObject(this, &ThisClass::OnEquipmentMontageBlendingOut, ActionId);
	AnimInstance->Montage_SetBlendingOutDelegate(OnBlendingOut, StartedMontage);
	UE_LOG(LogTemp, Display, TEXT("[EquipmentAnim] begin action=%s sequence=%s rate=%.3f"),
		*ActionId.ToString(), *GetNameSafe(Sequence), PlayRate);
}

void AAZ_Weapon::Multicast_EndEquipmentAnimation_Implementation(const FGuid& ActionId)
{
	if (!ActionId.IsValid()) return;
	if (!EndedEquipmentAnimationActions.Contains(ActionId))
	{
		if (EndedEquipmentAnimationActions.Num() >= 16) EndedEquipmentAnimationActions.RemoveAt(0);
		EndedEquipmentAnimationActions.Add(ActionId);
	}
	if (EquipmentAnimationActionId != ActionId) return;
	StopEquipmentAnimationLocal();
}

void AAZ_Weapon::StopEquipmentAnimationLocal()
{
	UAnimInstance* AnimInstance = EquipmentAnimationInstance.Get();
	UAnimMontage* Montage = EquipmentAnimationMontage.Get();
	const float BlendOut = EquipmentAnimationBlendOut;
	EquipmentAnimationActionId.Invalidate();
	EquipmentAnimationOwner.Reset();
	EquipmentAnimationInstance.Reset();
	EquipmentAnimationMontage = nullptr;
	if (IsValid(AnimInstance) && IsValid(Montage)) AnimInstance->Montage_Stop(BlendOut, Montage);
}

void AAZ_Weapon::InterruptEquipmentAnimation(const FGuid& ActionId)
{
	if (!ActionId.IsValid() || EquipmentAnimationActionId != ActionId) return;
	const FGuid InterruptedActionId = ActionId;
	if (HasAuthority()) Multicast_EndEquipmentAnimation(InterruptedActionId);
	else Multicast_EndEquipmentAnimation_Implementation(InterruptedActionId);
	if (HasAuthority()) OnEquipmentAnimationInterrupted.Broadcast(InterruptedActionId);
}

void AAZ_Weapon::OnEquipmentMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, FGuid ExpectedActionId)
{
	if (bInterrupted && EquipmentAnimationActionId == ExpectedActionId && EquipmentAnimationMontage == Montage)
	{
		InterruptEquipmentAnimation(ExpectedActionId);
	}
}

void AAZ_Weapon::BlendToEquipmentSocket(FName Socket, float Duration)
{
	if (!HasAuthority() || bEndingEquipmentPresentation || !IsValid(WeaponMesh3P) || !GetRootComponent()
		|| WeaponMesh3P->GetAttachParent() != GetRootComponent() || Socket.IsNone()) return;
	USkeletalMeshComponent* BodyMesh = nullptr;
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner())) BodyMesh = Hero->GetMesh();
	else if (const AAZ_HeroCharacter* HeroCharacter = Cast<AAZ_HeroCharacter>(GetOwner())) BodyMesh = HeroCharacter->GetThirdPersonMesh();
	if (!IsValid(BodyMesh) || !BodyMesh->DoesSocketExist(Socket)) return;
	FTransform DestinationRoot = BodyMesh->GetSocketTransform(Socket);
	// SnapToTargetNotIncludingScale preserves the root's current world scale.
	DestinationRoot.SetScale3D(GetRootComponent()->GetComponentScale());
	const FTransform InitialMeshOffset = WeaponMesh3P->GetComponentTransform().GetRelativeTransform(DestinationRoot);
	const float SafeDuration = FMath::IsFinite(Duration) ? FMath::Max(0.f, Duration) : 0.f;
	// Queue the reliable presentation before its new attachment can replicate. The
	// local implementation also performs the authority's one root socket handoff.
	Multicast_BlendToEquipmentSocket(BodyMesh, Socket, InitialMeshOffset, SafeDuration);
	ForceNetUpdate();
}

void AAZ_Weapon::Multicast_BlendToEquipmentSocket_Implementation(USkeletalMeshComponent* BodyMesh,
	FName Socket, const FTransform& InitialMeshOffset, float Duration)
{
	if (bEndingEquipmentPresentation || !IsValid(BodyMesh) || !IsValid(WeaponMesh3P) || !GetRootComponent()
		|| (GetOwner() && BodyMesh->GetOwner() != GetOwner()) || Socket.IsNone() || !BodyMesh->DoesSocketExist(Socket)
		|| InitialMeshOffset.ContainsNaN() || !FMath::IsFinite(Duration) || Duration < 0.f) return;
	// A dedicated server does not evaluate the reach montage. Every rendered peer
	// must preserve its own mesh placement, including any unfinished socket blend.
	const FTransform LocalMeshWorld = WeaponMesh3P->GetComponentTransform();
	const bool bPreserveLocalMesh = GetNetMode() != NM_DedicatedServer && !LocalMeshWorld.ContainsNaN()
		&& WeaponMesh3P->GetAttachParent() == GetRootComponent()
		&& GetRootComponent()->GetAttachParent() == BodyMesh;
	if (!AttachToComponent(BodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket)) return;
	const FTransform MeshOffset = bPreserveLocalMesh
		? LocalMeshWorld.GetRelativeTransform(GetRootComponent()->GetComponentTransform()) : InitialMeshOffset;
	BeginEquipmentSocketBlendLocal(BodyMesh, Socket, MeshOffset, Duration);
}

void AAZ_Weapon::BeginEquipmentSocketBlendLocal(USkeletalMeshComponent* BodyMesh, FName Socket,
	const FTransform& MeshOffset, float Duration)
{
	const bool bRestoreTick = bEquipmentSocketBlending ? bEquipmentSocketRestoreTick : IsActorTickEnabled();
	EquipmentSocketBlendParent = BodyMesh;
	EquipmentSocketBlendSocket = Socket;
	EquipmentSocketBlendStart = MeshOffset;
	EquipmentSocketBlendDuration = Duration;
	EquipmentSocketBlendElapsed = 0.f;
	bEquipmentSocketRestoreTick = bRestoreTick;
	bEquipmentSocketBlending = true;
	if (Duration <= UE_KINDA_SMALL_NUMBER || GetNetMode() == NM_DedicatedServer)
	{
		ClearEquipmentSocketBlend();
		return;
	}
	WeaponMesh3P->SetRelativeTransform(MeshOffset);
	SetActorTickEnabled(true);
}

void AAZ_Weapon::ClearEquipmentSocketBlend()
{
	if (!bEquipmentSocketBlending) return;
	bEquipmentSocketBlending = false;
	EquipmentSocketBlendParent.Reset();
	EquipmentSocketBlendSocket = NAME_None;
	EquipmentSocketBlendStart = FTransform::Identity;
	EquipmentSocketBlendDuration = EquipmentSocketBlendElapsed = 0.f;
	if (IsValid(WeaponMesh3P)) WeaponMesh3P->SetRelativeTransform(FTransform::Identity);
	SetActorTickEnabled(bEquipmentSocketRestoreTick);
}

void AAZ_Weapon::Multicast_PlayFirearmShot_Implementation(const FHitResult& Hit, bool bHitConfirmed,
	UParticleSystem* WorldImpactEffect, float WorldImpactScale)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// This is the authority's accepted scenery hit, independent of damage feedback
	// and of whether the weapon still has a valid muzzle when the RPC arrives.
	if (IsValid(WorldImpactEffect) && Hit.IsValidBlockingHit()
		&& FMath::IsFinite(WorldImpactScale) && WorldImpactScale > 0.f
		&& !Hit.ImpactPoint.ContainsNaN() && !Hit.ImpactNormal.ContainsNaN())
	{
		const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
		if (!SurfaceNormal.IsNearlyZero())
		{
			// The authored +X direction points away from the surface. A 1 cm offset
			// keeps the initial smoke sprites from being buried in the hit geometry.
			UGameplayStatics::SpawnEmitterAtLocation(this, WorldImpactEffect,
				Hit.ImpactPoint + SurfaceNormal, SurfaceNormal.Rotation(), FVector(WorldImpactScale),
				true, EPSCPoolMethod::AutoRelease, true);
		}
	}
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (bHitConfirmed && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled())
	{
		OnFirearmHitConfirmed.Broadcast(Hit);
	}
	if (!IsValid(WeaponMesh3P)
		|| FirearmMuzzleSocketName.IsNone() || !WeaponMesh3P->DoesSocketExist(FirearmMuzzleSocketName)) return;

	const FTransform Muzzle = WeaponMesh3P->GetSocketTransform(FirearmMuzzleSocketName, RTS_World);
	if (Muzzle.ContainsNaN()) return;
	// This multicast is sent only after authority commits a shot. This presentation
	// call changes only the weapon mesh, never ammunition or the hero's animation instance.
	if (IsValid(WeaponMeshFireAnimation))
	{
		const USkeleton* FireSkeleton = WeaponMeshFireAnimation->GetSkeleton();
		const USkeletalMesh* WeaponMeshAsset = WeaponMesh3P->GetSkeletalMeshAsset();
		if (FireSkeleton && WeaponMeshAsset && FireSkeleton->IsCompatibleMesh(WeaponMeshAsset))
		{
			WeaponMesh3P->PlayAnimation(WeaponMeshFireAnimation, false);
		}
	}
	if (IsValid(FirearmFireSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, FirearmFireSound, Muzzle.GetLocation(), Muzzle.Rotator());
	}
	if (IsValid(FirearmMuzzleFlash))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(FirearmMuzzleFlash, WeaponMesh3P, FirearmMuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
	else if (IsValid(FirearmCascadeMuzzleFlash))
	{
		UGameplayStatics::SpawnEmitterAttached(FirearmCascadeMuzzleFlash, WeaponMesh3P, FirearmMuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector,
			EAttachLocation::SnapToTarget, true, EPSCPoolMethod::AutoRelease, true);
	}
}

void AAZ_Weapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAZ_Weapon, bIsCosmeticOnly);

	DOREPLIFETIME_CONDITION(AAZ_Weapon, OwningCharacter, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, PrimaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, MaxPrimaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, SecondaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAZ_Weapon, MaxSecondaryClipAmmo, COND_OwnerOnly);
	DOREPLIFETIME(AAZ_Weapon, FirearmMuzzleSocketName);
	DOREPLIFETIME(AAZ_Weapon, FirearmFireSound);
	DOREPLIFETIME(AAZ_Weapon, FirearmMuzzleFlash);
	DOREPLIFETIME(AAZ_Weapon, FirearmCascadeMuzzleFlash);
	DOREPLIFETIME(AAZ_Weapon, FirearmSingleFireAnimation);
	DOREPLIFETIME(AAZ_Weapon, FirearmAutomaticFireAnimation);
	DOREPLIFETIME(AAZ_Weapon, FirearmCrouchingSingleFireAnimation);
	DOREPLIFETIME(AAZ_Weapon, FirearmCrouchingAutomaticFireAnimation);
	DOREPLIFETIME(AAZ_Weapon, FirearmAutomaticAnimationPlayRate);
	DOREPLIFETIME(AAZ_Weapon, FirearmAnimationSlot);
	DOREPLIFETIME(AAZ_Weapon, FirearmAnimationBlendIn);
	DOREPLIFETIME(AAZ_Weapon, FirearmAnimationBlendOut);
}

void AAZ_Weapon::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AAZ_Weapon, PrimaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
	DOREPLIFETIME_ACTIVE_OVERRIDE(AAZ_Weapon, SecondaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
}

void AAZ_Weapon::SetOwningCharacter(AAZ_HeroCharacter* InOwningCharacter)
{
	OwningCharacter = InOwningCharacter;
	if (OwningCharacter)
	{
		// Called when added to inventory
		//AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());
		SetOwner(InOwningCharacter);
		// Attach the weapon
		//ObjectLiftingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
		auto res = AttachToComponent(OwningCharacter->GetThirdPersonMesh(), AttachmentRules, CarrySocketName);
		UE_LOG(LogTemp, Warning, TEXT("Attached To Component:  %s"), res ? TEXT("Success") : TEXT("Failed")
);
		/*AttachToComponent(OwningCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);*/

		/*if (OwningCharacter->GetCurregentWeapon() != this)
		{
			WeaponMesh3P->CastShadow = false;
			WeaponMesh3P->SetVisibility(true, true);
			WeaponMesh3P->SetVisibility(false, true);
		}*/
	}
	else
	{
		//AZ_AbilitySystemComponent = nullptr;
		SetOwner(nullptr);
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AAZ_Weapon::NotifyActorBeginOverlap(class AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	/*if (const auto ASC = Cast<AAZ_HeroCharacter>(OtherActor)->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(WeaponTag);	
	}
	
	if (IsValid(this) && !OwningCharacter)
    {
    	PickUpOnTouch(Cast<AAZ_HeroCharacter>(OtherActor));
    }*/
}

void AAZ_Weapon::NotifyActorEndOverlap(class AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	/*if (const auto ASC = Cast<AAZ_HeroCharacter>(OtherActor)->GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(WeaponTag);	
	}*/
}

void AAZ_Weapon::MakeCosmetic()
{
	bIsCosmeticOnly = true;
	ApplyCosmeticPresentation();
}

void AAZ_Weapon::OnRep_CosmeticOnly()
{
	ApplyCosmeticPresentation();
}

void AAZ_Weapon::ApplyCosmeticPresentation()
{
	if (!bIsCosmeticOnly) return;
	// CommonUI inventory representations use AActor's attachment replication so
	// OnRep_AttachmentReplication can preserve the visible mesh at a handoff.
	// PickupSphere's inherited component replication would bypass that hook.
	// Legacy world weapons never enter this path and retain component replication.
	if (USceneComponent* WeaponRoot = GetRootComponent()) WeaponRoot->SetIsReplicated(false);
	SetActorEnableCollision(false);
	if (bEquipmentSocketBlending) bEquipmentSocketRestoreTick = false;
	else SetActorTickEnabled(false);
}

/*void AAZ_Weapon::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& OverlapInfo)
{
	if (const auto HeroCharacter = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (const auto ASC = HeroCharacter->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(EligibleToCollectTag);
			HeroCharacter->AddOverlappingWeapon(this);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString("Press 'E' to add item."));
		}
	}
}

void AAZ_Weapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const auto HeroCharacter = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (const auto ASC = HeroCharacter->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(EligibleToCollectTag);
			HeroCharacter->RemoveOverlappingWeapon(this);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString("Item Not Overlapping, Move closer."));
		}
	}
}*/

void AAZ_Weapon::Equip()
{
	if (!OwningCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s %s OwningCharacter is nullptr"), *FString(__FUNCTION__), *GetName());
		return;
	}

	if (WeaponMesh3P)
	{
		WeaponMesh3P->AttachToComponent(OwningCharacter->GetThirdPersonMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, RelaxedSocketName);
		WeaponMesh3P->SetRelativeLocation(WeaponMesh3PEquippedRelativeLocation);
		WeaponMesh3P->SetRelativeRotation(FRotator(0, 0, -90.0f));
		WeaponMesh3P->CastShadow = true;
		WeaponMesh3P->bCastHiddenShadow = true;

		if (OwningCharacter->IsInFirstPersonPerspective())
		{
			WeaponMesh3P->SetVisibility(true, true); // Without this, the weapon's 3p shadow doesn't show
			WeaponMesh3P->SetVisibility(false, true);
		}
		else
		{
			WeaponMesh3P->SetVisibility(true, true);
		}
	}
}

void AAZ_Weapon::UnEquip()
{
	StopFirearmAnimation();
	StopReloadAnimation();
	if (OwningCharacter == nullptr)
	{
		return;
	}

	WeaponMesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	WeaponMesh3P->CastShadow = false;
	WeaponMesh3P->bCastHiddenShadow = false;
	WeaponMesh3P->SetVisibility(true, true); // Without this, the unequipped weapon's 3p shadow hangs around
	WeaponMesh3P->SetVisibility(false, true);
}

void AAZ_Weapon::AddAbilities()
{
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}

	auto* ASC = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());

	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("%s %s Role: %s ASC is null"), *FString(__FUNCTION__), *GetName(), GET_ACTOR_ROLE_FSTRING(OwningCharacter));
		return;
	}

	// Grant abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	for (TSubclassOf<UAZ_GameplayAbility>& GPAbility : Abilities)
	{
		const auto Ability = Cast<UAZ_GameplayAbility>(GPAbility.GetDefaultObject());
		auto AbilitySpec = FGameplayAbilitySpec(Ability, GetAbilityLevel(Ability->EchoAbilityID), static_cast<int32>(Ability->EchoAbilityInputID), this);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(Ability->InputTag);
		AbilitySpecHandles.Add(ASC->GiveAbility(AbilitySpec));
	}
	
	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(OwningCharacter->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}
	}
}

void AAZ_Weapon::RemoveAbilities()
{
	if (!IsValid(OwningCharacter) || !OwningCharacter->GetAbilitySystemComponent())
	{
		return;
	}

	auto* ASC = Cast<UAZ_AbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());

	if (!ASC)
	{
		return;
	}

	// Remove abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	for (FGameplayAbilitySpecHandle& SpecHandle : AbilitySpecHandles)
	{
		ASC->ClearAbility(SpecHandle);
	}
}

void AAZ_Weapon::ResetWeapon()
{
	FireMode = DefaultFireMode;
	StatusText = DefaultStatusText;
}

void AAZ_Weapon::OnDropped_Implementation(FVector NewLocation)
{
	SetOwningCharacter(nullptr);
	ResetWeapon();

	SetActorLocation(NewLocation);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (WeaponMesh1P)
	{
		WeaponMesh1P->AttachToComponent(CollisionComp, FAttachmentTransformRules::SnapToTargetIncludingScale);
		WeaponMesh1P->SetVisibility(false, true);
	}

	if (WeaponMesh3P)
	{
		WeaponMesh3P->AttachToComponent(CollisionComp, FAttachmentTransformRules::SnapToTargetIncludingScale);
		WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
		WeaponMesh3P->CastShadow = true;
		WeaponMesh3P->SetVisibility(true, true);
	}
}

bool AAZ_Weapon::OnDropped_Validate(FVector NewLocation)
{
	return true;
}

int32 AAZ_Weapon::GetPrimaryClipAmmo() const
{
	return PrimaryClipAmmo;
}

int32 AAZ_Weapon::GetMaxPrimaryClipAmmo() const
{
	return MaxPrimaryClipAmmo;
}

int32 AAZ_Weapon::GetSecondaryClipAmmo() const
{
	return SecondaryClipAmmo;
}

int32 AAZ_Weapon::GetMaxSecondaryClipAmmo() const
{
	return MaxSecondaryClipAmmo;
}

void AAZ_Weapon::SetPrimaryClipAmmo(int32 NewPrimaryClipAmmo)
{
	int32 OldPrimaryClipAmmo = PrimaryClipAmmo;
	PrimaryClipAmmo = NewPrimaryClipAmmo;
	OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AAZ_Weapon::SetMaxPrimaryClipAmmo(int32 NewMaxPrimaryClipAmmo)
{
	int32 OldMaxPrimaryClipAmmo = MaxPrimaryClipAmmo;
	MaxPrimaryClipAmmo = NewMaxPrimaryClipAmmo;
	OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AAZ_Weapon::SetSecondaryClipAmmo(int32 NewSecondaryClipAmmo)
{
	int32 OldSecondaryClipAmmo = SecondaryClipAmmo;
	SecondaryClipAmmo = NewSecondaryClipAmmo;
	OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AAZ_Weapon::SetMaxSecondaryClipAmmo(int32 NewMaxSecondaryClipAmmo)
{
	int32 OldMaxSecondaryClipAmmo = MaxSecondaryClipAmmo;
	MaxSecondaryClipAmmo = NewMaxSecondaryClipAmmo;
	OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

bool AAZ_Weapon::HasInfiniteAmmo() const
{
	return bInfiniteAmmo;
}

UAnimMontage* AAZ_Weapon::GetEquip1PMontage() const
{
	return Equip1PMontage;
}

UAnimMontage* AAZ_Weapon::GetEquip3PMontage() const
{
	return Equip3PMontage;	
}

class USoundCue* AAZ_Weapon::GetPickupSound() const
{
	return PickupSound;
}

FText AAZ_Weapon::GetDefaultStatusText() const
{
	return DefaultStatusText;
}

AAZ_GATA_LineTrace* AAZ_Weapon::GetLineTraceTargetActor()
{
	if (LineTraceTargetActor)
	{
		return LineTraceTargetActor;
	}

	LineTraceTargetActor = GetWorld()->SpawnActor<AAZ_GATA_LineTrace>();
	LineTraceTargetActor->SetOwner(this);
	return LineTraceTargetActor;
}

AAZ_GATA_SphereTrace* AAZ_Weapon::GetSphereTraceTargetActor()
{
	if (SphereTraceTargetActor)
	{
		return SphereTraceTargetActor;
	}

	SphereTraceTargetActor = GetWorld()->SpawnActor<AAZ_GATA_SphereTrace>();
	SphereTraceTargetActor->SetOwner(this);
	return SphereTraceTargetActor;
}

void AAZ_Weapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingEquipmentPresentation = true;
	InterruptEquipmentAnimation(EquipmentAnimationActionId);
	ClearEquipmentSocketBlend();
	StopFirearmAnimationLocal();
	StopReloadAnimationLocal();
	if (LineTraceTargetActor)
	{
		LineTraceTargetActor->Destroy();
	}

	if (SphereTraceTargetActor)
	{
		SphereTraceTargetActor->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void AAZ_Weapon::PickUpOnTouch(AAZ_HeroCharacter* InCharacter)
{
	if (!InCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter is null."));
		return;
	}

	if (!InCharacter->IsHeroAlive())
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter is not alive."));
		return;
	}

	if (!InCharacter->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter does not have an Ability System Component."));
		return;
	}

	if (InCharacter->GetAbilitySystemComponent()->HasAnyMatchingGameplayTags(RestrictedPickupTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("PickUpOnTouch: InCharacter has restricted pickup tags."));
		return;
	}

	InCharacter->AddWeaponToInventory(this);
	
	/*if (InCharacter->AddWeaponToInventory(this, true) && OwningCharacter->IsInFirstPersonPerspective())
	{
		WeaponMesh3P->CastShadow = false;
		WeaponMesh3P->SetVisibility(true, true);
		WeaponMesh3P->SetVisibility(false, true);
	}*/
}

void AAZ_Weapon::OnRep_PrimaryClipAmmo(int32 OldPrimaryClipAmmo)
{
	OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AAZ_Weapon::OnRep_MaxPrimaryClipAmmo(int32 OldMaxPrimaryClipAmmo)
{
	OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AAZ_Weapon::OnRep_SecondaryClipAmmo(int32 OldSecondaryClipAmmo)
{
	OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AAZ_Weapon::OnRep_MaxSecondaryClipAmmo(int32 OldMaxSecondaryClipAmmo)
{
	OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

bool AAZ_Weapon::GetLeftHandSocket(USkeletalMeshComponent* CharMesh, FName BoneName, EAZ_WeaponPoseState PoseState, FTransform& OutTransform) const
{
	if (!CharMesh || !WeaponMesh3P)
	{
		return false;
	}

	if (!WeaponMesh3P->DoesSocketExist(LeftHandGripSocket))
	{
		return false;
	}

	// Get socket transform in world space from the weapon mesh
	const FTransform SocketWorld = WeaponMesh3P->GetSocketTransform(LeftHandGripSocket, RTS_World);

	// Transform to bone space relative to the specified bone (same as BP "Transform to Bone Space" node)
	FVector OutPosition;
	FRotator OutRotation;
	CharMesh->TransformToBoneSpace(BoneName, SocketWorld.GetLocation(), SocketWorld.GetRotation().Rotator(), OutPosition, OutRotation);

	// Single map lookup for the current pose state
	const FAZ_LeftHandIKAdjustment* Adj = LeftHandIKAdjustments.Find(PoseState);
	if (Adj)
	{
		OutPosition += Adj->LocationOffset;
		OutRotation += Adj->RotationOffset;
	}

	OutTransform.SetLocation(OutPosition);
	OutTransform.SetRotation(OutRotation.Quaternion());
	OutTransform.SetScale3D(FVector::OneVector);

	return true;
}

class UAbilitySystemComponent* AAZ_Weapon::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// Called when the game starts or when spawned
void AAZ_Weapon::BeginPlay()
{
	ResetWeapon();

	if (!OwningCharacter && bSpawnWithCollision)
	{
		// Spawned into the world without an owner, enable collision as we are in pickup mode
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	
	Super::BeginPlay();
	ApplyCosmeticPresentation();

	//CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAZ_Weapon::OnSphereBeginOverlap);
	//CollisionComp->OnComponentEndOverlap.AddDynamic(this, &AAZ_Weapon::OnSphereEndOverlap);
}

// Called every frame
void AAZ_Weapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bEquipmentSocketBlending) return;
	if (!IsValid(WeaponMesh3P) || !EquipmentSocketBlendParent.IsValid() || !GetRootComponent()
		|| GetRootComponent()->GetAttachParent() != EquipmentSocketBlendParent.Get()
		|| GetRootComponent()->GetAttachSocketName() != EquipmentSocketBlendSocket)
	{
		ClearEquipmentSocketBlend();
		return;
	}
	EquipmentSocketBlendElapsed += FMath::Max(0.f, DeltaTime);
	const float Alpha = EquipmentSocketBlendDuration > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(EquipmentSocketBlendElapsed / EquipmentSocketBlendDuration, 0.f, 1.f) : 1.f;
	if (Alpha >= 1.f)
	{
		ClearEquipmentSocketBlend();
		return;
	}
	FTransform MeshOffset;
	MeshOffset.Blend(EquipmentSocketBlendStart, FTransform::Identity, Alpha * Alpha * (3.f - 2.f * Alpha));
	WeaponMesh3P->SetRelativeTransform(MeshOffset);
}

