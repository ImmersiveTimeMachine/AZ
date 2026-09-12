# Quick select V3 — implemented, built and saved; gameplay check pending

Current installed revision is V4: eight cells TOTAL including mode0, smaller uniform scale, and destination-state icon/label. Source, artwork and assets are built/saved. See C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-compact-status.md. The former V3 layout below is superseded.

September 9, 2026. User requested two smaller cells per direction, the four central arrows from the mockup, a focused item name/description panel, and an Explore/Fight mode indicator for 0. They reiterated that clicking must change the actor's equipment exactly as pressing the corresponding key.

## Implemented behavior

- Eight physical slots, all manual and empty initially. Existing slots/actions 1–4 stay in their original first positions; 5–8 form the second Left/Right/Up/Down positions. Definition 0 remains the intrinsic Fists fallback and supplies the separate mode control. No automatic pickup/equip assignment returns.
- New Entry.PositionOrdinal groups each direction's two cells. Root retains the existing first SizeBox bindings and adds four second hosts. Authored ScaleBoxes fit the existing 168x128 composite cards into 116x96 cell bounds, preserving the internal name/icon layout and hit geometry.
- Four independent 9x9 upward-triangle images, rotated per direction and tinted #8E9992, occupy the central 56x56 area. The native GIMP triangle PNG/XCF were created and the texture was imported/saved with the explicit TextureFactory (no Interchange).
- The 0 mode card moves into clear diagonal space rather than covering the arrows. Mode text comes from committed Equipment selection, not Movement.Strafe or a second UI mode flag. A relaxed rifle remains Fight mode even without strafe.
- Key 0 and clicking the mode card now request an authoritative Explore/Fight toggle, checked against equipment generation. Explore commits no equipment; Fight defaults to intrinsic Fists. Optional editor property bRestoreLastWeaponOnFight can restore a valid remembered owned weapon, with Fists fallback. The user was asked which entry policy they prefer but has not answered; the current default preserves the original Fists fight behavior. Remembering a weapon never assigns a quick slot.
- Left-click now activates an assigned item, as does right-click. The previous overlay ignored left-click, which explained the apparent click/key difference. MMB remains assignment; both activation mouse buttons and number keys call the same component/equipment request path. Existing held-mouse suppression includes LMB/RMB.
- Focused name and description are added below the cross, using real inventory CompositeWidget/Text leaves. Hover selects a bound item; assignment mode previews its candidate. Empty-slot and mode hints use separate fallback text. No fabricated medkit description is inserted.
- New C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_QuickSelectPresentation.h resolves name/description roles consistently across cards, focused composites and fallback views. Name priority: canonical StaticText, canonical Name, then the legacy Ammo.Primary.Name tag used by the current potions. Description priority: Description, then Text. M16 has a real description; the sample potions have none, so their description stays absent.
- Mode control has its own FIGHT/EXPLORE state/action presentation, without the misleading Fists-only EQUIPPED marker. Physical equipment/READY consumable markers remain separate.

## Layout contract

At 1920x1080, the central arrow position remains the mockup's (1118,538). CrossLayout is 552x664, anchored at (.5822916667,.5), alignment (.5,240/664), offset (0,-2).

| Elements | Local boxes |
|---|---|
| Left cells | (124,192,116,96), (0,192,116,96) |
| Right cells | (312,192,116,96), (436,192,116,96) |
| Up cells | (218,108,116,96), (218,4,116,96) |
| Down cells | (218,276,116,96), (218,380,116,96) |
| Center arrows | 56x56 area at (248,212) |
| Mode card | (396,360,92,96), with mode label beneath |
| Item name/description | (0,492,552,100), 34px name and 20px description |
| Status / controls | y596 / y632 |

## Validation

Source review verified shared mouse/key routing, generation-checked mode changes, delayed/queued outcomes and the two-host ordinal mapping. Fixed the mode card's old Fists-specific equipment marker and consistent legacy name/description resolution. Whitespace checks pass; Rider reported no error items. No automated tests were added or run.

The user completed the full build/restart. UBT reports Result: Succeeded, 12.92 seconds. DLL timestamps are newer than the mode/UI/helper source, the editor process started afterward, and PositionOrdinal, ModeText and the mode API are loaded. bRestoreLastWeaponOnFight is false, preserving Fists as the default Fight target. No C++ edits followed this successful build.

After the user stopped Play, the focused name/description leaves and composite were verified, compiled and explicitly saved. Root and input/controller backup hashes still matched. The root layout was updated in place to 33 authored widgets, including all eight scaled hosts, four arrows, mode text and the composite/fallback description area. The root and controller compiled successfully through dedicated Blueprint tools.

Appended four player-mappable actions and slot definitions for 5–8. Fresh comparisons confirmed that existing definitions/actions 0–4, default bindings/readiness, protected controller references and every pre-existing input mapping were preserved. Only the root, controller, Pawn IMC, four new actions and three focused-description assets were saved. Final widget and nine-definition/nine-action readbacks passed with no dirty packages.

The earlier active-PIE blocker is resolved. No selector mutation was made while the guard detected Play. New focus assets are now confirmed saved; serialized dependency checks show root -> focused composite/arrow texture, composite -> text leaves, and controller/Pawn IMC -> actions 5–8. Updated disk timestamps were verified. Root helper accepts GetWidgets' Blueprint class path with or without the generated _C suffix.

Receipts: C:/UnrealEngine/Games/AZ/Saved/QuickSelect/v3-widgets-author.json and v3-widgets-verify.json; C:/UnrealEngine/Games/AZ/Saved/QuickSelectEightSlots/assign-readback.json and verify-readback.json; C:/UnrealEngine/Games/AZ/Saved/QuickSelectV3Widgets/focus-details-readback.json and saved-dependencies.json. All new UI code is in the successful full build; no C++ edits or Live Coding patches followed it in this continuation.

The user had PIE running at the first read; it was stopped by the user before backups/import. Codex did not start or stop PIE. Existing gameplay logs show actual Equipment commits between rifle and Fists, but do not validate the new LMB/mode/eight-slot code.

## Authoring tools retained for future maintenance

1. C:/UnrealEngine/Games/AZ/Tools/quick_select_focus_details.py — creates the real inventory text leaves/composite. They now exist: use MODE='verify', not the create modes.
2. C:/UnrealEngine/Games/AZ/Tools/quick_select_v3_widgets.py — guarded root layout authoring with explicit backups; MODE='verify' checks the installed geometry. Do not recreate the current root from an older script.
3. C:/UnrealEngine/Games/AZ/Tools/quick_select_eight_slots.py — appends 5–8 while preserving 0–4 and unrelated mappings. The assignment is complete; use main('verify') on freshly resolved defaults. No live PIE binding migration.
4. The remaining step is user gameplay review. Codex has not run PIE or automated tests.

Input/default backup completed at C:/UnrealEngine/Games/AZ/Saved/Backups/QuickSelectEightSlots/20260909T205141460659/. Receipt: C:/UnrealEngine/Games/AZ/Saved/QuickSelectEightSlots/backup-receipt.json. Root widget backup receipt: C:/UnrealEngine/Games/AZ/Saved/QuickSelectV3Widgets/backup-receipt.json. Refresh baselines if parallel/user edits changed these targets; never overwrite newer work.

Arrow asset already saved: /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_ArrowUp. Native editable source/export: C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/CHALK_ArrowUp_NATIVE.xcf and T_HUD_ArrowUp.png, generated by C:/UnrealEngine/Games/AZ/Tools/quick_select_arrow_art.py. No source-pack texture was modified.

Manual acceptance after activation: eight empty cells + separate 0 control + visible arrows; MMB assignment/wheel/cancel unchanged; LMB, RMB and key select the same physical weapon; 0 alternates actual Explore/Fight state; descriptions follow hover/candidate without stale text; keys 5–8 resolve the new positions; no duplicate/phantom equipped markers or automatic filling. Consumable selection still readies an item; actual use gameplay remains separate.
