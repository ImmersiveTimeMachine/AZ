# Magazine display and one-cell footprint — implemented

September 9 local / September 10 UTC, 2026. The user asked to keep the HUD magazine icon/count, correct the starting count shown on dropped magazines, and make each magazine occupy one inventory cell.

## Result

- Main HUD: restored the existing MagazineCountRow, MagazineIcon and SpareMagazinesText when a supported firearm is equipped. The count comes from the existing inventory snapshot and means compatible backpack magazines containing rounds; inserted and empty magazines remain excluded. Reload policy and physical magazine ownership are unchanged.
- World pickup text: GetPickupMessage now formats a magazine's real PickupState.CurrentRounds and capacity. A dropped empty magazine displays 0/30; a partially used magazine displays its actual remainder. Missing replicated state displays -- rather than an authored initial load. Non-magazine pickups retain their authored prompts.
- The controller refreshes a focused pickup when its message changes, even if the actor stays the same. This handles pickup state arriving after the manifest without repeatedly updating an unchanged prompt.
- Magazine footprint: all nine authored M16 magazine manifests changed from 1x2 to 1x1. This covers the three magazine Blueprint roots, the rifle's initially inserted magazine, three placed magazines and both placed rifles' initial contents in L_001. Rifle roots remain 3x2. No inventory panel layout was changed.

The apparent ammo reset was a display defect: the authoring script had baked 30/30, 17/30 or 0/30 into PickupMessage. Source review found that drop/re-pickup already copies runtime identity and rounds, including zero; that persistence path was preserved.

## Source and assets

- C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/AZ_Inv_CommonUI_ItemComponent.h
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_ItemComponent.cpp
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp
- C:/UnrealEngine/Games/AZ/Tools/magazine_grid_one_cell.py — guarded audit/backup/author/verify helper; importing it does nothing.
- C:/UnrealEngine/Games/AZ/Tools/rifle_inventory_content_setup.py — future defaults corrected to 1x1 and count-free static prompts. Do not rerun its main(), which overwrites later rifle/controller tuning.

Saved packages: BP_Pickup_M16Magazine, BP_Pickup_M16Magazine_Partial, BP_Pickup_M16Magazine_Empty, BPAZ_CommonUI_PickupItem under /Game/AZ/Blueprints/Items/Equippables/Weapons, plus /Game/AZ/Maps/L_001.

## Verification

CLI was blocked by the active Live Coding session. Live Coding then compiled successfully in 22.67s; AZ patch creation succeeded at 02:15:03 UTC and the editor reported Live Coding success at 02:15:06. No new reflected types or fields were added. These source changes are loaded in the current editor; perform a normal build before the next editor restart because Live Coding does not replace the base DLL.

All four target Blueprints compiled with the dedicated Blueprint tool. Their packages and L_001 were explicitly saved. Fresh verification confirmed nine 1x1 manifests and exact preservation of every other captured manifest field, initial round count, capacity, family, pickup prompt and radius. All five on-disk hashes changed. No target package remains dirty; unrelated animation work was left unsaved and untouched.

Backups and receipts: C:/UnrealEngine/Games/AZ/Saved/MagazineDisplayFix/. This includes grid-backup-receipt.json, grid-verify-readback.json, grid-saved-files.json, UBT-magazine-display.log and LiveCoding-magazine-display.log.

User gameplay acceptance remains pending: in a fresh Play session, inspect the HUD count, use some rounds, drop/re-pickup that magazine and an empty one, and check the single-cell footprint. Codex did not start/stop PIE, run editor tests or add automated tests. Existing live inventory objects are not resized in place; new pickups use the saved definitions.
