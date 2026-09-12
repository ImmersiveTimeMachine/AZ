# Pistol icon direction, existing ammo artwork and proportions

2026-09-11. User requested consistent icon direction, reuse available pistol/ammo
icons first, and correct proportions. Completed independently of the proposed
left-click/3second ready-state gameplay change.

- Reused existing T_HUD_Pistol native vector artwork and reversed it to face left,
  matching Rifle_PrimaryIcon. Kept512x384 source dimensions and128x96manifest size.
- Reused /Game/InventorySystemPro/ExampleContent/Common/Art/Ammo9mm/T_Ammo9mm for
  the pistol magazine inventory entries, matching the established rifle inventory
  convention of using ammunition artwork. This is UI reuse, not new loose-ammo
  items or a change to caliber, compatibility, capacity or per-magazine ownership.
  The main HUD's generic magazine icon/count remains as before.
- Updated exactly8 component templates/placed instances, including the pistol's
  initially inserted magazine manifest. Only the magazine icon/dimensions changed;
  all other exported fields, texts, settings, counts and families were retained.

Distortion was real: main HUD WeaponIcon filled174x87 (2:1), while the pistol's
2x2 inventory/drag image filled square cells. Added ScaleToFit around the existing
HUD WeaponIcon and grid/hover Image_Icon, retaining every outer slot and layout.
Quick-select already preserves aspect; description follows manifest dimensions.
The empty equipped-item Widget Blueprint was not given an invented widget tree;
its native slotted-item base receives the shared brush sizing correction.

UI-only C++ bodies now use natural texture proportions and fit brushes within the
supplied grid footprint. The user's normal build at20:51:20UTC succeeded8.30s;
DLL20:51:27, editor restart20:51:34 includes all three UI source files. No gameplay
changes were included. Three Widget Blueprints compiled/saved; saved readback
confirmed original widgets/properties/order and unrelated slot geometry retained.

Source scripts: Tools/pistol_icon_export_art.py, Tools/pistol_icon_assign.py,
Tools/weapon_icon_fit_setup.py. Receipts in Saved/PistolIcons include before,
authored, verified widget trees and eight-component manifest readback. Backups:
Saved/Backups/PistolIcons and Saved/Backups/WeaponIconFit/20260911T165517.
Final pistol-left and existing-ammo textures were visually inspected via live
asset previews. Target packages were clean at completion. Codex stopped user Play
for UI authoring; it did not start PIE or run gameplay tests.

Gameplay proposal, not implemented:
C:/UnrealEngine/Games/AZ/docs/design-briefs/firearm-ready-gameplay-plan.md.
