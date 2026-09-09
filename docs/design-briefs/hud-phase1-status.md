# CHALK HUD Phase 1 — implemented, gameplay acceptance pending

User authorized Phase 1 on 2026-09-08. Current scope is shared vitals/weapon data, approved core A HUD and matching inventory health, while preserving inventory appearance following user review. Compass, quick select, real consumables and later RPG systems are not part of this phase.

## Latest user review — inventory appearance restored

The user reported **HUD is okay** and objected to the inventory looking different. The automatic placeholder-hiding decision was reversed. Existing skills and infection/mortality displays are enabled again through saved Blueprint defaults and main-menu templates; currency amount/name/icon are restored from their exact pre-change receipt; the inventory health tint is restored to its authored color. Live health binding remains, and the new HUD is unchanged.

Four inventory Blueprints were compiled and explicitly saved. Readback confirms both class defaults and main-menu template flags are true, panel visibility is SelfHitTestInvisible, BACKPACK is preserved, currency text is restored and BP_AZ_PlayerController still selects WBP_AZ_GameHUD. Only inventory assets were saved; unrelated dirty MetaHuman assets were left alone. No C++ changes/build/restart or PIE were needed. Appearance restoration backups: C:/UnrealEngine/Games/AZ/Saved/Backups/HUDInventoryAppearance/20260908-202639/. Property receipts: C:/UnrealEngine/Games/AZ/Saved/HUDPhase1/inventory-appearance-flags.json and inventory-currency-restored.json.

The restored skill/illness/currency content retains its existing placeholder values; those gameplay systems are still separate later work. Future authoring preserves the inventory layout: the automatic cleanup call and cleanup assertions were removed from the authoring flow, and retired cleanup modes now stop with an explanation. Earlier sections below record the initial Phase 1 implementation, before this correction.

## Implemented native code

- New UAZ_PlayerUIComponent + FAZ_PlayerVitalsView/FAZ_PlayerWeaponView under C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI and Private/UI. Local-only, no tick or replicated duplicate state. Binds PS combat Vitals, physical-magazine inventory/equipment data, confirmed hit, inventory-full and menu visibility.
- Existing UAZ_Inv_CommonUI_InventoryHudWidget promoted to the real CommonUI HUD base with separate bound UMG fields, direct text/icon/visibility updates and an ApplyHealthPresentation(double Percent, FLinearColor FillColor, bool bImmediate) BP bridge for HQUI.
- Controller owns PlayerUI and refreshes on BeginPlay/PlayerState replication/possession. HUDWidget and InventoryHudWidgetClass now use the CommonUI HUD type. The native AZ_Item include was updated for that type. Existing uncommitted firearm input changes were preserved.
- Existing inventory vitals subscribe to the same component and call the same scalar health bridge. Unsupported illness widgets and skills panel are reversibly hidden; portrait/EKG and current inventory APIs remain.
- Native HUD handles Loaded/Empty/NoMagazine/Unavailable, clears old hit feedback on item changes, and uses delegate ordering safely for pickup prompts when closing inventory. Both inventory-open and HUD-return transitions snap the latest health before reveal; collapsed renderers cannot show stale interpolation values.

## Build and asset verification

The user completed the full build and restarted the editor. C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt reports **Result: Succeeded** after compiling the new UI component and HUD code and linking UnrealEditor-AZ.dll. DLL timestamp is newer than the modified native UI sources. The restarted editor exposes the new component and scalar health events. No additional C++ changes were made after that successful build.

Dedicated Blueprint compilation passed for the new HUD, existing inventory vitals/switcher and the updated controller. Explicit asset saves succeeded. All 15 required native widget names are present in the 26-widget HUD tree; both health graphs retain both immediate/interpolated execution paths and their data connections. Final controller readback points to the new HUD class, its PlayerUI component is present, unsupported-vitals/skills flags are false, and no dirty content packages remain.

Native code received independent lifecycle/ownership reviews. Pickup close-order and hidden-bar interpolation refresh issues were fixed before the user's successful build. Designer layout was visually inspected; its sample values are strictly guarded by IsDesignTime, with the runtime branch unconnected and runtime template text empty/containers collapsed until real data arrives. No PIE or tests were started by Codex.

## Saved assets and tooling

- C:/UnrealEngine/Games/AZ/Tools/hud_phase1_assets.py is the authoring script for editor ProgrammaticToolset.execute_tool_script (not ordinary Unreal Python). It contains HUD-tree, health-bridge, Designer-only preview, contrast, currency-cleanup and readback modes. Tool schemas and the programmatic execution environment were read before use. Both classes expose a HealthProgressBar getter, so the script resolves the getter by its actual native parent class to avoid ambiguity.
- C:/UnrealEngine/Games/AZ/Tools/hud_phase1_export_art.py renders the approved native GIMP heart/full-health vector definitions at import resolution to C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/. Heart 128×128 and mask 1024×48 were exported and visually inspected. Generated scene stays out of the game UI. Import these as /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Heart and T_HUD_HealthMask before authoring the HUD.
- Created and saved /Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD, based on UAZ_Inv_CommonUI_InventoryHudWidget. Weapon and health backdrops are separate children so the weapon backdrop disappears when unarmed. The HUD uses the approved heart/mask art, Oswald/Roboto, native UMG text/images and HQUI linear health renderer.
- Imported/saved /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Heart and T_HUD_HealthMask with UI texture settings, no mipmaps. No generated scene/background was imported into the game HUD.
- Updated the existing AZ_WBPCharacterVitalsPanel health interface graph. HQUI uses Static for the first value and Interpolated for later changes (0.12 seconds), both fed from the same PS Vitals snapshot as the HUD.
- Updated AZ_WBP_GameInventorySwitcher to clear/hide only the sample currency amount/name/icon; BACKPACK header, parent and slot were verified unchanged. Native visibility hides unsupported illness and skills content without deleting its widget structure.
- BP_AZ_PlayerController.InventoryHudWidgetClass is now /Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD_C. Old native HUD code and the old HUD Blueprint remain preserved but are no longer the configured active view.

## Receipts and preserved files

- Pre-edit asset backups and hashes: C:/UnrealEngine/Games/AZ/Saved/Backups/HUDPhase1/20260908-193800/receipt.json.
- C:/UnrealEngine/Games/AZ/Saved/HUDPhase1/asset-readback.json — widget tree, both health graphs, runtime defaults, Designer guard and currency cleanup.
- C:/UnrealEngine/Games/AZ/Saved/HUDPhase1/controller-readback.json — active HUD/component and unsupported-feature flags.
- C:/UnrealEngine/Games/AZ/Saved/HUDPhase1/currency-cleanup.json — reversible before/after widget properties and unchanged header layout.
- Original GIMP files, source packs and unrelated firearm/retargeting work were preserved. No commit was created.

## User manual acceptance remaining

1. Start PIE in L_001. Check the health bar, then take damage and open I: HUD and inventory health must agree. Closing inventory should immediately show the latest health.
2. Pick up/equip the rifle using E/1. Check the actual icon/name, inserted rounds/capacity and spare-magazine count. Fire and swap to fists with 0; count/hit feedback should update and the weapon cluster should hide when unarmed.
3. Repeatedly open/close I near a pickup. Prompt should return correctly, movement/aim/fire should retain the existing input behavior, and no permanent I hint should appear.
4. Check inventory-full feedback, empty/no-mag states when available, and critical health. The original inventory panels/currency display should now be restored; only health is newly connected to gameplay data.

Read C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log after the user performs these checks. Gameplay behavior and multiplayer acceptance remain unverified until that pass; static readback is not a runtime test.

Known vendor limitation: HQUI's unguarded retainer-material lookup can log Accessed None when a transient Designer/thumbnail widget is torn down before its latent initialization callback. This occurred during asset previews, with PIE off. The deferred percentage update rereads the current value and does not restore sample health. If the warning occurs during normal attached gameplay, investigate/guard an AZ-owned renderer copy rather than dismissing it as a preview warning. Source pack assets were not modified.
