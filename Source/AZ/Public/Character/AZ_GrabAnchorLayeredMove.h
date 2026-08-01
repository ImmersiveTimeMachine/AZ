// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "AZ_GrabAnchorLayeredMove.generated.h"

class USkeletalMeshComponent;

/**
 * FLayeredMove_AZ_GrabAnchor — ATTACHMENT for a live Mover pawn.
 *
 * WHY THIS EXISTS:
 *   AttachToComponent cannot hold a Mover pawn: the movement simulation stamps the capsule's world
 *   transform every tick, so the attachment's relative offset is recomputed away each frame. Mover's
 *   own mechanism for "something else owns my motion right now" is a layered move — so the attachment
 *   is expressed as one. The capsule stays fully under Mover (collision, floor snap, depenetration all
 *   keep working); only WHERE it wants to be is dictated from outside.
 *
 * WHAT IT DOES (socket-to-socket lock):
 *   Every sim tick it measures the world gap between the ANCHOR socket (the grabber's holding hand,
 *   e.g. hand_l on the Chalkie) and the GRABBED socket (the victim's own anchor point, e.g.
 *   GrabbedSocket on the hero's spine_05) and proposes exactly the velocity that closes it this tick.
 *   Because the mesh rides the capsule rigidly, moving the capsule by that gap moves the socket by it
 *   too — so the two sockets coincide and STAY coincident as the grabber's arm animates. That is what
 *   attachment looks like, minus the fight with the sim.
 *
 *   MaxCorrectionSpeed caps the corrective velocity, which does double duty: it turns the initial catch
 *   into a visible snatch instead of a teleport, and it damps the frame-to-frame chase once locked (the
 *   victim's own struggle animation moves its socket every frame, so the correction never fully stops).
 *
 * VERTICAL — DELIBERATELY NOT HERE:
 *   This move is XY-only, and no switch changes that. The hold runs in Walking mode, whose SimulationTick
 *   calls TryMoveToAdjustHeightAboveFloor EVERY tick and snaps the capsule straight back to the floor
 *   (the same mechanism that ate the jump's lift and forced UAZ_PawnMovementMode_RMAction into existence).
 *   Any Z we proposed here would be undone before it was ever drawn. Height matching is therefore done on
 *   the MESH instead — AAZ_PawnMoverHeroCharacter::UpdateGrabMeshAnchor lifts the mesh inside the capsule,
 *   so the victim can hang off the ground while the capsule stays grounded and collidable.
 *
 * LIFETIME:
 *   Queued by GA_PlayerGrabbed on the VICTIM's mover, cancelled on every exit path via
 *   CancelFeaturesWithTag(Mover.AZ.GrabAnchor). DurationMs is a backstop only — it must outlive the
 *   grabber's own MaxHoldSeconds so the tag cancel is what normally ends it.
 *
 * SP-FIRST: the socket transforms are read from live components inside the sim, so this move does not
 * survive a NetworkPrediction resimulation bit-exactly (the meshes are at their current pose, not the
 * replayed one). Acceptable for single-player; a co-op pass would ship the anchor point through the
 * InputCmd instead. Not async-safe either (SupportsAsync stays false — component reads are game-thread).
 */
USTRUCT()
struct AZ_API FLayeredMove_AZ_GrabAnchor : public FLayeredMoveBase
{
	GENERATED_BODY()

	FLayeredMove_AZ_GrabAnchor();
	virtual ~FLayeredMove_AZ_GrabAnchor() = default;

	/** The GRABBER's mesh — the thing we hang off. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> AnchorMesh = nullptr;

	/** Socket or bone on AnchorMesh that does the holding (Chalkie: "hand_l"). */
	UPROPERTY()
	FName AnchorSocket = NAME_None;

	/** The VICTIM's mesh — this move drives the pawn that owns it. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> GrabbedMesh = nullptr;

	/** Socket or bone on GrabbedMesh that gets held (hero: "GrabbedSocket" on spine_05). */
	UPROPERTY()
	FName GrabbedSocket = NAME_None;

	/** Extra offset in ANCHOR-SOCKET space (rotates with the hand): +X pushes further along the hand's
	 *  forward, +Y sideways, +Z up. Tunes the clinch without re-authoring the socket. */
	UPROPERTY()
	FVector AnchorSpaceOffset = FVector::ZeroVector;

	/** Ceiling on the corrective velocity, cm/s. 0 = uncapped (instant snap, reads as a teleport). */
	UPROPERTY()
	float MaxCorrectionSpeed = 500.f;

	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	/** Also finishes if either end of the lock has gone away (grabber destroyed mid-hold). */
	virtual bool IsFinished(double CurrentSimTimeMs) const override;

	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits<FLayeredMove_AZ_GrabAnchor> : public TStructOpsTypeTraitsBase2<FLayeredMove_AZ_GrabAnchor>
{
	enum
	{
		WithCopy = true
	};
};
