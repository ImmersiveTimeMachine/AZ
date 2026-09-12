# Rifle recoil — next step after restart

Implementation update, 2026-09-09: recoil is built, authored and saved; user gameplay
validation is next. Current resume point:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-recoil-status.md. The following is
the earlier planning record; its "no implementation" status is superseded.

Saved 2026-09-09. User confirmed **all firing now works**, requested recoil, then
asked to keep the next step in mind for the restart. **No recoil implementation or
asset edits have started.** Three read-only source reviews produced the plan below.
Do not restart the editor programmatically; the user controls its lifecycle.

## Confirmed baseline and scope

- Single shots, held automatic fire, X mode selection and both requested hero
  firing animations are the accepted working baseline. The one-frame AUTO release
  bug was fixed by removing PrimaryAttack's Pressed trigger and retaining each
  primary activation attempt in the controller's Started handler.
- Current handoff: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-automatic-status.md.
- **Recoil is the next task**, per the user's latest request. They asked for behavior
  like the GAS example, or a better implementation, with exposed recoil-radius
  parameters. Proposed default is separately tunable camera kick AND bullet spread;
  the user did not explicitly answer the earlier camera/spread options.
- Reload remains an outstanding request: R anytime, auto reload when empty, and
  rifle reload animation. Its magazine swap-versus-round-transfer semantics remain
  unanswered. Do not treat "all works" as proof that reload exists. The later
  request to work on recoil now supersedes the earlier sequencing for the next task.
- No automated tests unless explicitly requested. User runs PIE, then inspect logs.

## Proposed implementation

Use the existing accepted-shot transaction as the trigger. A rejected shot,
targeting preview, held-frame input or animation notify must never add recoil.

### Bullet spread

Keep tunables in the weapon definition, preferably a grouped recoil struct in
FAZ_Inv_CommonUI_WeaponStateFragment (a referenced tuning DataAsset is also viable).
Expose additional radius per shot, maximum additional radius, recovery delay and
recovery speed. Radius uses **cone half-angle degrees**, not world centimetres.
Existing SpreadAim/SpreadAngleDegrees are **full cone angles**; preserve their meaning.

Use a small transient replicated spread snapshot on the inventory weapon item:
peak additional radius, last accepted server time, and recovery settings captured
for that shot. The item survives fire ability restarts, mode changes and replacement
of the equipped weapon actor. Analytical decay avoids authority tick/replication
every frame:

```
extra = max(0, peak - max(0, serverNow - lastShot - recoveryDelay) * recoverySpeed)
fullConeDegrees = clamp(SpreadAim + 2 * extra, 0, 180)
```

Grow the snapshot **inside TryConsumeWeaponRound after ammo/revision/cadence commit
and before NotifyInventoryChanged**. The firing ability samples its pure getter
when constructing the shot; growth applies to subsequent shots. Use the owner's
GameState GetServerWorldTimeSeconds for analytical client recovery. Never mutate
spread from a getter. Explicitly decide/reset transient spread on drop/re-pick;
ordinary ItemState payload copying does not automatically copy a separate transient
spread member. Do not accidentally claim save-game persistence.

Existing AZ_GATA_Trace has BaseSpread, TargetingSpreadIncrement and TargetingSpreadMax,
but increments inside AimWithPlayerController during targeting evaluation. Reuse the
idea, not that update location. Equipped AAZ_Weapon.MakeCosmetic disables actor tick
and its representation can be destroyed on switching, so it is a poor long-lived
recovery owner.

### Camera kick

Prefer pending kick and outstanding recoil-return offset on AAZ_PlayerController.
Override UpdateRotation, inject degree deltas into RotationInput before Super, and
retain the standard camera manager pitch limits and control-rotation path. Direct
AddPitchInput would also apply sensitivity/inversion; a purely cosmetic camera shake
would decouple the visual aim from ballistics.

Consume outstanding return offset when the player counter-steers. Example: after
a +1-degree kick, a -1-degree mouse correction leaves no recoil to return; recovery
must not pull down a second degree. Track actual applied displacement near pitch
limits, not invisible accumulated kick that could later move the camera.

Send reliable owning-controller accepted-shot receipts, including weapon/item ID,
selection generation, shot identity and a tuning snapshot. Validate/deduplicate;
reject stale receipts after selection or possession changes. Existing muzzle/impact
multicast is unreliable and should remain cosmetic.

Expose enable, pitch kick, horizontal kick radius, maximum pitch/horizontal offset,
recovery delay/speed and optional crouch multiplier. Initial values are tuning
proposals only; validate manually. The hero spring arm already has rotation lag10,
so extra attack smoothing can compound that lag.

Clear pending/return ownership on menu/quick select, aim loss, reload, switch/drop,
death/grab/hard blocks, possession changes and EndPlay, preserving the current view.
Do not restore an old absolute camera rotation. Ordinary trigger release should
allow recovery. Grab camera code writes control rotation directly and must retain
priority. Continue using GetPlayerViewPoint for actual camera-origin aim.

### Crosshair

FAZ_PlayerReticleView already exposes full-cone SpreadAngleDegrees. PlayerUI should
read the same inventory-item getter after existing inventory/selection events and
sample recovery only while needed. The reticle widget's visible NativeTick can
update projection geometry for recoil, FOV transitions and resizing.

Reuse the existing ArmUp/Down/Left/Right and OutlineUp/Down/Left/Right images. Bind
them optionally and enable their variable flags with a narrow authoring update.
Project tan(fullCone/2) using LocalPlayer GetProjectionData and its constrained-view
rect; ScreenToWidgetLocal converts pixels through viewport/DPI/host scaling.

Preserve the 40x40 authored desired bounds and move arms with render translations
by max(0, projectedRadius - 6), retaining the decorative six-unit minimum gap.
Do not enlarge desired size and then have the host ScaleToFit shrink the design.
ScaleToFit itself does not clip; verify Inherit clipping through the host/leaf.
Keep reticle definition presentation-only; it must not own another spread setting.
Muzzle parallax and nearby cover still mean a camera reticle cannot guarantee a hit.

## Files to inspect before editing

- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Weapon/AZ_WeaponTypes.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/AZ_Inv_CommonUI_InventoryItem.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryItem.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryComponent.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmFire.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Player/AZ_PlayerController.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_PlayerUIComponent.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_PlayerUIComponent.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_HUDReticleWidget.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_HUDReticleWidget.cpp
- C:/UnrealEngine/Games/AZ/Tools/hud_reticle_assets.py
- C:/UnrealEngine/Games/AZ/Tools/hud_reticle_assign.py

Reticle baseline: C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-reticle-status.md.
Active visual assets: /Game/AZ/Blueprints/Menu/HUD/Reticles/WBP_AZ_Reticle_Rifle and
/Game/AZ/Blueprints/Menu/HUD/Reticles/DA_HUDReticle_Rifle. Preserve current inventory,
quick-select, firing animations, input fixes and bullet-impact work in the shared checkout.

Before requesting a rebuild, finish source and prepare target-only authoring scripts.
New reflected tuning/state requires a full build/restart; use existing build skill.
After success/reopen, assign defaults, compile/save only affected assets and verify
readback. Manual acceptance should cover single/auto growth and cap, recovery,
counter-steering, pitch limits, empty/rejected shots, mode/weapon/menu interruptions,
FOV/resolution behavior, and matching crosshair/shot spread. No tests were run during
this planning session.
