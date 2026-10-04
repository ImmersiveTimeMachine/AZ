// Copyright Artur. AZ project.

#include "NGAssetReaders.h"

#include "AnimPose.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Modules/ModuleManager.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	ng::X ToX(const FTransform& T)
	{
		const FQuat Q = T.GetRotation();
		const FVector L = T.GetTranslation();
		return ng::X(ng::Q(Q.X, Q.Y, Q.Z, Q.W), ng::V3(L.X, L.Y, L.Z));
	}

	/** Copy of GeometryScript's CopyMeshFromSkeletalMesh for LOD 0 with a source mesh description. */
	bool ToDynamicMesh(USkeletalMesh* Mesh, UE::Geometry::FDynamicMesh3& Out, FString& Error)
	{
		if (!Mesh)
		{
			Error = TEXT("no mesh");
			return false;
		}
		if (Mesh->GetNumSourceModels() < 1 || !Mesh->HasMeshDescription(0))
		{
			Error = FString::Printf(TEXT("%s: LOD 0 has no source mesh description"), *Mesh->GetName());
			return false;
		}
		const FMeshDescription* Source = Mesh->GetMeshDescription(0);
		if (!Source)
		{
			Error = FString::Printf(TEXT("%s: LOD 0 mesh description not available"), *Mesh->GetName());
			return false;
		}
		FMeshDescriptionToDynamicMesh Converter;
		Converter.bVIDsFromNonManifoldMeshDescriptionAttr = true;
		Converter.Convert(Source, Out, true);
		return true;
	}

	/** Component-space ref pose of a bone (the dump scripts' cs(): compose the ref locals from the root down). */
	FTransform BoneCS(const FReferenceSkeleton& Ref, int32 Bone)
	{
		TArray<int32> Chain;
		for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B))
		{
			Chain.Add(B);
		}
		FTransform T = FTransform::Identity;
		for (int32 I = Chain.Num() - 1; I >= 0; --I)
		{
			T = Ref.GetRefBonePose()[Chain[I]] * T;
		}
		return T;
	}
}

bool NGAssetReaders::ReadMarker(USkeletalMesh* Weapon, FName Socket, ng::V3& OutPos)
{
	const USkeletalMeshSocket* S = (Weapon && Socket != NAME_None) ? Weapon->FindSocket(Socket) : nullptr;
	if (!S)
	{
		return false;
	}
	const FReferenceSkeleton& Ref = Weapon->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(S->BoneName);
	if (Bone == INDEX_NONE)
	{
		return false;
	}
	const FVector P = (FTransform(S->RelativeRotation, S->RelativeLocation, S->RelativeScale) * BoneCS(Ref, Bone)).GetLocation();
	OutPos = ng::V3(P.X, P.Y, P.Z);
	return true;
}

bool NGAssetReaders::SampleHold(USkeletalMesh* Hero, const FHoldSampling& O, ng::HoldData& Out, FString& Log, FString& Error)
{
	const USkeletalMeshSocket* Sock = Hero ? Hero->FindSocket(O.WeaponSocket) : nullptr;
	if (!Sock)
	{
		Error = FString::Printf(TEXT("the hero mesh has no socket %s (the weapon's socket in the clips)"), *O.WeaponSocket.ToString());
		return false;
	}
	const FTransform SockRel(Sock->RelativeRotation, Sock->RelativeLocation, Sock->RelativeScale);
	const std::string S(1, O.Side);
	const FName HandBone(UTF8_TO_TCHAR(("hand_" + S).c_str()));
	const FName LowerArm(UTF8_TO_TCHAR(("lowerarm_" + S).c_str()));
	std::vector<std::string> Fingers = ng::HandData::SolverBones(O.Side);
	Fingers.erase(Fingers.begin());

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(*O.ClipFolder), Assets, true);
	auto Has = [](const FString& Name, const TArray<FString>& Keys)
	{
		for (const FString& K : Keys)
		{
			if (!K.IsEmpty() && Name.Contains(K))
			{
				return true;
			}
		}
		return false;
	};

	struct FFrame
	{
		FTransform Hand;
		FVector Elbow;
		TArray<FTransform> Locals;
		FString Clip;
		double Time;
	};
	TArray<FFrame> Aim, Relaxed;
	int32 Clips = 0;
	FAnimPoseEvaluationOptions Opts;
	for (const FAssetData& A : Assets)
	{
		if (A.AssetClassPath != UAnimSequence::StaticClass()->GetClassPathName())
		{
			continue;
		}
		const FString Name = A.AssetName.ToString();
		const bool bAim = Has(Name, O.AimFilters) && !Has(Name, O.AimExcludes);
		const bool bRelaxed = Has(Name, O.RelaxedFilters);
		if (!bAim && !bRelaxed)
		{
			continue;
		}
		const UAnimSequence* Seq = Cast<UAnimSequence>(A.GetAsset());
		if (!Seq)
		{
			continue;
		}
		++Clips;
		const double Len = Seq->GetPlayLength();
		for (int32 K = 0; K < O.FramesPerClip; ++K)
		{
			const double T = Len * K / O.FramesPerClip;
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtTime(Seq, T, Opts, Pose);
			const FTransform Weapon = SockRel * UAnimPoseExtensions::GetBonePose(Pose, Sock->BoneName, EAnimPoseSpaces::World);
			FFrame F;
			F.Hand = UAnimPoseExtensions::GetBonePose(Pose, HandBone, EAnimPoseSpaces::World).GetRelativeTransform(Weapon);
			F.Elbow = UAnimPoseExtensions::GetBonePose(Pose, LowerArm, EAnimPoseSpaces::World).GetRelativeTransform(Weapon).GetLocation();
			for (const std::string& B : Fingers)
			{
				F.Locals.Add(UAnimPoseExtensions::GetBonePose(Pose, FName(UTF8_TO_TCHAR(B.c_str())), EAnimPoseSpaces::Local));
			}
			F.Clip = Name;
			F.Time = T;
			if (bAim)
			{
				Aim.Add(F);
			}
			if (bRelaxed)
			{
				Relaxed.Add(F);
			}
		}
	}
	// medoid: the frame closest to all others (position cm + 0.1 x rotation degrees), on at most ~300 frames
	auto Medoid = [](const TArray<FFrame>& All) -> const FFrame*
	{
		if (All.Num() == 0)
		{
			return nullptr;
		}
		TArray<const FFrame*> F;
		const int32 Step = FMath::Max(1, All.Num() / 300);
		for (int32 I = 0; I < All.Num(); I += Step)
		{
			F.Add(&All[I]);
		}
		const FFrame* Best = nullptr;
		double BestSum = 0.0;
		for (const FFrame* A : F)
		{
			double Sum = 0.0;
			for (const FFrame* B : F)
			{
				Sum += (A->Hand.GetLocation() - B->Hand.GetLocation()).Size()
					+ 0.1 * FMath::RadiansToDegrees(A->Hand.GetRotation().AngularDistance(B->Hand.GetRotation()));
			}
			if (!Best || Sum < BestSum)
			{
				Best = A;
				BestSum = Sum;
			}
		}
		return Best;
	};
	const FFrame* A = Medoid(Aim);
	if (!A)
	{
		Error = FString::Printf(TEXT("no aim clip in %s matches %s"), *O.ClipFolder, *FString::Join(O.AimFilters, TEXT(",")));
		return false;
	}
	const FFrame* R = Medoid(Relaxed);
	if (!R)
	{
		R = A;
	}
	Out = ng::HoldData();
	Out.HandInWeapon = ToX(A->Hand);
	for (int32 I = 0; I < A->Locals.Num(); ++I)
	{
		Out.Locals.emplace_back(Fingers[static_cast<size_t>(I)], ToX(A->Locals[I]));
	}
	Out.bHasElbows = true;
	Out.ElbowAim = ng::V3(A->Elbow.X, A->Elbow.Y, A->Elbow.Z);
	Out.ElbowRelaxed = ng::V3(R->Elbow.X, R->Elbow.Y, R->Elbow.Z);
	Out.AimClip = TCHAR_TO_UTF8(*A->Clip);
	Out.RelaxedClip = TCHAR_TO_UTF8(*R->Clip);
	Out.AimTime = A->Time;
	Out.RelaxedTime = R->Time;
	Log += FString::Printf(TEXT("hold from clips: %d clips, %d aim frames, %d relaxed frames; aim pick %s @%.2f s, relaxed pick %s @%.2f s\n"), Clips,
		Aim.Num(), Relaxed.Num(), *A->Clip, A->Time, *R->Clip, R->Time);
	return true;
}

bool NGAssetReaders::ReadMeshTriangles(USkeletalMesh* Mesh, ng::TriMesh& Out, FString& Error)
{
	UE::Geometry::FDynamicMesh3 DM;
	if (!ToDynamicMesh(Mesh, DM, Error))
	{
		return false;
	}
	Out.Verts.clear();
	Out.Tris.clear();
	Out.Verts.reserve(static_cast<size_t>(DM.MaxVertexID()));
	for (int32 Vid = 0; Vid < DM.MaxVertexID(); ++Vid)
	{
		const FVector3d P = DM.IsVertex(Vid) ? DM.GetVertex(Vid) : FVector3d::ZeroVector;
		Out.Verts.emplace_back(P.X, P.Y, P.Z);
	}
	int32 Skipped = 0;
	for (int32 Tid = 0; Tid < DM.MaxTriangleID(); ++Tid)
	{
		if (!DM.IsTriangle(Tid))
		{
			++Skipped;                  // a triangle-ID gap (the Python list held (-1, -1, -1) there)
			continue;
		}
		const UE::Geometry::FIndex3i T = DM.GetTriangle(Tid);
		Out.Tris.push_back(T.A);
		Out.Tris.push_back(T.B);
		Out.Tris.push_back(T.C);
	}
	if (Skipped > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AZ Natural Grip: %s has %d triangle-ID gaps (skipped)"), *Mesh->GetName(), Skipped);
	}
	return true;
}

bool NGAssetReaders::ReadHand(USkeletalMesh* Hero, char Side, ng::HandData& Out, FString& Error)
{
	if (!Hero)
	{
		Error = TEXT("no hero mesh");
		return false;
	}
	const FReferenceSkeleton& Ref = Hero->GetRefSkeleton();
	Out = ng::HandData();
	Out.Bones = ng::HandData::SolverBones(Side);
	TArray<int32> Index;
	for (const std::string& Name : Out.Bones)
	{
		const int32 B = Ref.FindBoneIndex(FName(UTF8_TO_TCHAR(Name.c_str())));
		if (B == INDEX_NONE)
		{
			Error = FString::Printf(TEXT("%s has no bone %hs"), *Hero->GetName(), Name.c_str());
			return false;
		}
		Index.Add(B);
	}
	Out.Parents.assign(Out.Bones.size(), std::string());
	for (size_t I = 1; I < Out.Bones.size(); ++I)
	{
		const int32 P = Ref.GetParentIndex(Index[static_cast<int32>(I)]);
		Out.Parents[I] = P == INDEX_NONE ? std::string() : std::string(TCHAR_TO_UTF8(*Ref.GetBoneName(P).ToString()));
	}
	const FTransform Hand = BoneCS(Ref, Index[0]);
	for (size_t I = 1; I < Out.Bones.size(); ++I)
	{
		const int32 B = Index[static_cast<int32>(I)];
		Out.RefLocalMesh.emplace_back(Out.Bones[I], ToX(Ref.GetRefBonePose()[B]));
		Out.BoneHandSpace.emplace_back(Out.Bones[I], ToX(BoneCS(Ref, B).GetRelativeTransform(Hand)));
	}

	UE::Geometry::FDynamicMesh3 DM;
	if (!ToDynamicMesh(Hero, DM, Error))
	{
		return false;
	}
	const FTransform Inv = Hand.Inverse();
	for (int32 Vid = 0; Vid < DM.MaxVertexID(); ++Vid)
	{
		const FVector3d V = DM.IsVertex(Vid) ? DM.GetVertex(Vid) : FVector3d::ZeroVector;
		const FVector P = Inv.TransformPosition(FVector(V));
		const double Px = Side == 'r' ? P.X : -P.X;          // left-side bones point along +X (mirrored)
		if (-22.0 < Px && Px < 5.0 && FMath::Abs(P.Y) < 11.0 && FMath::Abs(P.Z) < 12.0)
		{
			Out.VertsHandSpace.emplace_back(ng::PyRound3(P.X), ng::PyRound3(P.Y), ng::PyRound3(P.Z));
		}
	}
	return true;
}
