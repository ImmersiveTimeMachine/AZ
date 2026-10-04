// Copyright Artur. AZ project.

#include "NGApply.h"

#include "AZNaturalGripProfile.h"
#include "AnimPose.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NGJobs.h"
#include "NGJson.h"
#include "NGSetup.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <memory>
#include <string>

#define LOCTEXT_NAMESPACE "AZNaturalGrip"

namespace
{
	struct FSolved
	{
		FString Path;
		FDateTime Time;
		FTransform Hand;                         // the solved hand in weapon space
		TArray<TPair<FName, FQuat>> Locals;      // finger bone local rotations
		FString P;
	};

	struct FChange
	{
		FString What, Old, New;
	};

	FString V(const FVector& L)
	{
		return FString::Printf(TEXT("(%.4f, %.4f, %.4f)"), L.X, L.Y, L.Z);
	}

	FString R(const FRotator& Rt)
	{
		return FString::Printf(TEXT("(P %.3f, Y %.3f, R %.3f)"), Rt.Pitch, Rt.Yaw, Rt.Roll);
	}

	FString TStr(const FTransform& T)
	{
		return V(T.GetLocation()) + TEXT(" ") + R(T.Rotator());
	}

	double AngleDeg(const FQuat& A, const FQuat& B)
	{
		return FMath::RadiansToDegrees(A.AngularDistance(B));
	}

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

	bool LoadSolved(const FString& Path, FSolved& Out, FString& Error)
	{
		ng::JValue J;
		std::string Err;
		if (!ng::LoadJsonFile(std::string(TCHAR_TO_UTF8(*Path)), J, Err))
		{
			Error = FString::Printf(TEXT("no solve result to apply (%s): run the final solve first"), UTF8_TO_TCHAR(Err.c_str()));
			return false;
		}
		try
		{
			const ng::JValue& H = J["hand_in_weapon"];
			Out.Hand = FTransform(FQuat(H.NumAt(3), H.NumAt(4), H.NumAt(5), H.NumAt(6)), FVector(H.NumAt(0), H.NumAt(1), H.NumAt(2)));
			for (const auto& KV : J["locals"].Obj)
			{
				const ng::JValue& Q = KV.second;
				Out.Locals.Emplace(FName(UTF8_TO_TCHAR(KV.first.c_str())), FQuat(Q.NumAt(0), Q.NumAt(1), Q.NumAt(2), Q.NumAt(3)));
			}
			if (const ng::JValue* P = J.Find("p"))
			{
				Out.P = TEXT("[");
				for (size_t I = 0; I < P->Size(); ++I)
				{
					Out.P += FString::Printf(TEXT("%s%g"), I ? TEXT(", ") : TEXT(""), P->NumAt(I));
				}
				Out.P += TEXT("]");
			}
		}
		catch (const std::exception& E)
		{
			Error = FString::Printf(TEXT("%s: %hs"), *Path, E.what());
			return false;
		}
		Out.Path = Path;
		Out.Time = IFileManager::Get().GetTimeStamp(*Path);
		return true;
	}

	FString PackageFile(const UPackage* Pkg)
	{
		FString File;
		FPackageName::TryConvertLongPackageNameToFilename(Pkg->GetName(), File, FPackageName::GetAssetPackageExtension());
		return FPaths::ConvertRelativePathToFull(File);
	}

	/** Copies the package's files (.uasset + .uexp / .ubulk when present) under Dir, keeping the package path. */
	bool BackupPackage(const UPackage* Pkg, const FString& Dir, TArray<TPair<FString, FString>>& Copied, FString& Error)
	{
		const FString Src = PackageFile(Pkg);
		const FString Rel = Pkg->GetName().RightChop(1);            // "Game/AZ/..." or "AZNaturalGrip/..."
		for (const TCHAR* Ext : {TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk")})
		{
			const FString From = FPaths::ChangeExtension(Src, Ext);
			if (!IFileManager::Get().FileExists(*From))
			{
				continue;
			}
			const FString To = FPaths::Combine(Dir, FPaths::ChangeExtension(Rel, Ext));
			if (IFileManager::Get().Copy(*To, *From, true, true) != COPY_OK)
			{
				Error = FString::Printf(TEXT("backup failed: %s -> %s"), *From, *To);
				return false;
			}
			Copied.Emplace(From, To);
		}
		return true;
	}
}

FString NGApply::ResultPath(const UAZNaturalGripProfile& Profile)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaturalGrip"), Profile.GetName(),
		Profile.IsTriggerRole() ? TEXT("trigger_solve.json") : TEXT("support_final.json")));
}

bool NGApply::Apply(UAZNaturalGripProfile& Profile, bool bDryRun, FString& Log)
{
	auto Line = [&Log](const FString& S) { Log += S + TEXT("\n"); };
	// the right hand (trigger roles, a right-hand handle) is written as the re-grip correction; the left one as the socket
	const bool bTrigger = Profile.SideChar() == 'r';
	Line(FString::Printf(TEXT("[%s] apply %s, %s hand"), *Profile.GetName(), bDryRun ? TEXT("(dry run)") : TEXT(""), bTrigger ? TEXT("trigger") : TEXT("support")));
	if (!bDryRun && GEditor && (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress()))
	{
		Line(TEXT("REFUSED: a Play-In-Editor session is running (saving is blocked during PIE). Stop PIE and apply again."));
		return false;
	}

	// --- the result ---
	FSolved Solved;
	FString Error;
	if (!LoadSolved(ResultPath(Profile), Solved, Error))
	{
		Line(TEXT("REFUSED: ") + Error);
		return false;
	}
	Line(FString::Printf(TEXT("result: %s (%s), p %s, %d finger bones"), *Solved.Path, *Solved.Time.ToString(), *Solved.P, Solved.Locals.Num()));

	// --- the assets ---
	UBlueprint* BP = Profile.WeaponBlueprint.LoadSynchronous();
	if (!BP || !BP->GeneratedClass)
	{
		Line(TEXT("REFUSED: the profile has no Weapon Blueprint"));
		return false;
	}
	UClass* Cls = BP->GeneratedClass;
	UObject* Cdo = Cls->GetDefaultObject();
	UAnimSequence* Pose = Profile.GripPoseOverride.LoadSynchronous();
	if (!Pose)
	{
		if (const FObjectPropertyBase* PP = FindFProperty<FObjectPropertyBase>(Cls, Profile.GripPoseProperty))
		{
			Pose = Cast<UAnimSequence>(PP->GetObjectPropertyValue_InContainer(Cdo));
		}
	}
	if (!Pose)
	{
		Line(FString::Printf(TEXT("REFUSED: no grip pose (override empty and %s has no %s)"), *BP->GetName(), *Profile.GripPoseProperty.ToString()));
		return false;
	}
	const FName FlagName = bTrigger ? Profile.BakedRightProperty : Profile.BakedLeftProperty;
	FBoolProperty* FlagProp = FindFProperty<FBoolProperty>(Cls, FlagName);
	if (!FlagProp)
	{
		Line(FString::Printf(TEXT("REFUSED: %s has no bool property %s"), *BP->GetName(), *FlagName.ToString()));
		return false;
	}

	TArray<FChange> Changes;
	TArray<UPackage*> Packages;
	auto AddPackage = [&Packages](UPackage* Pkg) { Packages.AddUnique(Pkg); };

	// support: the left-hand socket = the solved hand
	USkeletalMesh* Mesh = nullptr;
	USkeletalMeshSocket* Sock = nullptr;
	FTransform SockBone, SockLocal;
	if (!bTrigger)
	{
		Mesh = Profile.WeaponMesh.LoadSynchronous();
		Sock = Mesh ? Mesh->FindSocket(Profile.LeftHandSocket) : nullptr;
		if (!Sock)
		{
			Line(FString::Printf(TEXT("REFUSED: the weapon mesh has no socket %s"), *Profile.LeftHandSocket.ToString()));
			return false;
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 Bone = Ref.FindBoneIndex(Sock->BoneName);
		if (Bone == INDEX_NONE)
		{
			Line(FString::Printf(TEXT("REFUSED: socket bone %s is not in the mesh"), *Sock->BoneName.ToString()));
			return false;
		}
		SockBone = BoneCS(Ref, Bone);
		SockLocal = Solved.Hand.GetRelativeTransform(SockBone);
		const FTransform OldLocal(Sock->RelativeRotation, Sock->RelativeLocation, Sock->RelativeScale);
		const FTransform OldCs = OldLocal * SockBone;
		Changes.Add({FString::Printf(TEXT("socket %s on %s (owner %s)"), *Profile.LeftHandSocket.ToString(), *Sock->BoneName.ToString(), *Sock->GetOutermost()->GetName()),
			FString::Printf(TEXT("local %s %s"), *V(Sock->RelativeLocation), *R(Sock->RelativeRotation)),
			FString::Printf(TEXT("local %s %s  (moves %.3f cm, turns %.2f deg)"), *V(SockLocal.GetLocation()), *R(SockLocal.Rotator()),
				(OldCs.GetLocation() - Solved.Hand.GetLocation()).Size(), AngleDeg(OldCs.GetRotation(), Solved.Hand.GetRotation()))});
		AddPackage(Sock->GetOutermost());
	}

	// trigger: the right-hand correction = solved hand vs the clip hold (hand-local)
	FStructProperty* CorrProp = nullptr;
	FTransform NewCorr;
	if (bTrigger)
	{
		CorrProp = FindFProperty<FStructProperty>(Cls, Profile.CorrectionProperty);
		if (!CorrProp || CorrProp->Struct != TBaseStructure<FTransform>::Get())
		{
			Line(FString::Printf(TEXT("REFUSED: %s has no FTransform property %s"), *BP->GetName(), *Profile.CorrectionProperty.ToString()));
			return false;
		}
		FNGProfileData Data;
		if (!NGJobs::Snapshot(Profile, Data, Error))
		{
			Line(TEXT("REFUSED: ") + Error);
			return false;
		}
		ng::SetupConfig Cfg = Data.Base;
		Cfg.Thin = 1;
		Cfg.bFine = false;
		auto Su = std::make_unique<ng::Setup>();
		std::string Err;
		// only the clip hold matters here: the sampled one when the profile samples clips (no field files needed then)
		ng::SetupInputs In;
		ng::Lattice Dummy;
		Dummy.Dims[0] = Dummy.Dims[1] = Dummy.Dims[2] = 2;
		Dummy.D.assign(8, 10.0);
		if (Data.bAssets)
		{
			In.Hand = &Data.HeroHand;
			In.Coarse = &Dummy;
		}
		if (Data.bHasHold)
		{
			In.Hold = &Data.Hold;
		}
		if (!Su->Load(Cfg, Err, In))
		{
			Line(FString::Printf(TEXT("REFUSED: the clip hold: %s"), UTF8_TO_TCHAR(Err.c_str())));
			return false;
		}
		const FQuat Q = Solved.Hand.GetRotation();
		const FVector T = Solved.Hand.GetLocation();
		const ng::X New(ng::Q(Q.X, Q.Y, Q.Z, Q.W), ng::V3(T.X, T.Y, T.Z));
		const ng::X D = New * Su->Plc.HandW.Inv();                // rsolve export: D = new * HAND_W.inv()
		NewCorr = FTransform(FQuat(D.q.x, D.q.y, D.q.z, D.q.w), FVector(D.t.x, D.t.y, D.t.z));
		const FTransform OldCorr = *CorrProp->ContainerPtrToValuePtr<FTransform>(Cdo);
		Changes.Add({FString::Printf(TEXT("%s.%s"), *BP->GetName(), *Profile.CorrectionProperty.ToString()), TStr(OldCorr),
			FString::Printf(TEXT("%s  (moves %.3f cm, turns %.2f deg)"), *TStr(NewCorr), (OldCorr.GetLocation() - NewCorr.GetLocation()).Size(),
				AngleDeg(OldCorr.GetRotation(), NewCorr.GetRotation()))});
	}

	// the grip pose's finger rotations (translations stay the pose's own)
	FAnimPose OldPose;
	UAnimPoseExtensions::GetAnimPoseAtTime(Pose, 0.0, FAnimPoseEvaluationOptions(), OldPose);
	double MaxTurn = 0.0;
	FName MaxBone;
	for (const TPair<FName, FQuat>& L : Solved.Locals)
	{
		const FTransform& Old = UAnimPoseExtensions::GetBonePose(OldPose, L.Key, EAnimPoseSpaces::Local);
		const double A = AngleDeg(Old.GetRotation(), L.Value);
		if (A > MaxTurn)
		{
			MaxTurn = A;
			MaxBone = L.Key;
		}
	}
	Changes.Add({FString::Printf(TEXT("%s: %d finger bone rotations"), *Pose->GetName(), Solved.Locals.Num()), TEXT("current"),
		FString::Printf(TEXT("solved (largest change %.2f deg on %s)"), MaxTurn, *MaxBone.ToString())});
	AddPackage(Pose->GetOutermost());

	const bool bOldFlag = FlagProp->GetPropertyValue_InContainer(Cdo);
	Changes.Add({FString::Printf(TEXT("%s.%s"), *BP->GetName(), *FlagName.ToString()), bOldFlag ? TEXT("true") : TEXT("false"), TEXT("true")});
	AddPackage(BP->GetOutermost());

	Line(TEXT("changes:"));
	for (const FChange& C : Changes)
	{
		Line(FString::Printf(TEXT("  %s\n      now: %s\n      new: %s"), *C.What, *C.Old, *C.New));
	}
	Line(TEXT("packages written:"));
	for (const UPackage* Pkg : Packages)
	{
		Line(FString::Printf(TEXT("  %s  (%s)"), *Pkg->GetName(), *PackageFile(Pkg)));
	}
	for (const UPackage* Pkg : Packages)
	{
		if (Pkg->IsDirty())
		{
			Line(FString::Printf(TEXT("REFUSED: %s has unsaved changes; save or revert them first (Apply saves the whole package)"), *Pkg->GetName()));
			return false;
		}
	}
	if (bDryRun)
	{
		Line(TEXT("dry run: nothing written"));
		return true;
	}

	// --- backup ---
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H%M%S"));
	const FString BackupDir = FPaths::Combine(Profile.ResolveBackupDir(), Stamp + TEXT("_") + Profile.GetName());
	TArray<TPair<FString, FString>> Copied;
	for (const UPackage* Pkg : Packages)
	{
		if (!BackupPackage(Pkg, BackupDir, Copied, Error))
		{
			Line(TEXT("STOPPED before writing: ") + Error);
			return false;
		}
	}
	{
		ng::JWriter W(true);
		W.BeginObject();
		W.Key("profile").Value(std::string(TCHAR_TO_UTF8(*Profile.GetPathName())));
		W.Key("result").Value(std::string(TCHAR_TO_UTF8(*Solved.Path)));
		W.Key("restore").Value("close the editor, copy the files back over the 'from' paths");
		W.Key("files").BeginArray();
		for (const TPair<FString, FString>& C : Copied)
		{
			W.BeginObject();
			W.Key("from").Value(std::string(TCHAR_TO_UTF8(*C.Key)));
			W.Key("backup").Value(std::string(TCHAR_TO_UTF8(*C.Value)));
			W.EndObject();
		}
		W.EndArray();
		W.Key("changes").BeginArray();
		for (const FChange& C : Changes)
		{
			W.BeginObject();
			W.Key("what").Value(std::string(TCHAR_TO_UTF8(*C.What)));
			W.Key("old").Value(std::string(TCHAR_TO_UTF8(*C.Old)));
			W.Key("new").Value(std::string(TCHAR_TO_UTF8(*C.New)));
			W.EndObject();
		}
		W.EndArray();
		W.EndObject();
		if (!W.Save(std::string(TCHAR_TO_UTF8(*FPaths::Combine(BackupDir, TEXT("manifest.json"))))))
		{
			Line(TEXT("STOPPED before writing: cannot write the backup manifest"));
			return false;
		}
	}
	Line(FString::Printf(TEXT("backup: %d files -> %s"), Copied.Num(), *BackupDir));

	// --- write ---
	const FDateTime Before = FDateTime::UtcNow();
	if (Sock)
	{
		Sock->Modify();
		Sock->RelativeLocation = SockLocal.GetLocation();
		Sock->RelativeRotation = SockLocal.Rotator();
		Sock->RelativeScale = FVector::OneVector;
		Sock->GetOutermost()->MarkPackageDirty();
	}
	{
		const int32 NumKeys = FMath::Max(1, Pose->GetDataModel()->GetNumberOfKeys());
		IAnimationDataController& Ctrl = Pose->GetController();
		Ctrl.OpenBracket(LOCTEXT("ApplyGrasp", "AZ Natural Grip: apply the solved grasp"), false);
		for (const TPair<FName, FQuat>& L : Solved.Locals)
		{
			if (!Pose->GetDataModel()->IsValidBoneTrackName(L.Key))
			{
				Ctrl.AddBoneCurve(L.Key, false);
			}
			const FTransform& Old = UAnimPoseExtensions::GetBonePose(OldPose, L.Key, EAnimPoseSpaces::Local);
			TArray<FVector> Pos, Scl;
			TArray<FQuat> Rot;
			Pos.Init(Old.GetLocation(), NumKeys);
			Rot.Init(L.Value, NumKeys);
			Scl.Init(FVector::OneVector, NumKeys);
			Ctrl.SetBoneTrackKeys(L.Key, Pos, Rot, Scl, false);
		}
		Ctrl.CloseBracket(false);
		Pose->MarkPackageDirty();
	}
	Cdo->Modify();
	if (CorrProp)
	{
		*CorrProp->ContainerPtrToValuePtr<FTransform>(Cdo) = NewCorr;
	}
	FlagProp->SetPropertyValue_InContainer(Cdo, true);
	FKismetEditorUtilities::CompileBlueprint(BP);
	BP->MarkPackageDirty();
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Packages, false);

	// --- read back ---
	bool bOk = bSaved;
	Line(FString::Printf(TEXT("saved: %s"), bSaved ? TEXT("yes") : TEXT("NO")));
	if (Sock)
	{
		USkeletalMeshSocket* S2 = Mesh->FindSocket(Profile.LeftHandSocket);
		const FTransform Cs = FTransform(S2->RelativeRotation, S2->RelativeLocation, S2->RelativeScale) * SockBone;
		const double PosErr = (Cs.GetLocation() - Solved.Hand.GetLocation()).Size();
		const double RotErr = AngleDeg(Cs.GetRotation(), Solved.Hand.GetRotation());
		Line(FString::Printf(TEXT("check socket: position error %.5f cm, rotation error %.4f deg"), PosErr, RotErr));
		bOk = bOk && PosErr < 1e-3 && RotErr < 1e-3;
	}
	{
		FAnimPose NewPose;
		UAnimPoseExtensions::GetAnimPoseAtTime(Pose, 0.0, FAnimPoseEvaluationOptions(), NewPose);
		double Worst = 0.0;
		for (const TPair<FName, FQuat>& L : Solved.Locals)
		{
			Worst = FMath::Max(Worst, AngleDeg(UAnimPoseExtensions::GetBonePose(NewPose, L.Key, EAnimPoseSpaces::Local).GetRotation(), L.Value));
		}
		Line(FString::Printf(TEXT("check grip pose: largest rotation error %.4f deg"), Worst));
		bOk = bOk && Worst < 1e-2;
	}
	{
		UObject* Cdo2 = BP->GeneratedClass->GetDefaultObject();
		const bool bFlag = FindFProperty<FBoolProperty>(BP->GeneratedClass, FlagName)->GetPropertyValue_InContainer(Cdo2);
		FString CorrCheck;
		if (bTrigger)
		{
			const FTransform C2 = *FindFProperty<FStructProperty>(BP->GeneratedClass, Profile.CorrectionProperty)->ContainerPtrToValuePtr<FTransform>(Cdo2);
			const double PosErr = (C2.GetLocation() - NewCorr.GetLocation()).Size();
			const double RotErr = AngleDeg(C2.GetRotation(), NewCorr.GetRotation());
			CorrCheck = FString::Printf(TEXT(", correction error %.5f cm / %.4f deg"), PosErr, RotErr);
			bOk = bOk && PosErr < 1e-4 && RotErr < 1e-4;
		}
		Line(FString::Printf(TEXT("check blueprint: %s = %s%s"), *FlagName.ToString(), bFlag ? TEXT("true") : TEXT("false"), *CorrCheck));
		bOk = bOk && bFlag;
	}
	for (const UPackage* Pkg : Packages)
	{
		const FDateTime Stamp2 = IFileManager::Get().GetTimeStamp(*PackageFile(Pkg));
		const bool bNew = Stamp2 >= Before - FTimespan::FromSeconds(2);
		Line(FString::Printf(TEXT("check file: %s written %s"), *Pkg->GetName(), bNew ? TEXT("now") : TEXT("NOT (old timestamp)")));
		bOk = bOk && bNew;
	}
	Line(bOk ? FString(TEXT("APPLIED")) : FString::Printf(TEXT("APPLY CHECK FAILED (backup: %s)"), *BackupDir));
	return bOk;
}

#undef LOCTEXT_NAMESPACE
