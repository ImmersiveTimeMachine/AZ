#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_SkeletonUtils.generated.h"

class USkeleton;
class UBlendProfile;

/**
 * Utility functions for Skeleton and BlendProfile manipulation.
 * Exposes blend profile creation/copying to Blueprint/Python since the engine doesn't fully expose these.
 */
UCLASS()
class AZ_API UAZ_SkeletonUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Get all blend profile names on a skeleton. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static TArray<FName> GetBlendProfileNames(USkeleton* Skeleton);

	/** Get per-bone weights for a blend profile. Returns bone name -> scale pairs for non-default entries. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static TMap<FName, float> GetBlendProfileBoneWeights(USkeleton* Skeleton, FName ProfileName);

	/** Create a new empty blend profile on a skeleton. Returns the profile or nullptr if it already exists. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static UBlendProfile* CreateBlendProfile(USkeleton* Skeleton, FName ProfileName);

	/** Set a single bone's blend scale in a profile. Creates the entry if it doesn't exist. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static bool SetBlendProfileBoneWeight(USkeleton* Skeleton, FName ProfileName, FName BoneName, float Scale, bool bRecurse = false);

	/** Copy a blend profile from one skeleton to another. Both must share compatible bone hierarchies.
	 *  @return true if the profile was created and weights copied successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static bool CopyBlendProfile(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton, FName ProfileName);

	/** Copy all blend profiles from one skeleton to another. Skips profiles that already exist on target.
	 *  @return Number of profiles copied.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static int32 CopyAllBlendProfiles(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton);

	/** Get all bone names from a skeleton. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static TArray<FName> GetBoneNames(USkeleton* Skeleton);

	/** Register a montage slot in its group without disturbing other slots. Does not save. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton|Animation")
	static bool SetAnimationSlotGroup(USkeleton* Skeleton, FName SlotName, FName GroupName);

	UFUNCTION(BlueprintPure, Category = "AZ|Skeleton|Animation")
	static FName GetAnimationSlotGroup(USkeleton* Skeleton, FName SlotName);

	/** Re-route which animation slot a montage plays on, preserving its segments, notifies and timing.
	 *
	 *  ★ Exists because Python CANNOT do this. FSlotAnimationTrack lives in a TArray<FStruct>, and the Python
	 *  bindings hand back a COPY: setting SlotName on it reports success, and writing the whole array back
	 *  still leaves the asset unchanged (verified on AM_AZ_Throw_Carry, 2026-09-18). Scripted montage
	 *  re-routing silently no-ops without this.
	 *
	 *  Refuses multi-track montages rather than guessing which track was meant. Marks the package dirty but
	 *  does NOT save - callers save by path and verify by mtime.
	 *
	 *  @param Montage      The montage to re-route.
	 *  @param NewSlotName  Target slot. Must already exist in the skeleton's slot/group table.
	 *  @return true if the slot changed; false on a bad asset, a multi-track montage, or if it already matched.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton|Animation")
	static bool SetMontageSlotName(UAnimMontage* Montage, FName NewSlotName);

	// --- Sockets ---
	// Python cannot touch these at all: USkeleton::Sockets is protected to the reflection layer and
	// USkeletalMeshSocket::SocketName is read-only, so a socket can otherwise only be authored by hand
	// in the Skeleton editor. In C++ both are public, hence this bridge.

	/** Create (or update) a socket on a bone. Does NOT save — save the skeleton from the caller.
	 *  @param bReplaceExisting  false = leave an existing socket of that name untouched and return false.
	 *  @return true if the socket was created or updated. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static bool AddSocket(USkeleton* Skeleton, FName SocketName, FName BoneName,
		FVector RelativeLocation, FRotator RelativeRotation, bool bReplaceExisting = true);

	/** Remove a socket by name. @return true if one was found and removed. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static bool RemoveSocket(USkeleton* Skeleton, FName SocketName);

	/** One line per socket: "name bone=.. loc=.. rot=..". The only way to read them back from script. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Skeleton")
	static TArray<FString> ListSockets(USkeleton* Skeleton);
};
