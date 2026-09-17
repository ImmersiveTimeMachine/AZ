// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_ThrowPresentationProfile.generated.h"

class UAnimMontage;
class UAnimSequence;

/**
 * UAZ_ThrowPresentationProfile — how a throw LOOKS and where the object leaves the hand.
 *
 * Separated from UAZ_ThrowableDefinition because presentation varies by WEAPON CONTEXT while the item does
 * not: the same stone is thrown left-handed with a rifle still in the right hand, or right-handed unarmed.
 * One item therefore references several of these, and the equipment/hand situation picks one.
 *
 * ★ Release times here are MEASURED CANDIDATES, not proven physical release instants, and they are
 * per-clip: they must be re-measured for every family rather than copied. The MH pistol clips carry no
 * release notify, and thumb-to-middle fingertip distance is constant at 9.37cm throughout, so that
 * particular metric offers no opening cue (which is not the same as proving every finger track is static).
 * The numbers below come from arm kinematics — vertical hand velocity crossing zero at the arc apex/nadir
 * while horizontal hand speed peaks. That identifies a useful candidate; it does not prove the exact
 * instant the object leaves the hand, so each one needs confirmation against the real held prop, the
 * finalized pose and the first launched frame. Method, step size and evidence:
 * docs/design-briefs/throwable-phase0-status.md §1.3.
 */
UCLASS(BlueprintType)
class AZ_API UAZ_ThrowPresentationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Animation family ------------------------------------------------------------------------
	// Separate montages rather than one sectioned montage: Loop must repeat indefinitely while the button is
	// held and the others are one-shots, and the measured seams are exact to 0.006-0.009cm, so a short blend
	// between assets reproduces the authored continuity without section bookkeeping.

	/** Carriage -> ready pose. Entering Loop directly would skip this motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> StartMontage;

	/** Held ready pose. Repeats for as long as the player holds the button. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> LoopMontage;

	/**
	 * ONE throw shape, or two.
	 *
	 * false (default, user call 2026-09-16): both arcs play FarMontage and use its cue and anchor, so the
	 * character always throws the same way and only the BALLISTICS change - speed, plus the per-arc origin
	 * lift below. Swapping the clip made the whole motion jump between an overhand and an underhand lob as
	 * the player panned, which reads as the animation glitching rather than as a choice.
	 *
	 * true restores the authored two-clip behaviour; CloseMontage/CloseReleaseTime/CloseReleaseAnchor are
	 * measured and kept for that, and for families whose close throw is genuinely a different action.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	bool bSeparateArcMontages = false;

	/** Short throw + authored recovery. Only used when bSeparateArcMontages is true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> CloseMontage;

	/** Long throw + authored recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> FarMontage;

	/**
	 * Pose held while the item is merely READIED, layered over the legs so the character carries it around
	 * instead of standing still (user call 2026-09-16).
	 *
	 * ★ This is NOT a montage and must not be one. It rides the same upper-body lane the lowered-weapon pose
	 * uses (UAZ_MoverAnimInstance's WeaponRelaxedPose, layered above spine_02), which is exactly why walking,
	 * turning and crouching keep working underneath it. A FullBody loop montage would look the same standing
	 * still and root the player the moment they tried to move.
	 */
	/**
	 * Looping held-item idle, played while the item is READIED and no action owns the body.
	 *
	 * ★ A MONTAGE on the upper-body slot, not a pose handed to a variable. The MetaHuman hero's graph has no
	 * consumer for the relaxed-pose lane at all (verified 2026-09-17: the only upper-body references in
	 * AZ_ABP_MoverHero_MHC are AimAlpha and AimStanceAlpha), so assigning a sequence there animated nothing.
	 * A montage on the wired slot plays, advances and loops, and locomotion keeps root/pelvis/legs underneath.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> CarryMontage;

	/** Legacy pose lane, kept for families whose ABP does consume RelaxedUpperBodyPose. Unused by the MHC
	 *  hero; CarryMontage is what actually drives its held idle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimSequence> CarryPose;

	/** Lower out of the ready pose without throwing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	TObjectPtr<UAnimMontage> CancelMontage;

	/**
	 * The slot this family's montages are authored on. Record only — the montage asset itself carries the
	 * slot, and nothing reads this at runtime; it exists so a profile states which lane it belongs to.
	 *
	 * ★ FullBody for the unarmed family, and that is a CONTENT fact, not a preference. The MovementAnimsetPro
	 * throw swings the pelvis ~176 degrees and repositions the feet (measured 2026-09-16): on an upper-body
	 * slot that rotation would float on standing legs. The cost is real and deliberate — the player is
	 * rooted while aiming and throwing. A family authored as a torso-only throw would set an upper-body slot
	 * here and rebuild its montages on it; nothing else changes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation")
	FName MontageSlot = FName("FullBody");

	/** Blend used when branching out of an arbitrary Loop frame. A section/montage swap is not itself a
	 *  crossfade, and the player can release on any frame of the loop (unarmed: 0.833s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float BranchBlendTime = 0.12f;

	// ---- Measured cues ---------------------------------------------------------------------------

	/**
	 * Release time within CloseMontage. EVERY FAMILY MUST BE RE-MEASURED — these defaults are the unarmed
	 * MovementAnimsetPro numbers, and the MH pistol family measured 0.280/0.385 from the same method, so
	 * carrying one family's cue onto another throws the object from the wrong frame.
	 *
	 * Method: peak horizontal speed of the throwing hand, coinciding with the zero-crossing of its vertical
	 * velocity (the apex or nadir of the hand's arc). Unarmed Close is an underhand lob, so the crossing is
	 * a nadir.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Cues", meta = (ClampMin = "0", ForceUnits = "s"))
	float CloseReleaseTime = 0.490f;

	/** Release time within FarMontage. Unarmed overhand: peak hand speed 910 cm/s at the arc apex. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Cues", meta = (ClampMin = "0", ForceUnits = "s"))
	float FarReleaseTime = 0.700f;

	/**
	 * Where Start reaches the pose every continuation begins from. Unarmed: 0.900 (Start's own end),
	 * verified exact — Start's last frame matches Loop/Close/Far/Cancel's first across all 342 bones to
	 * within 0.009cm. MH pistol measured 0.766667 by the same test.
	 *
	 * A button release that arrives DURING Start latches one commit and branches here, so a quick tap still
	 * throws instead of being refused. Branching earlier than this has no supporting evidence and must not
	 * be invented.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Cues", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReadySeamTime = 0.900f;

	/**
	 * Where the grip bone IS at the moment of release, in mesh component space, per arc.
	 *
	 * ★ This is the calibrated preview anchor the plan requires, and it exists because the Loop hand pose is
	 * NOT the release pose. Previewing from the live held hand would draw an arc starting a metre from where
	 * the object actually leaves: the unarmed family releases Close at z 37.3 (an underhand lob past the
	 * hip) and Far at z 143.5 (overhand above the head) — 106cm apart. During aiming the preview uses these;
	 * at the actual release the live bone transform is queried instead and the geometry is re-checked.
	 *
	 * ★ The component-space frame of this pack has FORWARD = +Y (measured: every RunFwd/WalkFwd root delta
	 * is pure +Y), which is exactly what the hero mesh's -90 degree yaw offset undoes. Composing
	 * Anchor * Mesh->GetComponentTransform() is therefore not a formality — it is what turns +Y into the
	 * actor's forward.
	 *
	 * ★ These are measured AT THE GRIP BONE BELOW, not at hand_r — the hero's RightHandGrenageSocket sits
	 * 10.96cm from the wrist. Anchoring the preview on the bone while the release reads the socket would put
	 * the drawn arc 11cm from where the object actually leaves, every single throw. Re-measure BOTH whenever
	 * GripBone changes: Anchor = SocketParentRelative * BonePoseAtRelease.
	 *
	 * Defaults measured from the unarmed family at the grenade socket; at hand_r the same frames gave
	 * (-17.55, 31.69, 37.31) and (-7.16, 65.21, 143.51), and the MH pistol family measured
	 * (26.8, 41.0, 52.9) / (31.8, 61.3, 145.2). See throwable-phase0-status.md.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Cues")
	FTransform CloseReleaseAnchor = FTransform(FRotator(-24.00f, 130.33f, 86.19f), FVector(-22.77f, 33.06f, 27.77f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Cues")
	FTransform FarReleaseAnchor = FTransform(FRotator(78.53f, -4.91f, -65.60f), FVector(-12.91f, 66.25f, 152.78f));

	/**
	 * ★ SPACE: MESH COMPONENT space — the space the pose was sampled in — NOT actor space and not world.
	 * Compose it as Anchor * Mesh->GetComponentTransform(), then apply GripOffset. A component-space XYZ is
	 * not a projectile origin and using one directly would silently ignore the mesh's own transform,
	 * including the base rotation offset the MetaHuman body carries.
	 *
	 * Rotation is the grip's own orientation at the release frame, carried so the anchor is a full transform
	 * and not just a point; the launch DIRECTION never comes from it, only from the accepted aim.
	 */
	FTransform GetReleaseAnchor(EAZ_ThrowArc Arc) const
	{
		return UsesCloseClip(Arc) ? CloseReleaseAnchor : FarReleaseAnchor;
	}

	/** True only when this profile really plays a separate close clip. Everything per-arc keys off this, so
	 *  the cue time, the anchor and the montage can never disagree about which clip is running. */
	bool UsesCloseClip(EAZ_ThrowArc Arc) const
	{
		return bSeparateArcMontages && Arc == EAZ_ThrowArc::Close && CloseMontage != nullptr;
	}

	// ---- Grip ------------------------------------------------------------------------------------

	/**
	 * Bone or socket the held object rides, and the hand that throws.
	 *
	 * Handedness is per profile and NOT a global constant: the rifle/pistol families swing hand_l (155cm of
	 * travel versus 47cm on the right) because the other hand keeps the weapon, while the unarmed family
	 * swings hand_r. Hardcoding either would break the other context.
	 *
	 * A SOCKET is preferred over a bare bone — DoesSocketExist accepts both — because a socket is authored
	 * where the object actually sits in the fingers. The hero's RightHandGrenageSocket sits 10.96cm from
	 * hand_r, so the release anchors above must be re-measured whenever this changes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Grip")
	FName GripBone = FName("RightHandGrenageSocket");

	/** Local offset from that bone/socket to where the object actually sits. Identity when GripBone is a
	 *  calibrated socket, which already carries the offset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Throw|Grip")
	FTransform GripOffset = FTransform::Identity;

	/** Release time for the chosen arc. */
	float GetReleaseTime(EAZ_ThrowArc Arc) const
	{
		return UsesCloseClip(Arc) ? CloseReleaseTime : FarReleaseTime;
	}

	UAnimMontage* GetReleaseMontage(EAZ_ThrowArc Arc) const
	{
		return UsesCloseClip(Arc) ? CloseMontage : FarMontage;
	}

	bool IsUsable() const
	{
		// CloseMontage is optional: with one throw shape the far clip serves both arcs.
		return StartMontage && LoopMontage && FarMontage && (!bSeparateArcMontages || CloseMontage);
	}
};
