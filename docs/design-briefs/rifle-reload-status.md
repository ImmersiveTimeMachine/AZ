# Rifle reload — runtime swaps verified; loaded-spare HUD correction

**Superseded policy:** the user approved circular manual reload, exact inventory
magazine loading and inserted-only HUD ammunition. Continue from
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-circular-magazines-status.md.
The fullest-magazine rules and MAGS display below record the preceding slice.

Updated 2026-09-09. User accepted recoil and requested reload. Requirements carried
forward: R during partial/empty state, automatic reload when ammunition runs out,
and hero rifle reload animation. No gameplay tests were started by Codex.

Latest continuation: final build succeeded (25.46 seconds; main DLL23:45:51 UTC,
editor restart23:45:57), newer than all reload review fixes. User's R attempt had
no action because asset assignment was still pending: no Reload Blueprint/input
row and all four profile clips were None. Those assignments are now completed,
both affected Blueprints compiled UpToDate, and all five target packages saved.
The reload assets remain complete. The follow-up HUD correction below is a C++
function-body change; its compile status is recorded separately.

## Magazine-count and single-fire follow-up

September 9 local / September 10 UTC: the user reports MAGS never decreases and
asks whether SINGLE can reload. The counter counted every compatible backpack
magazine, including retained empty magazines. Corrected GetWeaponAmmoSnapshot to
count only compatible magazines containing rounds. Empty magazines still exist
in inventory. Keep the short MAGS label and existing icon/layout/native binding;
no new fields, pooled reserve store or asset edits are required.

Validation: UBT Result: Succeeded,31.90s; Live Coding confirmed the AZ module patch
at00:30:28.391 UTC and completed at00:30:30.442. The correction is loaded in the
current editor session. A normal build is still needed for the next editor launch
because Live Coding patches do not update the main DLL. Focused git diff --check
passed. The changed count still needs the user's empty-reload visual check.

An empty-to-loaded swap now reduces the displayed spare count by one. A partial
swap keeps it unchanged because the outgoing magazine still contains ammunition.
For example, 5 loaded + 17 spare becomes 17 loaded + 5 spare, not 22 loaded.
The count is not the number of immediately eligible reloads: a partly loaded
spare remains counted even if the currently inserted magazine contains more.

Observed user gameplay in C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:

- AUTO run at 00:21–00:22 UTC: six reserved/completed swaps, including two
  empty-magazine replacements. Incoming/outgoing IDs and counts match between
  reservation and commit. All six ended committed=1, cancelled=0.
- Exactly 77 accepted shots consumed 30 + 30 + 17 rounds across three magazine
  IDs; each magazine's recorded count decreased by one per accepted shot to zero,
  including after reinsertion. No reload-created ammunition appears in this run.
- At 00:21:44.667, incoming84C12.../17 replaced outgoing3F070.../11;
  subsequent reloads reused those same magazines with their remaining counts.
- SINGLE sessions at 00:24 and 00:26 show rifle pickups and shooting but no spare
  magazine pickups. The 00:24 rifle fired its original30 rounds to zero.
  Read-only live snapshot at00:28:47 confirms SINGLE,30/30,spares0 in the next
  user-started session. Missing spares explain why those runs could not reload.

Three independent source reviews confirm manual R and the final-shot automatic
reload path do not depend on SINGLE versus AUTO. R requires a compatible spare
with more rounds than the inserted magazine; full/no-fuller-spare attempts are
no-ops. SINGLE reload with a spare still needs an observed user gameplay pass.

Remaining tuning: make rejected R explain full/no fuller spare; review the5.3s
relaxed clip's feel versus2s aimed; align gameplay commit to an authored insertion
event and add magazine/weapon props in the later presentation pass. If different
magazine capacities are introduced, decide whether a full20/20 can upgrade to a
30/30 spare (current full-magazine rule rejects it). These are follow-ups, not
changes to the current swap policy.

## Magazine policy

Implementing the existing **magazine swap** contract. This was explicitly stated
as the working assumption after the optional 5-loaded/17-spare question received
no reply. Reload loads the fullest compatible spare and retains the old magazine's
remaining rounds. It does not top up or pool rounds. User may still correct this
policy; do not attribute an explicit answer to them.

Full inserted magazines and reloads with no fuller eligible spare are no-ops.
Empty/no-magazine weapons can load a nonempty compatible spare. Tie-breaking is
stable by GUID. Old partial/empty magazines remain real items. Firing after reload
requires a fresh primary press; automatic reload does not resume a held fire input.

## Source implementation

- Inventory component owns one native reload reservation: weapon/source/generation,
  action ID, both magazine IDs/counts/revisions, incoming placement and reserved
  outgoing return placement. Preflight accounts for the incoming magazine's vacated
  cells; other item footprints/categories use real grid capacity checks.
- Commit atomically swaps the existing objects' relationships/locations/placements,
  preserves all round counts, increments both magazine AmmoRevision values, marks
  committed before publishing, and tolerates duplicate live-token commit callbacks.
  End clears only the matching reservation, never reverses a committed swap.
- Reserved source/spare/return space is protected against competing fire, mode,
  moves, swapped targets, pickups and spare removals. Active-rifle drop retains the
  existing spawn-before-transfer order and guards reentry while canceling reload.
- New UAZ_GA_FirearmReload is cancelable, InstancedPerActor and ServerInitiated.
  Authority reserves and commits; owning clients mirror cancellation only, with
  bServerRespectsRemoteAbilityCancellation enabled. Current source/UI/equipment is
  rechecked even when server-success activation bypasses CanActivateAbility.
- Actual Mover crouch state is latched and stance changes cancel. Menu, quick select,
  hard interruption, source/owner change, drop and switch cancel consistently.
  Held aim's lifetime is preserved; the weapon AO layer fades during reload.
- Inventory commits at **full clip completion** in this initial animation slice,
  using Length/(Sequence RateScale * Profile PlayRate). No guessed insertion notify
  or animation-notify ammo write. Normal montage completion is not authoritative.
- Weapon owns a separate reliable reload animation token/montage in the existing
  RifleFire/WeaponFire upper-body slot. External StopReloadAnimation interrupts
  gameplay too, preventing invisible later commits after a failed weapon switch.
  Local-owner starts reject blocked UI/source/stance and retain stale-token guards;
  observers do not depend on private inventory/controller state.
- After the final accepted shot finishes recoil/damage/effects, firing ends and
  queues one weak next-tick RequestReloadIfEmpty with source/item/generation/magazine
  revision. Fresh dry LMB uses the same guarded entry. No inventory-notification loop.
- PC treats Reload as Started-only/unbuffered and ignores Held. R's fist-heavy
  mapping stays; only the active rifle gains the new Reload ability. Menu cleanup
  clears the Reload input as well as existing firearm inputs.

## Animation content and authoring

All four existing MetaHuman sources are nonadditive, RootMotion=false and RateScale1:

| Profile field | Source under /Game/AZ/Assets/M16/Riffle_RTG_MH/ | Natural duration |
|---|---|---:|
| StandingReloadAnimation | AZ_RTG_MH_W2_Stand_Relaxed_Reload_IPC | 5.3 s |
| StandingAimReloadAnimation | AZ_RTG_MH_W2_Stand_Aim_Reload_IPC | 2.0 s |
| CrouchingReloadAnimation | AZ_RTG_MH_W2_Crouch_Rlx_Reload | 2.333 s |
| CrouchingAimReloadAnimation | AZ_RTG_MH_W2_Crouch_Aim_Reload_IPC | 2.0 s |

New profile tuning: ReloadAnimationPlayRate1, BlendIn.1, BlendOut.15. Actual duration
uses these clips, not the unused legacy WeaponState.ReloadSpeed2 field. Source clip
flags/root tracks remain unchanged; existing graph mask excludes root/pelvis.
Detailed weapon-rig and detached-magazine prop animation remain a later presentation
pass under the prior major-systems-first direction.

Prepared C:/UnrealEngine/Games/AZ/Tools/rifle_reload_setup.py:

- main('audit') passed: three rifle definitions, all four clips, no Reload input
  row yet. Native class currently exists from an intermediate user build.
- main('assign') backs up current rifle/map/profile/input/new-ability assets, creates
  /Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/BP_AZ_GA_FirearmReload, assigns four
  profile clips/rates, adds Input.Action.Reload -> existing AZ_IA_RT_Reload, and adds
  the source-owned grant to the template and both L_001 rifles via detached manifests.
- Native compile/save happens after script return. main('verify') checks grants,
  preserved recoil/fire/reticle/initial-magazine fields, unrelated input rows and
  Blueprint UpToDate/saved state.
- Earlier aim/fire author scripts now preserve the known Reload grant on reruns
  without enabling it implicitly. Never run the unguarded inventory foundation script.

Reload assets are now written: new Reload Blueprint, four profile clips and timing
defaults, exactly one Input.Action.Reload -> AZ_IA_RT_Reload entry, and one Reload
grant alongside Aim/Fire in the rifle template and both placed L_001 rifles. All
unrelated manifest fields, initial magazines and existing input rows were preserved.
Backups: C:/UnrealEngine/Games/AZ/Saved/Backups/RifleReload/20260909T235418/.

The first assignment's verification exposed Python GameplayTag wrapper identity
comparison; it now compares the actual TagName, preventing duplicate rows on resume.
Unreal refused saves when user PIE restarted during authoring. Codex announced and
stopped those running sessions to finish the explicit asset saves; no PIE was
started by Codex. All targets saved at00:05:04–05 UTC September10 (September9 local).
Read-only verify now resolves the exact saved editor-object paths even during PIE,
so subsequent user gameplay need not be interrupted for verification.

## Verification and exact continuation

Three independent source reviews checked inventory conservation/reservations,
ability networking/timing and animation ownership. Fixed two identified issues:
late owner activation after UI/stance changes, and cosmetic stops leaving an active
timer that could commit invisibly. Script syntax and git diff --check passed.

The intermediate23:29 build was superseded by the successful23:45 final build,
including the bCrouched RPC argument and owner activation/cancellation fixes.
Final saved readback confirmed ServerInitiated policy, Input.Action.Reload, the
native instance's Ability.State.Reloading tag, all animation references/timings,
three rifle definitions and both Blueprints UpToDate. Unrelated dirty hero content
was left alone. Receipt:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-reload-asset-readback.json.
The user-run AUTO transactions above now verify ordinary and empty reloads with
ammunition conservation. SINGLE with a spare and interruption cases remain
unverified in gameplay. No automated tests added/run; Codex did not start PIE.

Next:

1. User PIE: pick up a full spare magazine, choose SINGLE, fire a few rounds, press R. The aimed
   clip completes in2 seconds; standing relaxed takes5.3 seconds. Ammo commits only
   at the end and the old magazine keeps its remaining rounds.
2. Check loaded-spare count decreases after empty reload, no-fuller-spare/full no-op, automatic empty reload in SINGLE,
   no-mag dry click, standing/crouched/aimed variants, move/crouch/UI/switch/drop
   interruptions and magazine conservation. Compare [Inventory] Reload reserved/
   committed/end and [Reload] begin/end logs with action and magazine IDs.

RMB release alone should not cancel an in-progress reload; changing actual stance
does. Manual R while firing cancels fire only after a reload candidate is reserved.
After completion a new LMB press is needed. The feature is ready for this user-run
check; the earlier R attempt preceded its missing asset assignments.
