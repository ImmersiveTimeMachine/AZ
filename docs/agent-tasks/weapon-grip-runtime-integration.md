# Task: wire the weapon grip node into the project (C++ edits to EXISTING files)

You are executing a precise spec. Do exactly this, nothing else. Other agents edit this repository in parallel:
touch ONLY the files listed below, make ADDITIVE edits anchored on the quoted content, never reformat or reorder
existing code, never delete anything. Do NOT build, do NOT run Unreal, do NOT run git commands that change state
(no add/commit/stash/checkout/reset/clean). Read-only git (status/diff) is fine.

Already written by the lead (do not modify, read them for context):
- `Source/AZ/Public/Animation/AnimNode_AZWeaponGrip.h`, `Source/AZ/Private/Animation/AnimNode_AZWeaponGrip.cpp`
  (runtime node `FAnimNode_AZWeaponGrip`, pins: `GripPose`, `LeftHandInWeaponBone`, `WeaponBoneName`, `GripAlpha`)
- `Source/AZEditor/**` (new editor module: `AZEditor.Build.cs`, `AZEditorModule.cpp`, `AnimGraphNode_AZWeaponGrip`)

## 1. `Source/AZ/AZ.Build.cs`
In the `PublicDependencyModuleNames.AddRange(new string[] { ... })` list (the line starting with `"Core", "CoreUObject"`),
append `"AnimationCore"` as the last entry of that array (the node includes `TwoBoneIK.h`). Change nothing else.

## 2. `AZ.uproject`
In `"Modules": [ ... ]`, right after the existing object whose `"Name": "AZ"`, add a second module object:
```json
		{
			"Name": "AZEditor",
			"Type": "UncookedOnly",
			"LoadingPhase": "Default"
		}
```
Keep the file's existing indentation style (tabs) and valid JSON (comma between the two module objects).

## 3. `Source/AZEditor.Target.cs`
After the line `ExtraModuleNames.Add("AZ");` add `ExtraModuleNames.Add("AZEditor");`. Do NOT touch `Source/AZ.Target.cs`.

## 4. `Source/AZ/Public/Weapon/AZ_Weapon.h` and `Source/AZ/Private/Weapon/AZ_Weapon.cpp`
Header: right AFTER the existing declaration
```cpp
	/** Socket on weapon mesh for left hand grip. */
	UPROPERTY(EditAnywhere, Category = "AZ|Weapon|Sockets")
	FName LeftHandGripSocket{ TEXT("LeftHandGrip") };
```
insert:
```cpp

	/** Weapon grip pose (Tools/az_grip_solve2.py -> e.g. /Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester):
	 *  a 1-frame pose whose finger bones hold THIS weapon. With LeftHandGripSocket on the weapon's mesh it drives the
	 *  hero's AZ Weapon Grip anim node (left hand IK + both hands' fingers). Null = no grip (old behaviour). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Weapon|Grip")
	TObjectPtr<UAnimSequence> GripPose = nullptr;

	/** The left hand's grip target (the hand_l transform stored in LeftHandGripSocket) relative to BoneName of
	 *  CharMesh. Searches this weapon's components for the first one that owns LeftHandGripSocket (static or skeletal
	 *  mesh). False when there is none. */
	bool GetLeftHandGripInBone(const USkeletalMeshComponent* CharMesh, FName BoneName, FTransform& OutInBone) const;
```
`UAnimSequence` is already forward-declared/used in this header (see `WeaponMeshFireAnimation`); if the compiler
would need it, a forward declaration `class UAnimSequence;` near the other forward declarations is enough.

Cpp: add at the END of `AZ_Weapon.cpp` (after the last function):
```cpp

bool AAZ_Weapon::GetLeftHandGripInBone(const USkeletalMeshComponent* CharMesh, FName BoneName, FTransform& OutInBone) const
{
	if (!CharMesh || BoneName == NAME_None || LeftHandGripSocket == NAME_None)
	{
		return false;
	}
	TInlineComponentArray<USceneComponent*> Components(this);
	for (const USceneComponent* Component : Components)
	{
		if (Component && Component->DoesSocketExist(LeftHandGripSocket))
		{
			const FTransform GripWorld = Component->GetSocketTransform(LeftHandGripSocket, RTS_World);
			const FTransform BoneWorld = CharMesh->GetSocketTransform(BoneName, RTS_World);
			OutInBone = GripWorld.GetRelativeTransform(BoneWorld);
			return true;
		}
	}
	return false;
}
```
Make sure `Components/SceneComponent.h` / `Components/SkeletalMeshComponent.h` are reachable (they almost certainly
already are through existing includes; add an include only if missing).

## 5. `Source/AZ/Public/Animation/AZ_MoverAnimInstance.h`
Right BEFORE the line
```cpp
	// ============ GRAB HAND-IK (idle + hands pinned on the grabber — user design 2026-07-24) ============
```
insert this block (keep the tab indentation of the surrounding members):
```cpp
	// ============ WEAPON GRIP (AZ Weapon Grip node: left hand IK + fingers onto the held weapon) ============
	// Gathered on the game thread from the equipped weapon (AAZ_Weapon::GripPose + its LeftHandGrip socket). The
	// target is RELATIVE TO THE BONE THE WEAPON HANGS ON, so the node resolves it against the current pose (no lag).

	/** The held weapon's grip pose (null = no grip). Bind to the node's Grip Pose pin. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Grip")
	TObjectPtr<UAnimSequence> WeaponGripPose = nullptr;

	/** hand_l grip target relative to WeaponGripBone. Bind to Left Hand In Weapon Bone. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Grip")
	FTransform WeaponGripLeftHandInBone = FTransform::Identity;

	/** Bone the held weapon is attached to (its attach socket's bone, e.g. az_weapon_r). Bind to Weapon Bone Name. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Grip")
	FName WeaponGripBone = NAME_None;

	/** 0..1, eased: a weapon with grip data is in the hands. Bind to Grip Alpha. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Grip")
	float WeaponGripAlpha = 0.f;

	/** WeaponGripAlpha ease rate (per second). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Grip", meta = (ClampMin = "0"))
	float WeaponGripBlendSpeed = 6.f;

```

## 6. `Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp`
a) Includes: make sure these are included (add only the missing ones, next to the existing `#include` lines):
```cpp
#include "Weapon/AZ_Weapon.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
```
b) In `UAZ_MoverAnimInstance::NativeUpdateAnimation`, find the statement
```cpp
	ActiveWeaponAnimationProfile = NewWeaponProfile;
```
and insert RIGHT AFTER it (before the `// ============================== GRAB HAND-IK GATHER` comment):
```cpp

	// ============================== WEAPON GRIP GATHER ==============================
	// The held weapon (attached at its Relaxed/Aim socket, not the carry socket) with grip data -> the AZ Weapon Grip
	// node's inputs. The target is expressed relative to the bone the weapon hangs on; while the weapon stays on that
	// socket it is a constant, and the node applies it to the CURRENT pose of that bone.
	{
		float TargetGripAlpha = 0.f;
		const APawn* GripPawn = TryGetPawnOwner();
		const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GripPawn ? GripPawn->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
		if (const AAZ_Weapon* Weapon = Equipment ? Equipment->GetActiveWeapon() : nullptr)
		{
			const USceneComponent* WeaponRoot = Weapon->GetRootComponent();
			const USkeletalMeshComponent* AttachMesh = WeaponRoot ? Cast<USkeletalMeshComponent>(WeaponRoot->GetAttachParent()) : nullptr;
			const FName AttachSocket = WeaponRoot ? WeaponRoot->GetAttachSocketName() : NAME_None;
			const bool bInHands = AttachMesh && (AttachSocket == Weapon->RelaxedSocketName || AttachSocket == Weapon->AimSocketName);
			const FName AttachBone = bInHands ? AttachMesh->GetSocketBoneName(AttachSocket) : NAME_None;
			FTransform LeftHandInBone;
			if (Weapon->GripPose && AttachBone != NAME_None && Weapon->GetLeftHandGripInBone(AttachMesh, AttachBone, LeftHandInBone))
			{
				WeaponGripPose = Weapon->GripPose;
				WeaponGripLeftHandInBone = LeftHandInBone;
				WeaponGripBone = AttachBone;
				TargetGripAlpha = 1.f;
			}
		}
		WeaponGripAlpha = FMath::FInterpConstantTo(WeaponGripAlpha, TargetGripAlpha, DeltaSeconds, WeaponGripBlendSpeed);
		if (WeaponGripAlpha <= 0.f && TargetGripAlpha <= 0.f)
		{
			WeaponGripPose = nullptr;       // keep the last pose while fading out, drop it once fully off
		}
	}
```
Check the local names: the function parameter is `DeltaSeconds` (verify in the signature; if it is named differently,
use that name). `RelaxedSocketName` / `AimSocketName` / `GetActiveWeapon()` are public (verify; if `GetActiveWeapon`
lives on another class name, report instead of guessing).

## 7. Verify and report
- `git diff --stat` for exactly these files: AZ.Build.cs, AZ.uproject, AZEditor.Target.cs, AZ_Weapon.h, AZ_Weapon.cpp,
  AZ_MoverAnimInstance.h, AZ_MoverAnimInstance.cpp. Nothing else may show your changes.
- Paste the final inserted blocks (with 3 lines of context) in your report.
- Report anything that did not match this spec (names, signatures, missing anchors) - do NOT improvise around it.
