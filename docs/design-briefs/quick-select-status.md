# CHALK quick select — implemented, built and saved; user gameplay check pending

**Superseded by the user's manual-slot/composite revision:** C:/UnrealEngine/Games/AZ/docs/design-briefs/quick-select-manual-slots-status.md. Current implementation has four visible empty generic slots, permanent icon-backed Fists 0, no auto-fill, consumable readiness and real inventory composite leaves. Do not use the old two-slot authoring/verification assumptions below on the new assets.

September 9, 2026. User approved implementation of the revised interaction. Tab toggles the overlay; hover + MMB starts assignment, wheel browses owned compatible inventory items, second MMB submits assignment only. RMB activates the confirmed binding. Escape cancels the draft or closes browsing. First supported content is Fists (0) and rifle (1); sidearm/care/utility slots and shortcuts 2–4 become functional when their definitions and gameplay exist.

## Source implemented

- New UAZ_QuickSelectComponent owns Closed/Browsing/EditingAssignment, locked edit slot and candidate GUID, current binding revision, request acknowledgement, focus/lifecycle cleanup and truthful entry snapshots. Assignment preview never changes equipment. Confirmed assignments wait for replicated binding state if the receipt arrives first. Open/close does not commit a draft.
- New UAZ_QuickSelectWidget : UCommonActivatableWidget and UAZ_QuickSelectEntryWidget render editable authored UMG. Five SizeBox hosts map configured positions to entries; unused hosts collapse. Pointer hover, MMB/double-click, wheel, RMB, Back and mapped keyboard inputs are handled by the overlay. Existing actual EnhancedInput mappings supply Tab/inventory/number-key recognition and labels.
- QuickBar now exposes slot/candidate enumeration and data-driven display fields. A replicated binding struct carries item GUIDs, explicit-assignment flags and revision together, with an event/RepNotify. Explicit assignments are not silently overwritten/refilled by unrelated equipment/inventory changes; a dropped explicit item leaves an empty explicit binding. One item retains one slot binding.
- Separate revision/identity-validated assignment and idempotent activation requests. Bounded owner receipts deduplicate request IDs and report Assigned/Rejected/Deferred/Activated/Superseded. The Equipment owner supplies terminal results for queued requests on completion, failure, interruption, item removal, possession or supersession. Existing equipment/grant/weapon presentation and concurrent automatic-fire work are preserved.
- PlayerController owns QuickSelect and its editor-assigned toggle action. Inventory and selector have separate capture reasons; move/look ignore calls change only at aggregate transitions, and server capture carries both reasons. The existing IsInventoryInputCaptured compatibility guard covers interactive UI for firearm/fire-mode checks. Held mouse buttons are suppressed until release after selector close. Weapon shortcut bindings now use Started; 0 retains its explicit Fists toggle, physical shortcuts request idempotent activation.
- PlayerUI hides the aim reticle during aggregate capture without raising the inventory visibility event that collapses the entire HUD. The existing transient HUD info lane can show rejected/deferred selection messages. The magazine icon, inventory layout and existing weapon/ammo/health appearance are preserved.

Source groups: C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_QuickSelectTypes.h, AZ_QuickSelectComponent.h, AZ_QuickSelectWidget.h, AZ_QuickSelectEntryWidget.h and corresponding Private/UI cpp files; existing QuickBar, EquipmentComponent, PlayerController, PlayerUIComponent and CommonUI HUD files.

## Review and build status

Independent source review covered request deduplication, deferred/terminal results, reentrant equipment publication, GUID/binding changes, UI capture and input teardown. Fixed blank-area clicks retaining the prior hovered slot; missing Grabbing/StruckPair gates; pause/takeover closure stealing input; stale widget reuse; remapped toggle hint; and presentation availability differing from authoritative candidate validity. Explicit detached-array typing was added for the local engine's templated GameplayTagContainer API.

Whitespace checks pass. Rider code analysis returned no error items for the six relevant cpp files; that is not a substitute for a compiler result. No automated tests or PIE started.

The user completed the full build/restart. Current UnrealBuildTool log reports **Result: Succeeded**, 15.38 seconds. The DLL is newer than the selector/QuickBar/controller source, the editor process started afterward, and live reflection exposes the new types and fields. No C++ changes followed this successful build. The prior Live Coding blocker is resolved.

## Saved content and verification

C:/UnrealEngine/Games/AZ/Tools/quick_select_assets.py authored the separate entry (10 widgets) and root (13 widgets), with all native BindWidget names, independent backgrounds/outline/text, fitted icons and the v03 cross layout. Both compiled through the dedicated Blueprint tool after the programmatic calls returned. Five independently positioned hosts are authored; native presentation creates configured entries and collapses unused categories. No NativePaint or flattened HUD image.

C:/UnrealEngine/Games/AZ/Tools/quick_select_assign.py created the player-mappable Boolean Tab action, appended one mapping to the existing shared IMC, assigned PC QuickSelectToggleAction, QuickSelect.WidgetClass and root EntryWidgetClass, and set only presentation fields on the existing Fists/rifle slots. Protected comparisons passed before and after compilation/save: slot gameplay fields, inventory/HUD references, existing 0/1 actions, ChangeFireModeAction and all other shared mappings are preserved. Fists has no installed fist texture and uses its real label. Detached QuickSlot.set_editor_property rejects EditDefaultsOnly fields; the helper now imports only the four display fields into detached struct copies, verifies all retained fields, then assigns the CDO's array with no reconstruction notifications.

Read-only audit: C:/UnrealEngine/Games/AZ/Saved/QuickSelect/prebuild-audit.json. Backup: C:/UnrealEngine/Games/AZ/Saved/Backups/QuickSelect/20260909T173723623751/. Active receipt: C:/UnrealEngine/Games/AZ/Saved/QuickSelect/backup-receipt.json. On restart the existing PC/IMC disk hashes still matched that baseline, so no newer work was overwritten. Only the explicit selector packages were saved; no map, inventory, animation or weapon assets were changed.

Saved assets:

- /Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelectEntry
- /Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelect
- /Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSelect
- Existing /Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed (one appended Tab mapping)
- Existing /Game/AZ/Blueprints/Player/BP_AZ_PlayerController (new selector references/display settings)

Recompiled entry, root and controller after assignment, reacquired current defaults, saved only those five packages, and passed both assignment main('verify') and programmatic MODE='verify'. Updated disk timestamps were checked. Serialized dependencies confirm PC -> root/action, root -> entry and shared IMC -> action. Final dirty-package readback was empty. Receipts under C:/UnrealEngine/Games/AZ/Saved/QuickSelect/: assign-readback.json, post-compile-readback.json, verify-readback.json, widget-final-readback.json and saved-dependencies.json.

A genuine live entry thumbnail was inspected at C:/UnrealEngine/Games/AZ/Saved/Screenshots/WindowsEditor/RiderMCP/20260909-180400_preview_WBP_AZ_QuickSelectEntry.png. It renders the empty designer card, not the runtime inventory-driven overlay, and does not validate mouse interaction or in-game layout. No PIE or automated tests were run.

Future read-only verification: `runpy.run_path('C:/UnrealEngine/Games/AZ/Tools/quick_select_assign.py', run_name='quick_select_helper')['main']('verify')`, plus programmatic MODE='verify'. Do not rerun author_entry/root on the existing trees or overwrite later controller/input changes with an old backup.

## Manual gameplay check remaining

Tab opens/closes. Hover rifle and MMB: candidate list contains only compatible owned items. Scroll between two real rifle instances if carried, MMB assign: binding changes, equipped rifle does not. RMB selects assigned rifle and closes; selecting it again keeps it equipped. Escape edit preserves the previous binding. Blank-space clicks do nothing. Drop removes the assigned item without silently substituting another. 1 activates rifle, 0 retains Fists. Inventory takeover, repeated open/close, held aim/fire, alt-tab, pause, zero health/grab and pawn changes leave input usable without unintended activation/shot. Inspect queued/refused requests and owner replication after user-run multiplayer checks. UI visual scale and gameplay behavior are unverified until that manual check.
