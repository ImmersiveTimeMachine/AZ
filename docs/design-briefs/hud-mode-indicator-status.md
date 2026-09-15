# Fight / Explore HUD indicator

Artur selected GIMP option **01 — Equipment row** on September 15, 2026 UTC. The selected visual is implemented in source and authored, compiled and saved in `WBP_AZ_GameHUD`. The coordinated C++ build passed and Live Coding applied it in editor PID37864. Runtime visual acceptance is the user's next step.

## Resulting behavior

- Fresh empty loadouts start explicitly in **Explore**, so the walking icon/name appear on the initial HUD snapshot. Existing/restored selections are preserved on later binding refreshes.
- Unarmed Fight shows the existing fist symbol and **FIGHT** above health.
- Explore shows the existing walking symbol and **EXPLORE** in the same position.
- A supported rifle/pistol uses the existing weapon, ammo and magazine display. Its fields and geometry are unchanged.
- The new indicator reads `FAZ_PlayerWeaponView.Profile`, already published when committed equipment changes. `Weapon.None` means Explore; another supported weapon-family profile means Fight. Unknown/unavailable profiles hide the mode group until state arrives.
- This is the current mode, distinct from quick-select slot 0's destination action. No new input, gameplay tags, polling, timers or independent mode state were introduced.
- Health, critical-health feedback, reticle, quick select, inventory appearance and equipment behavior are preserved.

## Implementation

- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp`: first-time authority initialization publishes `Weapon.None` instead of leaving the empty selection's profile invalid. The generation-zero/empty-selection guard prevents resetting existing equipment. Late ASC binding synchronizes this explicit empty profile. This follow-up changes no assets or input bindings and applies on the next fresh Play session.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.cpp`: body-only presentation helper and calls from the existing initial snapshot/equipment callback. Uses optional named widgets; no new reflected fields or native class layout changes.
- `/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD`: adds `ModeContainer`, `FightModeIcon`, `ExploreModeIcon`, `ModeNameText`. Reuses `/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Fists` and `T_HUD_Explore`; no texture reimport.
- `C:/UnrealEngine/Games/AZ/Tools/hud_mode_indicator_setup.py`: audit, backup, author and verify helpers, using registered native UMG/Object tools. Dedicated UMG compile and explicit package save happen separately after authoring returns.

At 1920 × 1080, the existing CoreHUD begins at (1455,820). The new mode group is local (51,80), giving the approved (1506,900) icon frame. Icons are 70 × 70. The text offset is (86,20), with the existing Oswald font at 20.25 Slate points, matching the mockup's 27 px convention. Existing health remains local (59,161). Warm-white tint matches the approved health/icon palette.

## Verification

Startup follow-up: UBT compiled `AZ_Inv_CommonUI_EquipmentComponent.cpp` and returned `Result: Succeeded` in 13.79 s. Live Coding applied the AZ.dll patch at 2026-09-15 03:57:26.716 UTC in the same editor. This body-only initialization fix was loaded without stopping the user's Play session; it applies to the next fresh session. No agent PIE or tests were run. The user's preceding “good” accepted the HUD appearance; startup Explore is the new manual check.

Three native GIMP mockups and their comparison were saved and reopened with native text/vector paths; all four documents are open in a dedicated GIMP window. Master folder: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Mode_Indicators_v01/`.

The HUD authoring readback confirms exactly four new widgets and all 30 pre-existing concrete widgets preserved, including captured presentation and slot properties. Dedicated `CompileWidgetBlueprint` returned true. Explicit HUD package save returned true, disk hash changed from the backup, and the editor's dirty-content list was empty afterward. A post-build readback again preserved all30 widgets and confirmed both mode textures as package dependencies. Receipts and the pre-change package backup are in `C:/UnrealEngine/Games/AZ/Saved/HUDMode/`.

The coordinated build initially passed in13.57s, then passed again in15.09s after the grab pair's final1x correction. The final compile list includes `AZ_Inv_CommonUI_InventoryHudWidget.cpp`; Live Coding confirms the AZ.dll patch succeeded at2026-09-15 03:39:32.052 UTC. The source changes are live in the current editor. Before a later editor restart, include them in the normal full build, per the project Live Coding workflow.

No agent-started PIE, runtime simulation or automated tests. Artur stopped their running Play session before authoring. Manual acceptance: press 0 in each direction; verify the current icon/name; equip rifle/pistol and check the existing ammo row; reopen inventory and confirm the HUD follows its existing visibility behavior.
