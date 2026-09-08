#include "Weapon/AZ_WeaponPoseDebug.h"

#include "AZ/AZ.h"
#include "AZ_ConsoleVariables.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "BlendStack/AnimNode_BlendStack.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EngineUtils.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UnrealType.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	void LogTransform(const TCHAR* Label, const FTransform& Transform)
	{
		UE_LOG(Log_AZ, Display, TEXT("[WeaponPose.Transform] frame=%llu %s pos=(%s) rot=(%s) scale=(%s)"),
			GFrameCounter, Label, *Transform.GetLocation().ToString(), *Transform.Rotator().ToString(), *Transform.GetScale3D().ToString());
	}

	FString PlayingStacks(UAnimInstance* Anim)
	{
		FString Result;
		if (const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(Anim->GetClass()))
		{
			for (const FStructProperty* Property : AnimClass->GetAnimNodeProperties())
			{
				if (Property && Property->Struct->IsChildOf(FAnimNode_BlendStack::StaticStruct()))
				{
					const FAnimNode_BlendStack* Stack = Property->ContainerPtrToValuePtr<FAnimNode_BlendStack>(Anim);
					Result += FString::Printf(TEXT("%s:%s@%.4f(outerWantedRate=%.3f) "), *Property->GetName(),
						*GetNameSafe(Stack->GetAnimAsset()), Stack->GetAccumulatedTime(), Stack->WantedPlayRate);
					for (const FBlendStackAnimPlayer& Player : Stack->AnimPlayers)
					{
						Result += FString::Printf(TEXT("[sample=%s@%.4f rate=%.3f blendIn=%.3f postponed=%d] "),
							*GetNameSafe(Player.GetAnimationAsset()), Player.GetAccumulatedTime(), Player.GetPlayRate(), Player.GetBlendInWeight(), Player.IsPostponed());
					}
				}
			}
		}
		return Result.IsEmpty() ? TEXT("none") : Result;
	}
}

void FAZ_WeaponPoseDebug::Startup()
{
#if !UE_BUILD_SHIPPING
	EndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FAZ_WeaponPoseDebug::OnEndFrame);
#endif
}

void FAZ_WeaponPoseDebug::Shutdown()
{
	FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
	EndFrameHandle.Reset();
	Samples.Empty();
}

void FAZ_WeaponPoseDebug::OnEndFrame()
{
	const int32 Level = AZCVars::GetWeaponDebug();
	if (Level <= 0 || !GEngine)
	{
		Samples.Empty();
		return;
	}
	const float ConfigInterval = AZCVars::GetWeaponDebugInterval();
	const float Interval = FMath::IsFinite(ConfigInterval) ? FMath::Clamp(ConfigInterval, .05f, 60.f) : .25f;
	const double Now = FPlatformTime::Seconds();
	for (auto It = Samples.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!World || !World->IsGameWorld() || World->bIsTearingDown) { continue; }
		for (TActorIterator<AAZ_Weapon> It(World); It; ++It)
		{
			AAZ_Weapon* Weapon = *It;
			USceneComponent* Root = Weapon->GetRootComponent();
			USkeletalMeshComponent* Body = Root ? Cast<USkeletalMeshComponent>(Root->GetAttachParent()) : nullptr;
			APawn* Pawn = Body ? Cast<APawn>(Body->GetOwner()) : nullptr;
			USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh3P();
			UAZ_MoverAnimInstance* Anim = Body ? Cast<UAZ_MoverAnimInstance>(Body->GetAnimInstance()) : nullptr;
			if (!Pawn || !WeaponMesh || !Anim) { continue; }
			// Preview actors never pass the game-world filter. The controller owns inventory selection.
			const AController* Controller = Pawn->GetController();
			const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = Controller ? Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
			const bool bSelected = Equipment && Equipment->GetActiveWeapon() == Weapon;
			const bool bSelectedRifle = bSelected && Equipment->GetActiveProfile().MatchesTag(FAZ_GameplayTags::Get().Weapon_Rifle);
			const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
			const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
			const bool bSprint = ASC && ASC->HasMatchingGameplayTag(Tags.Movement_Sprinting);
			const bool bAim = ASC && ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming);
			const FName ActualSocket = Root->GetAttachSocketName();
			// Remote roles may have no owning controller/selection: report unknown, not a false mismatch.
			const FName ExpectedSocket = !Equipment ? NAME_None : ((!bSelected || (bSelectedRifle && bSprint))
				? Weapon->CarrySocketName : (bAim ? Weapon->AimSocketName : Weapon->RelaxedSocketName));
			const FAZ_v2_ChooserContext& Context = Anim->ChooserContext;
			const FString Mode = FString::Printf(TEXT("selectionKnown=%d selected=%d sprint=%d aim=%d contextAim=%d stance=%s gait=%s state=%s socket=%s expected=%s"),
				Equipment != nullptr, bSelected, bSprint, bAim, Context.bIsAiming, *UEnum::GetValueAsString(Context.Stance), *UEnum::GetValueAsString(Context.Gait),
				*UEnum::GetValueAsString(Context.SMState), *ActualSocket.ToString(), *ExpectedSocket.ToString());
			FSampleState& Previous = Samples.FindOrAdd(Weapon);
			if (Previous.Mode == Mode && Now - Previous.LastSeconds < Interval) { continue; }
			Previous = {Now, Mode};
			const FString ItemId = bSelected && Equipment->GetActiveItem() ? Equipment->GetActiveItem()->GetInstanceId().ToString() : TEXT("inactive");
			UE_LOG(Log_AZ, Display, TEXT("[WeaponPose] frame=%llu time=%.3f world=%s pawn=%s weapon=%s role=%d item=%s %s socketOK=%d speed=%.2f dir8=%s start=%s movingTransition=%d landed=%d leftFootDown=%d aimAlpha=%.3f yaw=%.2f pitch=%.2f profile=%s selectedClip=%s playing={%s} montage=%s"),
				GFrameCounter, World->GetTimeSeconds(), *World->GetName(), *Pawn->GetName(), *Weapon->GetName(), int32(Pawn->GetLocalRole()), *ItemId, *Mode,
				Equipment ? int32(ActualSocket == ExpectedSocket) : -1, Context.Speed2D, *UEnum::GetValueAsString(Context.MovementDirection8), *UEnum::GetValueAsString(Context.StartDirection),
				Context.bMovingTransition, Context.bJustLanded, Context.bLeftFootDown, Anim->AimAlpha, Anim->AimYaw, Anim->AimPitch,
				*GetNameSafe(Anim->ActiveWeaponAnimationProfile), *GetNameSafe(Anim->BlendStackInputs.Anim), *PlayingStacks(Anim), *GetNameSafe(Anim->GetCurrentActiveMontage()));
			if (Level < 2) { continue; }
			UE_LOG(Log_AZ, Display, TEXT("[WeaponPose.Mesh] frame=%llu weapon=%s bodyLOD=%d bodyBoneCount=%d"),
				GFrameCounter, *Weapon->GetName(), Body->GetPredictedLODLevel(), Body->GetNumBones());
			LogTransform(TEXT("body.world"), Body->GetComponentTransform());
			LogTransform(TEXT("body.relative"), Body->GetRelativeTransform());
			LogTransform(TEXT("weaponRoot.relativeToAttachSocket"), Root->GetRelativeTransform());
			LogTransform(TEXT("weaponMesh.relativeToRoot"), WeaponMesh->GetRelativeTransform());
			LogTransform(TEXT("weaponMesh.world"), WeaponMesh->GetComponentTransform());
			if (const USkeletalMeshSocket* Socket = Body->GetSocketByName(ActualSocket))
			{
				LogTransform(*FString::Printf(TEXT("attachSocket.parentBone=%s.local"), *Socket->BoneName.ToString()), Socket->GetSocketLocalTransform());
			}
			for (const FName Bone : {FName("hand_r"), FName("hand_l"), FName("index_01_r"), FName("index_02_r"), FName("index_03_r"), FName("middle_01_r"), FName("thumb_03_r"), FName("middle_01_l")})
			{
				if (Body->GetBoneIndex(Bone) == INDEX_NONE) { continue; }
				const FTransform WorldBone = Body->GetSocketTransform(Bone, RTS_World);
				// Weapon space makes distances comparable between modes, movement and camera angles.
				LogTransform(*FString::Printf(TEXT("body.%s.weaponSpace"), *Bone.ToString()), WorldBone.GetRelativeTransform(WeaponMesh->GetComponentTransform()));
			}
			const FName LeftGrip = bAim && WeaponMesh->DoesSocketExist(TEXT("LeftHandGripAim")) ? FName("LeftHandGripAim") : Weapon->LeftHandGripSocket;
			if (WeaponMesh->DoesSocketExist(LeftGrip) && Body->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE)
			{
				const FVector Grip = WeaponMesh->GetSocketLocation(LeftGrip);
				const FVector Wrist = Body->GetBoneLocation(TEXT("hand_l"));
				LogTransform(TEXT("leftGrip.weaponSpace"), WeaponMesh->GetSocketTransform(LeftGrip, RTS_Component));
				UE_LOG(Log_AZ, Display, TEXT("[WeaponPose.Grip] frame=%llu weapon=%s marker=%s wristToMarkerCM=%.3f deltaWorld=(%s) rightHandReferenceIsNotAnAuthoredGrip=1"),
					GFrameCounter, *Weapon->GetName(), *LeftGrip.ToString(), FVector::Distance(Wrist, Grip), *(Grip - Wrist).ToString());
				if (Level >= 3)
				{
					DrawDebugSphere(World, Wrist, 1.f, 8, FColor::Green, false, Interval);
					DrawDebugSphere(World, Grip, 1.f, 8, FColor::Cyan, false, Interval);
					DrawDebugLine(World, Wrist, Grip, FColor::Yellow, false, Interval, 0, .6f);
					DrawDebugCoordinateSystem(World, WeaponMesh->GetComponentLocation(), WeaponMesh->GetComponentRotation(), 8.f, false, Interval);
					if (Body->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE)
					{
						DrawDebugSphere(World, Body->GetBoneLocation(TEXT("hand_r")), 1.f, 8, FColor::Red, false, Interval);
					}
				}
			}
		}
	}
}
