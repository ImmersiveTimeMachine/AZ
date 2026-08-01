#include "Animation/AZ_SkeletonUtils.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendProfile.h"
#include "Engine/SkeletalMeshSocket.h"

TArray<FName> UAZ_SkeletonUtils::GetBlendProfileNames(USkeleton* Skeleton)
{
	TArray<FName> Names;
	if (!Skeleton) return Names;

	for (const TObjectPtr<UBlendProfile>& Profile : Skeleton->BlendProfiles)
	{
		if (Profile)
		{
			Names.Add(Profile->GetFName());
		}
	}
	return Names;
}

TMap<FName, float> UAZ_SkeletonUtils::GetBlendProfileBoneWeights(USkeleton* Skeleton, FName ProfileName)
{
	TMap<FName, float> Weights;
	if (!Skeleton) return Weights;

	UBlendProfile* Profile = Skeleton->GetBlendProfile(ProfileName);
	if (!Profile) return Weights;

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	for (int32 i = 0; i < RefSkel.GetNum(); i++)
	{
		FName BoneName = RefSkel.GetBoneName(i);
		float Scale = Profile->GetBoneBlendScale(BoneName);
		if (!FMath::IsNearlyEqual(Scale, 1.0f, 0.001f))
		{
			Weights.Add(BoneName, Scale);
		}
	}
	return Weights;
}

UBlendProfile* UAZ_SkeletonUtils::CreateBlendProfile(USkeleton* Skeleton, FName ProfileName)
{
	if (!Skeleton) return nullptr;

	if (Skeleton->GetBlendProfile(ProfileName))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlendProfile '%s' already exists"), *ProfileName.ToString());
		return Skeleton->GetBlendProfile(ProfileName);
	}

	return Skeleton->CreateNewBlendProfile(ProfileName);
}

bool UAZ_SkeletonUtils::SetBlendProfileBoneWeight(USkeleton* Skeleton, FName ProfileName, FName BoneName, float Scale, bool bRecurse)
{
	if (!Skeleton) return false;

	UBlendProfile* Profile = Skeleton->GetBlendProfile(ProfileName);
	if (!Profile) return false;

	Profile->SetBoneBlendScale(BoneName, Scale, bRecurse, true);
	return true;
}

bool UAZ_SkeletonUtils::CopyBlendProfile(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton, FName ProfileName)
{
	if (!SourceSkeleton || !TargetSkeleton) return false;

	UBlendProfile* SrcProfile = SourceSkeleton->GetBlendProfile(ProfileName);
	if (!SrcProfile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Source profile '%s' not found"), *ProfileName.ToString());
		return false;
	}

	if (TargetSkeleton->GetBlendProfile(ProfileName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Target already has profile '%s'"), *ProfileName.ToString());
		return false;
	}

	UBlendProfile* DstProfile = TargetSkeleton->CreateNewBlendProfile(ProfileName);
	if (!DstProfile) return false;

	const FReferenceSkeleton& SrcRefSkel = SourceSkeleton->GetReferenceSkeleton();
	const FReferenceSkeleton& DstRefSkel = TargetSkeleton->GetReferenceSkeleton();
	int32 Copied = 0;

	for (int32 i = 0; i < SrcRefSkel.GetNum(); i++)
	{
		FName BoneName = SrcRefSkel.GetBoneName(i);
		float Scale = SrcProfile->GetBoneBlendScale(BoneName);
		if (!FMath::IsNearlyEqual(Scale, 1.0f, 0.001f))
		{
			// Only copy if the bone exists on the target skeleton
			int32 DstBoneIdx = DstRefSkel.FindBoneIndex(BoneName);
			if (DstBoneIdx != INDEX_NONE)
			{
				DstProfile->SetBoneBlendScale(DstBoneIdx, Scale, false, true);
				Copied++;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  Bone '%s' not found on target skeleton, skipping"), *BoneName.ToString());
			}
		}
	}

	TargetSkeleton->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("Copied blend profile '%s' (%d bone entries)"), *ProfileName.ToString(), Copied);
	return true;
}

int32 UAZ_SkeletonUtils::CopyAllBlendProfiles(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton)
{
	if (!SourceSkeleton || !TargetSkeleton) return 0;

	int32 Count = 0;
	for (const TObjectPtr<UBlendProfile>& SrcProfile : SourceSkeleton->BlendProfiles)
	{
		if (SrcProfile && CopyBlendProfile(SourceSkeleton, TargetSkeleton, SrcProfile->GetFName()))
		{
			Count++;
		}
	}
	return Count;
}

TArray<FName> UAZ_SkeletonUtils::GetBoneNames(USkeleton* Skeleton)
{
	TArray<FName> Names;
	if (!Skeleton) return Names;

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	for (int32 i = 0; i < RefSkel.GetNum(); i++)
	{
		Names.Add(RefSkel.GetBoneName(i));
	}
	return Names;
}

bool UAZ_SkeletonUtils::AddSocket(USkeleton* Skeleton, FName SocketName, FName BoneName,
	FVector RelativeLocation, FRotator RelativeRotation, bool bReplaceExisting)
{
	if (!Skeleton || SocketName.IsNone() || BoneName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkeletonUtils] AddSocket: null skeleton or empty name."));
		return false;
	}
	// A socket on a bone the skeleton doesn't have attaches to nothing and reports no error at runtime.
	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkeletonUtils] AddSocket: '%s' has no bone '%s'."),
			*Skeleton->GetName(), *BoneName.ToString());
		return false;
	}

	USkeletalMeshSocket* Socket = Skeleton->FindSocket(SocketName);
	const bool bExisted = (Socket != nullptr);
	if (bExisted && !bReplaceExisting)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkeletonUtils] AddSocket: '%s' already exists on '%s' (kept)."),
			*SocketName.ToString(), *Skeleton->GetName());
		return false;
	}

	Skeleton->Modify();
	if (!bExisted)
	{
		Socket = NewObject<USkeletalMeshSocket>(Skeleton);
		Socket->SocketName = SocketName;
		Skeleton->Sockets.Add(Socket);
	}
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = RelativeLocation;
	Socket->RelativeRotation = RelativeRotation;
	Skeleton->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("[SkeletonUtils] %s socket '%s' on %s.%s at (%.2f,%.2f,%.2f)."),
		bExisted ? TEXT("updated") : TEXT("created"), *SocketName.ToString(),
		*Skeleton->GetName(), *BoneName.ToString(),
		RelativeLocation.X, RelativeLocation.Y, RelativeLocation.Z);
	return true;
}

bool UAZ_SkeletonUtils::RemoveSocket(USkeleton* Skeleton, FName SocketName)
{
	if (!Skeleton)
	{
		return false;
	}
	USkeletalMeshSocket* Socket = Skeleton->FindSocket(SocketName);
	if (!Socket)
	{
		return false;
	}
	Skeleton->Modify();
	Skeleton->Sockets.Remove(Socket);
	Skeleton->MarkPackageDirty();
	return true;
}

TArray<FString> UAZ_SkeletonUtils::ListSockets(USkeleton* Skeleton)
{
	TArray<FString> Lines;
	if (!Skeleton)
	{
		return Lines;
	}
	for (const TObjectPtr<USkeletalMeshSocket>& Socket : Skeleton->Sockets)
	{
		if (!Socket)
		{
			continue;
		}
		Lines.Add(FString::Printf(TEXT("%s bone=%s loc=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f)"),
			*Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
			Socket->RelativeLocation.X, Socket->RelativeLocation.Y, Socket->RelativeLocation.Z,
			Socket->RelativeRotation.Pitch, Socket->RelativeRotation.Yaw, Socket->RelativeRotation.Roll));
	}
	return Lines;
}
