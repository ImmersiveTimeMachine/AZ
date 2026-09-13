// Copyright Artur. AZ project.
#include "Animation/AZ_PairedHandContactComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UAZ_PairedHandContactComponent::UAZ_PairedHandContactComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Actual MHC hand frames relative to the existing Chalkie grip sockets at the paired
	// Wrestle midpoint, including AZ_Catch_Fight role origins. Preserve socket positions.
	// Calibration receipt: Saved/ProceduralHero/paired-hand-calibration.json.
	LeftSocketOffset.SetRotation(FQuat(-0.503958805626, 0.684448621337, 0.229054670213, -0.474436049470).GetNormalized());
	RightSocketOffset.SetRotation(FQuat(-0.355536164672, 0.575218801185, 0.194963171861, 0.710427144746).GetNormalized());
}

void UAZ_PairedHandContactComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_PairedHandContactComponent, Action);
}

FGuid UAZ_PairedHandContactComponent::BeginPairedGrab(AActor* Partner, UAnimMontage* OwnMontage, UAnimMontage* PartnerMontage)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Partner) || Partner == GetOwner()
		|| !OwnMontage || !PartnerMontage || OwnMontage->GetSectionIndex(ContactSection) == INDEX_NONE
		|| PartnerMontage->GetSectionIndex(ContactSection) == INDEX_NONE)
	{
		return FGuid();
	}
	Action.Id = FGuid::NewGuid();
	Action.Partner = Partner;
	Action.OwnMontage = OwnMontage;
	Action.PartnerMontage = PartnerMontage;
	const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner());
	AuthorityOwnAnim = Hero && Hero->GetMesh() ? Hero->GetMesh()->GetAnimInstance() : nullptr;
	const USkeletalMeshComponent* PartnerMesh = FindPartnerMesh();
	AuthorityPartnerAnim = PartnerMesh ? PartnerMesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* OwnInstance = AuthorityOwnAnim.IsValid() ? AuthorityOwnAnim->GetActiveInstanceForMontage(OwnMontage) : nullptr;
	const FAnimMontageInstance* PartnerInstance = AuthorityPartnerAnim.IsValid() ? AuthorityPartnerAnim->GetActiveInstanceForMontage(PartnerMontage) : nullptr;
	if (!OwnInstance || !PartnerInstance)
	{
		ClearAuthorityAction();
		return FGuid();
	}
	OwnMontageInstanceId = OwnInstance->GetInstanceID();
	PartnerMontageInstanceId = PartnerInstance->GetInstanceID();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	StartController = Pawn ? Pawn->GetController() : nullptr;
	AuthorityStartTime = GetWorld()->GetTimeSeconds();
	SetComponentTickEnabled(true);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("[PairedHands] Begin %s partner=%s action=%s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Partner), *Action.Id.ToString());
	return Action.Id;
}

void UAZ_PairedHandContactComponent::EndPairedGrab(const FGuid& ActionId)
{
	if (GetOwner() && GetOwner()->HasAuthority() && ActionId.IsValid() && ActionId == Action.Id)
	{
		ClearAuthorityAction();
	}
}

void UAZ_PairedHandContactComponent::ClearAuthorityAction()
{
	if (Action.Id.IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("[PairedHands] End %s action=%s"), *GetNameSafe(GetOwner()), *Action.Id.ToString());
	}
	Action = FAZ_PairedHandContactAction();
	AuthorityOwnAnim.Reset();
	AuthorityPartnerAnim.Reset();
	OwnMontageInstanceId = PartnerMontageInstanceId = INDEX_NONE;
	SetComponentTickEnabled(false);
	if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UAZ_PairedHandContactComponent::OnRep_Action()
{
	// Observers derive targets from their own animated participant meshes. No world-space pose packets.
	if (Action.Id.IsValid() && Cached.ActionId != Action.Id) Cached = FAZ_PairedHandContactSnapshot();
}

USkeletalMeshComponent* UAZ_PairedHandContactComponent::FindPartnerMesh() const
{
	if (!IsValid(Action.Partner)) return nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes;
	Action.Partner->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->DoesSocketExist(LeftPartnerSocket) && Mesh->DoesSocketExist(RightPartnerSocket)) return Mesh;
	}
	return nullptr;
}

void UAZ_PairedHandContactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Action.Id.IsValid()) return;
	const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(GetOwner());
	const UAnimInstance* OwnAnim = Hero && Hero->GetMesh() ? Hero->GetMesh()->GetAnimInstance() : nullptr;
	const USkeletalMeshComponent* PartnerMesh = FindPartnerMesh();
	const UAnimInstance* PartnerAnim = PartnerMesh ? PartnerMesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* OwnInstance = OwnAnim ? OwnAnim->GetActiveInstanceForMontage(Action.OwnMontage) : nullptr;
	const FAnimMontageInstance* PartnerInstance = PartnerAnim ? PartnerAnim->GetActiveInstanceForMontage(Action.PartnerMontage) : nullptr;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Hero || !IsValid(Action.Partner) || Hero->GetGrabFacingTarget() != Action.Partner
		|| !ASC || !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed)
		|| OwnAnim != AuthorityOwnAnim.Get() || PartnerAnim != AuthorityPartnerAnim.Get()
		|| !OwnInstance || OwnInstance->GetInstanceID() != OwnMontageInstanceId
		|| !PartnerInstance || PartnerInstance->GetInstanceID() != PartnerMontageInstanceId
		|| (Pawn && Pawn->GetController() != StartController.Get())
		|| GetWorld()->GetTimeSeconds() - AuthorityStartTime > MaxActionSeconds)
	{
		ClearAuthorityAction();
	}
}

bool UAZ_PairedHandContactComponent::IsContactWindow(const UAnimInstance* Anim, const UAnimMontage* Montage) const
{
	if (!Anim || !Montage || !Anim->Montage_IsPlaying(Montage)
		|| Anim->Montage_GetCurrentSection(Montage) != ContactSection) return false;
	const int32 SectionIndex = Montage->GetSectionIndex(ContactSection);
	if (SectionIndex == INDEX_NONE) return false;
	float Start = 0.f, End = 0.f;
	Montage->GetSectionStartAndEndTime(SectionIndex, Start, End);
	const float Position = Anim->Montage_GetPosition(Montage) - Start;
	const float WindowEnd = ContactEndSeconds < 0.f ? End - Start : FMath::Min(ContactEndSeconds, End - Start);
	return Position >= ContactStartSeconds && Position <= WindowEnd;
}

bool UAZ_PairedHandContactComponent::BuildHandTarget(USkeletalMeshComponent* OwnMesh, USkeletalMeshComponent* PartnerMesh,
	FName UpperArm, FName LowerArm, FName Hand, FName Socket, const FTransform& Offset, FTransform& OutTargetCS) const
{
	const USkeletalMesh* Asset = OwnMesh->GetSkeletalMeshAsset();
	if (!Asset || !PartnerMesh->DoesSocketExist(Socket)) return false;
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const int32 UpperIndex = Ref.FindBoneIndex(UpperArm), LowerIndex = Ref.FindBoneIndex(LowerArm), HandIndex = Ref.FindBoneIndex(Hand);
	if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE) return false;
	const auto RefLocation = [&Ref](int32 Index)
	{
		FTransform Transform = Ref.GetRefBonePose()[Index];
		for (int32 Parent = Ref.GetParentIndex(Index); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
		{
			Transform *= Ref.GetRefBonePose()[Parent];
		}
		return Transform.GetLocation();
	};
	const FTransform MeshWorld = OwnMesh->GetComponentTransform();
	const FVector UpperRef = MeshWorld.TransformPosition(RefLocation(UpperIndex));
	const FVector LowerRef = MeshWorld.TransformPosition(RefLocation(LowerIndex));
	const FVector HandRef = MeshWorld.TransformPosition(RefLocation(HandIndex));
	const float Reach = (FVector::Distance(UpperRef, LowerRef) + FVector::Distance(LowerRef, HandRef)) * FMath::Clamp(ReachScale, .5f, 1.f);
	if (Reach < KINDA_SMALL_NUMBER) return false;
	FTransform TargetWorld = Offset * PartnerMesh->GetSocketTransform(Socket, RTS_World);
	if (TargetWorld.ContainsNaN()) return false;
	const FVector Shoulder = OwnMesh->GetSocketLocation(UpperArm);
	const FVector HandWorld = OwnMesh->GetSocketLocation(Hand);
	FVector Target = HandWorld + (TargetWorld.GetLocation() - HandWorld).GetClampedToMaxSize(FMath::Max(0.f, MaxHandCorrectionCm));
	Target = Shoulder + (Target - Shoulder).GetClampedToMaxSize(Reach);
	TargetWorld.SetLocation(Target);
	TargetWorld.SetScale3D(FVector::OneVector);
	OutTargetCS = TargetWorld.GetRelativeTransform(MeshWorld);
	OutTargetCS.SetScale3D(FVector::OneVector);
	OutTargetCS.NormalizeRotation();
	return !OutTargetCS.ContainsNaN();
}

FAZ_PairedHandContactSnapshot UAZ_PairedHandContactComponent::EvaluateHandContacts(USkeletalMeshComponent* OwnMesh, float DeltaSeconds)
{
	check(IsInGameThread());
	UAnimInstance* OwnAnim = OwnMesh ? OwnMesh->GetAnimInstance() : nullptr;
	if (LastOwnMesh.Get() != OwnMesh || LastOwnAnim.Get() != OwnAnim
		|| (Action.Id.IsValid() && Cached.ActionId != Action.Id))
	{
		Cached = FAZ_PairedHandContactSnapshot();
		LastOwnMesh = OwnMesh;
		LastOwnAnim = OwnAnim;
	}
	USkeletalMeshComponent* PartnerMesh = FindPartnerMesh();
	const UAnimInstance* PartnerAnim = PartnerMesh ? PartnerMesh->GetAnimInstance() : nullptr;
	// Authority validates the grab tag before publishing/retaining Action. Peers
	// consume that replicated identity: the ability's local loose tag is not an
	// observer-side contract. Both exact montage windows still have to agree.
	const bool bContact = bEnabled && OwnMesh && OwnMesh->GetOwner() == GetOwner() && Action.Id.IsValid()
		&& IsValid(Action.Partner) && PartnerMesh
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), Action.Partner->GetActorLocation()) <= FMath::Square(MaxPartnerDistanceCm)
		&& IsContactWindow(OwnAnim, Action.OwnMontage) && IsContactWindow(PartnerAnim, Action.PartnerMontage);
	float LeftTargetWeight = 0.f, RightTargetWeight = 0.f;
	if (bContact)
	{
		LeftTargetWeight = BuildHandTarget(OwnMesh, PartnerMesh, TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
			LeftPartnerSocket, LeftSocketOffset, Cached.LeftTargetCS) ? 1.f : 0.f;
		RightTargetWeight = BuildHandTarget(OwnMesh, PartnerMesh, TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
			RightPartnerSocket, RightSocketOffset, Cached.RightTargetCS) ? 1.f : 0.f;
	}
	const auto BlendWeight = [this, DeltaSeconds](float Current, float Target)
	{
		const float Seconds = Target > Current ? BlendInSeconds : BlendOutSeconds;
		return Seconds > KINDA_SMALL_NUMBER ? FMath::FInterpConstantTo(Current, Target, FMath::Max(0.f, DeltaSeconds), 1.f / Seconds) : Target;
	};
	Cached.LeftWeight = BlendWeight(Cached.LeftWeight, LeftTargetWeight);
	Cached.RightWeight = BlendWeight(Cached.RightWeight, RightTargetWeight);
	Cached.bActive = FMath::Max(Cached.LeftWeight, Cached.RightWeight) > KINDA_SMALL_NUMBER;
	Cached.ActionId = Action.Id;
	return Cached;
}

void UAZ_PairedHandContactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Cached = FAZ_PairedHandContactSnapshot();
	Action = FAZ_PairedHandContactAction();
	Super::EndPlay(EndPlayReason);
}
