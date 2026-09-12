# Quick select revision — four manual slots and inventory composites

Current requested revision is V3: eight smaller cells, center arrows, focused description and an Explore/Fight toggle for 0. Source/scripts are prepared; build/UI authoring pending. See C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-eight-slots-status.md. It supersedes the four-cell and idempotent-0 behavior described below.

September 9, 2026. Implemented and saved after the user corrected the first selector: all four outer positions must remain visible, begin empty, and be populated by the player. Slot 0 is permanent Fists. Mouse activation must equal the corresponding number key. Actual item names replace LONG GUN/category headings. This supersedes the two-slot/auto-fill/category-restricted design in earlier documents.

## Current behavior

- Five configured positions: 0 Fists at center; 1 left, 2 right, 3 top, 4 bottom. All enabled. The four physical bindings and ready-item identity are empty on the controller default. Picking up or equipping an item does not populate a quick slot.
- Any of slots 1–4 accepts an initialized, owned, top-level backpack weapon or consumable with positive quantity. Weapon definitions must retain valid equipment data. Loose magazines, arbitrary armor/accessories and craft-material entries are not quick-use candidates. Item-type restrictions were removed from these generic slots.
- Tab toggles. MMB locks an edit target, wheel previews compatible owned GUIDs, second MMB assigns only. RMB and keys 0–4 use the same idempotent ActivateSlot path. Slot 0 no longer toggles Fists off. Cancel/blank-click and authoritative revision checks remain.
- Weapons activate through Equipment. Consumables set a separate replicated ReadyItemId and show READY; this does not alter weapon grants, spend an item, or apply an effect. A newer consumable selection cancels an older pending equipment request. Removal, rebinding away, pawn changes and successful equipment changes clear readiness.
- Actual consumable use remains separate, following the prior selection/readiness contract. The user was asked whether immediate use is preferred but did not change that contract. Current sample potion effects are incomplete; no healing or quantities were invented. The revision makes those Consumable-category items assignable/selectable, not magically functional healing items.
- Empty physical cards show a centered dash and actual shortcut label, at 75% opacity, with no fake icon, item name or count. Assigned cards show the inventory item's name (such as M16), actual icon, actual magazine rounds/capacity or the bound consumable stack's GetTotalStackCount(). Fists has no quantity.

## Fists and knife decision

Fists remains an intrinsic weapon profile with its existing grants and a permanent 0 shortcut. It is not a stack of inventory objects, so no ammo/count/durability number is displayed. The compact 92x96 center card uses a newly drawn fist silhouette. Future knives should be real owned weapon items assignable to any physical slot; show durability only after real durability gameplay exists.

Reference context: Naughty Dog's [Part I accessibility controls](https://blog.playstation.com/2022/08/26/the-last-of-us-part-i-full-list-of-accessibility-features/) document melee combos separately from backpack weapon swapping. Capcom's [Resident Evil 4 (2023) Player Actions manual](https://game.capcom.com/manual/re4/en/ps5/page/2/2) documents knife readiness/parrying separately from weapon shortcuts; its [HUD manual](https://game.capcom.com/manual/re4/en/ps5/page/3/1) displays knife durability separately. These support treating melee availability differently from counted consumables. They do not establish that RE4 knives can be shortcut-assigned, and this document makes no such claim. Capcom's SPA content was verified from its official English locale data when web text fetching returned 403.

## Actual inventory composite reuse

The quick-select interaction shell now contains a real UAZ_Inv_CommonUI_CompositeWidget, with UAZ_Inv_CommonUI_LeafWidget_Text and UAZ_Inv_CommonUI_LeafWidget_Image children. This is the same inventory fragment-assimilation path, not a parallel icon/title renderer:

1. Collapse the previous composite leaves.
2. Set the exact name role to Item.Fragment.Name.StaticText or Item.Fragment.Name, and the image role to Item.Fragment.Icon.
3. Call Manifest.AssimilateInventoryFragments(ItemDetails).
4. Display live ammo/stack/ready/equipped state from the existing authoritative view, following the inventory description's distinction between static fragments and runtime state.

The native role setters are used because EditInstanceOnly leaf properties cannot be edited on WidgetTree templates through the editor tools. Empty preview text is retained; the stock inventory description widget was not embedded or changed. Physical category headings and legacy direct name/icon widgets are hidden. Intrinsic Fists uses its own compact card and direct intrinsic label/icon.

New assets under /Game/AZ/Blueprints/Menu/HUD/QuickSelect:

- WBP_AZ_QuickSelectNameLeaf (required Text_LeafText CommonTextBlock)
- WBP_AZ_QuickSelectImageLeaf (required SizeBox_Icon and Image_Icon)
- WBP_AZ_QuickSelectItemDetails (actual name/image leaves and fitted image layout)
- WBP_AZ_QuickSelectFists (compact center card)

Existing root and entry were updated in place. All four outer host positions were preserved; only the center host became compact. The root's CenterEntryWidgetClass selects the Fists card. Inventory assets/layout, rifle data, HUD health/ammo/magazine icon and fire-mode behavior were preserved.

## Input and artwork

Created player-mappable AZ_IA_QuickSlot_2, _3 and _4 under /Game/AZ/Blueprints/Input/AlwaysAllowed. Preserved 0/1 action references. In the active Pawn IMC, Two previously mapped to unused RT_SecondaryWeapon; only that mapping's action reference was replaced, preserving its other settings. Three/Four were appended. Every other Pawn mapping, shared Tab mapping and protected controller reference matched backup.

Artwork: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/T_HUD_Fists.png and CHALK_Fists_NATIVE.xcf, 128x128 with six native vector layers/paths. Drawn using GIMP's path tools through C:/UnrealEngine/Games/AZ/Tools/quick_select_fist_art.py. Imported to /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Fists with UI compression, no mipmaps and NeverStream. PNG was visually inspected.

## Crash recovery, build and verification

Default Interchange texture import stalled the editor at 19:25:24 UTC: no CPU/log progress, unresponsive process, no saved fist uasset. The user restarted and rebuilt. The source/artwork were safe. C:/UnrealEngine/Games/AZ/Tools/quick_select_import_fists.py now supplies an explicit TextureFactory; local AssetTools.cpp confirms this bypasses Interchange without changing global settings. That import completed and saved successfully.

Full build **Result: Succeeded, 14.11 seconds**, loaded reflected readiness/composite fields after restart. One later existing-function change assigns the image leaf's tag before assimilation; Live Coding **Result: Succeeded, 5.83 seconds** and patch creation succeeded. No new reflected field was introduced in that patch. It is active in the current editor; include the source in the next full build before a later editor restart.

Composite leaves/details, Fists, physical entry, root and controller were compiled through dedicated tools after authoring/default changes. Explicit saves covered only the new/modified quick-select assets, PC and Pawn IMC. Manual-slot, composite and widget verification passed on freshly resolved defaults. Final readback: five enabled slots, slot 0 icon/label/intrinsic grants preserved, four blank physical definitions with invalid default GUIDs, five shortcut action references, no dirty packages, no PIE. No automated tests were added or run. Runtime interaction and full in-game visual matching await the user's check.

## Scripts, backups and receipts

- C:/UnrealEngine/Games/AZ/Tools/quick_select_composite_assets.py — leaves → external compile → details → external compile → verify. Native entry supplies role tags at runtime; author_details can resume its own partial tree.
- C:/UnrealEngine/Games/AZ/Tools/quick_select_v2_widgets.py — updates entry/center host, creates compact Fists; verify accepts GetWidgets' Blueprint asset class-path representation.
- C:/UnrealEngine/Games/AZ/Tools/quick_select_manual_slots.py — audit/backup/assign/verify for five manual slots, keys 2–4 and center class. Preserves authoritative Fists fields and unrelated mappings/references. **Do not run the superseded two-slot quick_select_assign.py or original authoring routines over this revision.**
- UI backup: C:/UnrealEngine/Games/AZ/Saved/Backups/QuickSelectV2/20260909T193448545130/.
- Manual-slot backup: C:/UnrealEngine/Games/AZ/Saved/Backups/QuickSelectManualSlots/20260909T194311132979/.
- C:/UnrealEngine/Games/AZ/Saved/QuickSelectV2/ contains fist-import, composite-final, widgets-final and final-editor readbacks. C:/UnrealEngine/Games/AZ/Saved/QuickSelectManualSlots/ contains assignment/protection/verification receipts.

Next user check: open Tab with an empty loadout; verify all four empty cards plus Fists. Pick up items and confirm slots stay empty. Assign a rifle/consumable to any outer slot with MMB/wheel/MMB, verify actual name/icon/count, then compare RMB with that number key. Verify 0 repeatedly selects Fists, empty-space clicks do nothing, and assignments clear rather than auto-refill after drop. No consumable should spend or heal merely because it was assigned/readied.
