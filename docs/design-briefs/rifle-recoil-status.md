# Rifle recoil — built and saved; user gameplay check pending

2026-09-09: User said "ok good" and moved on to reload. Current reload continuation:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-reload-status.md.

Updated 2026-09-09 after the user completed the recoil build and restarted. Verified
Result: Succeeded (16.32 seconds), main DLL at22:02:27 UTC newer than the changed
native sources, and editor restart22:02:34. Recoil tuning and reticle bindings are
now authored, compiled and saved. Runtime recoil feel remains for user validation.

## Implemented source behavior

- FAZ_FirearmRecoilSettings is a grouped editable Recoil field in the existing
  FAZ_Inv_CommonUI_WeaponStateFragment. It separates bullet-cone radius from camera
  pitch/yaw kick; existing SpreadAim remains the full baseline cone angle.
- Inventory weapon items own a transient replicated spread snapshot, with pure
  synchronized-server-time getters. Only successful TryConsumeWeaponRound commits
  grow it, after ammunition/revision/cadence mutation and before inventory callbacks.
  Queries, rejected/dry shots, input frames and animation notifies cannot grow it.
- Fire traces use the pre-shot full cone. The accepted shot increases the next
  cone and sends one reliable owning-controller camera receipt with source/item/
  selection identity, shot ID and a tuning snapshot. Existing muzzle, damage,
  automatic cadence, impact and hero animation paths are retained.
- Item spread analytically recovers after its delay without ticking authority.
  Normal trigger release, fire-mode changes and replacement of the equipped actor
  preserve recovery. New/reconstructed inventory items, including drop/re-pick,
  explicitly start settled; this transient state is absent from pickup/save payloads.
- Controller UpdateRotation combines raw mouse input and degree recoil before
  Unreal's normal clamp. It tracks only applied kick, consumes counter-steering,
  and removes old recoil debt when the player's input pushes through camera limits.
  Source/mode/aim/menu/quick-select/reload/hard-block/possession cleanup preserves
  current view rather than restoring an old absolute camera rotation.
- PlayerUI uses the same item spread getter and a bounded visible recovery timer.
  Reticle NativeTick projects the full cone through the actual local-player camera,
  constrained viewport and DPI/host transforms. Existing eight arm/outline images
  translate while retaining stroke size, 40x40 authored bounds and minimum gap6.

## Initial exposed tuning

Edit the existing rifle pickup's Pickup Item Manifest -> Weapon State Fragment ->
Recoil. New pickups copy those settings; the setup script also updates both existing
L_001 placed-rifle overrides. These values are initial tuning, not visual acceptance.

| Property | Initial value |
|---|---:|
| Enabled / Camera Kick Enabled | true / true |
| SpreadRadiusPerShotDegrees | 0.08 |
| MaxAdditionalSpreadRadiusDegrees | 1.0 |
| SpreadRecoveryDelaySeconds | 0.15 |
| SpreadRecoverySpeedDegreesPerSecond | 1.25 |
| CameraPitchKickDegrees | 0.55 |
| CameraYawKickRadiusDegrees | 0.18 |
| MaxCameraPitchDegrees | 5.0 |
| MaxCameraYawDegrees | 1.5 |
| CameraRecoveryDelaySeconds | 0.12 |
| CameraRecoverySpeedDegreesPerSecond | 7.0 |

Radius settings are angular **half-angle degrees**. At the existing .5-degree full
baseline cone, maximum extra radius1 gives total radius1.25/full cone2.5 degrees.
Camera yaw kick radius is the random +/- horizontal range per accepted shot.

## Files and prepared authoring

Source changes are under C:/UnrealEngine/Games/AZ/Source/AZ:

- Public/Weapon/AZ_WeaponTypes.h; Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h.
- Public/Private InventoryUI/AZ_Inv_CommonUI_InventoryItem, existing inventory component,
  and Private/AbilitySystem/Abilities/AZ_GA_FirearmFire.cpp.
- Public/Private Player/AZ_PlayerController and UI/AZ_PlayerUIComponent,
  UI/AZ_HUDReticleWidget.

Authoring scripts:

- C:/UnrealEngine/Games/AZ/Tools/rifle_recoil_setup.py — ordinary Unreal Python,
  main('audit'|'assign'|'verify'). Patches ONLY Recoil through detached manifests,
  Modify + notify NEVER, preserving ammo/reticle/animation/mode/impact fields and
  contained magazines. Captures backups of rifle, map, reticle leaf and HUD before
  assignment. Never compiles or saves; caller uses dedicated native tools afterward.
- C:/UnrealEngine/Games/AZ/Tools/rifle_recoil_reticle.py — ProgrammaticToolset script,
  MODE='audit' by default; MODE='author' enables the existing eight variable flags
  and inherited clipping without recreating layout or artwork. Compile/save after.

Reticle audit/readback passed: 10 original widgets, all eight arm/outline variable
flags true, host/leaf clipping Inherit and TickFrequency Auto. Artwork, transforms
and layout were preserved. Both rifle and reticle leaf compiled UpToDate through
the dedicated native BlueprintTools tool and were explicitly saved with L_001.
Recoil defaults match on the pickup template and both placed rifles; unrelated
manifest fields and contained magazines are unchanged. No dirty content/maps or PIE
remained at final readback.

Backups: C:/UnrealEngine/Games/AZ/Saved/Backups/RifleRecoil/20260909T220403/.
Final receipt: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-recoil-asset-readback.json.
Saved working receipts are in C:/UnrealEngine/Games/AZ/Saved/RifleRecoil/.

## Review, limits and exact next action

Three source reviews covered ballistics, camera input and reticle geometry. Fixed
two camera-limit accounting issues before handoff: raw player input must combine
before clamping, and same-direction clipped input must consume old return debt.
Final independent review found no further concrete blocker. Whitespace and Python
syntax checks passed. No automated tests added/run; no PIE started by Codex.

The implementation and authoring steps are complete. No further rebuild/restart
is required. Next is the user's manual PIE check. The native compile tool requires
full object paths (Asset.Asset); the setup script now returns those for compilation
while retaining package paths for saves. No C++ source changes were made after the
successful recoil build during this authoring continuation.

Manual checks after build/readback: single vs automatic spread growth/cap/recovery,
camera counter-steering, pitch limits, aim/menu/mode/weapon interruption, no kick on
empty/refused shots, and reticle agreement across aim FOV/resolution. User performs
PIE; [Fire] logs now include spreadFullDeg for accepted shots.

Known validation boundary: multiplayer camera still uses the existing asynchronous
camera-cache architecture. A unique delayed camera receipt across a rapid aim-off/
aim-on cycle on the same item/generation lacks an aim epoch/timestamp; investigate
if that path is pursued. Co-op runtime behavior is unverified. The decorative
minimum crosshair gap is conservative for tiny/zero spread.

Reload remains requested but unimplemented; magazine swap-versus-transfer semantics
are unanswered. User explicitly moved recoil to the current task after confirming
automatic fire works. Preserve the reload request without treating the confirmation
as evidence that reloading exists.
