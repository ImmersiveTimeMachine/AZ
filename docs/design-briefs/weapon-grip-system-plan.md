# Weapon Grip System (WGS) - implementation plan

Design: `weapon-grip-system.md` (read it first). Status: **plan v1, 2026-09-27**, nothing below is started unless
the status table says so.

## Status

| id | task | owner | depends | status |
|---|---|---|---|---|
| 0.1 | Clip sampler (UE Python) | Sonnet | - | todo |
| 0.2 | Physics Asset + weapon parts dump, watertight check | Sonnet | - | todo |
| 0.3 | Baseline measurement report | Opus | 0.1, 0.2, 1.2 | todo |
| 1.1 | Master grip pose for the Winchester (Path S) | User (+Opus checks) | - | todo |
| 1.2 | `Tools/wgs/geom.py` - per-part signed distance, hand skin model | Opus | 0.2 | todo |
| 1.3 | Derive tool: LeftHandGrip + finger markers + report | Sonnet | 1.2 | todo |
| 1.4 | Solver v3 (assist, optional) | Opus | 1.2 | todo |
| 2.1 | `UAZ_WeaponGripData` | Sonnet | - | todo |
| 2.2 | `UAZ_CharacterGripProfile` | Sonnet | - | todo |
| 2.3 | `AZ_WeaponGripTypes.h` + `AAZ_Weapon::BuildGripInputs` | Sonnet | 2.1 | todo |
| 2.4 | Data assets DA_Grip_Winchester, DA_CharGrip_Hero (+ body capsules) | Sonnet | 2.1, 2.2, 0.2, user build | todo |
| 3.1 | `UAZ_WeaponGripComponent` | Sonnet | 2.x | todo |
| 3.2 | Anim instance pulls inputs from the component | Sonnet | 3.1 | todo |
| 3.3 | Component on the hero pawn (native) + profile assignment | Sonnet | 3.1, 2.4 | todo |
| 4 | Node v3: S1 push-out, S2, S3 reach, S4 both arms, S5 profile axes + thumb, S6 | Opus | 2.x, 3.x | todo |
| 5.1 | Validator core (replica of S1-S5) | Opus | 1.2, 4 | todo |
| 5.2 | Validator batch run + report + review actors | Sonnet | 5.1, 0.1 | todo |
| 6.0 | ChooserUtils: gameplay-tag cell setter + MatchExact setter | Sonnet | - | todo |
| 6.1 | Winchester locomotion rows (pack clips only) | Opus decides, Sonnet executes | 6.0 | todo |
| 6.2 | Next long guns (skeletal conversion + grip data) | Sonnet per weapon | 1.x-5.x | todo |
| 6.3 | Pistol HandOnHand mode | Opus | 4 | later |

Already done before this plan (2026-09-26/27): node v2 (left-hand IK, finger IK 3 DOF with backtracking, stock
elbow swing, `az.Weapon.Debug 3` draw), `SK_Winchester` rig (root fixed), `AZ_BP_Winchester`, `BP_Pickup_Winchester`,
`DA_WeaponAnim_Winchester`, tag `Weapon.Rifle.Winchester`, chooser idle rows 407/408, markers on `SK_Winchester`,
tools `az_grip_export/solve2/apply/refit`.

## Protected content - do not break the tuned sets

The M16 set, the pistol set and the unarmed (GASP / RTG) set were tuned for a long time and are close to perfect.
Every task in this plan must leave them untouched:
- Batch tools may WRITE only under `/Game/AZ/Assets/Master/`, `/Game/AZ/Assets/Weapons/<the weapon of the task>/`,
  `/Game/AZ/Blueprints/Weapon/`, `/Game/AZ/Blueprints/Animation/WeaponGrip/` and the paths a card names. Never write
  to `/Game/AZ/Assets/M16`, `/Pistol`, `/RTG`, `/GASP`, `/Riffle_RTG`, the M16 / pistol profiles
  (`DA_WeaponAnim_P01`, `DA_WeaponAnim_Pistol`), `AZ_BP_Rifle`, `AZ_BP_Pistol` or their hero sockets.
- Chooser edits only ADD rows with the Winchester (or the new weapon's) tag; existing rows and column settings are not
  changed (the one exception, c8 MatchExact, was checked: the M16 tag is exactly `Weapon.Rifle`).
- The grip node is inert without grip data (`GripPose` null -> alpha 0): M16 and pistol have none and must keep none
  until a card explicitly migrates them.
- Regression check after every phase that touches C++ or the hero ABP: the user runs PIE with the M16 and the pistol
  (idle, walk, run, crouch, aim, fire, reload, switch) and compares with before.
- Backups: gitignored assets copied to `C:/UnrealEngine/Games/AZ_Backups/2026-09-27_pre-WGS/` (5852 files, editor
  closed; restore with the editor closed, see its README); blueprint-tree assets are in git.

## Delegation protocol (token economy)

- **Opus (lead):** math, solver/node code, designs, every acceptance check. **Sonnet:** everything mechanical from a
  card. **Local model (LM Studio):** cards marked `[LM-ok]` when the server runs. Haiku: never.
- A card below is complete enough to be the task spec: when a task starts, the lead copies the card into
  `docs/agent-tasks/wgs-<id>.md`, adds any live facts, launches Sonnet, then runs the card's acceptance checks and
  updates the status table. A failed check goes back to the executor with the numbers; two failures -> the lead
  takes it over.
- **Executor rules (paste into every spec):** touch only the listed files/assets; never save the level; never run
  PIE; no git state changes; no C++ Live Coding harness unless the card says so (and then: work runs through
  `AsyncTask(ENamedThreads::GameThread, ...)`, no saves, no config writes, delete the TU afterwards); Python
  `@Description` ASCII and never containing ".py"; MCP Python runs async -> each script writes its result to
  `Saved/wgs/...` and the executor polls the file; verify every save by reloading (or file mtime); stop and report
  numbers when a check fails; report in English.
- **User checkpoints:** M0 baseline numbers, M1 master grip accepted, M2 full build after C++ data/component
  (header changes = editor closed + full build), M3 node v3 in PIE, M4 validator green, M5 Winchester fully playable.

---

## Phase 0 - baseline measurement (half a day)

### 0.1 Clip sampler - Sonnet
- **Goal:** world transforms of body + weapon for sampled frames of the validation clip set.
- **File:** `Tools/wgs/sample_clips.py` (UE Python, run via `exec(open(...).read(), {"CLIPS": [...]})`).
- **Clips:** Appendix A (paths `/Game/AZ/Assets/Master/RifleMega/<folder>/AZ_MST_<name>`); at most 6 clips per MCP call.
- **Sampling:** t = k / 15 s for k = 0..floor(length * 15), plus t = length.
- **Pose:** `unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, unreal.AnimPoseEvaluationOptions())`,
  `pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD)`.
- **Bones:** root, pelvis, spine_01..spine_05, neck_01, neck_02, head, clavicle_l/r, upperarm_l/r, lowerarm_l/r,
  hand_l/r, thigh_l/r, calf_l/r, az_weapon_r, and the 30 finger bones `<thumb|index|middle|ring|pinky>_0<1|2|3>_<l|r>`.
  Skip (and list) any bone missing on the skeleton.
- **Weapon:** socket `RightHandWinchesterSocket` of `/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh`
  -> `unreal.Transform(location=relative_location, rotation=relative_rotation, scale=relative_scale)`;
  `weapon_world = unreal.MathLibrary.compose_transforms(socket_local, az_weapon_r_world)`.
- **Curves:** `AZ_Grip_L`, `AZ_Grip_R` at each t (copy `curve_at()` from `Tools/az_grip_apply.py`).
- **Output:** `Saved/wgs/samples/<clip name>.json` = `{"clip", "length", "times": [...], "curves": {name: [...]},
  "weapon": [[x,y,z,qx,qy,qz,qw], ...], "bones": {bone: [[x,y,z,qx,qy,qz,qw], ...]}}` (translation cm, quaternion).
- **Acceptance:** one file per clip; `len(times) == floor(length*15)+1 (+1)`; for `Rifle02_St_Idle00` at t = 0
  `hand_r` equals a direct AnimPose query within 0.01 cm; in `Rifle02_St_Reload_Winch` the distance
  `|az_weapon_r - hand_r|` exceeds 20 cm at some t (weapon moves in the reload).

### 0.2 Physics Asset + weapon parts dump - Sonnet `[LM-ok for the report formatting]`
- **Physics:** hero body mesh `physics_asset` -> every `skeletal_body_setups[i]`: `bone_name`, `agg_geom.sphyl_elems`
  (center, rotation, radius, length), `box_elems` (center, rotation, x, y, z), `sphere_elems` (center, radius) ->
  `Saved/wgs/hero_physics.json`. Also list the hero pawn BP's skeletal mesh components (garments) with mesh names
  and bounds (informational).
- **Weapon parts:** the 8 static meshes in `/Game/AZ/Assets/Weapons/Winchester_Rifle/Meshes/` used by
  `SM_Winchester_Whole` (BasePart, Winchestere_ElitBase, ChargerBase, MazzleBase, Shutter, ShutterDet, SightPlank,
  StockBase): for each section `unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)` ->
  verts/tris (the parts share the weapon model space origin) -> `Saved/wgs/weapons/winchester_parts.json`
  `{part: {"verts": [...], "tris": [...]}}`.
- **Watertight check (offline CPython):** weld vertices by position (1e-4 cm), count edges not shared by exactly two
  triangles; report per part `closed` / `open (n boundary edges)` in `Saved/wgs/weapons/winchester_parts_report.md`.
- **Acceptance:** 8 parts, total triangles = 3908 (= the whole mesh); physics bodies include upperarm, lowerarm,
  spine and pelvis bones (list any missing).

### 0.3 Baseline measurement - Opus
`Tools/wgs/measure.py` (CPython): per sampled frame with `A >= 0.99`: palm / phalanx penetration and gap with the
CURRENT grip pose and markers, forearm / upper arm / torso / pelvis / thighs vs the weapon (per-part signed
distance, radii from the Physics Asset + inflation), left-hand reach. Output
`docs/design-briefs/weapon-grip-system-baseline.md`. -> **M0** with the user.

## Phase 1 - master grip (Path S) and derive tool

### 1.1 Master grip pose - User (Opus checks)
1. Open `/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester` (1 frame, skeleton `SK_AZ_Master`).
2. Preview: mesh `SKM_AZ_Master`; add preview asset `SK_Winchester` to socket `RightHandWinchesterSocket`.
3. Adjust the right-hand placement by moving that socket on `SKM_AZ_Master` (the tools copy it to the hero).
4. "Edit in Sequencer" -> FK Control Rig: pose the fingers of both hands and the left hand on the fore-end; bake back
   into `AS_Grip_Winchester` (keep 1 frame).
5. Tell the lead -> task 1.3 runs and reports numbers; iterate until the numbers pass (design section 10).

### 1.2 `Tools/wgs/geom.py` - Opus
Pure Python (runs in CPython and in UE Python): per-part exact nearest-triangle distance with a uniform-grid
bucket, per-part inside test (ray parity for closed parts, winding number for open parts), union SDF; hand skin
sample model; capsule-capsule and segment-segment distance.

### 1.3 Derive tool - Sonnet
- **File:** `Tools/wgs/derive_grip.py` (UE Python). Inputs: `WEAPON_SKM`, `WEAPON_SM` (optional mirror), `GRIP_POSE`,
  `HERO_SOCKET` (default the Winchester ones).
- **Steps:** (1) copy `RightHandWinchesterSocket` from `SKM_AZ_Master` to the hero body mesh + `Tools/hero_sockets.json`
  if different (same code as `Tools/az_grip_refit.py` step 0b); (2) AnimPose of `GRIP_POSE` at t = 0 in WORLD;
  weapon world = compose(socket_local, az_weapon_r_world); (3) `LeftHandGrip` = hand_l world relative to weapon world
  (`unreal.MathLibrary.make_relative_transform`) -> write to `WEAPON_SKM` (bone root) and mirror to `WEAPON_SM`;
  (4) fingertip pads (end of `_03` + 0.85 x offset `_02`->`_03`, in weapon space) -> sockets
  `Grip_<L|R>_<Finger>` on `WEAPON_SKM` (create with `unreal.new_object(unreal.SkeletalMeshSocket, outer=mesh)`,
  `add_socket(s, False)`, `rename_socket`, `set_socket_parent(mesh, "root")`, `set_socket_local_transform(...)`);
  (5) save both meshes, verify by reload; (6) call `geom.py` for palm + phalanx penetration / gap and write
  `Saved/wgs/derive_<weapon>.md`.
- **Acceptance:** reloaded sockets equal the computed values within 0.01 cm; report lists 30 phalanges + palm with
  numbers; nothing else on the meshes changed (socket count before/after + 10 markers + LeftHandGrip).

### 1.4 Solver v3 (optional) - Opus
`az_grip_solve2.py` -> v3 on `geom.py`: per-part union, profile flexion axes, thumb model, multi-start, placement
energy with the arm term from 0.1 samples. Produces the same artefacts as 1.1 + 1.3.

## Phase 2 - data model (C++, Sonnet; one full build at the end of phases 2 + 3)

### 2.1 `Source/AZ/Public/Weapon/AZ_WeaponGripData.h` (header-only)
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_WeaponGripData.generated.h"

class UAnimSequence;

UENUM(BlueprintType)
enum class EAZ_GripMode : uint8
{
	TwoHandLongGun,   // right hand carries the weapon (socket), left hand IK on LeftHandGrip
	HandOnHand,       // pistol: the left hand wraps the right hand
	OneHand
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponCapsule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") FName SocketA;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") FName SocketB;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0")) float Radius = 2.5f;
	/** The shoulder region may touch this capsule while aiming (stock butt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") bool bAllowShoulderContact = false;
};

/** Per-weapon grip semantics; geometry lives in sockets on the weapon mesh (see docs/design-briefs/weapon-grip-system.md). */
UCLASS(BlueprintType)
class AZ_API UAZ_WeaponGripData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") EAZ_GripMode Mode = EAZ_GripMode::TwoHandLongGun;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") TObjectPtr<UAnimSequence> GripPose = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") FName LeftHandGripSocket = TEXT("LeftHandGrip");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") FName TriggerSocket = TEXT("Trigger");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip") TArray<FAZ_WeaponCapsule> Capsules;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finger IK", meta = (ClampMin = "0", ClampMax = "1")) float FingerIKAlpha = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finger IK", meta = (ClampMin = "0")) float FingerIKMaxChangeDeg = 60.f;
};
```
**Acceptance:** compiles in the full build; no other file changed.

### 2.2 `Source/AZ/Public/Animation/AZ_CharacterGripProfile.h` (header-only)
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_CharacterGripProfile.generated.h"

USTRUCT(BlueprintType)
struct AZ_API FAZ_BodyCapsule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body") FName Bone;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body") FVector A = FVector::ZeroVector;   // bone space, cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body") FVector B = FVector::ZeroVector;   // bone space, cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0")) float Radius = 5.f;
	/** Torso, Pelvis, Thigh, Head, Shoulder, Backpack. Shoulder may touch capsules with bAllowShoulderContact while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body") FName Region;
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_FingerJointAxis
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") FName Bone;
	/** Unit flexion axis in the bone's REFERENCE local frame; a positive angle curls toward the palm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") FVector FlexAxis = FVector::UpVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") float MinDeg = -20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") float MaxDeg = 100.f;
};

/** Per-character (outfit) grip data: body collision, arm radii, finger kinematics. */
UCLASS(BlueprintType)
class AZ_API UAZ_CharacterGripProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body") TArray<FAZ_BodyCapsule> Body;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0")) float ClothingInflation = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0")) float ForearmRadius = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0")) float UpperArmRadius = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", ClampMax = "1")) float ForearmTestFraction = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", ClampMax = "90")) float ElbowSwingMaxDeg = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0")) float ElbowSwingRateDegPerSec = 240.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Push-out", meta = (ClampMin = "0")) float PushOutMax = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Push-out", meta = (ClampMin = "0")) float PushOutSpringHz = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") TArray<FAZ_FingerJointAxis> FingerAxes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers", meta = (ClampMin = "0", ClampMax = "1.5")) float DistalCoupling = 0.67f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers", meta = (ClampMin = "0")) float SpreadLimitDeg = 23.f;
	/** Thumb CMC opposition axis in thumb_01's reference frame (second DOF of the thumb base). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fingers") FVector ThumbOppositionAxis = FVector::ForwardVector;
};
```

### 2.3 Runtime input types + weapon helper - Sonnet
- New `Source/AZ/Public/Animation/AZ_WeaponGripTypes.h`: MOVE `FAZ_WeaponGripMarkers` here verbatim from
  `AnimNode_AZWeaponGrip.h` (the node header then includes this file), and add:
```cpp
USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponCapsuleInBone
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FVector A = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FVector B = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") float Radius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") bool bAllowShoulderContact = false;
};

/** Everything the AZ Weapon Grip node needs about the held weapon, relative to AttachBone. Built on the game thread. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponGripInputs
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") TObjectPtr<UAnimSequence> GripPose = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FName AttachBone = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FTransform LeftHandInBone = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") bool bHasLeftHand = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FAZ_WeaponGripMarkers Markers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") FVector TriggerInBone = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") bool bHasTrigger = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") TArray<FAZ_WeaponCapsuleInBone> Capsules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") bool bAiming = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") bool bReloading = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") float FingerIKAlpha = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip") float FingerIKMaxChangeDeg = 60.f;
};
```
- `AAZ_Weapon` (additive): `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Weapon|Grip")
  TObjectPtr<UAZ_WeaponGripData> GripData = nullptr;` and
  `bool BuildGripInputs(const USkeletalMeshComponent* CharMesh, FName AttachBone, FAZ_WeaponGripInputs& Out) const;`
  Behaviour: GripPose = GripData ? GripData->GripPose : GripPose (legacy); LeftHandInBone via the existing
  `GetLeftHandGripInBone` using `GripData->LeftHandGripSocket` when set; markers via `GetGripMarkersInBone`;
  trigger + capsules: socket positions -> `BoneWorld.InverseTransformPosition(...)`; legacy `StockFront`/`StockButt`
  become a capsule named `Stock` when `GripData` has no capsules; FingerIK values from GripData (else 1 / 60);
  returns false when GripPose is null or AttachBone is None.
- **Acceptance:** compiles; `GetGripMarkersInBone` / `GetLeftHandGripInBone` unchanged in behaviour; the node header
  still compiles with the moved struct (no duplicate definitions).

### 2.4 Data assets - Sonnet (after the user's full build)
- `/Game/AZ/Blueprints/Weapon/Grip/DA_Grip_Winchester` (`UAZ_WeaponGripData`): GripPose = AS_Grip_Winchester;
  Capsules = [{Stock, StockFront, StockButt, 3.0}] + barrel/receiver capsules from sockets the lead adds
  (`Col_Barrel_A/B`, `Col_Receiver_A/B`); set `AZ_BP_Winchester` CDO `GripData`; compile + save the BP.
- `/Game/AZ/Blueprints/Character/Grip/DA_CharGrip_Hero` (`UAZ_CharacterGripProfile`): `Body` from
  `Saved/wgs/hero_physics.json` - every sphyl -> capsule (A/B = center -/+ rotated half length along local Z,
  radius), bones: pelvis, spine_01..05, neck_01, head, clavicle_l/r (Region Shoulder), thigh_l/r, plus a manual
  Backpack capsule on spine_05 (values from the lead); FingerAxes from `Saved/wgs/finger_axes.json` (lead).
- **Acceptance:** reload both assets and print every field; values equal the inputs.

## Phase 3 - component (Sonnet; Opus reviews)

### 3.1 `Source/AZ/Public/Animation/AZ_WeaponGripComponent.h` / `.cpp`
```cpp
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_WeaponGripComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_WeaponGripComponent();   // PrimaryComponentTick.bCanEverTick = false

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Grip")
	TObjectPtr<UAZ_CharacterGripProfile> Profile = nullptr;

	/** Game thread, pulled by the anim instance every update. Fills Out for the held weapon; false (+ Status) when no grip applies. */
	bool BuildAnimInputs(const USkeletalMeshComponent* CharMesh, FAZ_WeaponGripInputs& Out);

	UFUNCTION(BlueprintPure, Category = "AZ|Grip") FString GetStatus() const { return Status; }

private:
	FString Status;
};
```
`BuildAnimInputs` = the current "WEAPON GRIP GATHER" block of `UAZ_MoverAnimInstance.cpp` moved here: pawn =
owner; equipment from `Pawn->GetController()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>()`;
active weapon must hang on its Relaxed or Aim socket of CharMesh; AttachBone = `CharMesh->GetSocketBoneName(socket)`;
`Weapon->BuildGripInputs(...)`; `bAiming` = ASC has `Ability_State_Aiming` or `Ability_State_FirearmReady`;
`bReloading` = `Ability_State_Reloading`. Status strings: "no controller", "no equipment", "no active weapon",
"weapon not in hands (socket X)", "weapon has no grip pose", "ok <weapon> on <bone>, markers 0x..., capsules n".
**Acceptance:** compiles; no logic left duplicated; every early return sets Status.

### 3.2 Anim instance - Sonnet
Replace the gather block by: find `UAZ_WeaponGripComponent` on the pawn; `bool bGrip = Comp &&
Comp->BuildAnimInputs(GetSkelMeshComponent(), WeaponGripInputs)`; keep `WeaponGripAlpha` easing; keep filling the
old members (`WeaponGripPose`, `WeaponGripLeftHandInBone`, `WeaponGripBone`, `WeaponGripMarkers`) from
`WeaponGripInputs` until phase 4 switches the node pins. Add `UPROPERTY(Transient, BlueprintReadOnly,
Category="AZ|V2|Anim|Grip") FAZ_WeaponGripInputs WeaponGripInputs;` and `TObjectPtr<UAZ_CharacterGripProfile>
WeaponGripProfile` (copied from the component). **Acceptance:** compiles; PIE probe (lead) shows the same values as
before.

### 3.3 Component on the hero - Sonnet
`AAZ_PawnMoverHeroCharacter`: `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Grip")
TObjectPtr<UAZ_WeaponGripComponent> WeaponGrip;` created in the constructor with `CreateDefaultSubobject`. After the
build: set `Profile = DA_CharGrip_Hero` on the component template of `AZ_BP_PawnMoverHero_MHC` (CDO subobject via
Python, `compile_blueprint`, save). **Acceptance:** PIE probe: `GetStatus()` = "ok ...".

## Phase 4 - node v3 - Opus
Pins become `Inputs` (`FAZ_WeaponGripInputs`) + `Profile`; stages S1-S6 exactly as in design section 6; warm starts
and springs stored per node instance; `GatherDebugData` prints delta, swings, reach error, per-finger IK error. The
lead rebinds the ABP pins (`UAZ_AnimGraphNodeUtils.set_pin_binding`), the user compiles + saves the ABP.
**Acceptance:** M3 in PIE with `az.Weapon.Debug 3`, then the validator (phase 5).

## Phase 5 - validation

### 5.1 Validator core - Opus
`Tools/wgs/validate.py`: replica of S1-S5 on the 0.1 samples with the profile + weapon data; metrics of design
section 10; per-frame result records.

### 5.2 Runner + report + review actors - Sonnet
- Run 5.1 over every sample file; write `docs/design-briefs/weapon-grip-system-validation.md` (table per clip:
  worst value + time per metric, pass / fail vs thresholds) and `Saved/wgs/validation.csv`.
- For the 10 worst frames: write a 1-frame preview animation from the validator's solved local transforms (same
  writing code as `Tools/az_grip_apply.py` MODE assets) into `/Game/AZ/Assets/Master/GripPreview/WGS/`, spawn a
  `SkeletalMeshActor` (mesh `SKM_AZ_Master`, single-node animation = that preview) + the weapon mesh attached to
  `RightHandWinchesterSocket`, labels `WGS_REVIEW_<clip>_<t>`, in a row 150 cm apart near the player start. Never
  save the level.
- **Acceptance:** report exists; every clip listed; actors present with the listed labels.

## Phase 6 - content rollout

### 6.0 ChooserUtils setters - Sonnet (C++, `Source/AZ/.../AZ_ChooserUtils.h/.cpp`, additive)
`static bool SetCellGameplayTagsOnSub(const FString& RootChooserPath, const FString& SubTableName, int32 RowIndex,
int32 ColumnIndex, const TArray<FString>& TagNames);` (replaces the cell's `FGameplayTagContainer` in
`FGameplayTagColumn::RowValues`) and `static bool SetGameplayTagColumnMatchExact(const FString& RootChooserPath, const
FString& SubTableName, int32 ColumnIndex, bool bExact);` - both `UFUNCTION(BlueprintCallable)`, `Modify()` the
table, no save (callers use `CompileAndSave`). **Acceptance:** compiles; a Python test on a DUPLICATE of CHT_v2 (in
`/Game/AZ/Temp/`) sets and reads back a tag cell.

### 6.1 Winchester locomotion rows - Opus decides, Sonnet executes
Before handing over, the lead must decide: (a) how rows with `bUseMM=True` resolve their assets (whether a
PoseSearch database is required - check the v2 MM pool code), (b) the fallback for states without pack clips
(Transition to Locomotion / to Idle - the pack has no starts or stops), (c) turn-in-place loops (Turn_Linear_90L/R
loopable?), (d) jumps and falls. Then Sonnet duplicates source rows (`DuplicateRowOnSub`), sets c8 =
`Weapon.Rifle.Winchester` (6.0) and the asset, per Appendix B, and saves with `CompileAndSave`.
**Acceptance:** the chooser dump lists every Appendix B row with the right asset and tag; PIE: walking / running /
crouching / aiming with the Winchester plays only `AZ_MST_*` clips (`[v2 Pick]` log lines).

### 6.2 Next long guns - Sonnet per weapon (card = `docs/agent-tasks/winchester-skeletal-and-item.md` + fixes)
Skeletal conversion from parts in Blender AS BEFORE, then the root fix (identity root via `SkeletonModifier`:
`set_bone_transform("root", Transform(), False)`, children re-placed at their cm positions,
`commit_skeleton_to_skeletal_mesh()`; bounds must match the static whole mesh), weapon BP, grip data, sockets
(hand, markers via 1.1 + 1.3), pickup, profile, rows. Order proposal: Remington 870 (pump = first moving-part test),
AK12, STG44, SVD, Hunter.

### 6.3 Pistol HandOnHand - Opus (later)
Left fingers target the right hand's skin: a second contact surface built from the right-hand bone capsules.

---

## Appendix A - validation clip set (master clips)
Rifle_Styly02_St/Rifle02_IdleSet: Rifle02_St_Idle00, Rifle02_St_Idle01 - Rifle_Styly02_St/Rifle02_LocomotionSet:
Rifle02_St_Walk_F, Rifle02_St_Walk_B, Rifle02_St_Walk_90L, Rifle02_St_Walk_90R, Rifle02_St_Walk_45L,
Rifle02_St_Walk_135R, Rifle02_St_Run_F - Rifle_Styly01_St/Rifle01_IdleSet: Rifle01_St_Idle00 -
Rifle_Styly01_St/Rifle01_LocomotionSet: Rifle01_St_Walk_F, Rifle01_St_Run_F, Rifle01_St_Sprint -
Rifle_Styly01_St/Rifle01_TurnSet: Riflel01_St_Turn90L - Rifle_Cr/Rifle_Cr_IdleSet: Rifle_Cr_Idle00 -
Rifle_Cr/Rifle_Cr_LocomotionSet: Rifle_Cr_Walk_F, Rifle_Cr_Run_F - Rifle_Transitions: Rifle02_St_to_Cr -
Rifle_TakeHide: Rifle02_St_TakeUp, Rifle02_St_HideDown - Rifle_ShootingSet/Winchester: Rifle01_St_Shoot_Winch,
Rifle_Cr_Shoot_Winch - Rifle_ReloadingSet/Winchester: Rifle02_St_Reload_Winch, Rifle01_St_Reload_Winch,
Rifle_Cr_Reload_Winch - Rifle_Styly01_St/Rifle01_OtherAnims: Rifle01_St_Jump_Walk.

## Appendix B - chooser row map (source = M16 row in CHT_v2, target = master clip `AZ_MST_<name>`)
Directions: M16 F, FR, R, BR, B, BL, L, FL = pack F, 45R, 90R, 135R, B, 135L, 90L, 45L (loops use the `_IPC` clips).

| source rows | state | target |
|---|---|---|
| 103 / 105 | idle relaxed stand / crouch | Rifle02_St_Idle00 / Rifle_Cr_Idle00 (DONE: rows 407 / 408) |
| 104 / 106 | idle aim stand / crouch | Rifle01_St_Idle00 / Rifle_Cr_Idle00 |
| 107-114 | walk relaxed, 8 dir | Rifle02_St_Walk_<dir>_IPC |
| 115-122 | walk aim, 8 dir | Rifle01_St_Walk_<dir>_IPC |
| 123-130 | run relaxed, 8 dir | Rifle02_St_Run_<dir>_IPC |
| 131-138 | run aim, 8 dir | Rifle01_St_Run_<dir>_IPC |
| 139-146 / 147-154 | crouch walk relaxed / aim | Rifle_Cr_Walk_<dir>_IPC |
| 402 | sprint | Rifle01_St_Sprint_IPC |
| 263 / 264 | stance relaxed St->Cr / Cr->St | Rifle02_St_to_Cr / Rifle02_Cr_to_St |
| 265 / 266 | stance aim St->Cr / Cr->St | Rifle01_St_to_Cr / Rifle01_Cr_to_St |
| 304-307 | aim turn in place L/R stand, crouch | decision 6.1(c) |
| 155-262, 276-279, 403-406 | starts / stops (none in the pack) | decision 6.1(b) |
| 280-303 | jumps / falls | decision 6.1(d) |
