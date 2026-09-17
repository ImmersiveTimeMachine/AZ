# CHALK UI design workspace

Throwable visuals: user selected **03 — Quiet Sage** from the three native GIMP studies in `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/`. Selected master `CHALK_Throw_03_NATIVE.xcf` has muted sage arc pulses/open target corners and warm-white HUD. Full implementation plan and Claude execution brief are under `C:/UnrealEngine/Games/AZ/docs/design-briefs/`; this task created mockups and planning documents only, not throwable gameplay.

Latest selection: **Fight / Explore current-mode indicator 01 — Equipment row**, from `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Mode_Indicators_v01/`. Open `CHALK_Mode_Comparison_NATIVE.xcf` to compare all three proposals. Individual native editable GIMP documents include both states, enlarged/game-size reviews and full-frame placement. Selected runtime implementation status: `C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-mode-indicator-status.md`. Existing HUD/selector artwork reused.

Protagonist face (September12): **03 — After a Hard Night is applied and saved**, with **02 — He Stayed** available separately. Native GIMP concepts, layered UV work, actual Unreal previews and preservation receipts are in C:/UnrealEngine/Games/AZ/UI Design/CHALK_Protagonist_Face_v01/. Both material families live outside the generated MetaHuman folders under /Game/AZ/Blueprints/Character/Appearance/CHALKTeacher. All original face textures/materials remain intact.

Magazine follow-up: the main HUD icon/count is restored; dropped-magazine prompts use current rounds and magazines occupy one inventory cell. Status: C:/UnrealEngine/Games/AZ/docs/design-briefs/magazine-display-fixes-status.md. Quick-select layout and assignment-border feedback are unchanged.

Current core HUD master: **C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/CHALK_HUD_v03_NATIVE.xcf**.

Current quick-select layout: **V5 is approved, implemented, compiled and saved**, with an additional 3 px lift for the bottom card text. Eight 104 × 72 cells, equal 12 px inner gaps, center arrows with equal 24 px clearance, and placement to the right of the character. Existing inventory composites and gameplay bindings are preserved. Status: C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-v5-layout-status.md. Native editable reference: **C:/UnrealEngine/Games/AZ/UI Design/CHALK_QuickSelect_v05/CHALK_QuickSelect_v05.xcf**. V4 and earlier layouts below are historical and superseded.

Current in-game selector is V4: eight cells total0–7, mode0 in the left pair, no separate mode card, whole layout scaled to70%, and an inverse action label/icon (Explore/walking while fighting, Fight/fist while exploring). Applied and saved. See C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-compact-status.md; previous larger layouts below are superseded.

Current game selector is V3: eight compact manual cells (two per direction), center arrows, focused item name/description from inventory composites, and a separate 0 Explore/Fight control. Built and saved; user gameplay check pending. Details: C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-eight-slots-status.md. This supersedes earlier two/four-slot descriptions below.

Quick select is now built and saved for the real Fists/rifle entries: Tab toggle, MMB + wheel + MMB assignment, RMB activation. The old GIMP "Release to equip" hint is superseded by these controls. Current status/receipts/manual check: C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-status.md. Sidearm, care and utility remain future supported content.

September 9: the approved magazine icon is now exported as a transparent PNG and a standalone native vector XCF under C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/, imported and saved in the game HUD beside its live magazine count. Details: C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-magazine-icon-status.md. Quick-select planning is complete, implementation pending review: C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-implementation-plan.md. The user's current order is weapon quick select next, ahead of compass work.

The rifle aiming reticle is now implemented with a configurable per-weapon definition and ProHUD's compact four-tick artwork. Status and extension recipe: C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-reticle-status.md. Gameplay check is pending; inventory appearance remains preserved.

**Inventory appearance must be preserved.** On 2026-09-08 the user said the HUD is okay but objected to the inventory looking different. The skills/vitals/currency display and original inventory health tint were restored, while live health binding was retained. Future HUD/data work must not hide or rearrange existing inventory panels merely because they contain placeholder data. Discuss those gameplay bindings separately.

Phase 1 implemented and activated on 2026-09-08: the core health/weapon HUD and shared inventory health are built and saved; user gameplay acceptance is pending. Status: C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-phase1-status.md. Compass and quick select remain later phases in C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-implementation-plan.md.

The user selected HUD direction A: quiet, clear survival HUD, compatible with CHALK's modern civilian horror setting and future RPG/consumable/crafting systems. No permanent I inventory hint. Quick access is a temporary overlay. **The user approved the v03 direction including its top compass on 2026-09-07 and deferred implementation to the next session.** Start that session by reviewing existing assets and developing the implementation plan. Full handoff: C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-design-next-session.md.

## User authoring requirement

All text and shapes that can be drawn with GIMP must use native GIMP tools and separate, named, independently toggleable layers. Use native text layers for text and GIMP vector layers with editable paths for shapes whenever possible. Keep soft painted/gradient effects on their own GIMP paint layers. Generated artwork remains an image layer. Do not flatten HUD elements into the generated scene or use a flattened comparison board as the editable source.

Native paths and fill/stroke properties are preserved in the v03 XCF. The rifle silhouette is traced from the actual temporary inventory icon using GIMP Selection to Path, not generated again. The generated background remains one image layer.

Previous XCFs and the original C:/UnrealEngine/Games/AZ/UI Design/HUD.xcf are preserved. v01/v02 overview boards contain flattened preview panels; they are review boards rather than the current editable master.

The visual direction is approved; implementation planning and implementation resume next session. No automated tests unless explicitly requested; no PIE/editor tests without asking the user, per C:/UnrealEngine/Games/AZ/AGENTS.md.
