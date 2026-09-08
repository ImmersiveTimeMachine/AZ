# Rifle inventory foundation — resume and manual acceptance

Updated 2026-09-07 after recovery from the interrupted implementation session.

The first pickup → inventory → equipment/switch → drop slice is implemented, built and confirmed working by the user after the capsule-overlap correction. The authorized scope is this first milestone from C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-inventory-magazines-plan.md; firing, aiming animations and functional magazine reload are later milestones.

## Implemented

- Stable rifle and magazine instance GUIDs, item locations, parent/inserted-magazine linkage and per-magazine rounds. Rifle and magazine items are non-stackable. Dropped rifles carry their inserted magazine record; the inventory transaction preserves both identities.
- Authoritative capacity and grid placement, independent of an open widget. Failed pickup/drop preparation preserves the current owner. Inventory widgets display committed placements and real magazine counts.
- I opens CommonUI inventory; E uses the current Mover pawn's nearby pickup. Menu capture resets movement intent and suppresses outgoing weapon input while release handling stays active.
- CommonUI equipment owns weapon selection, ability grants and basic hand/back presentation. QuickBar and inventory equip use that owner. Fists retain their existing three authored combat abilities and combat-ready effect. The first rifle grants no legacy firearm abilities.
- Selection queues at committed action boundaries. An owning-client RPC clears the same outgoing input tags as authority; grant/cancellation ownership stays on the server.
- Inventory descriptions reuse the actual widget ammo row and replace static demo values with the inserted magazine's rounds/capacity. Existing icons are temporary presentation assets.

## Saved content and controls

| Content | Configuration |
|---|---|
| /Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem | M16 rifle; 3×2 cells; Weapon.Rifle; M16.Standard magazine family; starts with one inserted 30/30 magazine |
| /Game/AZ/Blueprints/Items/Equippables/Weapons/BP_Pickup_M16Magazine | Separate 30/30 magazine; 1×2 cells |
| /Game/AZ/Blueprints/Items/Equippables/Weapons/BP_Pickup_M16Magazine_Partial | Separate 17/30 magazine; 1×2 cells |
| /Game/AZ/Blueprints/Items/Equippables/Weapons/BP_Pickup_M16Magazine_Empty | Separate 0/30 magazine; 1×2 cells |
| /Game/AZ/Blueprints/Player/BP_AZ_PlayerController | Existing fists slot 0 preserved; inventory-backed rifle slot 1 added |
| /Game/AZ/Blueprints/Input/InputActions/RT/AZ_IA_RT_Weapon_0 and AZ_IA_RT_Weapon_1 | One InputTriggerPressed each; no second selection on release |

Controls: **E** pickup, **I** inventory, **0** fists, **1** rifle. Selecting the already-active slot toggles it off. Rifle pickup binds its item to the rifle QuickBar slot; selecting fists carries the represented rifle on the back. Both body sockets exist on the active Mover hero's body mesh.

The existing level /Game/AZ/Maps/L_001 contains two rifle pickups and now three magazine pickups in the `Inventory pickups` folder, at (-120,310,240), (-20,310,240), and (80,310,240). The level file C:/UnrealEngine/Games/AZ/Content/AZ/Maps/L_001.umap is saved locally and excluded by the repository's existing ignore policy. The pickup Blueprint assets are under the tracked content tree.

Authoring script: C:/UnrealEngine/Games/AZ/Tools/rifle_inventory_content_setup.py. It writes pickup/controller/action defaults without starting PIE. Backups of overwritten assets and the local level are under C:/UnrealEngine/Games/AZ/Saved/Backups/RifleInventoryFoundation/.

## Verification completed

- Full editor-closed AZEditor Win64 Development build: **Result: Succeeded**, including the new client RPC and final description changes. A subsequent up-to-date build also succeeded.
- Editor restarted. Read-only inspection confirmed saved non-stackable manifests, rifle's 30/30 inserted magazine, separate 30/17/0 magazines, correct mesh assignments, both QuickBar definitions, single-press triggers and placed pickups. Receipt: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-inventory-asset-readback.json.
- Three independent source reviews covered inventory transactions, equipment and UI/input. `git diff --check` passed.
- No automated tests were added or run; no PIE or editor gameplay tests were started by Codex.

## User confirmation and regression checklist

User reported the missing E pickup prompt. Inspection found the Mover capsule had `bGenerateOverlapEvents=false` in both native construction and the active hero Blueprint, while the visual mesh had no collision. The pickup sphere therefore could not populate overlap targets. Enabled capsule overlap events in C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp and saved the active hero Blueprint's capsule override. The existing HUD ShowPickupMessage → SetText → Visible graph is connected correctly. Live Coding compilation and patch creation succeeded.

The user subsequently reported "everething works". Read-only inspection of their PIE log at 04:40–04:41 UTC confirms rifle pickup with one inserted magazine, full/partial magazine pickups, rifle selection with zero legacy ability grants, and successful rifle/magazine drops. The rifle's item and inserted-magazine IDs match between pickup and drop. The reviewed log does not establish re-pick of that same dropped rifle, full-capacity rejection, crouched interaction or network behavior individually; the following remains the regression checklist rather than a claim that every case was instrumented.

1. In L_001, pick up a rifle and each magazine with E. Open I and check the rifle's 30/30 badge and separate 30/30, 17/30 and 0/30 magazines.
2. Close inventory. Use 1 to select the rifle and 0 to select fists. Check hand/back attachment, no double switch on key release, and punches/heavy strikes after returning to fists.
3. Equip through the inventory equipment slot as well; QuickBar selection should agree. Use the inventory context action to drop and then recover the rifle and magazines. Verify their counts persist.
4. Repeat pickup/switch/drop while crouched, and check that closing inventory restores movement and fresh attack input.
5. Check a full equippable grid refuses another pickup while leaving it in the world. Check switching during an attack finishes or cancels through the existing recovery boundary.

After the user runs these checks, read C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log for `[Inventory]` and `[Equipment]`. Pickup/drop logs include item and inserted-magazine IDs, so the round-trip identity can be compared. Do not start PIE or editor tests without asking the user, per C:/UnrealEngine/Games/AZ/AGENTS.md.

Existing startup Blueprint errors in the old AZ_ABP_HeroPawn and MenuSystemPro BP_MenuSystemLibrary also occur in the pre-recovery log C:/UnrealEngine/Games/AZ/Saved/Logs/AZ-backup-2026.09.07-04.03.52.log. They are separate from this C++ build and remain untouched. Other pre-existing animation, obstacle-sensor, CMC, plugin and test-directory changes remain in the working tree; this recovery did not commit them.
