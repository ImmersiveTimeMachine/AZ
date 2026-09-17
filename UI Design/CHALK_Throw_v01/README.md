# CHALK throwable preview mockups

**Selected: 03 — Quiet Sage.** Artur chose the muted sage pulses and open-corner contact marker. These are visual proposals and native editable GIMP sources; this task did not implement throwable gameplay.

| Option | Preview | Native editable master |
|---|---|---|
| 01 — Chalk & Ember | `CHALK_Throw_01.png` | `CHALK_Throw_01_NATIVE.xcf` |
| 02 — Amber Thread | `CHALK_Throw_02.png` | `CHALK_Throw_02_NATIVE.xcf` |
| **03 — Quiet Sage** | `CHALK_Throw_03.png` | `CHALK_Throw_03_NATIVE.xcf` |

All paths above are relative to `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/`. `CHALK_Throw_Comparison_NATIVE.xcf` is an editable comparison board with enlarged marker detail. The three full-resolution previews are1920 ×1080. All four XCFs were reopened and their native text/vector paths verified, then opened in a dedicated GIMP window; the receipt is `gimp-visible-review.json`.

## Selected visual language

- Arc and marker corners: muted sage `#B5C8B7`. Center/text/health: approved warm white `#EEEAE0`.
- Fine connecting arc with spaced visible pulses; four open corners with a small white elliptical center marker. Restrained brightness, no heavy bloom or solid painted disk.
- Marker means predicted **first contact**, not grenade blast radius or final rest after bouncing. Its orientation follows the actual surface in implementation; this image shows flat pavement.
- HUD shows the selected item, real count and contextual control hints. The example stone/count3/range9.4m/health72% are illustrative, not new gameplay constants.
- Final user-confirmed controls, September16: **hold RMB to aim, release RMB to throw, click LMB to cancel**. The existing comparison-board caption predates this change and is a historical visual reference, not the current input specification. Cancellation disarms the later RMB release. Runtime hints should say “Release RMB: Throw / LMB: Cancel” while aiming.

## Editable layers and provenance

The UI is native GIMP vector/text: trajectory segments, ground marker, stone icon, health and labels. Each document has `20 / AIMING` visible and `80 / BLOCKED example` hidden; to inspect the blocked alternative, hide the aim group and show the blocked group. Review-note layers are hidden in the clean previews. The blocked example is illustrative styling, not a sampled collision result from this background.

The backdrop was made with the built-in image-generation tool by editing the previously approved Montréal alley plate into a generic right-handed stone preparation pose. Only this backdrop is raster; its prompt and copied source are in `sources/image-generation-prompt.txt` and `sources/montreal_throw_plate.png`. It is not a verified MH animation pose and is not intended for game import. Audited rifle/pistol grenade clips use the left hand; unarmed source clips use the right.

Authoring: `sources/build_throw_mockups.py`, reusing the project's existing native GIMP helpers/fonts. Each master has retained paths and native text, confirmed in `saved-file-receipt.json` and the `_layers.json` readbacks. No previous master was overwritten.

Implementation plan: `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-system-implementation-plan.md`.

Claude execution brief: `C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-system-execution.md`.
