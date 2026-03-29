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
