#pragma once

#include "CoreMinimal.h"

class AAZ_Weapon;

/** Module-owned observer; only reads finalized game-world poses. No UObject/static delegate lifetime. */
class FAZ_WeaponPoseDebug
{
public:
	void Startup();
	void Shutdown();

private:
	void OnEndFrame();
	struct FSampleState
	{
		double LastSeconds = 0.;
		FString Mode;
	};
	FDelegateHandle EndFrameHandle;
	TMap<TWeakObjectPtr<AAZ_Weapon>, FSampleState> Samples;
};
