# Gamepad support — implementation receipt

Executed 2026-09-21 against [gamepad-claude-execution-handoff.md](gamepad-claude-execution-handoff.md).
Everything below was read back from the live editor after saving; nothing here is predicted.

## Baseline and write ownership (Module 0)

The last successful native build is `Binaries/Win64/UnrealEditor-AZ.dll` @ 2026-09-21 12:38
(`UnrealBuildTool/Log.txt` → `Result: Succeeded`). Only two source files were newer than it at the
start of this work, both belonging to the root inventory task:

| File | Modified | Disposition |
| --- | --- | --- |
| `Source/AZ/Public/InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h` | 15:26 | **Not edited.** Carried into the build unchanged. |
| `Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.cpp` | 15:33 | **Not edited.** Carried into the build unchanged. |

No other agent session was running during this work. Root's carry/tab changes are part of the baseline
this build compiles; they were neither reverted nor reauthored.

## Changed files and assets

| Item | Change |
| --- | --- |
| `Content/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs.uasset` | +14 gamepad rows (38 → 52). Every keyboard/mouse row, action identity, trigger and modifier preserved. |
| `Content/AZ/Blueprints/Input/InventoryMenu/IMC_AZ_InventoryMenu.uasset` | +1 row: `IA_AZ_UI_InventoryContextMenu` → `Gamepad_RightShoulder` (17 → 18). |
| `Source/AZ/Public/UI/AZ_InputPresentationSubsystem.h` | `AnnounceGamepadArrival`, `AnnounceGamepadDeparture`, `HasOwnedConnectedGamepad`, `AnnouncedGamepadDevice`. |
| `Source/AZ/Private/UI/AZ_InputPresentationSubsystem.cpp` | One-time input-method transition on owned-gamepad arrival; fallback on departure/repairing. |

Backups of both touched assets are in the session scratchpad
(`AZ_IMC_RT_PawnInputs.uasset.bak`, `IMC_AZ_InventoryMenu.uasset.bak`).

## Final physical layout (Module 1)

All rows added to `AZ_IMC_RT_PawnInputs`, priority 2, unchanged registration lifecycle.
Boolean mappings carry no extra triggers, so Enhanced Input's default `Down` trigger at actuation 0.5
applies.

**Where the trigger threshold actually comes from.** `Plugins/Runtime/Windows/XInputDevice/.../XInputInterface.cpp`
registers `Buttons[10] = FGamepadKeyNames::LeftTriggerThreshold` and `Buttons[11] = RightTriggerThreshold`
— i.e. `Gamepad_LeftTrigger` / `Gamepad_RightTrigger` arrive as ordinary **buttons** with press/release,
gated by XInput's own `XINPUT_GAMEPAD_TRIGGER_THRESHOLD`. The analog travel is published separately as
`LeftTriggerAnalog` / `RightTriggerAnalog` (`Gamepad_*TriggerAxis`), which these mappings do not use. So the
value reaching the Boolean action is 0 or 1, the 0.5 actuation threshold is never the deciding factor, and
the pull required is XInput's threshold (~12% of travel), not half the trigger. This also confirms the
held-button barrier works on them: they have real `FKeyState::bDown`.

| Operation | Key | Action | Mapping modifiers |
| --- | --- | --- | --- |
| Move | `Gamepad_Left2D` | `AZ_IA_RT_Move` (Axis2D) | DeadZone radial 0.15 / 0.98 |
| Look | `Gamepad_Right2D` | `AZ_IA_RT_Look` (Axis2D) | DeadZone radial 0.15 / 0.98 → Negate Y → Scalar (180, 120, 1) → ScaleByDeltaTime |
| Jump | `Gamepad_FaceButton_Bottom` | `AZ_IA_RT_Jump` | — |
| Crouch | `Gamepad_FaceButton_Right` | `AZ_IA_RT_Crouch` | — |
| Interact | `Gamepad_FaceButton_Left` | `AZ_IA_RT_Interact` | — |
| Reload | `Gamepad_FaceButton_Top` | `AZ_IA_RT_Reload` | — |
| Run | `Gamepad_LeftShoulder` | `AZ_IA_RT_Run` | — |
| Sprint | `Gamepad_RightShoulder` | `AZ_IA_RT_Sprint` | — |
| Primary attack / throw | `Gamepad_RightTrigger` | `AZ_IA_RT_PrimaryAttack` | — |
| Aim / cancel throw | `Gamepad_LeftTrigger` | `AZ_IA_RT_Aim` **and** `AZ_IA_RT_SecondaryAttack` | — |
| Heavy strike | `Gamepad_RightThumbstick` | `AZ_IA_RT_HeavyStrike` → `Input.Action.MeleeAttack` | — |
| Change fire mode | `Gamepad_LeftThumbstick` | `AZ_IA_RT_ChangeFireMode` | — |
| Weapon cross (Quick Select) | `Gamepad_DPad_Up/Down/Left/Right` | `AZ_IA_QuickSelect` (`AZ_IMC_AlwaysAllowed`) | — |

### Second pass — moved toward the The Last of Us Part II default (user call)

Verified TLOU II defaults from two agreeing sources (SpecialEffect GameAccess and Game8); a gamepressure
page and a GameWith page were discarded, the latter self-identifying as pre-release speculation.

| Change | Why |
| --- | --- |
| Quick Select: Face Top → **all four D-pad directions** | TLOU II's D-pad opens the Weapon Cross. Our Quick Select is the same idea. |
| `AZ_IA_RT_Weapon_0` → **row dropped from the pad** | Explore/Fight is Quick Select slot 0, so the D-pad reaches it through the radial exactly as TLOU reaches a holster. The `Zero` keyboard row is untouched. |
| Reload: L3 → **Face Top** | Face Top was freed by Quick Select leaving. Gets reload off a stick click, which was the weakest point of the previous pass. |
| Change fire mode: D-pad Right → **L3** | Displaced by the weapon cross. Infrequent and deliberate, and an accidental stick click here is harmless — unlike an accidental reload. |
| Sprint → LB, Run → RB | TLOU II puts sprint on **L1**, the left shoulder. Ours had it on the right. **Reverted below.** |

Remaining deliberate divergence from TLOU II: they use Square for melee and Triangle for interact, we use
Face Left for interact and R3 for heavy strike. Not changed without a user call.

D-pad Up/Down additionally carry the vendor plugin's `IA_PushToTalk` / `IA_DiscoveryMenu` rows. Asset-registry
referencers put their only handlers on `BP_MenuSystemCharacter` (MenuSystemPro's example pawn, which this
project does not use), so nothing fires from them here. Vendor content was left untouched.

Inventory `Gamepad_Special_Left` and Pause `Gamepad_Special_Right` were already authored and left
alone. Item slots 1–7 are reached through Quick Select, per the recommended layout.

### Quick Select moved to Face Top — user call

The user found clicking the left stick awkward and asked for Quick Select on Y. Face Top was Reload, and
Quick Select lives in `AZ_IMC_AlwaysAllowed` at **priority 0** while the gameplay context is priority 2 —
so simply adding Y there would have been consumed by Reload and never fired. The two were therefore
swapped, not duplicated:

| Action | Context | Was | Now |
| --- | --- | --- | --- |
| `AZ_IA_QuickSelect` | `AZ_IMC_AlwaysAllowed` (0) | `Gamepad_LeftThumbstick` | `Gamepad_FaceButton_Top` |
| `AZ_IA_RT_Reload` | `AZ_IMC_RT_PawnInputs` (2) | `Gamepad_FaceButton_Top` | `Gamepad_LeftThumbstick` |

Reload took the freed stick click rather than a D-pad direction so the thumb stays on the movement stick
during a reload. The legacy `PushToTalk` / `DiscoveryMenu` rows on D-pad Up/Down were left untouched.
Both keyboard rows (`Tab` for Quick Select, `R` for Reload) are unchanged.

Scoped contexts that also use these keys are all higher priority and only active while they own input, so
nothing collides: Inventory Split and Map ClearWaypoint on Face Top, Map TrackSelection on L3.

`Gamepad_DPad_Up` / `Gamepad_DPad_Down` were left to their existing legacy PushToTalk / DiscoveryMenu
rows in `AZ_IMC_AlwaysAllowed`; nothing was repurposed.

## Analog correctness (Module 2) — traced, not assumed

`OnMoveTriggered` clamps per component and does **not** normalize. `ProduceInput_Implementation`
rotates by control yaw, and `UAZ_MovementDirectionCapabilityComponent::ConstrainIntent` returns
`WorldIntent` unchanged when the path is clear and a proportional `ComputeSlideDelta` when sliding.
`SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove)` receives the magnitude intact — partial
stick deflection therefore produces partial speed. The radial dead zone caps stick magnitude at 1, so
diagonals stay inside the envelope. Keyboard rows are untouched, so WASD behaviour is bit-identical.

`OnLookTriggered` adds `LookVector.X * LookRateYaw` / `-LookVector.Y * LookRatePitch` with **no**
delta-time term. Time scaling therefore happens exactly once, on the stick mapping only; mouse deltas
are not scaled. Rates are editable in the mapping: 180 °/s yaw, 120 °/s pitch at full stick, and
`bEnableLegacyInputScales=True` with both scales left at the engine default of 1.0, so those numbers
reach `RotationInput` unmultiplied.

### Pitch sign — corrected after user test

The first pass shipped the stick without a negate, on the assumption that `MouseY` and `Gamepad_RightY`
share a sign. **They do not**, and the user reported the right stick inverted. The handler's `-` exists to
compensate for the mouse, whose Y is down-positive here; the stick's Y is up-positive, so the two
negations cancelled and pitch came out reversed:

| Source | Raw Y when pushed up | `-Y` reaches `AddPitchInput` | Result |
| --- | --- | --- | --- |
| Mouse | negative | positive | look up (correct) |
| Right stick, as first shipped | positive | negative | look **down** (inverted) |

Fixed with a mapping-local `InputModifierNegate` on the Look stick row, **Y only** (`x=false, y=true,
z=false`), inserted after the dead zone and before the rate scalar. Yaw was left alone: `MouseX` and
`Gamepad_RightX` are both right-positive, so it was never reversed. The mouse row is untouched.

### Dead zones do not stack

`Config/DefaultInput.ini` carries legacy `AxisConfig` entries with `DeadZone=0.25` for
`Gamepad_Left/Right X/Y`. `EnhancedPlayerInput.cpp` contains **no reference to `AxisConfig`**, so that
legacy per-axis dead zone never reaches an Enhanced Input mapping. The radial 0.15 authored on the stick
rows is the only dead zone in the path, and no global config was edited.

## GAS, capture and prompt routing — verified, unchanged

- `AbilityInputTagPressed`, `OnChangeFireModeInput` and `OnQuickSlotInput` all already return early on
  `IsInventoryInputCaptured()`, and Move/Look are gated by `SetIgnoreMoveInput/LookInput`. No gameplay
  action can fire behind an open UI, so the shared-button conflicts (B crouch vs Back, X interact vs
  carry, LT/RT aim/fire vs tabs, D-pad mode vs pan/focus) are structurally resolved. No new guard added.
- `SuppressHeldButtonsAfterMenu` enumerates **every Boolean action mapping** and tests `FKeyState::bDown`,
  so the new gamepad rows are covered without touching the barrier — including the triggers, which the
  Windows XInput interface delivers as real buttons (see the trigger note above).
- `RefreshInteractionPrompt` already resolves `Input.Action.Interact` through
  `CommonUI::GetFirstKeyForInputType` for the **current** device and already listens to
  `OnInputMethodChangedNative` and `ControlMappingsRebuiltDelegate`. Its `Unbound` text came solely from
  `CurrentKey.IsValid()` being false with no gamepad mapping present. Item, quest and checkpoint prompts
  all funnel through `AAZ_PlayerController::SetActivePickUpActor` → `PickupCaption` → that one function,
  so the single mapping fixes all three. The hardcoded legacy "Press E" strings are only used when the
  current device is keyboard/mouse **and** `E` is still a live binding, so they cannot leak onto a pad.

## Connect / disconnect hint switching (Module 7)

`UAZ_InputPresentationSubsystem` already chose the glyph *family* on device events but never changed the
active input *method*, so hints stayed on keyboard until the player pressed a pad button. Added:

- `AnnounceGamepadArrival` — on an owned, connected, hardware-identified gamepad, performs one
  `SetCurrentInputType(Gamepad)` if the platform supports it. Keyed on the device id, so repeated
  notifications about the same pad do not fight the player, and a pad that cannot be classified at
  connection time completes the transition when its metadata arrives.
- `AnnounceGamepadDeparture` — on disconnect or repairing away, falls back to keyboard/mouse **only**
  when the departing pad was the one presenting, no other owned pad remains, and the platform supports
  keyboard/mouse. Presentation only: no action cancellation, no preference write.
- Identity change clears the claim alongside the existing per-user reset.

The saved `SetUIPreferences(Family, MasterVolume, Error)` record is never written by any of this.

## Dormant actions — classified, not implemented

Native consumer counts (`Source/AZ`, action asset and input tag):

| Action asset | Native refs | Disposition |
| --- | --- | --- |
| `AZ_IA_RT_TogglePerspective`, `HoldBreath`, `ChangeShoulder`, `WeaponAccessory`, `Lethal`, `ToggleWeapon` | 0 | Dormant. Keyboard rows retained, no pad key, no feature invented. |
| `AZ_IA_RT_Melee` | 0 | Dormant. The three `Melee` hits in source are `Input.Action.MeleeAttack`, which belongs to **HeavyStrike**. |
| `Walk`, `Strafe`, `SecondaryWeapon` | 0 | Assets exist, absent from the RT context. Left absent. |

## Controller-only UI reachability (Module 6) — verified against current code

- **Grid navigation and target identity.** The grid wires explicit cell-to-cell navigation
  (`SetNavigationRuleExplicit`) and derives its carry/placement tile from the cursor position. With
  `bLinkCursorToGamepadFocus=True` (set in `Config/DefaultInput.ini`, consumed by
  `FCommonAnalogCursor::Tick`), CommonUI centres the cursor on the focused widget every tick, so the
  hover-derived tile and the cursor-positioned item popup both follow gamepad focus. No separate cursor
  or navigation system was added, per the brief's instruction to inspect this first.
- **Map.** Zoom (LB/RB), recenter (R3), place marker (X), clear (Y) and track (L3) are IMC-mapped.
  **Pan (D-pad) and select-at-center (Face Bottom) are not IMC actions** — `UAZ_MapCanvasWidget::NativeOnKeyDown`
  handles those keys directly and returns `FReply::Handled()`, so they consume the key at the Slate focus
  layer before Enhanced Input sees it. The brief's D-pad scope conflict is therefore resolved twice over:
  by Slate consumption, and by the capture guards in `OnQuickSlotInput` / `OnChangeFireModeInput`.
- **Quick Select.** All nine scoped commands already carry pad keys; cells 0–7 are reachable by D-pad
  focus plus Face Bottom activate.

### Rotate / MoveItem / Split are dead actions, not a conflict

Asset-registry referencer counts outside the mapping contexts:

| Action | Referencers | Meaning |
| --- | --- | --- |
| `IA_AZ_UI_InventoryRotate` | **0** | No consumer in C++ or any Blueprint. |
| `IA_AZ_UI_InventoryMoveItem` | **0** | Same. |
| `IA_AZ_UI_InventorySplit` | **0** | Same. |
| `IA_AZ_UI_InventoryContextMenu` | 1 | `AZ_WBP_GameInventoryMenu` — real consumer. |
| `IA_AZ_UI_InventorySelect` | 2 | Real. |
| `IA_AZ_UI_InventoryBack` | 3 | Real. |
| `IA_AZ_UI_InventoryTabLeft` | 2 | Real. |

Rotate and MoveItem sharing `Gamepad_FaceButton_Left` is therefore **not a functional conflict** — neither
does anything, and the shared key is consumed harmlessly inside a UI context where gameplay is already
captured. They are recorded as dormant. No rotation, move or split behaviour was invented to justify the
keys, and none of the three is advertised as working.

## Remaining limitations

1. The cursor-follows-focus behaviour depends on CommonUI's analog cursor being active for the local
   player in gamepad mode. The setting and the engine consumer were read; the on-screen result was not
   observed and should be confirmed in the manual pass.
2. LT/RT fire at XInput's own trigger threshold (~12% of travel), not at half-pull — see above. If that
   proves too light in the hand, the fix is an explicit `InputTriggerDown` with a raised
   `ActuationThreshold` on the analog `Gamepad_*TriggerAxis` key, not a change to the Boolean action.
   Not measured against hardware.
3. `Saved/SaveGames` holds no Enhanced Input user profile, so no stale remap can mask the new defaults.
   Nothing was deleted to achieve this.
4. Glyph artwork depends on the existing `BP_AZ_InventoryControllerData_*` assets. A valid mapping with
   no art will truthfully show the key's display name rather than falsely reading `Unbound`.
5. Nothing here was exercised in PIE. No automated tests were added and no synthetic input was injected.

## Module 6 follow-up — stick-driven map pointer

The brief's added section: the mouse can place the personal marker anywhere, the controller could only
place it at the view centre, and `ClampView` pins that centre to 0.5 at fit zoom and to an interior
interval at any zoom — so map edges and corners were unreachable with a pad. Implemented as a pointer that
moves independently of the view.

| Piece | Where |
| --- | --- |
| Pointer state, speed and dead zone | `UAZ_MapCanvasWidget` — `PointerLocal`, `bPointerActive`, `PointerSpeed` (900 u/s), `PointerDeadZone` (0.15) |
| Stick read | `UpdateControllerPointer` in `NativeTick`, from `GetInputAnalogKeyState(Gamepad_RightX/Y)` |
| Bounds | `ClampPointer` — drawn-map rect intersected with the widget rect |
| Drawing | `NativePaint` — gapped crosshair plus a ring when the pointer is active; the old faint centre tick remains for the no-pointer case |
| Commit | `PlaceWaypointAtPointer` / `SelectMarkerAtPointer`, both still going through `PlaceWaypointAt` → `LocalToMap` → `MakeWaypointFromMap` → `SetWaypoint` |
| Action routing | `UAZ_QuestMapPage::HandleMapPlaceWaypoint` and the canvas's Face Bottom select |

Decisions worth stating:

- **No Axis2D action was added to the Map context.** `HasSafeMapMappingContext` accepts exactly six
  non-consuming, triggerless Boolean UI actions; an Axis2D entry would fail it and disable the whole Map
  context along with its six working commands. Reading the axis directly also sidesteps the analog-cursor
  preprocessor, which can consume stick events before a widget sees them.
- **Right stick, not left.** The left stick drives CommonUI focus navigation between the journal and the
  canvas; using it for the pointer would move focus while aiming. Gameplay look is already ignored while
  the map owns input, so the right stick is free.
- **Bounds are the drawn map ∩ the widget.** At fit zoom the image is letterboxed and a pointer in the
  letterbox maps to nothing; zoomed in, the widget is the tighter bound. The intersection is what lets the
  pointer sit exactly on an edge or corner in both cases.
- **Nothing is committed by moving.** Only the Place action calls `SetWaypoint`; an invalid point is
  refused inside `PlaceWaypointAt` and the previous marker survives. No draft is published or saved.
- **Mouse handoff** is on the first real mouse move, set before the drag early-out, so placement can never
  use a stale stick pointer. Reopening the map resets the pointer and leaves the surface to the mouse until
  the stick is touched.

Not done, and why: the Map's on-screen instruction text lives in `WBP_AZ_QuestMapPage` as authored content,
not in C++ — there is no bound text block for it. It still needs a line explaining stick movement and the
real placement button.

Unverified until a controller pass: that `GetInputAnalogKeyState` actually returns the stick while the Map
owns input in this project's UI input mode. If it reads zero, the fallback is a scoped Axis2D action plus a
narrow widening of `HasSafeMapMappingContext` — not a global second cursor.

## Shoulders reverted to Run = LB, Sprint = RB — user call

After playing it, the user asked for the two shoulders back the other way round: **Run on LB, Sprint on RB**.
This is a deliberate departure from the TLOU II reference, which puts sprint on L1. Recorded rather than
argued: the layout is the user's, and the reference was only ever a starting point. Everything else in the
TLOU-aligned pass stands.
