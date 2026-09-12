# Rifle animation and automatic fire — built and wired, ready for user validation

2026-09-09: User confirmed everything works after the held-input fix. Recoil is
saved as the next task after restart; no recoil code has been changed. Resume from
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-recoil-next-session.md.

## Latest: one-frame AUTO release fixed; user confirmed working

2026-09-09 user reported automatic fire not working. AZ.log contains 23 AUTO
actions, each one accepted shot, followed by FireAnim end interrupt=0 about one
frame later (latest examples 22–26 ms). Example 17:42:34.268 begins action
D46E14424CCEBFF71D222D9A6BE30324; .293 ends it despite seven rounds remaining.

The pre-fix PrimaryAttack action had InputTriggerPressed (threshold .5), no modifiers,
and its LMB mapping has no additional triggers. Pressed transitions from Triggered
to None on the next held frame, generating Completed. Completed routes to ASC
InputReleased and WaitInputRelease, which normally ends AUTO and stops its montage.
This is an input-lifetime defect; log playing=1 alone never proved rendered motion.

Built fix in the existing PC function bodies: PRIMARY now attempts activation
once from Started, retaining melee's buffered attempt and firearm's unbuffered
attempt; PRIMARY Held is ignored. Then replace the action's Pressed trigger with
default Boolean hold evaluation. This preserves one-shot SINGLE and the prior
one-pulse primary melee behavior while making release mean actual mouse release.
Three independent source reviews corroborated the diagnosis/fix. No second
concrete animation graph fault was found. No new tests or PIE were started.

**Applied and saved at 17:59:04 UTC, September 9.** User completed the full build
and restarted. Verified Result: Succeeded (15.38 seconds), main DLL timestamp
17:55:58 newer than the controller edit, and editor start17:56:11.
Ran `prepare_primary_hold_input()` from
C:/UnrealEngine/Games/AZ/Tools/rifle_automatic_setup.py. It removed only the
PrimaryAttack Pressed trigger, preserving Boolean type, empty modifiers, and the
single unchanged LMB mapping. The action and mapping now both have no explicit
triggers. Final readback found no dirty content or PIE. No further build/restart
is required for this fix. The user subsequently confirmed everything works.

Receipt: C:/UnrealEngine/Games/AZ/Saved/RifleAutomatic/primary-hold-input.json.
Backup: C:/UnrealEngine/Games/AZ/Saved/Backups/RifleAutomaticInput/20260909T175904/.
Do not rerun main(prepare=True) just to fix input: it also resets profile tuning.

Deciding user-run check: hold LMB in AUTO. One action ID must produce multiple
[Fire] entries roughly .1 s apart and only end when released/empty/interrupted.
Then check SINGLE and a primary fist tap for one activation, and menu/mode/switch
for no restart from a held button.

Updated 2026-09-09. User confirmed the first single-shot firing slice works, then
requested hero firing animation and automatic mode. They subsequently ordered
reload before configurable recoil. No new gameplay tests were started.

## Implemented in source

- Automatic fire repeats inside the existing authority-owned firing ability.
  Each callback accepts at most one shot, revalidates source/item/equipment/mode
  revision, and debits the canonical magazine. Inventory deadlines compensate
  ordinary frame quantization without catch-up bursts after a hitch.
- SINGLE keeps one shot per press. Automatic fire starts from a fresh press and
  stops on release, empty magazine, aim/menu/mode/selection/hard interruption.
- Per-rifle Single/Automatic selection and revision live in ItemState and travel
  with pickup/drop payloads. Definition exposes DefaultFireMode/SupportedFireModes.
  Equipment validates mode requests on authority and cancels fire while preserving
  held aim. PC ChangeFireModeAction binds Started; the intended initial key is X,
  preserving the existing mapping. Optional B/MMB choice remains unanswered.
- Reliable tokened animation begin/end controls exact dynamic montages on the
  hero's actual body mesh. Normal SINGLE release finishes its recoil; later aim
  release still stops the remaining tail. WeaponFire montage group protects
  FullBody actions. Mode/selection/owner/destruction cleanup stops only this montage.
- Profile fields: SingleFireAnimation, AutomaticFireAnimation, FireAnimationSlot,
  FireAnimationBlendIn/Out and AutomaticFireAnimationShotsPerCycle. Actor mirrors
  only observer presentation settings. Shared PlayerUI view carries selected mode;
  active CommonUI HUD supports optional FireModeText (SINGLE/AUTO).

## Asset findings and saved authoring

Requested sources, both ordinary nonadditive MetaHuman poses with root motion off:

- /Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_W2_Stand_Fire_Single_IPC — 1 second.
- /Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_W2_Stand_Fire_Continuous — .433333 seconds.

Raw pose sampling found four continuous recoil pulses; first/last hand transforms
match within .00002 cm. Author shots-per-cycle=4, giving playback rate1.083333 at
10 shots/second. The continuous loop is a Default-section self-loop, not a montage
restart per bullet. Pose samples: Saved/RifleAutomatic/source-poses.json.

The existing AZ_AM_Rifle_Fire montage uses a different non-IPC sequence and a
SurvivalMan skeleton; the new path uses the user's exact requested sources.

Authoring scripts executed successfully; their explicit target assets are saved:

- C:/UnrealEngine/Games/AZ/Tools/rifle_automatic_setup.py — after full build,
  assigns profile clips, registers RifleFire slot in WeaponFire group on both
  MetaHuman and ABP/SurvivalMan skeletons, assigns PC ChangeFireModeAction.
- C:/UnrealEngine/Games/AZ/Tools/rifle_automatic_graph_setup.py — caches AdditiveLeans
  output, masks RifleFire slot above spine_02, feeds existing AdiativePoses cache.
  Both existing AO paths receive the result; legs, curves, root and downstream
  FullBody/ORB/hand correction nodes remain. prepare() never compiles/saves the ABP.
- C:/UnrealEngine/Games/AZ/Tools/rifle_fire_mode_widget.py — ProgrammaticToolset
  audit/author of one native editable FireModeText widget in the existing weapon HUD.

Live graph has 23 nodes. Anchor source F6B656114D0583D74A57979ABE09B9B6; destination
F4FF81E249001A7E2683E780203B448A. Hand correction nodes are wired again in live state;
do not unlink them based on the older retarget handoff.

Five firing nodes were added to that 23-node baseline: one cache, two cache uses,
RifleFire slot and a spine_02 mask. All original nodes remain. Graph receipt:
C:/UnrealEngine/Games/AZ/Saved/RifleAutomatic/graph-receipt.json. Both skeletons
report RifleFire=WeaponFire and FullBody=DefaultGroup.

The new FireModeText is authored within the existing weapon HUD, using its Oswald
font and an empty/collapsed runtime default. Native code supplies SINGLE/AUTO.
The authoring script handles native BindWidgetOptional placeholders (GetWidgets
returns widget=None until a real widget is authored). Profile/input/skeleton/HUD
backups: C:/UnrealEngine/Games/AZ/Saved/Backups/RifleAutomatic/20260909T173101/.

## Build and completion

User completed the full build and restarted. Verified Result: Succeeded in the
9.33-second final build, main UnrealEditor-AZ.dll newer than changed native sources,
and all new profile/mode/slot APIs exposed in the restarted editor. Build receipt:
C:/UnrealEngine/Games/AZ/Saved/RifleAutomatic/native-build.log.

The MHC AnimBP, PlayerController BP and WBP_AZ_GameHUD compiled through the dedicated
native BlueprintTools.compile_blueprint tool and were explicitly saved. All three
report UpToDate. Graph verify passed; readback confirms both exact source clips,
four pulses/cycle, X input, independent slot groups, and Single/Automatic support
in the pickup template and both placed rifles. No dirty content/maps or PIE remained.
Final receipt: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-automatic-asset-readback.json.

No further rebuild/restart is needed for this slice. Next is user PIE: SINGLE/AUTO,
quick taps/empty, standing/crouched movement, aim/menu/switch cleanup and mode
persistence through drop/re-pick. A very short AUTO tap or a burst with one remaining
round can end before its first animation tick; inspect visual acceptance of that
edge. No automated tests or gameplay tests were started by Codex.

## Requested next steps (preserve this order)

1. **Reload before recoil.** R should work with a partial magazine; an empty
   inserted magazine should trigger automatic reload. Add rifle reload animation.
   The user said capacity should use what remains in existing magazines, which is
   ambiguous against the established individual-magazine swap design. A question
   is pending: swap to the fullest compatible spare, keeping all counts/identities,
   or transfer rounds from spare magazines to top up the current magazine. Do not
   silently implement pooling or treat the suggested option as an answer.
   Sources found: AZ_RTG_MH_W2_Stand_Relaxed_Reload_IPC, Stand_Aim_Reload_IPC,
   Crouch_Aim_Reload_IPC and Crouch_Rlx_Reload in the same MH folder. They still need
   inspection for timing/pose; no reload source or asset changes have been made.
2. **Configurable recoil.** User asks for GAS-example-like recoil or a better
   approach, with adjustable recoil radius. Camera-kick vs spread-vs-both question
   remains pending. Existing AZ_GATA_Trace has BaseSpread/TargetingSpreadIncrement/
   TargetingSpreadMax, but increments inside targeting trace evaluation, not the
   accepted shot. Recommended next design: shot-driven authoritative spread with
   max angular radius, per-shot growth, recovery delay/speed; separately adjustable
   owner camera kick. Drive crosshair radius from the same cone and current FOV.
   Keep ammunition and shot acceptance in their existing owners. No recoil driver
   or tuning fields were implemented in this animation/automatic slice.
