# Quick Select v05 — layout proposal

Native mockup created on the existing 1920 × 1080 Montréal scene. The user reviewed and approved this layout; it has now been implemented in the existing Unreal selector widgets with an additional 3 px lift for the bottom card text. See C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-v5-layout-status.md. The mockup authoring pass itself changed no Unreal assets.

Open `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuickSelect_v05/CHALK_QuickSelect_v05.xcf` in GIMP 3.2 or later.

## Layout

- Eight cells total: mode action 0 and seven manually assigned item slots 1–7; two cells per direction.
- Each cell is 104 × 72 px. The cross spans 568 × 408 px, centered at (1368, 468), to the right of the character and above the weapon HUD.
- Minimum rectangle clearance is 12 px, including the inner corners. All 28 card pairs were checked; none intersects.
- Following the user's review, the left/right pairs sit outside the projected edges of the vertical cards with the same 12 px gap at every inner corner. The central opening is 128 × 96 px. Four arrows span 80 × 48 px, leaving 24 px clearance to the nearest card in every direction.
- Selected item name and description remain below the cross. Empty cells keep their shortcut and a small dash instead of repeating EMPTY.
- Slot 0 displays the destination action. These previews assume the player is currently fighting, so 0 offers EXPLORE with the walking icon. During exploration it must offer FIGHT with the existing fist icon.
- Ammo and equipped rifle are illustrative scene context. An equipped inventory weapon does not imply an automatic quick-slot assignment.

## Editable states

The XCF opens with `30 / M16 SELECTED - toggle this state` visible. Hide that group and show `20 / EMPTY BINDINGS - toggle this state` to review seven empty item bindings plus the permanent mode action. Keep only one state visible at a time. `90 / LAYOUT GUIDES` is hidden by default.

Text, card fills, borders, arrows and walking symbol use native GIMP text/vector layers. The M16 silhouette is traced from the existing inventory icon. The original generated scene stays a single image layer; existing soft HUD effects remain separate paint layers. Saved-file reopening verified 39 text layers, 174 vector layers with 174 editable paths, 58 groups, 2 paint layers and 1 scene image.

Previews:

- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuickSelect_v05/01_Empty_Bindings.png`
- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuickSelect_v05/02_M16_Selected.png`
- `C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuickSelect_v05/03_Layout_Guides.png`

Native GIMP authoring script, local font configuration and build logs are retained under `sources`. Geometry and saved-layer receipts are adjacent to this file.

Placement was compared visually with [The Last of Us Part I](https://cdn.mos.cms.futurecdn.net/JgqFExixufqS9WanU4WP6L.jpg) and [Resident Evil 4 Remake](https://wanuxi-storage.sgp1.cdn.digitaloceanspaces.com/2023/03/Resident-Evil-4-Preview-21.png). The proposed measurements are specific to this project.

The approved geometry is now applied to the existing composite widgets as runtime V5. User gameplay visual/input acceptance remains pending.
