# CHALK weapon reticle — implemented, user gameplay check pending

User requested a center aim reticle using the installed assets, suitable for the rifle and scalable to other reticle types. Inventory appearance and the accepted Phase 1 HUD must remain unchanged.

September 8 restart correction during impact integration: the prior in-memory reticle readback did not represent the saved rifle/map state; all three references loaded as None. Restored the reference using detached manifest copies, corrected authoring helpers to avoid borrowed-struct mutation and reconstructing property notifications, and now read back freshly resolved components after compilation. Explicitly saved rifle/map and verified the BP's serialized DA_HUDReticle_Rifle dependency. Details/receipts: C:/UnrealEngine/Games/AZ/docs/design-briefs/bullet-impact-status.md. The visual definition/widget remain unchanged; gameplay verification is still pending.

## Selected visual

ProHUD Crosshair1 textures, adapting the compact four-tick WB_CrosshairV1_5_H design: thin chalk-white orthogonal ticks, open center, subtle dark underlays. The intended 40×40 authored frame uses a 6-unit inner gap radius and 2×10-unit arms. Definition tint is #EEEAE0. The existing diagonal hit-confirmation X remains independent and above the aiming reticle.

Reused textures: /Game/ProHUDV2_Horror/Textures/Crosshair/V1/Crosshair1/T_CH1_Up_H, T_CH1_Down_H, T_CH1_Left_H and T_CH1_Right_H. Genuine reference exports and live vendor-tree readback are under C:/UnrealEngine/Games/AZ/Saved/HUDReticle/References/.

Do not instantiate the stock vendor leaf unchanged: its Construct locates WB_CrosshairV1_H with GetAllWidgetsOfClass(...)[0] and binds global recoil callbacks. The AZ leaf reuses only the directional textures.

## Implemented source

- New UAZ_HUDReticleDefinition DataAsset: WidgetClass, Size, Tint, bAimOnly. A nullable ReticleDefinition reference is added to the existing weapon-state fragment. No rifle-specific switch is needed in HUD code.
- New UAZ_HUDReticleWidget : UCommonUserWidget visual base with a read-only current view and optional BP OnReticleViewChanged event. The rifle leaf is static; future classes can render rings, dots, chevrons or scope-style visuals through the same host.
- New FAZ_PlayerReticleView and UAZ_PlayerUIComponent.GetReticleView / OnReticleChanged. Driven by committed equipment, inventory, PS Vitals, GAS aim/block tags, possession, weapon destruction and ownership changes. No tick/polling or new gameplay authority.
- Reticle requires a valid local pawn and its owned committed weapon, a valid configured widget definition, positive real health, closed inventory and no blocking action tag. Empty ammo does not remove the aim reference. Rifle definition is aim-only, following Ability.State.Aiming; other definitions can use equipped visibility.
- AAZ_Weapon native OnOwnershipChanged delegate is emitted after actual SetOwner changes and from OnRep_Owner. UI subscribes as soon as the weapon resolves, independently of item/owner arrival order. This prevents future equipped-visible reticles sticking hidden after late Owner replication. Existing firing behavior is preserved.
- Existing HUD has an optional ReticleHost USizeBox. It creates/reuses the configured visual with the correct owning player, applies tint, and wraps it in a runtime ScaleBox for uniform size changes. Anchor is the exact local viewport center, outside SafeZone, matching the firing camera and existing hit marker. Host uses the view's live menu condition rather than a cached menu flag.

Source files: C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_HUDReticleDefinition.h; AZ_HUDReticleWidget.h and Private/UI/AZ_HUDReticleWidget.cpp; existing PlayerUITypes/PlayerUIComponent; CommonUI HUD header/cpp; weapon-state fragment header; AZ_Weapon header/cpp.

Source reviews checked delegate cleanup, client unpossession, late ownership, positive-health suppression, inventory listener ordering and ScaleBox/API signatures. The user completed the full build and restarted Editor. C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt reports **Result: Succeeded**, 29.06 seconds; the DLL is newer than the reticle/ownership source files, and the new reflected types are loaded. Whitespace checks pass. No C++ changes followed this successful build.

## Ballistics/meaning

The center indicates the camera's intended aim direction. Actual fire traces from camera, then corrects muzzle direction toward that point with nearby-obstruction protection, so the reticle does not guarantee a clear muzzle path. Current GA_FirearmFire uses WeaponStateFragment.SpreadAim as a full cone angle (half-angle passed to VRandCone); movement/recoil bloom is not part of this firearm path. The view carries this actual angle for future presentations, but the current reticle remains static.

Do not use IsActiveWeaponSource as a synchronous presentation predicate inside equipment publication: bCommitting is still true during OnEquipmentChanged. Read the committed fields and ownership as implemented.

## Saved assets and scripts

- C:/UnrealEngine/Games/AZ/Tools/hud_reticle_assets.py — ProgrammaticToolset source, default read-only audit. Creates /Game/AZ/Blueprints/Menu/HUD/Reticles/WBP_AZ_Reticle_Rifle (native reticle parent), a root 40×40 SizeBox + canvas, and the centered ReticleHost in existing WBP_AZ_GameHUD. HitMarker Z becomes 20, reticle host Z 10. No inventory changes.
- C:/UnrealEngine/Games/AZ/Tools/hud_reticle_assign.py — ordinary Unreal Python, default read-only. Creates /Game/AZ/Blueprints/Menu/HUD/Reticles/DA_HUDReticle_Rifle and patches only ReticleDefinition in the existing rifle BP component template and two L_001 placed pickup overrides. Compares all other manifest fields/fragments and contained magazine exports against backup.
- Backup completed through main('backup'): C:/UnrealEngine/Games/AZ/Saved/Backups/HUDReticle/20260909T012344479774/. Active receipt C:/UnrealEngine/Games/AZ/Saved/HUDReticle/backup-receipt.json captures all three manifests and HUD/rifle/map files. If user saves additional rifle/map changes before the rebuild, re-audit and refresh the backup before assignment; never overwrite the newer work.
- Created and compiled WBP_AZ_Reticle_Rifle with 10 independent UMG elements (size wrapper, canvas, four arms and four dark underlays). Created DA_HUDReticle_Rifle and configured its widget, 40×40 size, #EEEAE0 tint and aim-only policy.
- Added the real ReticleHost under HUDRoot with exact center anchors/alignment, autosizing and Z=10. HitMarker remains at its original center/geometry, now Z=20. Other HUD tree elements were compared before/after and preserved. GetWidgets lists unbound native BindWidget declarations with widget=None; the script now distinguishes those from actual authored widgets.
- Patched the rifle template and both placed pickup manifests. Readback before and after rifle Blueprint compilation confirmed all other top-level fields/fragments and contained magazine exports are identical to backup.
- Explicitly saved the leaf, HUD, definition, rifle BP and L_001. The map initially remained clean after component setters, so the current world was explicitly marked modified and saved; its updated file timestamp was verified. The assignment script now marks the blueprint/world/components modified before writing, preventing a future clean-package save from skipping placed overrides.
- Inventory assets/appearance, inputs, animation/skeleton assets and firearm settings were not changed by this reticle addition. Unrelated dirty MetaHuman/retargeter packages were excluded from saves.
- Saved receipts: C:/UnrealEngine/Games/AZ/Saved/HUDReticle/assign-readback.json, verify-readback.json, widget-readback.json and final-editor-readback.json. A genuine live widget thumbnail was captured to C:/UnrealEngine/Games/AZ/Saved/Screenshots/WindowsEditor/RiderMCP/20260909-020432_preview_WBP_AZ_Reticle_Rifle.png; it is thumbnail-scale, not a runtime gameplay capture.

## User gameplay check remaining

Equip the rifle and hold aim: the four-tick reticle should appear at screen center. Release aim, open inventory or holster: it should disappear. A confirmed hit should flash the separate diagonal X. Empty ammunition should leave the aim reference available. Also check normal firing and the preserved inventory appearance. The user starts PIE; Codex has not started PIE or automated tests. Runtime multiplayer/late-owner behavior remains source-reviewed rather than gameplay-tested.

## Adding another reticle

Create a Blueprint child of UAZ_HUDReticleWidget for the new visual, create/duplicate a UAZ_HUDReticleDefinition, select its widget/size/tint/aim-only policy, and assign it to that weapon's ReticleDefinition field. The same centered host, lifecycle and GAS visibility pipeline are reused. Null definition means no reticle. Keep decorative recoil/bloom separate from actual shot-spread data; no new C++ switch on weapon tags is required.

Never import/rerun rifle_inventory_content_setup.py: its unguarded main recreates the old foundation manifest and would erase subsequent rifle work. The assignment script uses only guarded pure helpers from rifle_p01_activate.py. No animation/skeleton, input, inventory-layout or firing-data edits belong in reticle authoring. No automated tests or PIE started by Codex.
