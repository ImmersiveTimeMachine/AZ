# Player animation playback: fixed 1x

2026-09-11 local / 2026-09-12 UTC. User requested all player animations at authored speed, neither faster nor slower, and explicitly confirmed removing player melee hit-stop too.

## Findings and implementation

- Latest user logs showed the inner turn-in-place sample reaching 3.6x from body yaw speed / 67 degrees per second, even though the outer BlendStack log printed rate=1. GetWeaponLoopPlayRate now returns 1 for turns and locomotion; its existing turn diagnostic reports the fixed rate. Preserve this policy when tuning facing or movement: do not reintroduce animation speed compensation.
- Automatic firearm presentation no longer calculates playback from fire cadence and recoil pulses per cycle. Both single and automatic hero fire play at 1. Actual shot timing/ammo remain inventory-owned.
- Both weapon profiles were saved with loop scaling disabled and min=max=1. Reload and switch rates were already 1 and remain 1, so their existing authority duration/attachment calculations still match playback. Do not hardcode a different duration or alter fire cadence as compensation.
- ApplyHitStop skips player-state-owned ASCs and player-controlled pawns. This covers local and remote player identities and prevents impact slowdown or added hit-stop watchdog time for the player. NPC hit-stop behavior is retained.
- Eight wall-jab montages now retract at -1 instead of -0.8 (negative direction is necessary to retract). Their durations were rebuilt to 0.08/0.14/0.20/0.26 seconds; source ranges, section starts, blend settings, and zero-notify behavior are preserved. Tools/melee_wall_content_build.py now authors RETRACT_RATE=1.
- AM_Grab_Hero GroundMunch segment previously used 1.01492536 to fit 2.266667 source seconds into the paired 2.233333-second section. Its rate is now 1 and source end is 2.233333345, preserving all section boundaries, loop links and the 13.2-second montage length. Raw end-frame differences were at most 0.0241 cm / 0.0208 degrees, with unchanged root/pelvis. Original source sequence is untouched.

## Verification and saved receipts

Live audit checked 1,237 weapon sequences (919 rifle, 300 pistol, 18 pack); all RateScale values were already 1. Seven graph sequence/blendspace players, the outer BlendStack, and hero/weapon mesh global rates were also 1. The inner BlendStack uses GetWeaponLoopPlayRate. Other inspected player montage tasks (melee, hit reaction, strike, grabbed/escape, death) already explicitly pass 1.

Live Coding succeeded: locomotion/fire fix UBT 10.85s, AZ patch 02:15:35.962 UTC; player hit-stop fix UBT 13.30s, AZ patch 02:16:59.868 UTC, editor reported success at 02:17:03.350 UTC. No new reflected fields or header changes were made by this task. Concurrent work changed/reinstanced WeaponAnimationProfile during the second patch; final live profile readback still confirmed all requested settings.

Both profiles and nine melee montages are saved. Final melee readback checked nine montages/23 segments at playback magnitude 1, including source RateScale and montage RateScale. Wall retraction remains reverse. Whitespace checks passed. No PIE or tests were started by Codex; visual gameplay acceptance remains with the user.

Receipts and original asset backups: C:/UnrealEngine/Games/AZ/Saved/AnimationRate/. Relevant files: live-weapon-rate-audit.json, fixed-rate-profiles.json, final-live-profiles.json, wall-retraction-rates.json, grab-segment-rate.json, final-melee-rate-readback.json. The first profile save attempt occurred during user-started PIE and failed; both profiles were successfully saved after the user ended PIE. Do not treat the failed attempt as completion.

The running editor has the code patches. Use a normal build before a later editor restart so the main DLL retains them. Movement-facing tuning at fixed animation speed and multiplayer visual appearance remain user-review items.
