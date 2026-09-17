# CHALK HUD — iteration 02

The user selected A, questioned the permanent I hint, and requested simple quick access inspired by the supplied cross-shaped selector, with room for consumables, crafted items and RPG systems.

## Included

- 01_Normal_Play.png and CHALK_HUD_v02.xcf: A with the inventory hint and sample pickup prompt hidden. The pickup prompt remains an editable hidden layer for contextual use.
- 02_Quick_Select.png and CHALK_Quick_Select_v02.xcf: a proposed four-direction quick-access overlay, with a med kit selected.
- CHALK_HUD_v02_Review.png / .xcf: comparison and information hierarchy.

All gameplay changes are proposals. No Unreal code/assets, input bindings, PIE or tests were changed. New consumable icons are native GIMP draft geometry; the rifle is the existing inventory texture. The scene is the previous iteration's generated concept plate.

## Visual and interaction decisions

Normal play shows only useful immediate state. I remains an inventory binding but need not occupy the normal HUD; teach it during onboarding and keep it in controls/help. Do not add permanent XP, ingredient totals, recipe lists, or inactive buffs just because RPG systems exist. An active effect can temporarily show one compact icon and remaining time near health, if that duration affects a player decision. That effect state remains a later visual iteration.

Quick access appears only on request. Proposed fixed directions: up = care; left = long gun; right = sidearm; down = utility. One primary item is visible per direction; subtle dots indicate another assigned item in that category. This is a draft bounded quick loadout, not the full inventory. Weapons show ammunition; supplies show quantity. The highlighted item has both a stronger border and a name, so focus does not depend on color alone.

Selecting equips the item; a separate deliberate use action consumes it. Cancel leaves the previous selection unchanged. Holding to open and releasing to equip is one possible input mode; also support a toggle/confirm mode. Exact keys and controller bindings are not assigned in this draft. Existing 0/1 weapon bindings are not changed. Any later modal capture must release held gameplay inputs cleanly.

Crafting and RPG depth live primarily in inventory: recipes, material requirements, equipment comparison, upgrades, and progression. A crafted bandage belongs in care and a crafted smoke bomb belongs in utility. Their origin does not need a permanent HUD label. Do not rearrange quick-slot positions when a stack empties. Keep its slot stable, dim the icon, show 0, and block use. Recipe availability must be distinct from an already-carried item count.

No health meter should disappear while the player needs damage information. Weapon/ammo can fade in exploration and return on selection/aim/change; provide an always-visible option. Critical state should remain visible. HUD scale, contrast and hold/toggle support are part of the design quality, rather than interpreting 'minimal' as tiny or faint. The official [Naughty Dog accessibility reference](https://www.naughtydog.com/blog/the_last_of_us_part_ii_accessibility_features_detailed) documents HUD size/background controls and hold/toggle weapon swapping.

World time during the selector remains a gameplay decision: slowing time may suit solo play, but is not assumed for cooperative play. Crafting should respect the survival stakes and cannot silently be presumed instant or safe. This mockup makes no time-control or recipe-system commitments.

## Existing system check

Current C:/UnrealEngine/Games/AZ/Source/AZ/Public/Inventory/AZ_QuickBarComponent.h exposes weapon selection, cycling and bound inventory identities. Its comment explicitly gives the equipment component ownership of grants and transitions. The new selector should be another presentation of these owners, not a second inventory or weapon state store.

C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h already declares consumable fragments. Their existence does not establish a finished modern crafting or quick-use system. The med kit, sidearm, smoke bomb, counts and effect description in this mockup are illustrative design examples, not claims that these exact items are playable today.

## GIMP editing

Both main XCFs contain the same editable normal and quick-select groups, saved with different initial visibility. The review-board scene previews are flattened, while its labels remain editable. The original user file and v01 files were preserved, including unsaved live GIMP edits.

The builder reuses native helpers and source fonts/textures from C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v01/sources. Reopen with the v01 task-local GIMP config to load the exact fonts; no global font settings were changed. The MCP port was still off during this iteration, so GIMP batch authoring was used again.
