# Rifle firing — built and wired, ready for user validation

2026-09-09: User confirmed this single-shot slice works. Animation + automatic fire
are now underway; reload is requested before configurable recoil. Current resume
point: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-automatic-status.md.

Updated 2026-09-08. User resumed rifle mechanics. The next planned slice is firing
after the existing pickup/equipment/aim integration. Aim-required, single-shot is
the stated initial assumption; the optional hip-fire question has not been answered.
Reload and the finished HUD remain later milestones.

## Source implementation

- New `UAZ_GA_FirearmFire` uses fresh press only. Holding primary input does not
  repeat or defer a refused shot. The existing fist held-input path is preserved.
- Authority resolves the active inventory rifle, actor source and equipment
  generation; traces camera intent, capsule interior to muzzle, then muzzle to
  aim. Nearest Visibility scenery or Pawn-object hit wins because the current
  pawn collision profiles ignore Visibility. This first slice targets physical
  pawn query geometry; it does not implement headshots or bullet penetration.
- Inventory owns the debit. The request compares magazine identity and persisted
  `AmmoRevision`, validates reciprocal parent links, then decrements one round and
  increments revision before publishing. Cadence is enforced per inventory weapon
  even after ability regrant. Shot IDs are diagnostics; revision and the one-shot
  ability guard prevent duplicate acceptance. Revision travels with pickup/drop state.
- Accepted hostile live-target hits use `UAZ_GE_Damage`, `SetByCaller.Damage` and
  the hit-result effect context. Authority publishes hearing noise and cosmetic
  sound/Niagara; only actual health loss triggers the owner's hit indicator.
- Existing active `UAZ_InventoryHudWidget` displays rounds/capacity and compatible
  spare magazine count (including empty magazines). It observes inventory/equipment
  changes, distinguishes no magazine from unresolved replication, and has brief
  confirmed-hit feedback. This is a functional readout, not the approved HUD finish.
- Menu/hard-block cleanup also cancels firearm input abilities. Selection/drop
  already cancel outgoing grants. Aim-required firing ends when aim disappears.

## Content setup applied and saved

`C:/UnrealEngine/Games/AZ/Tools/rifle_fire_setup.py` audits by default and authors
only with `main(prepare=True)` after native class availability and no PIE. It backs
up the affected assets/map, adds the measured Muzzle socket, creates
`/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_AZ_GA_FirearmFire`, and appends
its grant to the existing pickup template and placed rifles. Magazine definitions
and unrelated manifest fields are preserved. The earlier aim-authoring script now
preserves this known firing grant when rerun.

Applied to the pickup template and both placed rifles in L_001. The firing
Blueprint and pickup Blueprint were compiled natively and saved afterward;
both report UpToDate. The initial magazine manifests are unchanged. Backups:
`C:/UnrealEngine/Games/AZ/Saved/Backups/RifleFire/20260908T185017/`.

The socket was independently measured from the read-only FBX: 75 front-ring
vertices in the compensator, mesh-space (-1.0823059, 60.2704086, 11.9348431),
forward +Y. Current live mesh bounds and reference bone transform were checked.
Local offset on `UE4_M16_CompensatorMod` is
(-1.0823059, 60.2804844, 11.9165097), rotation approximately (pitch=-0.000011,
yaw=90, roll=0). The socket is now authored and saved. Readback reproduced the
measured location and forward (0, 1, 0); final visual alignment remains pending.

Existing assets selected for initial presentation:

- `/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue`
- `/Game/sA_Megapack_v1/sA_ShootingVfxPack/FX/NiagaraSystems/NS_AR_Muzzleflash_1_ONCE`

Their existence/classes were verified live; audio/VFX appearance remains a manual
acceptance item. The prototype does not yet play a character recoil animation.

## Verification and next action

- Source reviewed across inventory, firing/input, equipment and cosmetics.
- `git diff --check` and Python syntax checks passed.
- Read-only setup audit passed. Receipt:
  `C:/UnrealEngine/Games/AZ/Saved/RifleFire/audit.json`.
- User completed the regular AZEditor build and restarted. Read the build log:
  **Result: Succeeded**, 50 actions, 30.18 seconds, including FirearmFire and HUD
  compilation plus the main UnrealEditor-AZ.dll link.
- Native Blueprint compilation logged `[RifleFireAssetCompile] Fire=1 Pickup=1`.
  Its temporary guarded C++ helper compiled successfully and was immediately
  removed from the source tree after execution. No runtime source fixes were needed.
- Final saved readback verified PrimaryAttack input, the native instance's
  Ability.State.Shooting tag, Weapon.Rifle requirement, one firing grant per
  rifle, unchanged magazine defaults, and the muzzle position/axis. No dirty
  content or map packages remained. Receipt:
  `C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-firing-asset-readback.json`.
- No automated tests were added/run. No PIE/editor gameplay tests were started.
  PIE was stopped before asset authoring and remained stopped at final readback.

Next: user performs manual PIE acceptance, then read the logs. No further rebuild
or restart is required for this firing slice. Runtime firing, damage, visual effects
and interruption behavior remain unverified until that run.

Manual acceptance after a successful build and content readback: E pickup, 1 rifle,
hold RMB, click LMB. Each accepted shot must produce one `[Inventory] Shot debited`
and one `[Fire]` line with matching IDs and exactly one round lost. Hold LMB to
verify no repeats; release RMB, switch to fists, open inventory, or drop the rifle
to verify cleanup. Verify standing/crouched near-cover obstruction, hostile damage,
empty/no-magazine rejection, and drop/re-pick preservation. Remote moving camera
aim uses the server camera cache and remains unverified in co-op.
