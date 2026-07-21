---
name: reference_project_quickref
description: "Project overview detail moved out of MEMORY.md: game systems summary (inventory/weapon/GAS/anim v1), naming conventions, key source directories. Load when orienting in an unfamiliar part of the codebase."
metadata: 
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-20T23:58:21.671Z
---

# CHALK / AZ project quickref (detail formerly in MEMORY.md)

## Systems summary
- **Type:** third-person survival/action-horror; inventory, equipment, combat. UE 5.8, `C:\UnrealEngine\Games\AZ`, engine `C:\UnrealEngine\Engine\`.
- **Inventory:** dual — Old (UMG, deprecated) + New (CommonUI, active); Fragment/Manifest pattern (TInstancedStruct), composite description UI, FastArray replication — [[inventory-system]].
- **Weapon:** single-actor flow (carried+active states); Relaxed/Aim socket interp; GA_Shoot Single/Auto/Burst; FOV+boom+offset per stance — [[weapon_swap_architecture]], [[feedback_ik_setup]].
- **GAS:** custom ASC, 3+ AttributeSets (WeaponAttributeSet on PlayerState); GA_Jump/Crouch/Aim/Shoot/Interact; tags Ability.State.* / Ability.Cooldown.* / Weapon.Slot.* — [[project_gas_gameplay]].
- **Anim v1 (legacy):** C++ AnimInstance bools (bIsJumping/Falling/Crouching/Aiming/Shooting/WantsAimPose); per-stance blend spaces; Rifle SM 4 loco + 3 jump states; Layered Blend per Bone (spine_02); AO_Rifle_Aim camera trace; WeaponAnimSpeedMultiplier.

## Naming
Old `AZ_Inv_*`, New `AZ_Inv_CommonUI_*`; widgets `WBP_`, components `BP_`, data assets `DA_`.

## Key source directories (Source/AZ/Public/)
- `Animation/` — AZ_AnimInstance, AZ_LocomotionTypes, AZ_*Utils
- `Character/` — hero + infected Mover pawns, movement modes
- `Player/` — AZ_PlayerController (IMC, GAS InputConfig)
- `Equipment/`, `Items/`, `Inventory/` (shared data), `InventoryOld/` (deprecated UMG), `InventoryUI/` (CommonUI, active), `Weapon/`, `AI/` (Chalkie controller, BT tasks, HordeSubsystem)
