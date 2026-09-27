# Separate main-menu level — implementation checkpoint

User authorized implementation on September 22, 2026; this supersedes the earlier design-only restriction. Existing Field Notes main-menu visuals remain; background art comes later.

## Built, editor-verified and saved; user runtime verification pending

After the user reopened Unreal, readback confirmed both native classes, frontend flag, existing Field Notes widget class, null pawn/spectator/HUD, independent map defaults, GameDefaultMap and prefix mapping. Opened L_MainMenu, explicitly assigned AZ_MainMenuGameMode in World Settings, saved successfully; no dirty packages. Editor is left on L_MainMenu for user-run Play. Root did not start Play. Earlier asset byte count/build-receipt asset hash predates this World Settings assignment.

- Created and saved `/Game/AZ/Maps/L_MainMenu` (8,403-byte empty world). Reopened L_001 afterwards. Editor readback: no dirty content/maps, PIE Idle. Existing ignore policy excludes maps from Git; this map is a local asset, not a tracked file. Never delete ignored/untracked assets.
- GameDefaultMap now points to L_MainMenu; EditorStartupMap remains L_001 for development. GameModeMapPrefixes resolves L_MainMenu to the new native AZ_MainMenuGameMode. Explicit cook entries include both maps.
- GameInstance config separates MainMenuMap and FirstCampaignMap (L_001). It retains exact selected save snapshot, travel intent, and save-protection state across world destruction.
- Native MainMenuGameMode/MainMenuPlayerController reuse the existing Field Notes menu and pause input assets; no gameplay pawn, spectator or HUD is spawned. Base controller and UI components skip gameplay initialization for the frontend role.
- New Game loads first episode; previous checkpoint files remain until a successful new save. Main Menu performs actual level travel, with the existing unsaved-progress confirmation.
- Continue resolves newest valid save header, travels to its level, waits up to 30 seconds for grounded player/components, and restores exactly that snapshot. Existing same-world load path remains. Destination restore failures retain capture/save protection; Back returns to frontend. Missing maps produce an error.
- Coordinator blocks writes/autosaves while restore is pending. Existing transactional restore/rollback semantics remain.
- Shared gameplay mapping context is removed on departing controller, so LocalPlayer does not retain it in frontend.

## Build boundary

CLI AZEditor build attempted and stopped with `Unable to build while Live Coding is active`. This is not a successful compilation or a code-error diagnosis. New UCLASS/UPROPERTY additions require editor closed and ordinary CLI build. Do not use Live Coding to claim reflected types are ready.

User closed Unreal. Full build compiled the new menu/coordinator/controller code, but failed on an unrelated camera variable that another worker was editing concurrently. That worker corrected it; root did not edit the camera file. The subsequent CLI build returned `Target is up to date` and `Result: Succeeded`. Receipt and copied UBT log: `Saved/MainMenuIntegration/build-receipt.json` and `UBT-succeeded.log`. DLL timestamp was checked against changed source/header timestamps.

Next: user reopens Unreal; verify native classes/config/menu world, then user performs PIE/Standalone validation. Do not launch editor yourself. No tests/PIE were started and no automated tests added.

## Acceptance still pending

1. Start on L_MainMenu: current menu appears without hero/gameplay HUD.
2. New Game opens fresh L_001. Main Menu confirmation unloads it and returns to L_MainMenu.
3. Save at checkpoint, return to menu, Continue restores saved position, quests, inventory/equipment, and waypoint.
4. Missing/incompatible save, failed travel, and restore timeout remain explicit failures without overwriting saves or allowing accidental fresh-world play.
5. Keyboard/mouse and controller focus work across transitions. Settings survive travel. Test in packaged startup when a packaged build is available.

Unrelated pre-existing changes: three hero Blueprint assets, PawnMoverHeroCharacter_Camera.cpp and QuickSelectEntryWidget.cpp; preserve them. No git history changes, deletions, commits or pushes performed.
