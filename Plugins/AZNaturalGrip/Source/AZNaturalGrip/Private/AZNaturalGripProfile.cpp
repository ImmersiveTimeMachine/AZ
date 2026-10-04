// Copyright Artur. AZ project.

#include "AZNaturalGripProfile.h"

#include "Misc/App.h"
#include "Misc/Paths.h"

FString UAZNaturalGripProfile::ResolveDataDir() const
{
	FString Dir = DataDir.Path.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("wgs")) : DataDir.Path;
	Dir = FPaths::ConvertRelativePathToFull(Dir);
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}

FString UAZNaturalGripProfile::ResolveBackupDir() const
{
	FString Dir = BackupDir.Path;
	if (Dir.IsEmpty())
	{
		Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), FString(FApp::GetProjectName()) + TEXT("_Backups"));
	}
	Dir = FPaths::ConvertRelativePathToFull(Dir);
	FPaths::CollapseRelativeDirectories(Dir);
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}
