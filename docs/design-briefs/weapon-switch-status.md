# Animated weapon switching — built, configured and saved

2026-09-11. User requested rifle/pistol holster and draw animations, with outgoing weapons remaining visible on their associated carry sockets.

## Design and verified source findings

The old equipment component kept one PresentedItem/PresentedWeapon pair. Physical-to-physical CommitSelection first attached the outgoing actor to CarrySocketName, then DestroyPresentation deleted it. Rifle-to-fists happened to retain that one actor. The fix retains one actor per selected inventory identity; drop/removal, pawn replacement and end play still clean up those actors.

Existing sockets and user tuning are preserved: rifle BackRifleSocket (mesh-owned, spine_04), pistol PistolHolsterSocket (skeleton-owned, thigh_r), on C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh.uasset. Hand sockets already exist for both weapons. Only one inactive weapon is displayed per carry socket, preferring the most recently holstered item; extra weapons sharing a socket stay cached and hidden.

Equipment owns an interruptible holster → draw → final commit transaction. QuickBar receives Deferred while it runs and Activated only at completion. Authority controls attachment and recovery deadlines independently of animation rendering. A transition token prevents old callbacks from completing a newer request. The committed selection and its magazine identity are retained until completion.

The existing RifleFire slot in WeaponFire is reused. Live MHC graph mask is spine_01/depth1 with mesh-space rotation, not the stale spine_02 recorded in older helpers. Source clips are nonadditive and have root motion disabled. Weapon-switch state suppresses aim/relaxed overlays, reticle, equipment-sourced actions and sprint until recovery. Moving legs remain controlled by existing locomotion; a stance change cancels/reconciles the transition.

Reliable weapon-actor cosmetic messages carry animation to observers; the controller-owned equipment component alone would not reach remote pawn observers. The same coordinator actor sequences outgoing/incoming poses. Socket changes preserve current visual placement and blend to the destination because sampled clip hand/carry origins do not align exactly with the user-tuned sockets.

## Profile authoring

C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_WeaponAnimationProfile.h adds FAZ_WeaponSwitchAnimation (Animation, AttachTime in source seconds), StandingDraw, StandingHolster, CrouchingDraw, CrouchingHolster, and shared slot/play-rate/blend/socket-transfer tuning.

Rifle sources under /Game/AZ/Assets/M16/Riffle_RTG_MH/:

| Clip suffix after AZ_RTG_MH_W2_ | Length | Candidate transfer time |
|---|---:|---:|
| Stand_Rlx_Equip_Back_Get_From_MOB | 1.533333 s | 0.566667 s |
| Stand_Rlx_Equip_Back_Return_To_MOB | 1.433333 s | 0.666667 s |
| Crouch_Equip_Back_Get_From_MOB | 1.900000 s | 0.533333 s |
| Crouch_Equip_Back_Return_To_MOB | 1.766667 s | 0.566667 s |

These times mark the measured reach-behind phase, not verified grip contact. Rifle prop origins remain roughly 40–53 cm apart even at their closest sampled approach, which is why transfer interpolation is required.

Pistol sources /Game/AZ/Assets/Pistol/AZ_Pistol_Equip (0.866667 s, candidate transfer 0.033333 s) and AZ_Pistol_UnEquip (1.333333 s, transfer 0.666667 s). Their closest hand/carry placement still differs by about 8–11 cm and 95–111 degrees. No crouched pistol equip/holster clips exist; its upper-body playback deliberately falls back to the standing clips. These transfer timings and blend durations require the user's visual review. No source clips, sockets, or AnimGraph changes are planned.

## Build and saved verification

Completed after the user's full build/restart. Main UnrealEditor-AZ.dll is dated 2026-09-11 22:14:01 UTC, newer than all changed switch sources; editor PID42508 started at22:14:14 UTC. Latest UBT check reported Result: Succeeded in2.43s. Fresh native reflection exposed FAZ_WeaponSwitchAnimation and every required profile field; no missing fields remained.

Ran C:/UnrealEngine/Games/AZ/Tools/weapon_switch_setup.py audit → backup → author → verify. Both DA_WeaponAnim_P01 and DA_WeaponAnim_Pistol were saved at22:17:19 UTC. Both on-disk hashes changed, both packages were clean, all assigned clips/timings/settings matched, and every unrelated profile field, socket transform and source clip hash matched the fresh baseline. No AnimBP or socket edits were required. No further build/restart is needed for this slice.

Receipts and the two-profile backup are under C:/UnrealEngine/Games/AZ/Saved/WeaponSwitch/: audit-readback.json, backup-readback.json, author-readback.json, verify-readback.json, saved-profiles.json and UBT-post-user-build.log. Future read-only inspection is main('verify'); do not recreate actors/assets or repeat authoring after a successful saved readback.

The user entered PIE before profiles were assigned; logs at22:15–22:16 correctly rejected missing switch clips. Play was stopped before authoring. The issued StopPIE tool found that it had already stopped. The user resumed Play after saving. No agent-started PIE or tests occurred, and no post-assignment switch attempt has yet been observed. Handoff timing, crouched-pistol fallback and network appearance still need the user's visual review.

Source review fixed shared-socket visibility priority, stale participant/actor validation, tracked rejection receipts, nested guards and recovery that outlasts a socket blend. All changed source files passed whitespace checks; Weapon code also passed Rider error inspection. Compilation is verified; gameplay visual/network acceptance remains user-owned.

Replication review found the inherited PickupSphere replicated independently, bypassing actor attachment callbacks. CommonUI cosmetic weapons now replicate their cosmetic flag and disable root-component replication for those representations only, using actor attachment replication. Each rendered peer preserves its own weapon placement during a transfer; an early attachment property update receives a provisional blend until the authored transfer message arrives. Legacy pickup roots retain their original replication. Existing fire/reload code is preserved.

Concurrent firearm-ready work added AZ_Inv_CommonUI_EquipmentReady.cpp and Ability.State.FirearmReady during implementation. Those changes were retained. ClearOutgoingInput now begins with its CancelFirearmReady cleanup. Do not overwrite the concurrent ready-state fields/functions or replace these shared source files from an old snapshot.

No automated tests or PIE/editor tests are authorized. User owns the gameplay check: rifle↔pistol, either weapon↔fists, quick-select mouse versus keys, interrupted switches, inventory drops, and visible back/thigh carry after switching.

## Follow-up: extra pose transition after switching

The user's 22:37–22:39 UTC log capture shows one selection commit and one incoming relaxed-idle pick per switch, with no FirearmReady, replay, or snap events. The runtime montage used automatic blend-out, which Unreal starts during the final 0.12 seconds of each clip. Equipment commits only at the full phase deadline, so this prematurely exposed the outgoing base pose before the new weapon's relaxed idle was selected.

AZ_Weapon.cpp now creates the transient switch montage explicitly and disables bEnableAutoBlendOut before Montage_Play (the instance snapshots the flag at playback). The final authored pose stays active until the existing phase advance, completion, cancellation, or ownership cleanup explicitly stops it. Inventory commit timing and profile assets are unchanged.

Read-only source pose sampling confirmed the rifle draw endpoint already matches relaxed idle (upper-body local rotations within 0.01 degrees). Pistol draw ends in the raised pistol idle and therefore still needs its normal single lowering blend into relaxed. Neither relaxed idle begins with a large extra movement. No user gameplay acceptance has yet been recorded for this fix.

Validation: UBT Result: Succeeded in 12.30 seconds; Live Coding applied the AZ module patch successfully at 22:45:45.858 UTC on 2026-09-11. Whitespace check passed. No editor restart, PIE start, or automated tests were performed. The running editor has the fix; a normal build before a later restart will retain it in the main DLL.

### Follow-up: remaining pistol-to-rifle twitch

User confirmed pistol looks correct but pistol-to-rifle still briefly leaves relaxed and returns. Post-patch logs at 22:47:44.667 and 22:47:54.192 show the incoming rifle idle's 0.20-second base crossfade begins only after commit, whereas the held draw fades out in 0.12 seconds. The remaining outgoing pistol base consequently leaks through that blend. Fresh live graph/profile inspection found no relaxed upper-body overlay; both profiles have RelaxedUpperBodyPose=null, and the RifleFire slot sits downstream of the base BlendStack.

Added the non-reflected, memberless TryGetDrawAnimationPresentation getter. A valid draw with its socket transfer applied supplies the target profile/tag solely to the AnimInstance's copied chooser inputs. Existing GetActiveAnimationProfile remains committed because the pawn also uses it for movement speeds. No ASC tags, inventory selection, ability grants, or magazine state change early. The base now gets approximately 0.97 seconds beneath rifle draw (0.83 beneath pistol draw) to complete its usual blend before commit. Cancel/invalid context falls back to the committed profile. Profile changes bypass the locomotion-transition push lock so moving switches can also prime their incoming base. The new profile log includes draw=1 to verify timing in the next user run.

This priming path uses the existing authority-owned WeaponTransition and addresses standalone/listen-host presentation. Remote presentation staging remains unverified and requires incoming profile/tag publication through the switch cosmetic protocol; the existing RPC carries animation/slot/rate only. No reflected fields or RPC signatures changed in this fix.

Build/recovery: Live Coding compilation succeeded in 32.45 seconds, but applying the patch triggered class reinstancing and an editor UObjectHash.cpp:650 fatal error at 22:52:50 UTC. It must not be treated as an applied patch. A normal build completed afterward: main UnrealEditor-AZ.dll is dated 22:53:19.352 UTC, newer than both changed implementation files, and the normal UBT verification returned Result: Succeeded in 1.78 seconds (up to date). The user restarted the editor, PID47124 at 22:53:28.615 UTC. This fresh editor loads the normal DLL containing the fix. No agent initiated editor restart, PIE, or tests. User visual acceptance remains pending.
