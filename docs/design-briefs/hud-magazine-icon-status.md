# HUD magazine icon — implemented and saved

Latest: the user requested keeping this row after the circular-reload work hid it. Native visibility/count binding is restored using the same saved artwork and layout. Current status: C:/UnrealEngine/Games/AZ/docs/design-briefs/magazine-display-fixes-status.md.

September 9, 2026. The user requested the small magazine icon from the approved HUD reference, then a plan for quick select.

The icon already existed as native GIMP geometry in the v03 mockup, but it had not been exported/imported into Unreal. Reused that exact silhouette and its two grooves. C:/UnrealEngine/Games/AZ/Tools/hud_magazine_export_art.py renders it with GIMP's native vector tools at 4x size, preserving three vector layers and three paths in the standalone XCF.

- Editable source: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/CHALK_Magazine_NATIVE.xcf
- Transparent 72x120 PNG: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/T_HUD_Magazine.png
- Imported texture: /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Magazine — UI compression/group, no mipmaps, sRGB and NeverStream.

Added MagazineCountRow (HorizontalBox) and MagazineIcon (Image) inside WBP_AZ_GameHUD's existing WeaponContainer. The icon is 18x30 logical units, tinted with the existing chalk text color, with an 8-unit gap before SpareMagazinesText. Reparented the existing native-bound text into that row; its value, font/style and native magazine snapshot binding are unchanged. The row sizes to its content and aligns to the original right edge (308,87), so the icon stays next to counts of different widths. It inherits weapon/menu visibility from its parent.

No C++ edits or rebuild needed. C:/UnrealEngine/Games/AZ/Tools/hud_magazine_widget.py is a guarded ProgrammaticToolset author/audit helper. Before/after snapshots confirmed all other HUD widgets/properties unchanged. After dedicated Blueprint compilation and explicit HUD/texture save, a fresh tree snapshot matched the authored result exactly; the HUD's saved dependency list includes the new texture. The native BindWidget SpareMagazinesText reference remains intact. No inventory, rifle, input, reticle or effect changes.

Backup: C:/UnrealEngine/Games/AZ/Saved/Backups/HUDMagazine/20260909T165547542693/. Receipts: C:/UnrealEngine/Games/AZ/Saved/HUDMagazine/backup-receipt.json, widget-author-readback.json, widget-final-readback.json and import-final-readback.json. GIMP export receipt is beside the PNG. The PNG was visually inspected. No PIE or automated tests run; in-game appearance remains for the user's normal visual check.

Quick-select work is planning only: C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-implementation-plan.md. Recommended first slice is a separate controller-owned selector component plus CommonUI widget/entry, using the existing Fists/rifle slots and authoritative equipment requests. No selector code or mapping edits were made.
