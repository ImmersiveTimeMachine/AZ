---
name: weapon_swap_architecture
description: Complete weapon system - single actor, GAS attributes, fire modes, camera system, implemented 2026-03-15 to 2026-03-23
type: project
---

## Weapon System Architecture (Implemented 2026-03-15 to 2026-03-23)

### Actor Hierarchy
- **AAZ_Item** — base item (mesh, sphere, GUID, replication, virtual overlap handlers)
- **AAZ_PickupItem** (inherits AAZ_Item) — world pickup for CommonUI system, overrides overlap for HUD/PlayerController
- **AAZ_Weapon** (inherits AAZ_Item) — lives on character in both carried (cosmetic) and active states. Has ASC, target actors, MakeCosmetic(), sockets (CarrySocketName, RelaxedSocketName, AimSocketName)
- **AAZ_Inv_EquipActor** — reserved for future item inspection/3D preview (minimal, just equipment type tag)

### Single Actor Flow (no swap)
- Pickup: spawn AAZ_Weapon at CarrySocket, MakeCosmetic()
- Equip: reattach to RelaxedSocket, tag with Weapon.Slot.Primary, push ASC state
- Aim/Shoot: NativeUpdateAnimation copies AimSocket relative transform with interp
- Unequip: save ASC state, reattach to CarrySocket, remove tags
- Drop: save state, clear tags, destroy

### Fragment Architecture
- **EquipmentFragment** — pure state (None/Carried/Equipped) + EquipModifiers + EquipmentType tag + EquipActorClass (for future)
- **WeaponStateFragment** — WeaponActorClass + ammo/damage/fire rate persistence. ApplyToASC()/SaveFromASC()
- **AbilityGrantFragment** — grants/removes abilities on equip/unequip

### GAS Integration
- **WeaponAttributeSet** on player ASC (via PlayerState subobject)
- On equip: WeaponStateFragment pushes values to player ASC
- On unequip: player ASC values saved back to fragment
- GA_Shoot reads BaseDamage/ammo from player ASC
- Weapon actor tagged with Weapon.Slot.Primary (AActor::Tags) for runtime lookup
- ASC tagged with State.Equipped.Weapon.Primary (loose gameplay tag)

### Weapon Finding (GetPrimaryWeapon)
- Helper on UAZ_GameplayAbility and UAZ_AnimInstance
- Tries GetCurrentSourceObject() first, then finds by Weapon.Slot.Primary actor tag
- Used by GA_Shoot, GA_Aim, AnimInstance IK/positioning

### Fire System (GA_Shoot)
- **Single fire**: one shot per activation, montage plays, ability ends on montage complete
- **Auto fire**: loops via timer while input held, ends on InputReleased
- **Burst**: auto mode with BurstCount > 0 (e.g. 3 = 3-round burst)
- Fire montages: FireMontageSingle, FireMontageAuto (per-mode)
- PlayMontageAndWaitForEvent task with OnCompleted/OnBlendOut callbacks
- VFX: muzzle flash particle at Muzzle socket
- Audio: fire sound at Muzzle socket
- bIsShooting bool with 5s timeout timer (ShootingPoseTimeout) — keeps aim pose between rapid shots
- bWantsAimPose = bIsAiming || bIsShooting — used by SM transitions

### Weapon Positioning (AnimInstance)
- Weapon attached to RelaxedSocket normally
- When bIsAiming || bIsShooting: NativeUpdateAnimation computes AimSocket relative transform
- Smooth interpolation via CurrentWeaponRelativeTransform.BlendWith()
- Snap (no interp) when bIsShooting for IK consistency
- Returns to identity (RelaxedSocket) when not aiming/shooting

### Left Hand IK
- C++ IK code removed — done in ABP via FABRIK node
- GetPrimaryWeapon() available in ABP to find weapon
- Dual grip sockets: LeftHandGrip (relaxed), LeftHandGripAim (aim)
- FABRIK reads weapon grip socket transform in ABP Event Blueprint Update Animation

### Camera System (on AZ_HeroCharacter)
- **Stand**: CameraBoomArmLength, CameraBoomSocketOffsetY/Z, Default3PFOV
- **Aim**: AimFOV, AimBoomLength, AimSocketOffsetY/Z, CameraAimInterpSpeed
- **Crouch**: CrouchBoomLength, CrouchSocketOffsetY/Z, CameraCrouchInterpSpeed
- **Crouch compensation**: CapsuleDelta added to TargetOffsetZ to prevent spike on capsule shrink
- **Per-direction movement offsets**: 4 directions × 3 stances (Default/Aim/Crouch) = 12 FVector2D properties
- All interpolated via FMath::FInterpTo in AnimInstance

### Abilities
- **GA_Jump**: sets bIsJumping, calls Character->Jump(), ActivationBlockedTags configurable in BP
- **GA_Crouch**: toggle, calls Crouch/UnCrouch, one-shot (ends immediately)
- **GA_Aim**: toggle bIsAiming, AnimInstance handles camera + weapon positioning
- **GA_Shoot**: fire modes (Single/Auto), montage task, VFX, sound, ammo check/consume

### Gameplay Tags Added
- Weapon.Slot.Primary/Secondary/Sidearm/Melee (actor tags on weapon)
- Ability.State.Shooting/Reloading/Aiming/Sprinting/MeleeAttacking/Interacting/Throwing
- Ability.Cooldown.Shoot/Reload/Melee/Interact/Dash/Grenade
- Movement.Aiming/Swimming/Climbing
- Combat.Firing/MeleeSwing
- Character.Downed

### Blend Spaces Created
- NoWeapon: BS_NoWeapon, _Crouch, _JumpStart, _Fall, _Landing
- Rifle Relaxed: BS_Rifle, _Crouch, _JumpStart, _Fall, _Landing
- Rifle Aim: BS_Rifle_Aim, _Crouch_Aim, _Aim_JumpStart, _Aim_Fall, _Aim_Landing

### State Machine (Rifle SM)
- 4 locomotion states: Idle/Walk/Run, Aim Walk/Run, Crouch Walk, Crouch Aim Walk
- Jump states: JumpStart, FallLoop, Landing
- Transitions use bIsJumping, bIsFalling, bIsCrouching, bWantsAimPose
- Layered Blend per Bone (spine_01) for fire montage upper body overlay

**How to apply:** This is the fully implemented weapon + animation + camera system. When adding new weapon types, create AAZ_Weapon BP, add WeaponStateFragment to manifest, assign blend spaces and montages.
