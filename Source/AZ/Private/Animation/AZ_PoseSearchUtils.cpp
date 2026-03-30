#include "Animation/AZ_PoseSearchUtils.h"
#include "PoseSearch/PoseSearchDatabase.h"

bool UAZ_PoseSearchUtils::AddSequenceToDatabase(UPoseSearchDatabase* Database, UAnimSequence* Sequence)
{
	if (!Database || !Sequence)
	{
		return false;
	}

	FPoseSearchDatabaseAnimationAsset NewAsset;
	NewAsset.AnimAsset = Sequence;

	Database->AddAnimationAsset(NewAsset);
	return true;
}

int32 UAZ_PoseSearchUtils::AddSequencesToDatabase(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Sequences)
{
	if (!Database)
	{
		return 0;
	}

	int32 Added = 0;
	for (UAnimSequence* Seq : Sequences)
	{
		if (Seq)
		{
			FPoseSearchDatabaseAnimationAsset NewAsset;
			NewAsset.AnimAsset = Seq;
			Database->AddAnimationAsset(NewAsset);
			Added++;
		}
	}

	return Added;
}

void UAZ_PoseSearchUtils::RemoveAnimationAtIndex(UPoseSearchDatabase* Database, int32 Index)
{
	if (Database && Index >= 0 && Index < Database->GetNumAnimationAssets())
	{
		Database->RemoveAnimationAssetAt(Index);
	}
}

void UAZ_PoseSearchUtils::ClearDatabase(UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return;
	}

	while (Database->GetNumAnimationAssets() > 0)
	{
		Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets() - 1);
	}
}
