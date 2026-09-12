// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_WeaponAnimationProfile.generated.h"

class UBlendSpace;
class UAnimSequence;
class UPoseSearchDatabase;

/** A character pose and the authored sequence time for its weapon/socket transfer. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponSwitchAnimation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	TObjectPtr<UAnimSequence> Animation = nullptr;
	/** Seconds on the source clip timeline, before play-rate scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch", meta=(ClampMin="0", ForceUnits="s"))
	float AttachTime = 0.3f;
};

/** Weapon animation tuning and optional pools; the existing main CHT remains the only clip selector. */
UCLASS(BlueprintType)
class AZ_API UAZ_WeaponAnimationProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Optional crouched clips fall back to the standing upper-body pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	FAZ_WeaponSwitchAnimation StandingDraw;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	FAZ_WeaponSwitchAnimation StandingHolster;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	FAZ_WeaponSwitchAnimation CrouchingDraw;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	FAZ_WeaponSwitchAnimation CrouchingHolster;
	/** Reuses the existing masked upper-body slot; locomotion retains the legs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch")
	FName SwitchAnimationSlot = TEXT("RifleFire");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch", meta=(ClampMin="0.01"))
	float SwitchAnimationPlayRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch", meta=(ClampMin="0", ClampMax="1"))
	float SwitchAnimationBlendIn = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch", meta=(ClampMin="0", ClampMax="1"))
	float SwitchAnimationBlendOut = 0.12f;
	/** Visual transfer duration in game seconds; preserves the current world placement before blending. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|Animation|Switch", meta=(ClampMin="0", ClampMax="1", ForceUnits="s"))
	float SwitchSocketBlendDuration = 0.15f;

	/** Null pools retain the main chooser's selected clip; weapon profiles never borrow an unarmed database. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> WalkLocoDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> RunLocoDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeWalkDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeRunDatabase = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeCrouchDatabase = nullptr;

	/** Enable only after verifying that this profile's transitions end at their matching loop's frame zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bPhaseLockedTransitionToLoop = false;

	/** The current MHC graph contains unarmed additive lean assets. Most weapon profiles must bypass them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bUseUnarmedLeans = false;

	/** THREE-PHASE JUMP. Enable for profiles whose takeoff clips are CLOSED (a baked landing and only a few
	 *  tenths of a second of air, like the P01 rm_W2_*_Jump pack): at the takeoff's apex handoff the SM
	 *  advances to InAirLoop and the chooser's InAirLoop rows push a LOOPING air cycle for the rest of the
	 *  fall, so a drop longer than the takeoff clip cannot freeze on its last frame. Touchdown still selects
	 *  the land clip on real floor contact, unchanged.
	 *  False = the unarmed TWO-PHASE jump: TransitionToInAir persists for the whole airborne duration and the
	 *  takeoff clip's own open-ended fall tail covers the descent.
	 *  Requires InAirLoop rows in CHT_v2 matching this profile's weapon, and those rows' clips must LOOP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation")
	bool bUseAirLoop = false;

	/** Opt in only after verifying the listed moving loops' depicted-speed curves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	bool bUseLoopPlayRate = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	FName SpeedCurveName = TEXT("Speed");
	/** Explicit moving-loop ownership; idle curves can contain nonzero values and must not qualify. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate")
	TArray<TObjectPtr<UAnimSequence>> PlayRateLoopAssets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0.01"))
	float LoopPlayRateMin = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0.01"))
	float LoopPlayRateMax = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|PlayRate", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LoopPlayRateMinDepictedSpeed = 1.f;

	/** Optional authored lowered-weapon pose, layered above spine_02 during relaxed locomotion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Relaxed")
	TObjectPtr<UAnimSequence> RelaxedUpperBodyPose = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Relaxed", meta = (ClampMin = "0"))
	float RelaxedPoseBlendSpeed = 8.f;

	/** Authored full poses; the firearm slot is masked to the upper body before the aim offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire")
	TObjectPtr<UAnimSequence> SingleFireAnimation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire")
	TObjectPtr<UAnimSequence> AutomaticFireAnimation = nullptr;
	/** Crouched-stance fire poses, selected at fire start from the Mover's crouch state — the same per-stance
	 *  selection the reload poses use. Leave unset to fall back to the standing clip for that mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire")
	TObjectPtr<UAnimSequence> CrouchingSingleFireAnimation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire")
	TObjectPtr<UAnimSequence> CrouchingAutomaticFireAnimation = nullptr;
	/** Number of recoil pulses in one authored loop; playback is scaled to the weapon's shots per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire", meta = (ClampMin = "1"))
	int32 AutomaticFireAnimationShotsPerCycle = 1;
	/** This slot must belong to WeaponFire on the animation and character skeletons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire")
	FName FireAnimationSlot = TEXT("RifleFire");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire", meta = (ClampMin = "0", ClampMax = "1"))
	float FireAnimationBlendIn = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Fire", meta = (ClampMin = "0", ClampMax = "1"))
	float FireAnimationBlendOut = 0.08f;

	/** One-shot reload poses selected from stance and aim at reload start; uses the masked RifleFire slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload")
	TObjectPtr<UAnimSequence> StandingReloadAnimation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload")
	TObjectPtr<UAnimSequence> StandingAimReloadAnimation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload")
	TObjectPtr<UAnimSequence> CrouchingReloadAnimation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload")
	TObjectPtr<UAnimSequence> CrouchingAimReloadAnimation = nullptr;
	/** Gameplay completes at the selected clip's full duration, including its own sequence RateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload", meta = (ClampMin = "0.01"))
	float ReloadAnimationPlayRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload", meta = (ClampMin = "0", ClampMax = "1"))
	float ReloadAnimationBlendIn = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Reload", meta = (ClampMin = "0", ClampMax = "1"))
	float ReloadAnimationBlendOut = 0.15f;

	/** PER-WEAPON GAIT SPEEDS. Leave at 0 to keep the walking mode's own value for that gait.
	 *
	 *  WHY this exists: a weapon set's loops depict whatever speed they were authored at, and dragging the capsule
	 *  faster than that slides the feet. Measured 2026-09-11 against the mode's 165/450/200: the pistol set depicts
	 *  159-206 walking, so it matches and looks right at play rate 1; the RIFLE set depicts a cautious 98-130 walk
	 *  (median 123), 227-399 jog (median 347) and a 78-102 crouch walk (median 93) — a 34% / 30% / 115% overspeed,
	 *  i.e. a permanent skate in every direction. Scaling the clips up instead would be visible fast-motion, so the
	 *  speed comes down to the animation, which is also the honest read: a rifle in a ready stance walks slower.
	 *
	 *  Applied on the game thread whenever the committed weapon changes (AAZ_PawnMoverHeroCharacter), not per
	 *  frame and not through the sim input — equipment is replicated state, so every machine converges on the same
	 *  numbers, and a rollback re-simulates with the values already in place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WalkSpeedOverride = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunSpeedOverride = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeedOverride = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeedOverride = 0.f;

	/** Authored angular rate of this set's aim turn-in-place LOOP clips (deg/s). The anim instance plays those clips
	 *  at (actual body yaw rate / this), so the feet step exactly as fast as the capsule turns at ANY turn speed —
	 *  the walking mode's AimTurnInPlaceRateDegPerSec then becomes a pure feel knob. Rifle set: 45 deg per 0.67 s
	 *  loop = 67. Measure a new set with the pelvis-yaw swing of the NON-IPC twin over one loop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "1", ForceUnits = "deg/s"))
	float AimTurnInPlaceClipRateDegPerSec = 67.f;

	/** Full upper-body source poses for the aim lock, before the additive aim offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UAnimSequence> StandingAimPose = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UAnimSequence> CrouchingAimPose = nullptr;

	/** Mesh-space additive aim offsets. X = local aim yaw, Y = local aim pitch, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UBlendSpace> StandingAimOffset = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim")
	TObjectPtr<UBlendSpace> CrouchingAimOffset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimInterpSpeed = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimBlendInSpeed = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0"))
	float AimBlendOutSpeed = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxAimYaw = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float MaxAimPitch = 90.f;
	/** Aim-offset YAW range fade. The aim offset may only carry the small residual between camera and body; when the
	 *  residual is large the body is turning to the camera and the torso must not twist to the clamp and then unwind
	 *  as the legs arrive. Recorded 2026-09-11: aim pressed 162 deg off -> the torso snapped to the +90 clamp, then
	 *  swung back to 0 over the last 0.17 s of the body turn - the "upper body sways, lower body is fine" report.
	 *  Fade weight = 1 at |camera-body| <= FadeStart, 0 at >= FadeEnd; TargetAimYaw = clamp(delta) * weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "0", ForceUnits = "degrees"))
	float AimOffsetYawFadeStartDeg = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Animation|Aim", meta = (ClampMin = "1", ForceUnits = "degrees"))
	float AimOffsetYawFadeEndDeg = 60.f;
};
