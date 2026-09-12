# Quick Select V5 — implemented, compiled and saved

Assignment feedback fix: removed the in-card ASSIGN candidate counter, which overflowed into the ammo text. The editing card now pulses only its existing border opacity once per second. Confirmation or cancellation immediately restores a steady border; names, icons and counts remain steady. Empty-candidate guidance is shown in the focused description below the selector. The user's post-crash full build succeeded in 35.26 seconds and explicitly compiled both changed selector sources. DLL timestamp is newer than the edited header/sources, and the restarted editor is newer than that DLL. Saved card registry tags are WillTick / Native / Auto for both physical and mode widgets. No asset authoring or further rebuild is needed. Runtime visual acceptance remains user-owned. Evidence: C:/UnrealEngine/Games/AZ/Saved/QuickSelectAssignmentPulse/.

The user approved the GIMP V5 layout, including equal inner-corner spacing, and requested implementation with text inside each card raised by 2–3 px.

## Applied changes

`C:/UnrealEngine/Games/AZ/Tools/quick_select_v5_widgets.py` updates eight existing presentation Widget Blueprints. It retains the widget trees, native bindings and real inventory composites. The layout pass required no C++ changes; the subsequent assignment-border fix above is included in the verified full build.

- Eight 104 × 72 cells, two per direction, with 12 px gaps around the projected inner edges.
- At the 1920 × 1080 design reference, the center is (1368, 468) and the card cross spans 568 × 408. The root uses normalized viewport anchors and the existing project DPI rules.
- Remove the old 0.70 render scale; size cards, icons and typography directly.
- Keep the four center arrows, each 24 px from its nearest card.
- Keep slot 0 within the eight cells and retain its existing destination-action behavior.
- Move ammo text from draft y47 to y44 and mode-label text from y50 to y47. Bottom text boxes leave 5–7 px of interior clearance. Keep live assignment/equipment state feedback in its own compact bottom-left area.
- Retain actual item names/icons through the existing inventory composite and name/image leaves; update focused name and description dimensions and fonts to the mockup.
- Leave gameplay activation, manual bindings, input mappings, item manifests, inventory widgets and core HUD unchanged.

## Current state

The user stopped Play and continued. Applied the approved geometry to the eight existing widget packages, compiled every widget successfully with the dedicated UMG compile tool in dependency order, and saved only those eight assets. Fresh post-compile readback passed for all planned dimensions, anchors, fonts and padding. All eight on-disk package hashes/timestamps changed; all 74 protected inventory/controller/native UI files matched their baseline.

Native EditorLoadingAndSavingUtils dirty-package readback confirmed no QuickSelect package was dirty. Other work has dirty weapon/input/animation packages; these were not saved or modified by this task. AssetTools.is_dirty incorrectly reports true when given an object path instead of a package path, so that earlier check was replaced with the native package-list result.

User resumed PIE after the save. No PIE session or editor test was started/stopped by this task. Runtime visual/input acceptance remains user-owned; no C++ rebuild is needed.

Evidence and backup receipt: `C:/UnrealEngine/Games/AZ/Saved/QuickSelectV5/`. The backup includes SHA-256 receipts for the eight packages plus 74 protected inventory/controller/native UI files.

## Maintenance

Implementation is complete. Use MODE='verify' for a read-only layout inspection; do not rerun V2–V4 authoring scripts over V5. The script's initial verification compared intermediate wrapping settings against the final settings; the verification plan now combines changes per object, and the resulting final-state readback passed before and after compilation. No additional asset edits were needed for that correction.

Receipts: `C:/UnrealEngine/Games/AZ/Saved/QuickSelectV5/v5-author-before-first.json`, `v5-verify.json`, `saved-file-receipt.json` and `dirty-readback.json`. The existing native GIMP V5 is the approved visual reference; in-game bottom text has the additional requested 3 px lift.

Do not start PIE or run editor tests. User owns gameplay visual/input checks. No automated tests were added.
