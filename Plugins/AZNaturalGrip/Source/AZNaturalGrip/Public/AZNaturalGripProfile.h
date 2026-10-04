// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZNaturalGripProfile.generated.h"

class UAnimSequence;
class UBlueprint;
class USkeletalMesh;

/** The hand's ROLE = which grasp rules solve it (the clips only say WHERE the hand is on the weapon). */
UENUM()
enum class EAZGripHand : uint8
{
	/** Right hand on a pistol grip: index pad on the trigger, the other fingers round the grip, thumb on the far side
	 *  (rifle, carbine, pistol). */
	Trigger UMETA(DisplayName = "Trigger: pistol grip (right)"),
	/** Left hand on the handguard / forend / pump: power grasp, closure around it, thumb along the near side. */
	Support UMETA(DisplayName = "Handguard / forend / pump (left)"),
	/** Right hand on a straight stock wrist (shotgun, lever rifle): the trigger rules without the pistol-grip limits. */
	TriggerStraight UMETA(DisplayName = "Trigger: straight wrist / lever (right)"),
	/** A handle (knife, machete, bat, axe): all four fingers round it, thumb closing it; no trigger. Side = Side. */
	Handle UMETA(DisplayName = "Handle: knife / melee"),
	/** Pistol, second hand: the left hand cups the right hand and the grip - palm on the free side of the grip, fingers
	 *  over the right fingers, thumb along the right thumb pointing forward. Needs OtherHand (the right hand's profile,
	 *  solved first). */
	PistolCup UMETA(DisplayName = "Pistol second hand (left)"),
};

/** Weapon type = a set of hand roles (Tools > AZ Natural Grip > Create profiles). */
UENUM()
enum class EAZWeaponGripType : uint8
{
	Rifle UMETA(DisplayName = "Rifle / carbine: trigger + handguard"),
	Shotgun UMETA(DisplayName = "Shotgun / lever rifle: straight wrist + forend"),
	Pistol UMETA(DisplayName = "Pistol / revolver: trigger + second hand"),
	Knife UMETA(DisplayName = "Knife / one-hand melee: handle"),
	TwoHandMelee UMETA(DisplayName = "Two-hand melee: handle + handle"),
};

UENUM()
enum class EAZGripSide : uint8
{
	Right,
	Left,
};

/** Where the solver's inputs come from. */
UENUM()
enum class EAZGripInputSource : uint8
{
	/** The JSON files the Python solver reads (Saved/wgs): exact parity with the Python results. */
	PythonFiles UMETA(DisplayName = "Python data files (parity)"),
	/** Read the hero and weapon meshes directly (no export step). The hold still comes from the data files. */
	Assets UMETA(DisplayName = "Assets (direct)"),
};

/** A hand placement relative to the clip hold: rotation about the hold's middle knuckle (degrees, weapon axes) then a
 *  translation (cm, weapon space). Same parameters as the Python solver's p = (yaw, pitch, roll, dx, dy, dz). */
USTRUCT(BlueprintType)
struct FAZGripPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") double Yaw = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") double Pitch = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") double Roll = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector Offset = FVector::ZeroVector;

	FAZGripPlacement() = default;
	FAZGripPlacement(double InYaw, double InPitch, double InRoll, const FVector& InOffset)
		: Yaw(InYaw), Pitch(InPitch), Roll(InRoll), Offset(InOffset) {}

	void ToArray(double Out[6]) const
	{
		Out[0] = Yaw; Out[1] = Pitch; Out[2] = Roll; Out[3] = Offset.X; Out[4] = Offset.Y; Out[5] = Offset.Z;
	}
};

/**
 * One weapon hand for the AZ Natural Grip solver (Tools > AZ Natural Grip). Holds what the Python solver took from its
 * NATGRIP_* environment and script constants; the defaults are the M16 values of 2026-10-01.
 * See docs/design-briefs/natural-grip-cpp-plugin.md.
 */
UCLASS(BlueprintType)
class AZNATURALGRIP_API UAZNaturalGripProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- weapon and hand ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	/** File key of the weapon in the data folder (Saved/wgs/<key>_hold_pick.json, fields/<key>_field.json, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FString WeaponKey = TEXT("m16");

	/** The hand's role (which grasp rules). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand", meta = (DisplayName = "Role"))
	EAZGripHand Hand = EAZGripHand::Support;

	/** Handle role: which hand holds it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand", meta = (EditCondition = "Hand == EAZGripHand::Handle"))
	EAZGripSide Side = EAZGripSide::Right;

	/** Pistol second hand: the right (trigger) hand's profile - its last solve is the surface this hand lies on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand", meta = (EditCondition = "Hand == EAZGripHand::PistolCup"))
	TSoftObjectPtr<UAZNaturalGripProfile> OtherHand;

	/** The weapon's forward (muzzle) direction in its mesh space: the pistol second hand's thumb points along it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	FVector WeaponForward = FVector(0.0, 1.0, 0.0);

	/** The hand's side for its role. */
	char SideChar() const
	{
		if (Hand == EAZGripHand::Handle)
		{
			return Side == EAZGripSide::Right ? 'r' : 'l';
		}
		return (Hand == EAZGripHand::Trigger || Hand == EAZGripHand::TriggerStraight) ? 'r' : 'l';
	}
	bool IsTriggerRole() const { return Hand == EAZGripHand::Trigger || Hand == EAZGripHand::TriggerStraight; }

	/** The hero body mesh (hand skin and bones). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	TSoftObjectPtr<USkeletalMesh> HeroMesh;

	// --- inputs ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	EAZGripInputSource Input = EAZGripInputSource::PythonFiles;

	/** The data folder (holds, fields, hand dumps). Empty = <Project>/Saved/wgs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ContentDir = "false"))
	FDirectoryPath DataDir;

	/** Support hand: which medoid clip hold of <key>_left_picks.json (aim | relaxed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FString LeftPick = TEXT("aim");

	/** Input = Assets: the folder of the weapon's clips; the hold (where the hand sits on the weapon, the elbows) is
	 *  sampled from them. Empty = the hold from the data files (<key>_hold_pick.json / <key>_left_picks.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips", meta = (ContentDir))
	FDirectoryPath ClipFolder;

	/** The hero mesh socket the weapon hangs on in these clips (e.g. RightHandM16Socket). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips")
	FName WeaponSocketOnHero;

	/** Aim clips: the name contains one of these (comma separated) ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips")
	FString AimClipFilter = TEXT("Aim");

	/** ... and none of these. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips")
	FString AimClipExclude = TEXT("Relaxed");

	/** Relaxed clips (support hand: the second elbow for the wrist bend). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips")
	FString RelaxedClipFilter = TEXT("Relaxed,Walk");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Clips", meta = (ClampMin = "1"))
	int32 FramesPerClip = 8;

	/** Skin vertices per bone used for contact: every Nth (1 = all). The searches use 2, the final solves 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "1"))
	int32 SearchThin = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "1"))
	int32 FinalThin = 1;

	// --- field ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0.05"))
	double CoarseSpacing = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0"))
	double CoarseMargin = 3.0;

	/** Exact narrow-band field around this hand's grip (weapon space, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	FVector FineBoxMin = FVector(-8.0, 12.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	FVector FineBoxMax = FVector(10.0, 34.0, 20.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0.05"))
	double FineSpacing = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0"))
	double FineBand = 1.3;

	/** With a hold sampled from the clips: the fine box = the hand +- this (cm) instead of FineBoxMin / FineBoxMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	bool bAutoFineBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "5", EditCondition = "bAutoFineBox"))
	double FineBoxHalfSize = 13.0;

	// --- marker sockets on the weapon mesh (Input = Assets): when present they replace the numbers below ---
	/** Trigger pad target -> TriggerPoint; its X -> GripX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName TriggerMarker = TEXT("NG_Trigger");

	/** The grip's front face (below the trigger guard) -> WrapFrontY = its Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName GripFrontMarker = TEXT("NG_GripFront");

	/** The highest the trigger hand's thumb tip may go (under the receiver) -> ThumbZMax = its Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName ThumbLimitMarker = TEXT("NG_ThumbLimit");

	/** A point on the handguard / forend AXIS (its centre) -> HandguardAxisXZ = its (X, Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName HandguardMarker = TEXT("NG_Handguard");

	/** Handle role: the handle axis, back end (butt / pommel) ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName HandleBackMarker = TEXT("NG_HandleBack");

	/** ... and front end (toward the blade / head). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Markers")
	FName HandleFrontMarker = TEXT("NG_HandleFront");

	// --- trigger hand ---
	/** Trigger pad target (weapon space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	FVector TriggerPoint = FVector(-1.08, 6.65, 5.0);

	/** Trigger pull direction (weapon space): the pad's palm side faces it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	FVector PullDirection = FVector(0.0, -1.0, 0.0);

	/** Grip centre X: wrapped pads and the thumb tip go past it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	double GripX = -1.08;

	/** +1: the far side of the grip is +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	double ThumbSide = 1.0;

	/** The thumb tip stays below this Z (under the receiver). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	double ThumbZMax = 4.0;

	/** The grip's front face (weapon Y): wrapped pads end behind it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	double WrapFrontY = 2.5;

	/** The placement search is centred on this (the previously approved re-grip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	FAZGripPlacement SearchCentre = FAZGripPlacement(10.0, -10.0, 10.0, FVector(-1.0, -1.0, -1.0));

	/** Placement of the final solve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	FAZGripPlacement TriggerPlacement = FAZGripPlacement(15.0, -5.0, 0.0, FVector(-2.0, -1.0, -1.5));

	/** Index start of the final solve: side angle, MCP, PIP, DIP (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Hand")
	FVector4 IndexStart = FVector4(2.0, 20.0, 40.0, 26.0);

	// --- support hand ---
	/** Handguard axis (weapon X, Z); it runs along +Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand")
	FVector2D HandguardAxisXZ = FVector2D(-1.1, 11.85);

	/** Power grasp (firm hold when shooting): the handguard seated in the hand, closure of 220+ degrees, the thumb along the
	 *  near side pressing with its pad, roll about the handguard axis. "Solve weapon hand" uses it. Off = the old rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand")
	bool bPowerGrasp = true;

	/** Power grasp: placements whose fingers are solved in stage 2 (spread over the roll angles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand", meta = (ClampMin = "9"))
	int32 PowerCandidates = 54;

	/** Stage 2 solves the fingers for this many best stage-1 placements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand", meta = (ClampMin = "1"))
	int32 Stage2Top = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand", meta = (ClampMin = "1"))
	double Stage2Step = 8.0;

	/** Placement of the final solve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support Hand")
	FAZGripPlacement SupportPlacement = FAZGripPlacement(0.0, 10.0, 15.0, FVector(1.0, 0.0, 0.0));

	// --- apply (writes the last final solve into the weapon's assets, with a backup first) ---
	/** The weapon actor Blueprint (correction + baked-grasp flags live on its class defaults; GripPose is read from it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply")
	TSoftObjectPtr<UBlueprint> WeaponBlueprint;

	/** The grip pose to write the finger rotations into. Empty = the Blueprint's GripPose property. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply")
	TSoftObjectPtr<UAnimSequence> GripPoseOverride;

	/** Support hand: the weapon mesh socket the left hand is IK'd onto (on the mesh or on its skeleton). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply")
	FName LeftHandSocket = TEXT("LeftHandGrip");

	/** Backups go to <BackupDir>/<date>_<time>_<profile>/. Empty = <Project>/../<Project>_Backups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply", meta = (ContentDir = "false"))
	FDirectoryPath BackupDir;

	/** Property names on the weapon Blueprint's class defaults (the plugin finds them by name, it does not depend on the
	 *  game module). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply", AdvancedDisplay)
	FName GripPoseProperty = TEXT("GripPose");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply", AdvancedDisplay)
	FName CorrectionProperty = TEXT("RightHandGripCorrection");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply", AdvancedDisplay)
	FName BakedRightProperty = TEXT("bBakedRightHandGrasp");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apply", AdvancedDisplay)
	FName BakedLeftProperty = TEXT("bBakedLeftHandGrasp");

	/** <DataDir> resolved (empty = <Project>/Saved/wgs), forward slashes, no trailing slash. */
	FString ResolveDataDir() const;

	/** <BackupDir> resolved (empty = <Project>/../<Project>_Backups). */
	FString ResolveBackupDir() const;
};
