# Winchester tube magazine (R3) - design

Status: **design v1, 2026-09-27, waiting for the user's go.** Plan: `winchester-rifle-integration-plan.md` R3 / R4.
Decision D5 (tube + loose cartridges, one shell per cycle, interruptible) is the requirement.

## 1. Idea - the tube IS a magazine item that never leaves the gun

Every firearm path in the project (fire, readiness, recoil, spread, HUD ammo, quick select, campaign save, pickup
payload) is keyed on `FAZ_Inv_CommonUI_WeaponStateFragment::bUsesDetachableMagazines` and reads rounds from the
magazine item inserted in the weapon (`InsertedMagazineId`, child record with `Location = WeaponMagazine`). A pickup
already carries its inserted magazine (`UAZ_Inv_CommonUI_ItemComponent::InitialContainedItemManifests`).

So the Winchester keeps that model unchanged and only changes what "reload" means:
- `BP_Pickup_Winchester`: `bUsesDetachableMagazines = true`, `MagazineFamily = Winchester.Tube`, one contained item
  `Item_Winchester_Tube` (`MagazineFragment`: family `Winchester.Tube`, Capacity 7, InitialRounds 7).
- New flag on the weapon fragment: `bInternalMagazine` (default false). True means: the inserted magazine is part of
  the gun - it is never swapped, detached, dropped separately or shown as a separate grid item.
- New item `Item_Cartridge_3030`: stackable, new fragment `FAZ_Inv_CommonUI_CartridgeFragment { FName CartridgeFamily }`
  (`Winchester.3030`); the tube's `MagazineFragment` gets `FName CartridgeFamily` (empty for every existing magazine).
- Reload with `bInternalMagazine`: repeat { take 1 cartridge from the inventory stack -> tube rounds + 1 } while
  rounds < capacity, cartridges > 0 and no interrupt; every transfer is one server-authoritative commit.

Result: fire, HUD, save/load, recoil, spread, quick select, pickups work for the Winchester with zero new code; the
M16 / pistol paths are untouched because every new branch is behind `bInternalMagazine` (false for them).

## 2. Changes (all additive)

| file | change |
|---|---|
| `AZ_Inv_CommonUI_ItemFragment.h` | `WeaponStateFragment::bInternalMagazine`; `MagazineFragment::CartridgeFamily`; new `CartridgeFragment` |
| `AZ_Inv_CommonUI_InventoryComponent` | `CanLoadCartridge(WeaponSource, WeaponItemId, Generation)`, `TryBeginCartridgeLoad(..., ActionId)`, `TryCommitCartridgeLoad(ActionId)` (server: consume 1 from a cartridge stack of the tube's family, tube `CurrentRounds + 1`, `AmmoRevision + 1`, same identity / generation / revision checks as `TryBegin/CommitMagazineReload`), `EndCartridgeLoad`; `GetWeaponAmmoSnapshot`: reserve = cartridge count for internal magazines; guards: `CanReloadMagazine`, detach / unload / drag of an internal magazine -> false |
| `AZ_GA_FirearmReload` | branch on `bInternalMagazine`: montage with sections Start -> Load (loop) -> End; a timer per Load cycle commits one cartridge at the cycle's "shell in" time; next cycle only if `CanLoadCartridge` and no interrupt; End section then `EndAbility`. Interrupt: a fire press sets `bInterruptRequested`; the current cycle completes, End plays, the ability ends, the fire input is re-queued (held input re-activates - see `feedback_held_input_retries_ability`) |
| `UAZ_WeaponAnimationProfile` | `ReloadMontageStanding / Aim / Crouching` (UAnimMontage) + section names; used only when set |
| `AAZ_Weapon` | `Multicast_BeginReloadAnimation` overload for montage + section jump (cosmetic, same as today) |
| UI / HUD | ammo widget: rounds in tube + cartridges in the backpack (data from the snapshot, no widget logic change); grid: the tube never shown as a separate item (same rule as an inserted magazine today) |
| Content | `Item_Winchester_Tube`, `Item_Cartridge_3030` + `BP_Pickup_WinchesterAmmo` (box of 10), tags `Winchester.Tube` / `Winchester.3030` |

## 3. Animation (R4 feeds this)
**R4 measured 2026-09-27** (hand_r / index_03_r / thumb_03_r / lowerarm_r in weapon space, 60 Hz): the three clips
`AZ_MST_Rifle01_St_Reload_Winch`, `Rifle02_St_Reload_Winch`, `Rifle_Cr_Reload_Winch` (4.667 s each) have the SAME hand
motion relative to the gun, so one section layout serves all three:
- hold 0-0.80 s; the right hand leaves the grip at 0.80 (`AZ_Grip_R` 1 -> 0), fetches shells, reaches the gate ~1.70;
- **shell push cycle = 0.300 s** (best period, mean seam error 0.06 cm; index tip lowest = push at 1.733, 2.033, 2.333,
  2.633, 2.933, 3.233 s - six pushes in the clip);
- the hand returns to the grip 3.53-4.13 (`AZ_Grip_R` back to 1 at 4.13), still to 4.667.
Sections: **Start 0-1.733, Load 1.733-2.033 (loops, seam 0.07 cm), End 3.233-4.667**. Commit one cartridge at the end
of each Load cycle (the push). Full 7-shell reload = 1.733 + 7 x 0.300 + 1.433 = 5.27 s; one shell = 3.47 s.
`AZ_MST_Rifle0{1,2}_St_Reload_Winch`, `Rifle_Cr_Reload_Winch`: measure the right hand vs the loading gate (weapon
space) and the `az_weapon_r` track -> Start (gun turned, first reach), Load = one push-in cycle (loop point at the same
hand pose), End (gun back to carry). Montages `AM_Winchester_Reload_<Rifle01|Rifle02|Cr>`; commit time = the frame the
hand pushes the shell in. Acceptance: Load loops with <= 1 cm hand pose delta at the seam.

## 4. Failure axes (checked in the implementation review)
1. Network: every transfer is a server commit with action id + generation + revisions; a stale client request is
   rejected like a stale magazine swap today. Client prediction: none (cosmetic montage only).
2. Interrupt timing: fire pressed during the last cycle -> no extra cycle; pressed during End -> fire after End.
3. Crouch / stand mid-reload: today's reload ends on a stance change (`bReloadCrouching` check). Tube: finish the
   current cycle, then End in the new stance's montage (no rounds lost; the committed ones stay).
4. Empty inventory mid-loop: `CanLoadCartridge` false -> End.
5. Weapon switch / holster / death / grab mid-loop: the ability is cancelled; committed shells stay, the in-flight one
   is not consumed (commit happens only at "shell in").
6. Save / load: the tube is the inserted magazine record -> saved today; cartridges are normal stacks.
7. Drop the Winchester: the pickup payload keeps its contained tube with its rounds (existing code).
8. Other magazine UI (load-magazine menu, quick-select magazine ring): an internal magazine is excluded everywhere
   `CanReloadMagazine` is asked; the quick-select shows no magazine ring for it.
9. Fire-mode UI: `SupportedFireModes = {Single}` for the Winchester; the lever cycle (R5) sets `FireRate`.
10. Regression: M16 / pistol reload, magazine load menu, HUD, save/load (user PIE list in R10).

## 5. Work split
Opus: this design, the reload-ability branch and the inventory transfer (network-critical), review. Sonnet: fragments,
items, pickups, profile fields, montages from the R4 numbers, HUD data hookup. One editor-closed build (header changes).
